#ifndef MPU_CONTROL_H
#define MPU_CONTROL_H

#include <Arduino.h>
#include <Wire.h>
#include "MPU6050.h"

class MPUControl {
public:
  MPUControl();

  void mpuInit();
  void mpuUpdate();
  void setTestMode(bool enable);
  bool isTestMode() const;
  
  void enableMPU(bool enable);   
  
  bool isMPUEnabled() const;

  void setPIDTunings(float p, float i, float d);
  
  float getAngleX() const;
  float getPIDOutput() const;
  

private:
  MPU6050 mpu;

  // PID
  float setPoint;
  float Kp, Ki, Kd;
  float erro, erroAnt, integral;
  uint32_t lastPID;

  // Filtro complementar
  float angX;
  float alpha;
  uint32_t lastLoop;

  // Teste
  bool modoTeste;

  void sendPositions(int qEsq, int qDir, int jEsq, int jDir);
};

#endif
