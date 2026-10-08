/*
 * Robot Car - ESP32 + MQTT
 * Recebe comandos do dashboard React e publica telemetria.
 *
 * Bibliotecas (Library Manager da Arduino IDE):
 *   - PubSubClient   (Nick O'Leary)
 *   - ArduinoJson    (Benoit Blanchon, v7)
 *
 * Placa: "ESP32 Dev Module" (core esp32 da Espressif, 2.x ou 3.x)
 *
 * Driver de motor assumido: L298N / similar (2 motores, ponte H dupla):
 *   ENA/ENB = PWM de velocidade, INx = sentido.
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ======================= CONFIGURAÇÃO =======================

// Wi-Fi
const char* WIFI_SSID = "SEU_WIFI";
const char* WIFI_PASS = "SUA_SENHA";

// Broker MQTT (o ESP32 fala MQTT puro na porta TCP 1883;
// o dashboard no navegador fala WebSocket no mesmo broker).
const char* MQTT_HOST = "broker.hivemq.com";  // troque pelo seu broker
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER = "";                   // deixe vazio se não usar
const char* MQTT_PASS = "";

// Pinos do driver de motor
const int PIN_ENA = 25;   // PWM motor esquerdo
const int PIN_IN1 = 26;
const int PIN_IN2 = 27;
const int PIN_ENB = 13;   // PWM motor direito
const int PIN_IN3 = 14;
const int PIN_IN4 = 12;

// Leitura da bateria (divisor resistivo -> pino ADC1, ex.: GPIO34)
// Ex.: R1 = 30k (bateria -> pino), R2 = 7.5k (pino -> GND)  => razão 5.0
const int   PIN_BAT = 34;
const float BAT_DIVIDER_RATIO = 5.0;
const float BAT_VOLT_MIN = 9.0;    // 0%   (ex.: 3S Li-ion/LiPo)
const float BAT_VOLT_MAX = 12.6;   // 100%

// PWM
const int PWM_FREQ = 1000;
const int PWM_RES  = 8;            // 8 bits -> 0..255

// Telemetria
const unsigned long TELEMETRY_INTERVAL_MS = 2000;

// Tópicos: PRECISAM ser iguais aos de MQTT_CONFIG no RobotMonitor.jsx.
// Dica: se usar broker público, coloque um prefixo único (ex.: "meunome123/robot/...")
// nos dois lados, senão qualquer pessoa pode controlar seu robô.
#define T_BATTERY      "robot/battery"
#define T_TEMPERATURE  "robot/temperature"
#define T_DIRECTION    "robot/direction"
#define T_MOTOR        "robot/motor"
#define T_PWM          "robot/pwm"

#define C_MOTOR_ON     "robot/cmd/motor_on"
#define C_MOTOR_OFF    "robot/cmd/motor_off"
#define C_FORWARD      "robot/cmd/forward"
#define C_REVERSE      "robot/cmd/reverse"
#define C_LEFT         "robot/cmd/motor_e"
#define C_RIGHT        "robot/cmd/motor_d"
#define C_PWM_ON       "robot/cmd/pwm_on"
#define C_PWM_OFF      "robot/cmd/pwm_off"
#define C_PWM_25       "robot/cmd/pwm_25"
#define C_PWM_50       "robot/cmd/pwm_50"
#define C_PWM_75       "robot/cmd/pwm_75"
#define C_SLIDER       "robot/cmd/slider"
#define C_DRIVE        "robot/cmd/drive"
#define C_REQ_TEMP     "robot/cmd/temperature"

// ======================= ESTADO =======================

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

bool motorEnabled = false;
bool pwmEnabled   = true;
int  speedPercent = 50;            // 0..100 (slider / botões PWM)
String direction  = "stopped";

unsigned long lastTelemetry = 0;
unsigned long lastReconnectAttempt = 0;

// ======================= PWM (compatível core 2.x e 3.x) =======================

void pwmSetup() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_ENA, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_ENB, PWM_FREQ, PWM_RES);
#else
  ledcSetup(0, PWM_FREQ, PWM_RES);
  ledcSetup(1, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_ENA, 0);
  ledcAttachPin(PIN_ENB, 1);
#endif
}

void pwmWrite(int motor, int duty) {  // motor: 0 = esquerdo, 1 = direito
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(motor == 0 ? PIN_ENA : PIN_ENB, duty);
#else
  ledcWrite(motor, duty);
#endif
}

// ======================= MOTORES =======================

// speed: -100..100 (negativo = ré)
void setMotor(int motor, int speed) {
  speed = constrain(speed, -100, 100);
  int in1 = (motor == 0) ? PIN_IN1 : PIN_IN3;
  int in2 = (motor == 0) ? PIN_IN2 : PIN_IN4;

  if (speed > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else if (speed < 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }
  pwmWrite(motor, map(abs(speed), 0, 100, 0, 255));
}

void setMotors(int left, int right) {
  if (!motorEnabled) {
    left = 0;
    right = 0;
  }
  setMotor(0, left);
  setMotor(1, right);
}

void stopMotors() {
  setMotors(0, 0);
}

int effectiveSpeed() {
  return pwmEnabled ? speedPercent : 0;
}

// ======================= TELEMETRIA =======================

float readBatteryVoltage() {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogReadMilliVolts(PIN_BAT);
  float pinVolts = (sum / 16.0) / 1000.0;
  return pinVolts * BAT_DIVIDER_RATIO;
}

int voltageToPercent(float v) {
  float pct = (v - BAT_VOLT_MIN) / (BAT_VOLT_MAX - BAT_VOLT_MIN) * 100.0;
  return (int)constrain(pct, 0.0, 100.0);
}

void publishBattery() {
  float v = readBatteryVoltage();
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"voltage\":%.2f,\"percent\":%d}", v, voltageToPercent(v));
  mqtt.publish(T_BATTERY, buf);
}

void publishTemperature() {
  char buf[48];
  snprintf(buf, sizeof(buf), "{\"temperature\":%.1f}", temperatureRead());  // sensor interno do chip
  mqtt.publish(T_TEMPERATURE, buf);
}

void publishDirection() {
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"direction\":\"%s\"}", direction.c_str());
  mqtt.publish(T_DIRECTION, buf);
}

void publishMotor() {
  mqtt.publish(T_MOTOR, motorEnabled ? "{\"motor\":\"on\"}" : "{\"motor\":\"off\"}");
}

void publishPwm() {
  char buf[48];
  snprintf(buf, sizeof(buf), "{\"pwm_value\":%d}", effectiveSpeed());
  mqtt.publish(T_PWM, buf);
}

void publishAll() {
  publishBattery();
  publishTemperature();
  publishDirection();
  publishMotor();
  publishPwm();
}

// ======================= COMANDOS =======================

void setSpeed(int pct) {
  speedPercent = constrain(pct, 0, 100);
  pwmEnabled = true;
  publishPwm();
}

void moveDirection(const char* dir, int leftSign, int rightSign) {
  direction = dir;
  int s = effectiveSpeed();
  setMotors(leftSign * s, rightSign * s);
  publishDirection();
}

void handleDrive(const byte* payload, unsigned int length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;

  float angle = doc["angle"] | 0.0f;   // 0..360, 0 = direita, 90 = frente
  float force = doc["force"] | 0.0f;   // 0..1

  if (force < 0.05f) {                 // joystick solto
    direction = "stopped";
    stopMotors();
    publishDirection();
    return;
  }

  float rad  = angle * PI / 180.0f;
  float fwd  = sinf(rad) * force;      // componente frente/ré
  float turn = cosf(rad) * force;      // componente giro

  int left  = (int)((fwd + turn) * 100.0f);
  int right = (int)((fwd - turn) * 100.0f);

  direction = (fwd >= 0) ? "forward" : "reverse";
  setMotors(left, right);
}

void onMessage(char* topic, byte* payload, unsigned int length) {
  if      (!strcmp(topic, C_MOTOR_ON))  { motorEnabled = true;  publishMotor(); }
  else if (!strcmp(topic, C_MOTOR_OFF)) {
    motorEnabled = false;
    direction = "stopped";
    stopMotors();
    publishMotor();
    publishDirection();
  }
  else if (!strcmp(topic, C_FORWARD))   moveDirection("forward", 1, 1);
  else if (!strcmp(topic, C_REVERSE))   moveDirection("reverse", -1, -1);
  else if (!strcmp(topic, C_LEFT))      moveDirection("left", -1, 1);
  else if (!strcmp(topic, C_RIGHT))     moveDirection("right", 1, -1);
  else if (!strcmp(topic, C_PWM_ON))    { pwmEnabled = true;  publishPwm(); }
  else if (!strcmp(topic, C_PWM_OFF))   { pwmEnabled = false; stopMotors(); publishPwm(); }
  else if (!strcmp(topic, C_PWM_25))    setSpeed(25);
  else if (!strcmp(topic, C_PWM_50))    setSpeed(50);
  else if (!strcmp(topic, C_PWM_75))    setSpeed(75);
  else if (!strcmp(topic, C_SLIDER)) {
    char buf[8];
    unsigned int n = min(length, (unsigned int)(sizeof(buf) - 1));
    memcpy(buf, payload, n);
    buf[n] = '\0';
    setSpeed(atoi(buf));
  }
  else if (!strcmp(topic, C_DRIVE))     handleDrive(payload, length);
  else if (!strcmp(topic, C_REQ_TEMP))  publishTemperature();
}

// ======================= CONEXÃO =======================

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWi-Fi OK, IP: ");
  Serial.println(WiFi.localIP());
}

bool connectMqtt() {
  String clientId = "robot_" + String((uint32_t)ESP.getEfuseMac(), HEX);
  bool ok = (strlen(MQTT_USER) > 0)
              ? mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)
              : mqtt.connect(clientId.c_str());
  if (!ok) {
    Serial.printf("MQTT falhou, rc=%d\n", mqtt.state());
    return false;
  }
  Serial.println("MQTT conectado");

  const char* cmds[] = {
    C_MOTOR_ON, C_MOTOR_OFF, C_FORWARD, C_REVERSE, C_LEFT, C_RIGHT,
    C_PWM_ON, C_PWM_OFF, C_PWM_25, C_PWM_50, C_PWM_75,
    C_SLIDER, C_DRIVE, C_REQ_TEMP
  };
  for (const char* c : cmds) mqtt.subscribe(c);

  publishAll();  // dashboard recebe o estado atual assim que conecta
  return true;
}

// ======================= SETUP / LOOP =======================

void setup() {
  Serial.begin(115200);

  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  pwmSetup();
  analogReadResolution(12);
  stopMotors();

  connectWifi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setBufferSize(512);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    stopMotors();                      // segurança: sem rede, robô para
    connectWifi();
  }

  if (!mqtt.connected()) {
    stopMotors();                      // segurança: sem broker, robô para
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 3000) {
      lastReconnectAttempt = now;
      connectMqtt();
    }
  } else {
    mqtt.loop();

    unsigned long now = millis();
    if (now - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
      lastTelemetry = now;
      publishBattery();
      publishTemperature();
    }
  }
}









//#include <ESP8266WiFi.h>

#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "CDB";
const char* password = "Janeiro28abcdef";
const char* mqtt_server = "broker.mqtt-dashboard.com";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
int lm35 = 33;
const int led = 21;
const int lamp = 03;
const int sensor_1 = 05;
const int sensor_2 = 01;
//const int sala;

const int room_light = 04;
const int room_temp = 33;
const int room_amp = 01;

const int aqua_light = 21;
const int aqua_pump = 03;
const int aqua_heat = 0;
const int aqua_temp = 33;


char msgtemp[10];
int temperatura;
const byte AC_PIN = 5;
const byte BUILTIN_LED =2;
void setup() {
  pinMode(AC_PIN, INPUT);
  pinMode(lm35 , INPUT);
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  pinMode(led, OUTPUT);
  pinMode(lamp, OUTPUT);
  
  pinMode(room_light, OUTPUT);
  pinMode(room_temp, INPUT);
  pinMode(room_amp, INPUT);
  
  pinMode(aqua_light, OUTPUT);
  pinMode(aqua_pump, OUTPUT);
  pinMode(aqua_heat, OUTPUT);
  pinMode(aqua_temp, INPUT);
        
   
  pinMode(sensor_1, INPUT);
  pinMode(sensor_2, INPUT);
    
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String payloadStr = "";
  for (int i = 0; i < length; i++) {
    payloadStr +=((char)payload[i]);
  }
  Serial.println();
  //if (String(topic).equals("bh/inTopic")){  
  if (String(topic).equals("room_light")){  
  //if (String(topic).equals("aqua_light")){  

  // Switch on the LED if an 1 was received as first character
  if (payloadStr.equals("1")) {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    digitalWrite(led, LOW);   // Turn the LED on (Note that LOW is the voltage level
    digitalWrite(aqua_light, LOW);
    //digitalWrite(room_light, LOW);
    Serial.println("LOW");
      
  } else if (payloadStr.equals("0")) {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
     digitalWrite(led,LOW);   // Turhe LED on (Note that LOW is the voltage level
     digitalWrite(room_light, LOW);
     //digitalWrite(aqua_light, HIGH);
     Serial.println("HIGH");
  } else {
    Serial.println("Invalid option"); 
  }
 }
   if (String(topic).equals("bh/inTopic")){ 
  // if (String(topic).equals("")){   
   }
  if (payloadStr.equals("2")) {
  digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    digitalWrite(lamp, LOW);
    
    
  } else if (payloadStr.equals("3")) {
   digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
    digitalWrite(lamp, HIGH);
  
  } else {
    Serial.println("Invalid option"); 
    
  
 }
}

void reconnect() {
  
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    String clientId = "cdbiot123";
    clientId += String(random(0xffff), HEX);

    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("bh/outTopic", "reconectado");
      //client.publish("room_light", "reconectado");
      //client.publish("aqua_temp", "reconectado");
      client.subscribe("robot/battery/telemetry");
      
      // ... and resubscribe
      //client.subscribe("bh/inTopic");  
      //client.subscribe("room_light");
      //client.subscribe("aqua_light");
       client.subscribe("robot/battery");
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void sensores(void){
int flag_1;
int flag_2;

if (digitalRead (sensor_1) == 1)
    {
    flag_1 = 1;
    Serial.print("S1: ");
    Serial.println(digitalRead(sensor_1));
    snprintf (msg, 10, "%ld", flag_1);
    client.publish("Detector1", msg);
    }
    else
    { 
    flag_1 = 0 ;
    Serial.print("S1: ");
    Serial.println(digitalRead(sensor_1));
    snprintf (msg, 10, "%ld", flag_1);
    client.publish("Detector1", msg);
    }
return;
}

int analogAverage(const byte a) {
unsigned long lfn = 0;
  for (byte bfn = 0 ; bfn < 100; bfn ++) {
    lfn += analogRead(a);
    delay(3) ;
  }
  return lfn / 100;
}


void loop() {

int ac_read = 0;
int ac_temp = 0;

ac_temp = int(analogAverage(lm35)*1.75/10.24);
ac_read = int(analogRead(lm35)*1.75/10.24);

//float celsius = ac_temp;

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;
    Serial.print("Valor: ");
    Serial.println(analogRead(lm35));
    
   // snprintf (msg, 50, "msg! #%ld", value);
    //client.publish("bh/outTopic", msg);
    //client.publish("room_temp", msg);
    //client.publish("aqua_temp",msg);
    
    //snprintf (msgtemp,10, "%2.2d",ac_temp );
    //snprintf(msgtemp,10,"{\"Local\":\"Sala\",\"temperature\":%2f}",ac_read);
    snprintf(msgtemp,10,"%2.2f",ac_read);
   //client.publish("Sala", String(ac_temp).c_str());
   //client.publish("room_temp", msgtemp);
   //client.publish("aqua_temp", msgtemp);
   client.publish("robot/battery/", msgtemp);
   Serial.print("V: ");
   Serial.println(ac_temp);
  // snprintf (temperature,10, "%2.2d",temperatura );
  // client.publish("Temp_aqua", String(ac_read).c_str());
   Serial.print("A: ");
   Serial.println(ac_read);
   
   sensores();
  }
  }
