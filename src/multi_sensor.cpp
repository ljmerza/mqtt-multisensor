
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include "config.h"
#include "multi_sensor.h"

int OTAport = 8266;

float diffTEMP = 0.2;
float tempValue;
float diffHUM = 1;
float humValue;

int pirValue;
int pirStatus;
bool motionStatus;
const char* on_cmd = "ON";
const char* off_cmd = "OFF";

char message_buff[100];
const int BUFFER_SIZE = 300;
#define MQTT_MAX_PACKET_SIZE 512

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  pinMode(PIRPIN, INPUT);
  pinMode(DHTPIN, INPUT);

  Serial.begin(9600);
  delay(10);

  ArduinoOTA.setPort(OTAport);
  ArduinoOTA.setHostname(SENSORNAME);
  ArduinoOTA.setPassword((const char *)OTApassword);

  Serial.println("Starting Node named " + String(SENSORNAME));
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  ArduinoOTA.onStart([]() {
    Serial.println("Starting");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IPess: ");
  Serial.println(WiFi.localIP());

  reconnect();
}

void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void sendState() {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  root["humidity"] = humValue;
  root["motion"] = motionStatus ? on_cmd : off_cmd;
  root["temperature"] = tempValue;
  root["heatIndex"] = dht.computeHeatIndex(tempValue, humValue, IsFahrenheit);

  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  Serial.println(buffer);
  client.publish(light_state_topic, buffer, true);
}

void reconnect() {

  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    if (client.connect(SENSORNAME, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      sendState();

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

bool checkBoundSensor(float newValue, float prevValue, float maxDiff) {
  return newValue < prevValue - maxDiff || newValue > prevValue + maxDiff;
}


void loop() {
  ArduinoOTA.handle();
  
  if (!client.connected()) {
    softwareReset();
  }

  client.loop();
  
  // get pir valuea and send it
  pirValue = digitalRead(PIRPIN);
  if (pirValue == LOW && pirStatus != 1) {
    motionStatus = false;
    sendState();
    pirStatus = 1;

  }else if (pirValue == HIGH && pirStatus != 2) {
    motionStatus = true;
    sendState();
    pirStatus = 2;
  }

  delay(100);

  // get dth values and send them
  float newTempValue = dht.readTemperature(IsFahrenheit);
  float newHumValue = dht.readHumidity();

  if (checkBoundSensor(newTempValue, tempValue, diffTEMP)) {
    tempValue = newTempValue;
    sendState();
  }

  if (checkBoundSensor(newHumValue, humValue, diffHUM)) {
    humValue = newHumValue;
    sendState();
  }
}

void softwareReset() {
  Serial.print("resetting");
  ESP.reset(); 
}
