#include <OneWire.h>
#include <DallasTemperature.h>
#include "MQ135.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

const int oneWireBus = 4;
OneWire oneWire(oneWireBus);
DallasTemperature tempsensor(&oneWire);

WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

const char* ssid = "Wifi SSID";
const char* password = "Wifi Password";

#define trigPin 33
#define echoPin 32
#define gasPin 35
#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701

MQ135 gasSensor(gasPin);

float ppm;
long duration;
float distanceCm;
float gasValue;

#define USE_SERIAL Serial

void hexdump(const void* mem, uint32_t len, uint8_t cols = 16) {
  const uint8_t* src = (const uint8_t*)mem;
  USE_SERIAL.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
  for (uint32_t i = 0; i < len; i++) {
    if (i % cols == 0) {
      USE_SERIAL.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
    }
    USE_SERIAL.printf("%02X ", *src);
    src++;
  }
  USE_SERIAL.printf("\n");
}
void connectToWiFi() {
  USE_SERIAL.println("Connecting to WiFi...");
  WiFiMulti.addAP("Wifi SSID", "Wifi Password");

  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }
  USE_SERIAL.println("Connected to WiFi");
}

void setupWebSocket() {
  USE_SERIAL.println("[SETUP] Starting WebSocket connection setup...");
  webSocket.begin("192.168.1.72", 8080, "/bishal");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void setup() {
  USE_SERIAL.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  tempsensor.begin();

  // Connect to Wi-Fi
  connectToWiFi();

  // Setup WebSocket
  setupWebSocket();
}

void loop() {
  tempsensor.requestTemperatures();
  float temperatureC = tempsensor.getTempCByIndex(0);
  ppm = gasSensor.getPPM();
  digitalWrite(trigPin, LOW);
  delayMicroseconds(10);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  delayMicroseconds(100);
  distanceCm = duration * SOUND_SPEED / 2;
  gasValue = ppm;
  delay(500);

  sendSensorData("HC-SR04", distanceCm);
  sendSensorData("DS18B20", temperatureC);
  sendSensorData("MQ-135", ppm);


  webSocket.loop();
}

void sendSensorData(const char* sensorType, float sensorValue) {
  DynamicJsonDocument jsonDocument(200);
  jsonDocument["sensor_id"] = sensorType;
  jsonDocument["sensor_data"] = sensorValue;

  String jsonString;
  serializeJson(jsonDocument, jsonString);

  webSocket.sendTXT(jsonString);
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  USE_SERIAL.printf("[WSc] Received WebSocket message of type %d\n", type);
  USE_SERIAL.printf("[WSc] WebSocket payload: %s\n", payload);
  USE_SERIAL.printf("[WSc] WebSocket payload length %d\n", length);
  switch (type) {
    case WStype_DISCONNECTED:
      USE_SERIAL.printf("[WSc] Disconnected!\n");
      break;

    case WStype_CONNECTED:
      {
        String connectedUrl = String((char*)payload).substring(0, length);
        USE_SERIAL.printf("[WSc] Connected to url: %s\n", connectedUrl.c_str());
        webSocket.sendTXT("Connected");
        break;
      }

    case WStype_TEXT:
      {
        USE_SERIAL.printf("[WSc] get text: %s\n", payload);
        String payloadString = String((char*)payload);
        USE_SERIAL.println("Received JSON data:");
        USE_SERIAL.println(payloadString);

        StaticJsonDocument<200> jsonDocument;
        DeserializationError error = deserializeJson(jsonDocument, payloadString);

        if (error) {
          USE_SERIAL.printf("[WSc] Error parsing JSON: %s\n", error.c_str());
        } else {
          const char* sensorId = jsonDocument["sensor_id"];
          float sensorData = jsonDocument["sensor_data"].as<float>();
          USE_SERIAL.printf("[WSc] Parsed JSON - Sensor ID: %s, Sensor Data: %f\n", sensorId, sensorData);
        }
        break;
      }

    case WStype_BIN:
      USE_SERIAL.printf("[WSc] get binary length: %u\n", length);
      hexdump(payload, length);
      break;

    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}
