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

const char* ssid = "Smart Solutions";
const char* password = "913niraj913913";

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
  // Your hexdump function remains unchanged
}

void connectToWiFi() {
  USE_SERIAL.println("Connecting to WiFi...");
  WiFiMulti.addAP("Smart Solutions", "913niraj913913");

  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }
  USE_SERIAL.println("Connected to WiFi");
}

void setupWebSocket() {
  USE_SERIAL.println("[SETUP] Starting WebSocket connection setup...");
  webSocket.begin("192.168.1.47", 8080, "/bishal");
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
  distanceCm = duration * SOUND_SPEED / 2;
  gasValue = ppm;

  sendSensorData("HC-SR04", distanceCm);
  sendSensorData("DS18B20", temperatureC);
  sendSensorData("MQ-135", ppm);


  webSocket.loop();
}

void sendSensorData(const char* sensorType, float sensorValue) {
  DynamicJsonDocument jsonDocument(200);
  jsonDocument["sensorType"] = sensorType;
  jsonDocument["sensorValue"] = sensorValue;

  String jsonString;
  serializeJson(jsonDocument, jsonString);

  webSocket.sendTXT(jsonString);
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  USE_SERIAL.printf("[WSc] Received WebSocket message of type %d\n", type);
  USE_SERIAL.printf("[WSc] WebSocket payload %s\n", payload);
  USE_SERIAL.printf("[WSc] WebSocket message length %d\n", length);
  switch (type) {
    case WStype_DISCONNECTED:
      USE_SERIAL.printf("[WSc] Disconnected!\n");
      break;

    case WStype_CONNECTED:
      {
        // Extract the connected URL from payload
        String connectedUrl = String((char*)payload).substring(0, length);
        USE_SERIAL.printf("[WSc] Connected to url: %s\n", connectedUrl.c_str());

        // send message to server when Connected
        webSocket.sendTXT("Connected");
        break;
      }

    case WStype_TEXT:
      {
        USE_SERIAL.printf("[WSc] get text: %s\n", payload);

        // Convert payload to String
        String payloadString = String((char*)payload);

        // Display the JSON data before parsing
        USE_SERIAL.println("Received JSON data:");
        USE_SERIAL.println(payloadString);

        // Parse the received JSON data
        StaticJsonDocument<200> jsonDocument;
        DeserializationError error = deserializeJson(jsonDocument, payloadString);

        // Check if parsing was successful
        if (error) {
          USE_SERIAL.printf("[WSc] Error parsing JSON: %s\n", error.c_str());
        } else {
          // Access JSON data
          const char* message = jsonDocument["message"];
          int value = jsonDocument["value"].as<int>();  // Corrected to read as int
          USE_SERIAL.printf("[WSc] Parsed JSON - Message: %s, Value: %d\n", message, value);
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
