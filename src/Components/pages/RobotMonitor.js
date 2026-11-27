import React, { useState, useEffect, useRef, useCallback } from 'react';
import mqtt from 'mqtt';
import styles from "../../styles/RobotMonitor.css"

const TOTAL_SEGMENTS = 12;
const JOYSTICK_THROTTLE_MS = 150;

/**
 * MQTT configuration.
 * - brokerUrl must be a WebSocket endpoint (ws:// or wss://), not the raw
 *   MQTT TCP port (1883). Mosquitto, for example, needs a `listener 9001`
 *   + `protocol websockets` block in its config to expose this.
 * - `topics` are what the ESP32 PUBLISHES to (dashboard subscribes).
 * - `commands` are what the ESP32 SUBSCRIBES to (dashboard publishes).
 * Adjust these strings to match your firmware.
 */
const MQTT_CONFIG = {
  brokerUrl: 'wss://broker.hivemq.com:8884/mqtt',
  options: {
    clientId: `dashboard_${Math.random().toString(16).slice(2, 10)}`,
    // username: 'user',
    // password: 'pass',
    reconnectPeriod: 2000,
  },
  topics: {
    battery: 'robot/battery',         // payload: {"voltage":12.1,"percent":76}
    temperature: 'robot/temperature', // payload: {"temperature":34.2}
    direction: 'robot/direction',     // payload: {"direction":"forward"}
    motor: 'robot/motor',             // payload: {"motor":"on"}
    pwm: 'robot/pwm',                 // payload: {"pwm_value":50} (velocidade da marcha, %)
    imu: 'robot/imu',                 // payload: {"pitch":1.2,"roll":-0.4}
    servos: 'robot/servos',           // payload: [90,90,90,90,90,90,90,90,90,90]
  },
  commands: {
    motorOn: 'robot/cmd/motor_on',
    motorOff: 'robot/cmd/motor_off',
    forward: 'robot/cmd/forward',
    reverse: 'robot/cmd/reverse',
    left: 'robot/cmd/motor_e',
    right: 'robot/cmd/motor_d',
    pwmOn: 'robot/cmd/pwm_on',
    pwmOff: 'robot/cmd/pwm_off',
    pwm25: 'robot/cmd/pwm_25',
    pwm50: 'robot/cmd/pwm_50',
    pwm75: 'robot/cmd/pwm_75',
    slider: 'robot/cmd/slider',       // payload: "50"
    drive: 'robot/cmd/drive',         // payload: {"angle":90,"force":0.8}
    requestTemperature: 'robot/cmd/temperature', // LER button
    stop: 'robot/cmd/stop',           // para de andar e fica em pé
    relax: 'robot/cmd/relax',         // solta todos os servos imediatamente
    pose: 'robot/cmd/pose',           // "stand" | "crouch"
    balance: 'robot/cmd/balance',     // "on" | "off"
    imuZero: 'robot/cmd/imu_zero',    // define a posição atual como nivelada
    servo: 'robot/cmd/servo',         // {"id":3,"angle":95} calibração ao vivo
  },
};

// Nomes das 10 juntas: 0-4 lado direito, 5-9 lado esquerdo (3 da perna + 2 do braço)
const JOINT_NAMES = ['Quadril', 'Coxa', 'Perna', 'Ombro frente/trás', 'Ombro lateral'];
const SERVO_LABELS = [
  ...JOINT_NAMES.map((n) => `D · ${n}`),
  ...JOINT_NAMES.map((n) => `E · ${n}`),
];

/** Circular gauge, replaces the old Google Charts gauge */
function CircularGauge({ value, min = 0, max = 100, label, unit = '', size = 160 }) {
  const radius = size / 2 - 12;
  const circumference = 2 * Math.PI * radius;
  const clamped = Math.min(Math.max(value, min), max);
  const pct = (clamped - min) / (max - min);
  const offset = circumference * (1 - pct);

  const color = pct < 0.2 ? '#ff3b3b' : pct < 0.5 ? '#ffaa00' : '#00eaff';

  return (
    <div className="gauge" style={{ width: size, height: size }}>
      <svg width={size} height={size}>
        <circle
          cx={size / 2}
          cy={size / 2}
          r={radius}
          className="gauge-track"
        />
        <circle
          cx={size / 2}
          cy={size / 2}
          r={radius}
          stroke={color}
          strokeWidth="10"
          fill="none"
          strokeDasharray={circumference}
          strokeDashoffset={offset}
          strokeLinecap="round"
          transform={`rotate(-90 ${size / 2} ${size / 2})`}
          style={{ transition: 'stroke-dashoffset 0.5s ease, stroke 0.3s ease' }}
        />
      </svg>
      <div className="gauge-readout">
        <span className="gauge-value">{value != null ? value.toFixed ? value.toFixed(1) : value : '--'}{unit}</span>
        <span className="gauge-label">{label}</span>
      </div>
    </div>
  );
}

/** Segmented battery bar, replaces the manually-built <div class="segment"> nodes */
function BatteryBar({ percent }) {
  const activeCount = Math.floor((percent / 100) * TOTAL_SEGMENTS);
  return (
    <div className="battery-bar">
      {Array.from({ length: TOTAL_SEGMENTS }).map((_, i) => (
        <div
          key={i}
          className={`segment${i < activeCount ? ' active' : ''}`}
          style={{ animationDelay: i < activeCount ? `${i * 0.1}s` : '0s' }}
        />
      ))}
    </div>
  );
}

/** Custom joystick (replaces nipplejs). Reports angle (deg) and force (0-1) while dragging. */
function Joystick({ onMove, onEnd, size = 140 }) {
  const zoneRef = useRef(null);
  const knobRef = useRef(null);
  const draggingRef = useRef(false);
  const lastSentRef = useRef(0);

  const handlePointerMove = useCallback((clientX, clientY) => {
    const zone = zoneRef.current;
    if (!zone) return;
    const rect = zone.getBoundingClientRect();
    const cx = rect.left + rect.width / 2;
    const cy = rect.top + rect.height / 2;
    let dx = clientX - cx;
    let dy = clientY - cy;

    const maxR = rect.width / 2;
    const dist = Math.min(Math.hypot(dx, dy), maxR);
    const angleRad = Math.atan2(-dy, dx); // invert y so "up" is positive
    const angleDeg = (angleRad * 180) / Math.PI;
    const force = +(dist / maxR).toFixed(2);

    const knobX = Math.cos(angleRad) * dist;
    const knobY = -Math.sin(angleRad) * dist;
    if (knobRef.current) {
      knobRef.current.style.transform = `translate(${knobX}px, ${knobY}px)`;
    }

    const now = Date.now();
    if (now - lastSentRef.current > JOYSTICK_THROTTLE_MS) {
      onMove({ angle: (angleDeg + 360) % 360, force });
      lastSentRef.current = now;
    }
  }, [onMove]);

  const resetKnob = () => {
    if (knobRef.current) knobRef.current.style.transform = 'translate(0px, 0px)';
  };

  const stopDragging = useCallback(() => {
    if (!draggingRef.current) return;
    draggingRef.current = false;
    resetKnob();
    onEnd();
  }, [onEnd]);

  useEffect(() => {
    const onMouseMove = (e) => draggingRef.current && handlePointerMove(e.clientX, e.clientY);
    const onTouchMove = (e) => {
      if (!draggingRef.current) return;
      const t = e.touches[0];
      if (t) handlePointerMove(t.clientX, t.clientY);
    };
    window.addEventListener('mousemove', onMouseMove);
    window.addEventListener('mouseup', stopDragging);
    window.addEventListener('touchmove', onTouchMove, { passive: true });
    window.addEventListener('touchend', stopDragging);
    return () => {
      window.removeEventListener('mousemove', onMouseMove);
      window.removeEventListener('mouseup', stopDragging);
      window.removeEventListener('touchmove', onTouchMove);
      window.removeEventListener('touchend', stopDragging);
    };
  }, [handlePointerMove, stopDragging]);

  return (
    <div
      ref={zoneRef}
      className="joystick-zone"
      style={{ width: size, height: size }}
      onMouseDown={(e) => { draggingRef.current = true; handlePointerMove(e.clientX, e.clientY); }}
      onTouchStart={(e) => {
        draggingRef.current = true;
        const t = e.touches[0];
        if (t) handlePointerMove(t.clientX, t.clientY);
      }}
    >
      <div ref={knobRef} className="joystick-knob" />
    </div>
  );
}

export default function RobotMonitor() {
  const [voltage, setVoltage] = useState(null);
  const [percent, setPercent] = useState(0);
  const [temperature, setTemperature] = useState(null);
  const [direction, setDirection] = useState('--');
  const [motorStatus, setMotorStatus] = useState('--');
  const [pwmValue, setPwmValue] = useState('--');
  const [sliderValue, setSliderValue] = useState(50);
  const [connected, setConnected] = useState(false);
  const [imu, setImu] = useState(null); // null = ainda não chegou nenhuma mensagem do ESP32
  const [servoAngles, setServoAngles] = useState(Array(10).fill(90)); // valores dos sliders de calibração
  const [balanceOn, setBalanceOn] = useState(false);
  const lastServoSentRef = useRef(0);

  const hoverSoundRef = useRef(null);
  const clickSoundRef = useRef(null);
  const clientRef = useRef(null);
  const audioUnlockedRef = useRef(false);

  // Browsers only allow audio.play() in direct response to a "real" user
  // gesture (click/keydown/touch) — a mouseenter (hover) does NOT count,
  // so the hover sound gets silently blocked until the audio element has
  // been "unlocked" by at least one successful play() from a real gesture.
  // This listens for the page's first click and primes both <audio> tags.
  useEffect(() => {
    const unlock = () => {
      if (audioUnlockedRef.current) return;
      [hoverSoundRef.current, clickSoundRef.current].forEach((el) => {
        if (!el) return;
        el.play()
          .then(() => {
            el.pause();
            el.currentTime = 0;
          })
          .catch((err) => console.warn('Audio unlock failed:', err));
      });
      audioUnlockedRef.current = true;
      window.removeEventListener('click', unlock);
    };
    window.addEventListener('click', unlock);
    return () => window.removeEventListener('click', unlock);
  }, []);

  /** Publish helper: no-ops silently if the broker isn't connected yet */
  const publish = useCallback((topic, payload = '') => {
    const client = clientRef.current;
    if (!client || !client.connected) {
      console.warn(`MQTT not connected, dropped publish to ${topic}`);
      return;
    }
    client.publish(topic, typeof payload === 'string' ? payload : JSON.stringify(payload));
  }, []);

  // Connect once on mount, subscribe to telemetry topics, route incoming
  // messages by topic into state, and clean up on unmount.
  useEffect(() => {
    const client = mqtt.connect(MQTT_CONFIG.brokerUrl, MQTT_CONFIG.options);
    clientRef.current = client;

    client.on('connect', () => {
      setConnected(true);
      Object.values(MQTT_CONFIG.topics).forEach((topic) => client.subscribe(topic));
    });
    client.on('reconnect', () => setConnected(false));
    client.on('close', () => setConnected(false));
    client.on('error', (err) => console.error('MQTT error:', err));

    client.on('message', (topic, messageBuf) => {
      const raw = messageBuf.toString();
      let data;
      try {
        data = JSON.parse(raw);
      } catch {
        data = raw; // some firmwares may send plain values instead of JSON
      }

      switch (topic) {
        case MQTT_CONFIG.topics.battery:
          if (data?.voltage != null) setVoltage(data.voltage);
          if (data?.percent != null) setPercent(data.percent);
          break;
        case MQTT_CONFIG.topics.temperature:
          if (data?.temperature != null) setTemperature(data.temperature);
          break;
        case MQTT_CONFIG.topics.direction:
          if (data?.direction != null) setDirection(data.direction);
          break;
        case MQTT_CONFIG.topics.motor:
          if (data?.motor != null) setMotorStatus(data.motor);
          break;
        case MQTT_CONFIG.topics.pwm:
          if (data?.pwm_value != null) setPwmValue(data.pwm_value);
          break;
        case MQTT_CONFIG.topics.imu:
          if (data?.pitch != null) setImu({ pitch: data.pitch, roll: data.roll, ok: data.ok !== false });
          break;
        default:
          break;
      }
    });

    return () => {
      client.end(true);
      clientRef.current = null;
    };
  }, []);

  const playHover = () => {
    if (hoverSoundRef.current) {
      hoverSoundRef.current.currentTime = 0;
      // If this rejects with NotAllowedError, the element hasn't been
      // unlocked yet (user hasn't clicked anywhere on the page this session).
      hoverSoundRef.current.play().catch((err) => console.warn('hover sound blocked:', err));
    }
  };
  const playClick = () => {
    if (clickSoundRef.current) {
      clickSoundRef.current.currentTime = 0;
      // If this rejects with a network/404-style error, check that
      // /public/click.mp3 exists and was actually deployed to Vercel.
      clickSoundRef.current.play().catch((err) => console.warn('click sound blocked:', err));
    }
  };

  // LER button: some setups push temperature on a timer already, but this
  // lets the dashboard explicitly ask the ESP32 to publish a fresh reading.
  const refreshTemperature = () => publish(MQTT_CONFIG.commands.requestTemperature);

  const motorOn = () => publish(MQTT_CONFIG.commands.motorOn);
  const motorOff = () => publish(MQTT_CONFIG.commands.motorOff);
  const forward = () => publish(MQTT_CONFIG.commands.forward);
  const reverse = () => publish(MQTT_CONFIG.commands.reverse);
  const turnLeft = () => publish(MQTT_CONFIG.commands.left);
  const turnRight = () => publish(MQTT_CONFIG.commands.right);
  const stopWalking = () => publish(MQTT_CONFIG.commands.stop);
  const relaxServos = () => publish(MQTT_CONFIG.commands.relax);
  const setPose = (pose) => {
    setServoAngles(Array(10).fill(90));   // volta os sliders de calibração ao centro
    publish(MQTT_CONFIG.commands.pose, pose);
  };
  const toggleBalance = () => {
    const next = !balanceOn;
    setBalanceOn(next);
    publish(MQTT_CONFIG.commands.balance, next ? 'on' : 'off');
  };
  const zeroImu = () => publish(MQTT_CONFIG.commands.imuZero);
  const updateServo = (id, angle) => {
    setServoAngles((prev) => prev.map((a, i) => (i === id ? angle : a)));
    const now = Date.now();
    if (now - lastServoSentRef.current > 80) {   // limita a taxa de mensagens
      lastServoSentRef.current = now;
      publish(MQTT_CONFIG.commands.servo, { id, angle });
    }
  };
  const commitServo = (id, angle) => publish(MQTT_CONFIG.commands.servo, { id, angle }); // valor final
  const pwmOn = () => publish(MQTT_CONFIG.commands.pwmOn);
  const pwmOff = () => publish(MQTT_CONFIG.commands.pwmOff);
  const pwm25 = () => publish(MQTT_CONFIG.commands.pwm25);
  const pwm50 = () => publish(MQTT_CONFIG.commands.pwm50);
  const pwm75 = () => publish(MQTT_CONFIG.commands.pwm75);
  const updateValue = (val) => {
    setSliderValue(val);
    publish(MQTT_CONFIG.commands.slider, String(val));
  };

  const handleJoystickMove = ({ angle, force }) => {
    publish(MQTT_CONFIG.commands.drive, { angle, force });
  };
  const handleJoystickEnd = () => {
    publish(MQTT_CONFIG.commands.drive, { angle: 0, force: 0 });
  };

  const batteryColor = percent < 20 ? '#ff3b3b' : '#00eaff';

  return (
    <div className="container">
      {/*
        Absolute paths (leading "/") so these resolve correctly from any
        route on Vercel. The files themselves must live in your project's
        /public folder: public/hover.mp3 and public/click.mp3 — Vite/CRA
        copy everything in /public to the site root at build time.
        Vercel's filesystem is case-sensitive (unlike Windows/macOS), so
        double-check the filenames match exactly, including extension case.
      */}
      <audio ref={hoverSoundRef} src="/hover.mp3" preload="auto" />
      <audio ref={clickSoundRef} src="/click.mp3" preload="auto" />

      <a href="/">Home</a>
      <div className="header-row">
        <h2>Data Robot</h2>
        <span className={`mqtt-status${connected ? ' online' : ''}`}>
          <span className="mqtt-dot" /> {connected ? 'MQTT connected' : 'MQTT offline'}
        </span>
      </div>

      <table>
        <thead>
          <tr>
            <th>eCarData</th>
            <th>Situation</th>
            <th>Position</th>
            <th>Hora</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td><h2>Battery</h2></td>
            <td><h2 style={{ color: batteryColor }}>{voltage != null ? `${voltage.toFixed(2)} V` : '--'}</h2></td>
            <td><h2>{percent}%</h2></td>
            <td><h2>--</h2></td>
          </tr>
          <tr>
            <td><p>Direction</p></td>
            <td><h2 id="direction">{direction}</h2></td>
            <td><h2>--</h2></td>
            <td><h2 id="pwm">{pwmValue}</h2></td>
          </tr>
          <tr>
            <td>CPU</td>
            <td><h2 id="temperature">{temperature != null ? `${temperature}°C` : '--'}</h2></td>
            <td><h2>--</h2></td>
            <td><h2 id="pwm">{pwmValue}</h2></td>
          </tr>
          <tr>
            <td>
              <button type="button" onClick={refreshTemperature}>LER</button>
            </td>
            <td colSpan={3}>
              <div className="joystick-wrap">
                <span className="joystick-label">Joystick Robot Car</span>
                <Joystick onMove={handleJoystickMove} onEnd={handleJoystickEnd} />
              </div>
            </td>
          </tr>
          <tr>
            <td>
              <button className="btn" onMouseEnter={playHover} onClick={playClick}>MANUAL</button>
              <button className="btn" onMouseEnter={playHover} onClick={playClick}>OVERRIDE</button>
            </td>
            <td>
              <button className="btn alt" onMouseEnter={playHover} onClick={playClick}>SCAN</button>
            </td>
          </tr>
        </tbody>
      </table>

      <table>
        <tbody>
          <tr>
            <td><button type="button" onClick={motorOn}>ON</button></td>
            <td><button type="button" onClick={motorOff}>OFF</button></td>
          </tr>
          <tr>
            <td><button type="button" onClick={turnLeft}>Left</button></td>
            <td><button type="button" onClick={forward}>F</button></td>
            <td><button type="button" onClick={turnRight}>Right</button></td>
          </tr>
          <tr>
            <td><button type="button" onClick={reverse}>R</button></td>
            <td><button type="button" onClick={stopWalking}>STOP</button></td>
            <td><button type="button" onClick={relaxServos}>RELAX</button></td>
          </tr>
          <tr>
            <td>
              <input
                type="range"
                min="1"
                max="100"
                value={sliderValue}
                className="slider"
                onChange={(e) => updateValue(e.target.value)}
              />
            </td>
            <td><label>Value: {sliderValue}</label></td>
          </tr>
        </tbody>
      </table>

      <table>
        <tbody>
          <tr>
            <th colSpan={4}>IMU / Postura</th>
          </tr>
          <tr>
            <td>
              {imu === null
                ? 'Aguardando dados do MPU…'
                : !imu.ok
                  ? 'MPU não detectado'
                  : <>Pitch: <b>{Number(imu.pitch).toFixed(1)}°</b></>}
            </td>
            <td>{imu?.ok ? <>Roll: <b>{Number(imu.roll).toFixed(1)}°</b></> : '--'}</td>
            <td><button type="button" onClick={zeroImu}>Zerar nível</button></td>
            <td>
              <button type="button" onClick={toggleBalance}>
                Equilíbrio: {balanceOn ? 'ON' : 'OFF'}
              </button>
            </td>
          </tr>
          <tr>
            <td colSpan={2}><button type="button" onClick={() => setPose('stand')}>Em pé</button></td>
            <td colSpan={2}><button type="button" onClick={() => setPose('crouch')}>Agachar</button></td>
          </tr>
        </tbody>
      </table>

      <table>
        <tbody>
          <tr><th colSpan={2}>Calibração dos servos (ao vivo, 90° = centro)</th></tr>
          {SERVO_LABELS.map((label, i) => (
            <tr key={i}>
              <td>{label}</td>
              <td>
                <input
                  type="range"
                  min="0"
                  max="180"
                  value={servoAngles[i]}
                  className="slider"
                  onChange={(e) => updateServo(i, Number(e.target.value))}
                  onMouseUp={(e) => commitServo(i, Number(e.target.value))}
                  onTouchEnd={(e) => commitServo(i, Number(e.target.value))}
                />
                {' '}{servoAngles[i]}°
              </td>
            </tr>
          ))}
        </tbody>
      </table>

      <table>
        <tbody>
          <tr>
            <th colSpan={7}>PWM</th>
            <td><button type="button" onClick={pwmOff}>PWM_OFF</button></td>
            <td><button type="button" onClick={pwmOn}>PWM_ON</button></td>
          </tr>
          <tr>
            <td><button type="button" onClick={pwm25}>PWM_25%</button></td>
            <td><button type="button" onClick={pwm50}>PWM_50%</button></td>
            <td><button type="button" onClick={pwm75}>PWM_75%</button></td>
          </tr>
        </tbody>
      </table>

      <div className="dials-row">
        <div className="circle">
          <span>{voltage != null ? `${voltage.toFixed(1)}V` : 'V'}</span>
        </div>

        <BatteryBar percent={percent} />

        <CircularGauge value={voltage ?? 0} min={0} max={15} label="Voltage" unit="V" />
        <CircularGauge value={percent ?? 0} min={0} max={100} label="Battery" unit="%" />

        <div className="circle">
          <span>{percent}%</span>
        </div>
      </div>
    </div>
  );
}
