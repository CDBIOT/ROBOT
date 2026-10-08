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
