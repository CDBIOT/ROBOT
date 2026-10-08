
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <SPIFFS.h>
#include "FS.h"

#include <ArduinoJson.h>
#include <WiFiManager.h>
//#include <DNSServer.h>
#include <Time.h>

#include <SPI.h>
#include <Wire.h>

#include "MPU6050.h"
#include "mpuControl.h"

// Cria o objeto global para controle do MPU
MPUControl mpuControl;


//float getAngleX() { return angX; }

// Tamanho do Objeto JSON
const   size_t    JSON_SIZE = JSON_OBJECT_SIZE(5) + 130;

const byte        DNSSERVER_PORT                  = 53;

DNSServer         dnsServer;
WebServer server(80);   

 
// Variáveis Globais ------------------------------------

//char              ssid[30];     // Rede WiFi
//char              pw[30];       // Senha da Rede WiFi

// Replace with your network credentials
const char* ssid     = "CDB";
const char* password = "Janeiro28abcdef";
         
//Mapeamento de pinos do NodeMCU
#define motor_1   21
#define motor_2   23
#define motor_3   22
#define led_in 2

float sensorPin = 33;
const int Pwm = 25;
int pwmValue;
int sensorValue = 0;  
int valueD0 = HIGH;
 
 float t ;

MPUControl getAngleX();


bool balanceMode = false;
int cont = 0;

//Variables will change
int lastCounter = 1;
float fator;

void balanceOn()
{
    balanceMode = true;
    server.send(200,"text/plain","ON");
}

void balanceOff()
{
    balanceMode = false;
    server.send(200,"text/plain","OFF");
}

void sendBalance()
{
    float pid = mpuControl.getPIDOutput();

    pid = constrain(pid, -15, 15);

    int coxaE = 90 + pid;
    int coxaD = 90 - pid;

    Serial.printf(
        "PID=%.2f E=%d D=%d\n",
        pid,
        coxaE,
        coxaD
    );

    Serial1.printf(
        "B,%d,%d\n",
        coxaE,
        coxaD
    );
}


String hexStr(const unsigned long &h, const byte &l = 8) {
  // Retorna valor em formato hexadecimal
  String s;
  s= String(h, HEX);
  s.toUpperCase();
  s = ("00000000" + s).substring(s.length() + 8 - l);
  return s;
}

// Funções Genéricas ------------------------------------
void log(String s) {
  // Gera log na Serial
  Serial.println(s);
}

String deviceID() {
    return "CDB SCAN-" + hexStr(ESP.getEfuseMac());
}

String ipStr(const IPAddress &ip) {
  // Retorna IPAddress em formato "n.n.n.n"
  String sFn = "";
  for (byte bFn = 0; bFn < 3; bFn++) {
    sFn += String((ip >> (8 * bFn)) & 0xFF) + ".";
  }
  sFn += String(((ip >> 8 * 3)) & 0xFF);
  return sFn;
}

void onNotFound() 
{
  server.send(404, "text/plain", "Not Found" );
}

// Write the sensor readings on the SD card
void logdata() {

File file = SPIFFS.open(F("/data.txt"), "a");
  if (file) {
      file.printf("Registro %u\r\n", t);
  }

  file.close();
}


void handleLogView() {
  
File file = SPIFFS.open(F("/log.htm"), "r");

 if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();
     
      //Serial.print(s);
   server.send(200, F("text/html"), s);
  }
}

void pwm_on()
{
pwmValue = 1024;
int i;
   // ledcWrite(0, i);//Escrevemos no canal 0, o duty cycle "i".
   delay(1000);
   ledcWrite(led_in, pwmValue);
   ledcWrite(Pwm, pwmValue);
   pwmValue == i;
  
   //delay(3000);
  // ledcWrite(0, 0);
  String json = "{\"pwm_value\":"+String(pwmValue)+"}";
  server.send (200, "application/json", json);
   
Serial.print("pwm = ");
Serial.println(pwmValue);
Serial.println(i);
}

void pwm_off()
{
pwmValue = 0 ;
int i;

ledcWrite(led_in, pwmValue);
ledcWrite(Pwm, pwmValue);
  delay(10);  

   String json = "{\"pwm_value\":"+String(pwmValue)+"}";
  server.send (200, "application/json", json);

  Serial.print("pwm = ");
  Serial.println(pwmValue);

}

void pwm_25()
{
pwmValue = 256;
int i;
ledcWrite(led_in, pwmValue);
ledcWrite(Pwm, pwmValue);
  delay(10);  
   String json = "{\"pwm_value\":"+String(pwmValue)+"}";
  server.send (200, "application/json", json);
  
  Serial.print("pwm = ");
  Serial.println(pwmValue);
}

void pwm_50()
{
pwmValue= 512;
int i;
ledcWrite(led_in, pwmValue);
ledcWrite(Pwm, pwmValue);
  delay(10);  
   String json = "{\"pwm_value\":"+String(pwmValue)+"}";
  server.send (200, "application/json", json);
  
  Serial.print("pwm = ");
  Serial.println(pwmValue);
}

void pwm_75()
{
pwmValue = 756;
int i;
ledcWrite(led_in, pwmValue);
ledcWrite(Pwm, pwmValue);
  delay(10);  
   String json = "{\"pwm_value\":"+String(pwmValue)+"}";
  server.send (200, "application/json", json);
  
  Serial1.print("pwm = ");
  Serial1.println(pwmValue);
}

void motor_off()
{   
  Serial1.print(1);
   
  String json = "{\"motor\":\"Desligado\"}";
  server.send (200, "application/json", json);
  //Serial1.println("walk");
}

void motor_on()
{   
  Serial1.print(0x00);
   
  String json = "{\"motor\":\"Ligado\"}";
  server.send (200, "application/json", json);
  //Serial1.println("wait");
}

void motor_D()
{   
  Serial1.print(0x30);
     
  String json = "{\"direction\":\"Right\"}";
  server.send (200, "application/json", json);
  Serial.println("Right");
}

void motor_E()
{   
  Serial1.print(0x31);
   
 String json = "{\"direction\":\"Left\"}";
  server.send (200, "application/json", json);
  Serial.println("Left");
}

void forward()
{   
  Serial1.print(0x32);
   String json = "{\"direction\":\"Forward\"}";
  server.send (200, "application/json", json);
 // Serial.println("Forward");
}

void reverse()
{   
  Serial1.print(0x33);
   String json = "{\"direction\":\"Reverse\"}";
  server.send (200, "application/json", json);
  //Serial.println("Reverse");
}


void handleAngle()
{
//float angle = angX;
float angle = mpuControl.getAngleX();
//float pid  =  mpuControl.getPIDOutput();

String json = "{\"angle\":" + String(angle, 2) +   ",\"pid\":" + String(angle, 2) + "}";
  //String json = "{\"angle\":"  String(angle,2) + ",\"pid\":" + String(pid,2) + "}";
  server.send(200,"application/json",json);
    
  // Debug
  //Serial.printf("[MPU] Ângulo: %.2f°, PID: %.2f\n", angX, pid);
}


void getTemperature()
{
  int valor = analogRead(sensorPin);
  t = ((valor*250)/4095);
 
   //fator = 0.39;
  sensorValue = analogRead(sensorPin);
  float temp1 = 0;
  float temp2 = 0;
  float Amp = 0;
 
  
   for (int i = 0; i < 500; i++)
   {
    temp1 = temp1 + analogRead(sensorPin);
    delayMicroseconds(5);
    }
    temp1 = temp1 / 500; //Media de cem leituras
    temp2 = (temp1 *10)/1023.0;
    Amp = (temp2/12.2);
    
    delay(2); 

 // String json2 = "{\"temperature\":"+String(temp2)+"}";
 // server.send (200, "application/json", json);
 String json = "{\"temperature\":" + String(valor)+
                ",\"amp\":" + String(Amp,2) +"}";
 server.send (200, "application/json", json);

 
Serial.print("sensor = ");
Serial.println(t);
Serial.print("temp1 = ");
Serial.println( temp1);
Serial.print("temp2 = ");
Serial.println( temp2);
Serial.print("Amp = ");
Serial.println( Amp);
delay(100);

}

void handleCSS() 
{
  // Arquivo CSS
  File file = SPIFFS.open(F("/Style.css"), "r");
  if (file) {
    // Define cache para 3 dias
    server.sendHeader(F("Cache-Control"), F("public, max-age=172800"));
    server.streamFile(file, F("text/css"));
    file.close();
    log("CSS - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("CSS - ERROR 500"));
    log(F("CSS - ERRO lendo arquivo"));
  }
}


void handlePWM() 
{
  File file = SPIFFS.open(F("/Monitor.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

  sensorValue = analogRead(sensorPin);
  float amp1 = 0;
  float amp2 = 0;
  float Amp = 0;
   
   for (int i = 0; i < 500; i++)
   {
   amp1 = amp1 + analogRead(sensorPin);
    delayMicroseconds(5);
    }
    amp1 = amp1 / 500; //Media de cem leituras
    amp2= (amp1 *5.0)/4100.0;
    Amp = (amp2/12.2);
//    
//  String json = "{\"Corrente\":"+String(Amp)+"}";
//  server.send (200, "application/json", json);
  
    // Envia dados
    server.send(200, F("text/html"), s);
  //  log("Home - Cliente: " + ipStr(server.client().remoteIP()) + (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  } else {
    server.send(500, F("text/plain"), F("Home - ERROR 500"));
  //  log(F("Home - ERRO lendo arquivo"));
  
  }
}

// Requisições Web --------------------------------------
void handleHome() {
  // Home
  File file = SPIFFS.open(F("/Home.htm"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();
   
    // Atualiza conteúdo dinâmico
   // s.replace(F("#led#")  , ledOn ? F("Ligado") : F("Desligado"));
   // s.replace(F("#id#")   , id);
    //s.replace(F("#ssid#") , ssid);
    //s.replace(F("#pw#")   , pw);
   // s.replace(F("#sysIP#")    , ipStr(WiFi.status() == WL_CONNECTED ? WiFi.localIP() : WiFi.softAPIP()));
   // s.replace(F("#clientIP#") , ipStr(server.client().remoteIP()));
    
    // Envia dados
    server.send(200, F("text/html"), s);
  //  log("Home - Cliente: " + ipStr(server.client().remoteIP()) + (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  } else {
    server.send(500, F("text/plain"), F("Home - ERROR 500"));
  //  log(F("Home - ERRO lendo arquivo"));
  }
}





void setup_wifi() 
{
  //client.setServer(mqtt_server, 1883);
  //client.setCallback(callback);
  
  // WiFi Station
  WiFi.begin(ssid, password);
//  log("Conectando WiFi " + String(ssid));
  byte b = 0;
  while(WiFi.status() != WL_CONNECTED && b < 60) {
    b++;
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    // WiFi Station conectado
    log("WiFi conectado (" + String(WiFi.RSSI()) + ") IP " + ipStr(WiFi.localIP()));
  } else {
 
// Configura WiFi AP para ESP32
   
   // WiFi.mode(WIFI_AP);
   
   WiFi.setHostname("ROBOT");
   // WiFi.softAP(deviceID().c_str(), deviceID().c_str());
   //WiFi.softAP(ap_ssid,ap_pass);
   //IP 192.168.4.1

   WiFi.softAP("ROBOT","12345678");
    IPAddress myIP = WiFi.softAPIP();
    log("WiFi AP " + deviceID() + " - IP " + ipStr(WiFi.softAPIP()));
 
  // Habilita roteamento DNS
  //dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  //dnsServer.start(DNSSERVER_PORT, "*", WiFi.softAPIP());
  
  
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  Serial.print("MAC: ");
   Serial.println(WiFi.macAddress());
  Serial.println("");
  }
}

void setup () 
{
 Serial.begin(9600);
 Serial1.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17 (pinos padrão)
 while(!Serial);

  // SPIFFS
  if (!SPIFFS.begin()) {
    log(F("SPIFFS ERRO"));
    while (true);
  }
ledcAttachPin(Pwm, 0);//Atribuimos o pino 25 ao canal 0.
ledcAttachPin(led_in, 0);//Atribuimos o pino 2 ao canal 0.

ledcSetup(Pwm, 1000, 10);//Atribuimos ao canal 0 a frequencia de 1000Hz com resolucao de 10bits.
ledcSetup(led_in, 1000, 10);//Atribuimos ao canal 0 a frequencia de 1000Hz com resolucao de 10bits.

pinMode(sensorPin, INPUT);
pinMode(led_in, OUTPUT);//Definimos o pino 2 (LED) como saída.
pinMode(Pwm, OUTPUT);//Definimos o pino 25 (PWM) como saída.

pinMode(motor_1, OUTPUT);
pinMode(motor_2, OUTPUT);
pinMode(motor_3, OUTPUT);

ledcWrite(0, 1024);
setup_wifi();

mpuControl.mpuInit();

server.on("/", handleHome);
server.on("/temperature", HTTP_GET, getTemperature);
server.on("/pwm_on",HTTP_GET, pwm_on);
server.on("/pwm_25",HTTP_GET, pwm_25);
server.on("/pwm_50",HTTP_GET, pwm_50);
server.on("/pwm_75",HTTP_GET, pwm_75);
server.on("/pwm_off",HTTP_GET, pwm_off);
server.on("/motor_on",HTTP_GET, motor_on);
server.on("/motor_off",HTTP_GET, motor_off);
server.on("/motor_D",HTTP_GET, motor_D);
server.on("/motor_E",HTTP_GET, motor_E);
server.on("/forward",HTTP_GET, forward);
server.on("/reverse",HTTP_GET, reverse);
server.on("/Monitor",HTTP_GET,handlePWM);
server.on("/angle",HTTP_GET,handleAngle);
server.on("/balance_on", balanceOn);
server.on("/balance_off", balanceOff);

server.on("/log", handleLogView); 
server.on("/css" , handleCSS);
server.onNotFound(onNotFound);
server.begin();
Serial.println("Servidor HTTP iniciado");
}

void loop() 
{
// WatchDog ----------------------------------------
yield();
dnsServer.processNextRequest();
server.handleClient();
//readMpu();
mpuControl.mpuUpdate();

static uint32_t lastBalance = 0;

if(balanceMode)
{
  
if(millis() - lastBalance > 20)
{
    lastBalance = millis();

    mpuControl.mpuUpdate();

    sendBalance();
}
}

}
