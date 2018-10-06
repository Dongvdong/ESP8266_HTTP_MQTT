/*

  Copyright (c) 2017 @KmanOz
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.


  **** This firmware was adapted from @KmanOz, and was modified by Jones@bbs.hassbian.com ****
  **** USE THIS Firmware for: HASSMART 3Key ****
  **** Make sure to select "Generic ESP8285 Module" from the BOARD menu in TOOLS ****
  **** Flash Size "1M (64K SPIFFS)" ****

  VERSION: 1.01
  Last update: 20170930

*/

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <Ticker.h>

#define BUTTON1         0                                    // (Do not Change)
#define BUTTON2         14                                   // (Do not Change)
#define BUTTON3         2                                    // (Do not Change)
#define RELAY1          12                                   // (Do not Change)
#define RELAY2          5                                    // (Do not Change)
#define RELAY3          4                                    // (Do not Change)
#define LED             13                                   // (Do not Change)

#define MQTT_CLIENT     "hassmart_3key"                      // mqtt client_id (Must be unique for each Switch)
#define MQTT_SERVER     "192.168.6.8"                        // mqtt server
#define MQTT_PORT       1883                                 // mqtt port
#define MQTT_TOPIC      "home/hassmart_3key"                 // mqtt topic (Must be unique for each Switch)
#define MQTT_USER       "mqtt"                               // mqtt user
#define MQTT_PASS       "mqtt"                               // mqtt password
#define HOST_NAME       "Hassmart_3Key_Bedroom"              // the host name for OTA (Must be unique for each Switch)

#define WIFI_SSID       "SSID"                               // wifi ssid
#define WIFI_PASS       "PASSWORD"                           // wifi password

#define VERSION    "\n\n---------------  HASSMART 3KEY v1.01 with OTA  ---------------"

bool rememberRelayState1 = true;                             // If 'true' remembers the state of the relay 1 before power loss.
bool rememberRelayState2 = true;                             // If 'true' remembers the state of the relay 2 before power loss.
bool rememberRelayState3 = true;                             // If 'true' remembers the state of the relay 3 before power loss.
bool requestRestart = false;                                 // (Do not Change)
bool sendStatus1 = false;
bool sendStatus2 = false;                                    // (Do not Change)
bool sendStatus3 = false;                                    // (Do not Change)
bool OTAupdate = false;                                      // (Do not Change)

int kUpdFreq = 1;                                            // Update frequency in Mintes to check for mqtt connection
int kRetries = 10;                                           // WiFi retry count. Increase if not connecting to router.
int lastRelayState1;
int lastRelayState2;                                         // (Do not Change)
int lastRelayState3;                                         // (Do not Change)

unsigned long TTasks;                                        // (Do not Change)
unsigned long count1 = 0;                                    // (Do not Change)
unsigned long count2 = 0;                                    // (Do not Change)
unsigned long count3 = 0;                                    // (Do not Change)

extern "C" { 
  #include "user_interface.h" 
}

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient, MQTT_SERVER, MQTT_PORT);
Ticker btn_timer1, btn_timer2, btn_timer3;

void callback(const MQTT::Publish& pub) {
  if (pub.topic() == MQTT_TOPIC"/key1") {
    if (pub.payload_string() == "stat") {

    } else if (pub.payload_string() == "on") {
      digitalWrite(RELAY1, HIGH);
      sendStatus1 = true;
    } else if (pub.payload_string() == "off") {
      digitalWrite(RELAY1, LOW);
      sendStatus1 = true;
    } else if (pub.payload_string() == "reset") {
      requestRestart = true;
    }
  } else if (pub.topic() == MQTT_TOPIC"/key2") {
    if (pub.payload_string() == "stat") {

    } else if (pub.payload_string() == "on") {
      digitalWrite(RELAY2, HIGH);
      sendStatus2 = true;
    } else if (pub.payload_string() == "off") {
      digitalWrite(RELAY2, LOW);
      sendStatus2 = true;
    } else if (pub.payload_string() == "reset") {
      requestRestart = true;
    }
  } else if (pub.topic() == MQTT_TOPIC"/key3") {
    if (pub.payload_string() == "stat") {

    } else if (pub.payload_string() == "on") {
      digitalWrite(RELAY3, HIGH);
      sendStatus3 = true;
    } else if (pub.payload_string() == "off") {
      digitalWrite(RELAY3, LOW);
      sendStatus3 = true;
    } else if (pub.payload_string() == "reset") {
      requestRestart = true;
    }
  }
}

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(BUTTON1, INPUT);
  pinMode(BUTTON2, INPUT);
  pinMode(BUTTON3, INPUT);
  digitalWrite(LED, HIGH);
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  digitalWrite(RELAY3, LOW);
  Serial.begin(115200);
  EEPROM.begin(8);
  lastRelayState1 = EEPROM.read(0);
  lastRelayState2 = EEPROM.read(1);
  lastRelayState3 = EEPROM.read(2);
  if (rememberRelayState1 && lastRelayState1 == 1) {
     digitalWrite(RELAY1, HIGH);
  }
  if (rememberRelayState2 && lastRelayState2 == 1) {
     digitalWrite(RELAY2, HIGH);
  }
  if (rememberRelayState3 && lastRelayState3 == 1) {
     digitalWrite(RELAY3, HIGH);
  }
  btn_timer1.attach(0.05, button1);
  btn_timer2.attach(0.05, button2);
  btn_timer3.attach(0.05, button3);
  mqttClient.set_callback(callback);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  ArduinoOTA.setHostname(HOST_NAME);
  ArduinoOTA.onStart([]() {
    OTAupdate = true;
    blinkLED(LED, 400, 2);
    digitalWrite(LED, HIGH);
    Serial.println("OTA Update Initiated . . .");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update Ended . . .s");
    OTAupdate = false;
    requestRestart = true;
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    digitalWrite(LED, LOW);
    delay(5);
    digitalWrite(LED, HIGH);
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    blinkLED(LED, 40, 2);
    OTAupdate = false;
    Serial.printf("OTA Error [%u] ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(". . . . . . . . . . . . . . . Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println(". . . . . . . . . . . . . . . Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println(". . . . . . . . . . . . . . . Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println(". . . . . . . . . . . . . . . Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println(". . . . . . . . . . . . . . . End Failed");
  });
  ArduinoOTA.begin();
  Serial.println(VERSION);
  Serial.print("\nUnit ID: ");
  Serial.print("esp8266-");
  Serial.print(ESP.getChipId(), HEX);
  Serial.print("\nConnecting to "); Serial.print(WIFI_SSID); Serial.print(" Wifi"); 
  while ((WiFi.status() != WL_CONNECTED) && kRetries --) {
    delay(500);
    Serial.print(" .");
  }
  if (WiFi.status() == WL_CONNECTED) {  
    Serial.println(" DONE");
    Serial.print("IP Address is: "); Serial.println(WiFi.localIP());
    Serial.print("Connecting to ");Serial.print(MQTT_SERVER);Serial.print(" Broker . .");
    delay(500);
    while (!mqttClient.connect(MQTT::Connect(MQTT_CLIENT).set_keepalive(90).set_auth(MQTT_USER, MQTT_PASS)) && kRetries --) {
      Serial.print(" .");
      delay(1000);
    }
    if(mqttClient.connected()) {
      Serial.println(" DONE");
      Serial.println("\n----------------------------  Logs  ----------------------------");
      Serial.println();
      mqttClient.subscribe(MQTT_TOPIC"/key1");
      mqttClient.subscribe(MQTT_TOPIC"/key2");
      mqttClient.subscribe(MQTT_TOPIC"/key3");
      blinkLED(LED, 40, 8);
      digitalWrite(LED, LOW);
    }
    else {
      Serial.println(" FAILED!");
      Serial.println("\n----------------------------------------------------------------");
      Serial.println();
    }
  }
  else {
    Serial.println(" WiFi FAILED!");
    Serial.println("\n----------------------------------------------------------------");
    Serial.println();
  }
}

void loop() { 
  ArduinoOTA.handle();
  if (OTAupdate == false) { 
    mqttClient.loop();
    timedTasks();
    checkStatus();
  }
}

void blinkLED(int pin, int duration, int n) {             
  for(int i=0; i<n; i++)  {  
    digitalWrite(pin, HIGH);        
    delay(duration);
    digitalWrite(pin, LOW);
    delay(duration);
  }
}

void button1() {
  if (!digitalRead(BUTTON1)) {
    count1++;
  } 
  else {
    if (count1 > 1 && count1 <= 40) {   
      digitalWrite(RELAY1, !digitalRead(RELAY1));
      sendStatus1 = true;
    } 
    else if (count1 >40){
      Serial.println("\n\nRebooting . . . . . . . . Please Wait"); 
      requestRestart = true;
    } 
    count1=0;
  }
}

void button2() {
  if (!digitalRead(BUTTON2)) {
    count2++;
  } 
  else {
    if (count2 > 1 && count2 <= 40) {   
      digitalWrite(RELAY2, !digitalRead(RELAY2));
      sendStatus2 = true;
    } 
    count2=0;
  }
}

void button3() {
  if (!digitalRead(BUTTON3)) {
    count3++;
  } 
  else {
    if (count3 > 1 && count3 <= 40) {   
      digitalWrite(RELAY3, !digitalRead(RELAY3));
      sendStatus3 = true;
    } 
    count3=0;
  }
}

void checkConnection() {
  if (WiFi.status() == WL_CONNECTED)  {
    if (mqttClient.connected()) {
      Serial.println("mqtt broker connection . . . . . . . . . . OK");
    }
    else {
      Serial.println("mqtt broker connection . . . . . . . . . . LOST");
      requestRestart = true;
    }
  }
  else { 
    Serial.println("WiFi connection . . . . . . . . . . LOST");
    requestRestart = true;
  }
}

void checkStatus() {
  if (sendStatus1) {
    if(digitalRead(RELAY1) == LOW)  {
      if (rememberRelayState1) {
        EEPROM.write(0, 0);
      }
      mqttClient.publish(MQTT::Publish(MQTT_TOPIC"/key1/stat", "off").set_retain().set_qos(1));
      Serial.println("Relay 1 . . . . . . . . . . . . . . . . . . OFF");
    } else {
      if (rememberRelayState1) {
        EEPROM.write(0, 1);
      }
    mqttClient.publish(MQTT::Publish(MQTT_TOPIC"/key1/stat", "on").set_retain().set_qos(1));
    Serial.println("Relay 1 . . . . . . . . . . . . . . . . . . ON");
    }
    sendStatus1 = false;
  }
  if (sendStatus2) {
    if(digitalRead(RELAY2) == LOW)  {
      if (rememberRelayState2) {
        EEPROM.write(1, 0);
      }
      mqttClient.publish(MQTT::Publish(MQTT_TOPIC"/key2/stat", "off").set_retain().set_qos(2));
      Serial.println("Relay 2 . . . . . . . . . . . . . . . . . . OFF");
    } else {
      if (rememberRelayState2) {
        EEPROM.write(1, 1);
      }
    mqttClient.publish(MQTT::Publish(MQTT_TOPIC"/key2/stat", "on").set_retain().set_qos(2));
    Serial.println("Relay 2 . . . . . . . . . . . . . . . . . . ON");
    }
    sendStatus2 = false;
  }
  if (sendStatus3) {
    if(digitalRead(RELAY3) == LOW)  {
      if (rememberRelayState3) {
        EEPROM.write(2, 0);
      }
      mqttClient.publish(MQTT::Publish(MQTT_TOPIC"/key3/stat", "off").set_retain().set_qos(3));
      Serial.println("Relay 3 . . . . . . . . . . . . . . . . . . OFF");
    } else {
      if (rememberRelayState3) {
        EEPROM.write(2, 1);
      }
    mqttClient.publish(MQTT::Publish(MQTT_TOPIC"/key3/stat", "on").set_retain().set_qos(3));
    Serial.println("Relay 3 . . . . . . . . . . . . . . . . . . ON");
    }
    sendStatus3 = false;
  }
  if (rememberRelayState1 || rememberRelayState2 || rememberRelayState3) {
    EEPROM.commit();
  }
  if (requestRestart) {
    blinkLED(LED, 400, 4);
    ESP.restart();
  }
}

void timedTasks() {
  if ((millis() > TTasks + (kUpdFreq*60000)) || (millis() < TTasks)) { 
    TTasks = millis();
    checkConnection();
  }
}
