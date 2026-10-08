#include "mpuControl.h"
#include <Wire.h>
#include "MPU6050.h"

//MPU6050 mpu;
bool mpuEnabled = true;

MPUControl::MPUControl(){
  
  }


// ===== Variáveis PID e filtro =====
float setPoint = 0.0;
float Kp = 2.0, Ki = 0.5, Kd = 1.2;
float erro = 0, erroAnt = 0, integral = 0;
float angX = 0;
float alpha = 0.98;
//uint32_t lastLoop = 0;

void MPUControl::mpuInit() {
  Wire.begin();
  mpu.initialize();

  if (!mpu.testConnection()) {
    Serial.println("⚠️ MPU6050 não encontrado!");
    mpuEnabled = false;
  } else {
    Serial.println("✅ MPU6050 conectado com sucesso.");
  }
  lastLoop = millis();
}

void MPUControl::mpuUpdate() {
  if (!mpuEnabled) return;

  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  uint32_t now = millis();
  float dt = (now - lastLoop) / 1000.0f;
  if (dt <= 0) dt = 0.01;
  lastLoop = now;

  // Acelerômetro → ângulo (pitch aproximado)
  float accAngleX = atan2(ay, az) * 180.0 / PI;

  // Giroscópio → taxa de rotação em °/s
  float gyroRateX = gx / 131.0;

  // Filtro complementar
  angX = alpha * (angX + gyroRateX * dt) + (1 - alpha) * accAngleX;

  // PID
  erro = setPoint - angX;
  integral += erro * dt;
  float deriv = (erro - erroAnt) / dt;
  erroAnt = erro;

  float pidOut = Kp * erro + Ki * integral + Kd * deriv;

  // Debug
  //Serial.printf("[MPU] Ângulo: %.2f°, PID: %.2f\n", angX, pidOut);
}

float MPUControl::getAngleX() const { return angX; }

float MPUControl::getPIDOutput() const
{
    float pidOut = Kp * erro + Ki * integral + Kd * (erro - erroAnt);
    return pidOut;
}

void MPUControl::setPIDTunings(float p, float i, float d) {
  Kp = p; Ki = i; Kd = d;
}

void MPUControl::enableMPU(bool enable) {
  if (enable) {
    mpu.initialize();  // Liga e inicializa o MPU6050
  } else {
    // Aqui você pode colocar o código para desabilitar, se desejar
  }
}
//void MPUControl::enableMPU(bool enable) { mpuEnabled = enable; }

bool MPUControl::isMPUEnabled() const {
  return mpuEnabled;
}
