#include <SPI.h>
#include <LoRa.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "MQ135.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include "time.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

const int oneWireBus = 4;
OneWire oneWire(oneWireBus);
DallasTemperature tempsensor(&oneWire);

// const char* ssid = "Smart Solutions";
// const char* password = "913niraj913913";
const char* ssid = "Team . NET";
const char* password = "Nepo913913";
IPAddress customIP(192, 168, 18, 110);
IPAddress customSoftIP(192, 168, 4, 5);
IPAddress gateway(192, 168, 18, 1);
IPAddress subnet(255, 255, 255, 0);
WebServer server(80);

#define ss 5
#define rst 14
#define dio0 2
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
const int trigPin = 33;
const int echoPin = 32;
const int gasPin = 35;
struct tm timeinfo;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
MQ135 gasSensor = MQ135(gasPin);
#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701
float ppm;
long duration;
float distanceCm;
float gasValue;
String formattedTime;

String enteredUsername = "";
String enteredPassword = "";

const int numReadings = 5;
float distanceArray[numReadings];
float temperatureArray[numReadings];
float rzeroArray[numReadings];
float ppmArray[numReadings];


void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  tempsensor.begin();

  WiFi.config(customIP, gateway, subnet);

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("WiFi Connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Configure WiFi access point mode
  WiFi.softAPConfig(customSoftIP, gateway, subnet);

  // Set timezone and initialize time object
  initTime("NST-5:45");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  setTimezone("<+0545>-5:45");
  formattedTime = getFormattedTime();
  Serial.println("Formatted Time: " + formattedTime);

  server.on("/", handle_OnConnect);
  server.on("/test", handle_OnTest);
  server.on("/dashboard", handle_Dashboard);
  server.on("/esp32dashboard", handle_esp32_Dashboard);
  server.on("/error", handle_Error);
  server.onNotFound(handle_NotFound);
  server.begin();

  Serial.println("HTTP server started");
  Serial.println("Ready.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  while (!Serial)
    ;

  // Start LoRa
  LoRa.setPins(ss, rst, dio0);
  Serial.println("LoRa Transmitter Module");
  while (!LoRa.begin(433E6)) {
    Serial.println(".");
    delay(500);
  }
  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Transmitter Module Initializing OK!");
}

void loop() {
  formattedTime = getFormattedTime();
  tempsensor.requestTemperatures();
  float temperatureC = tempsensor.getTempCByIndex(0);
  float rzero = gasSensor.getRZero();
  float ppm = gasSensor.getPPM();
  digitalWrite(trigPin, LOW);
  delayMicroseconds(10);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distanceCm = duration * SOUND_SPEED / 2;
  gasValue = ppm;
  while (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    delay(500);
  }

  displayOled("HC-SR04", distanceCm);
  printPayload("NODE-3|HC-SR04", distanceCm);
  sendLoraPacket("NODE-3|HC-SR04|", distanceCm, formattedTime);
  delay(2000);

  displayOled("DS18B20", temperatureC);
  printPayload("NODE-3|DS18B20", temperatureC);
  sendLoraPacket("NODE-3|DS18B20|", temperatureC, formattedTime);
  delay(2000);

  displayOled("MQ-135", ppm);
  printPayload("NODE-3|MQ-135", ppm);
  sendLoraPacket("NODE-3|MQ-135|", ppm, formattedTime);
  delay(2000);

  Serial.println("-----------------------------------------------------");
  delay(2000);

  server.handleClient();
}

void sendLoraPacket(String node_desc, float value, String formattedTime) {
  LoRa.beginPacket();
  LoRa.print(node_desc);
  LoRa.print(value);
  LoRa.print("|" + formattedTime + ".000+00:00");
  LoRa.endPacket();
}

void printPayload(String node_desc, float value) {
  Serial.print("Sending Data: " + node_desc + "|");
  Serial.print(value);
  Serial.println("|" + formattedTime + ".000+00:00");
}

void displayOled(String node_desc, float value) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(node_desc);
  display.print("Val: ");
  display.println(value);
  display.display();
}

void setTimezone(String timezone) {
  Serial.printf("  Setting Timezone to %s\n", timezone.c_str());
  setenv("TZ", timezone.c_str(), 1);
  tzset();
}

void initTime(String timezone) {
  Serial.println("Setting up time");
  configTime(0, 0, ntpServer);
  if (!getLocalTime(&timeinfo)) {
    Serial.println("  Failed to obtain time in init function.");
    return;
  }
  Serial.println("  Got the time from NTP");
  setTimezone(timezone);
}

void setTime(int yr, int month, int mday, int hr, int minute, int sec, int isDst) {
  struct tm tm;
  tm.tm_year = yr - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = mday;
  tm.tm_hour = hr;
  tm.tm_min = minute;
  tm.tm_sec = sec;
  tm.tm_isdst = isDst;
  time_t t = mktime(&tm);
  Serial.printf("Setting time: %s", asctime(&tm));
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);
}

String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "Failed to obtain time";
  }
  char timeString[50];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeString);
}

void handle_OnConnect() {
  if (server.method() == HTTP_POST) {
    enteredUsername = server.arg("username");
    enteredPassword = server.arg("password");
    if (enteredUsername.equals("admin") && enteredPassword.equals("password")) {
      server.sendHeader("Location", "/dashboard", true);
      server.send(302, "text/plain", "");
      return;
    } else if (enteredUsername.equals("bishal") && enteredPassword.equals("shrestha")) {
      server.sendHeader("Location", "/esp32dashboard", true);
      server.send(302, "text/plain", "");
      return;
    } else {
      server.sendHeader("Location", "/error", true);
      server.send(302, "text/plain", "");
      return;
    }
  }
  server.send(200, "text/html", SendHomePage());
}

void recordSensorData() {
  for (int i = 0; i < numReadings; ++i) {
    // Your sensor reading code here
    digitalWrite(trigPin, LOW);
    delayMicroseconds(10);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    long duration = pulseIn(echoPin, HIGH);
    float distanceCm = duration * SOUND_SPEED / 2;

    tempsensor.requestTemperatures();
    float temperatureC = tempsensor.getTempCByIndex(0);

    float rzero = gasSensor.getRZero();
    float ppm = gasSensor.getPPM();

    // Store the readings in the arrays
    distanceArray[i] = distanceCm;
    temperatureArray[i] = temperatureC;
    rzeroArray[i] = rzero;
    ppmArray[i] = ppm;

    delay(2000);  // Delay between readings
  }
}

void handle_Dashboard() {
  recordSensorData();

  String dashboardContent = "<!DOCTYPE html> <html>\n";
  dashboardContent += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  dashboardContent += "<title>ESP32 Charts</title>\n";
  dashboardContent += "<style> html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  dashboardContent += "body { font-family: Arial, sans-serif; background-color: #000; color: #fff;} h3 {color: #fafafa;}\n";
  dashboardContent += "p {font-size: 14px;color: #fff;margin-bottom: 10px;}\n";
  dashboardContent += "a{color: inherit;} h1 {color: #00b8e6;} nav{display: flex;justify-content: space-between;width: 100%;} .container{padding: 1rem;display: flex;flex-direction: column;justify-content: center;align-items: center;}";
  dashboardContent += ".container-nav{height: 10vh;} .container-body{display: flex; flex-wrap: wrap; justify-content: space-around;} .chart-box{margin: 20px; background-color: #333; border-radius: 10px; padding: 20px; width: calc(50% - 40px);}";
  dashboardContent += ".container-footer{height: 10vh;} input{margin: 4px;padding: 4px;} .info{border: 1px solid #000; padding: 1rem;}";
  dashboardContent += "</style>\n";
  dashboardContent += "<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>\n";
  dashboardContent += "</head>\n";
  dashboardContent += "<body>\n";
  dashboardContent += "<div class=\"container container-nav\"><nav><h1><a href=\"/\">ESP32</a></h1> <div style=\"display: flex; gap: 6px;\"><a href=\"/esp32dashboard\">Info</a><a href=\"/\">Log Out</a></div></nav></div>\n";

  // Displaying chart canvas
  dashboardContent += "<div class=\"container container-body\">";

  dashboardContent += "<div class=\"chart-box\">";
  dashboardContent += "<h2>HC-SR04 | Distance Sensor Data</h2>";
  dashboardContent += dashboardChart("distanceChart");
  dashboardContent += "</div>";

  // Displaying chart canvas
  dashboardContent += "<div class=\"chart-box\">";
  dashboardContent += "<h2>DS18B20 | Temperature Sensor Data</h2>";
  dashboardContent += dashboardChart("temperatureChart");
  dashboardContent += "</div>";

  // Displaying chart canvas
  dashboardContent += "<div class=\"chart-box\">";
  dashboardContent += "<h2>MQ135 | Gas Sensor Data | rZero Values</h2>";
  dashboardContent += dashboardChart("rzeroChart");
  dashboardContent += "</div>";

  // Displaying chart canvas
  dashboardContent += "<div class=\"chart-box\">";
  dashboardContent += "<h2>MQ135 | Gas Sensor Data | ppm Values</h2>";
  dashboardContent += dashboardChart("ppmChart");
  dashboardContent += "</div>";

  dashboardContent += "</div>";

  // Displaying footer
  dashboardContent += "<div class=\"container container-footer\"><p>V 1.0.0 Copyright Smart Solutions Technology 2023</p></div>\n";

  String dashboardChartScriptFunction = dashboardChartScript("distanceChart", "One", "Two", "Three", "Four", "Five", distanceArray[0], distanceArray[1], distanceArray[2], distanceArray[3], distanceArray[4]);
  dashboardChartScriptFunction += dashboardChartScript("temperatureChart", "One", "Two", "Three", "Four", "Five", temperatureArray[0], temperatureArray[1], temperatureArray[2], temperatureArray[3], temperatureArray[4]);
  dashboardChartScriptFunction += dashboardChartScript("rzeroChart", "One", "Two", "Three", "Four", "Five", rzeroArray[0], rzeroArray[1], rzeroArray[2], rzeroArray[3], rzeroArray[4]);
  dashboardChartScriptFunction += dashboardChartScript("ppmChart", "One", "Two", "Three", "Four", "Five", ppmArray[0], ppmArray[1], ppmArray[2], ppmArray[3], ppmArray[4]);
  dashboardContent += dashboardChartScriptFunction;

  dashboardContent += "</body>\n";
  dashboardContent += "</html>\n";

  server.send(200, "text/html", dashboardContent);
}

String dashboardChart(String chartId) {
  String returningHtml = "<div class=\"chart-container\" style=\"position: relative; height:auto; width:200px\">";
  returningHtml += "<canvas id=\"" + chartId + "\"></canvas>";
  returningHtml += "</div>";
  return returningHtml;
}

String dashboardChartScript(String chartId, String label1, String label2, String label3, String label4, String label5, int data1, int data2, int data3, int data4, int data5) {
  String dashboardScriptContent = "<script>";
  dashboardScriptContent += "document.addEventListener('DOMContentLoaded', function() {";
  dashboardScriptContent += "  var ctx = document.getElementById('" + chartId + "').getContext('2d');";
  dashboardScriptContent += "  var sensorChartAPI = new Chart(ctx, {";
  dashboardScriptContent += "    type: 'bar',";
  dashboardScriptContent += "    data: {";
  dashboardScriptContent += "      labels: ['" + label1 + "','" + label2 + "','" + label3 + "','" + label4 + "','" + label5 + "'],";
  dashboardScriptContent += "      datasets: [{";
  dashboardScriptContent += "        label: 'Sensor Data (API)',";
  dashboardScriptContent += "        data: [" + String(data1) + "," + String(data2) + "," + String(data3) + "," + String(data4) + "," + String(data5) + "],";
  dashboardScriptContent += "        backgroundColor: [\"rgba(75,192,192,0.2)\", \"rgba(255,99,132,0.2)\", \"rgba(255,206,86,0.2)\", \"rgba(54,162,235,0.2)\", \"rgba(153,102,255,0.2)\"],";
  dashboardScriptContent += "        borderColor: [\"rgba(75,192,192,1)\", \"rgba(255,99,132,1)\", \"rgba(255,206,86,1)\", \"rgba(54,162,235,1)\", \"rgba(153,102,255,1)\"],";
  dashboardScriptContent += "        borderWidth: 1";
  dashboardScriptContent += "      }]";
  dashboardScriptContent += "    },";
  dashboardScriptContent += "    options: {";
  dashboardScriptContent += "      scales: {";
  dashboardScriptContent += "        y: {";
  dashboardScriptContent += "          beginAtZero: false";
  dashboardScriptContent += "        }";
  dashboardScriptContent += "      }";
  dashboardScriptContent += "    }";
  dashboardScriptContent += "  });";
  dashboardScriptContent += "});";
  dashboardScriptContent += "</script>";
  return dashboardScriptContent;
}

void handle_esp32_Dashboard() {
  tempsensor.requestTemperatures();
  float temperatureC = tempsensor.getTempCByIndex(0);
  float rzero = gasSensor.getRZero();
  float ppm = gasSensor.getPPM();
  digitalWrite(trigPin, LOW);
  delayMicroseconds(10);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  float distanceCm = duration * SOUND_SPEED / 2;
  gasValue = ppm;

  String dashboardContent = "<!DOCTYPE html> <html>\n";
  dashboardContent += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  dashboardContent += "<title>ESP32 Dashboard</title>\n";
  dashboardContent += "<style> html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  dashboardContent += "body { font-family: Arial, sans-serif; background-color: #000; color: #f4f4ff;} h3 {color: #00b8e6;}\n";
  dashboardContent += "p {font-size: 16px;color: #bbb;margin-bottom: 10px;}\n";
  dashboardContent += "a{color: inherit;} h1 {color: #00b8e6;} nav{display: flex;justify-content: space-between;width: 100%;} .container{padding: 1rem;display: flex;flex-direction: column;justify-content: center;align-items: center;}";
  dashboardContent += ".container-nav{height: 10vh;} .container-body{width: 100%;} .container-footer{height: 10vh;} input{margin: 4px;padding: 4px;} .info{border: 1px solid #555; border-radius: 10px; padding: 1rem; margin-top: 20px;}";
  dashboardContent += ".button-container { margin-top: 20px; } button { background-color: #00b8e6; color: #fff; padding: 10px 20px; font-size: 16px; border: none; border-radius: 5px; cursor: pointer; }";
  dashboardContent += "</style>\n";
  dashboardContent += "<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>\n";
  dashboardContent += "<script>";
  dashboardContent += "function refresh() { location.reload(); }";
  dashboardContent += "function applyAPConfig() {";
  dashboardContent += "  WiFi.softAPIP(newAPIP);";
  dashboardContent += "}";
  dashboardContent += "</script>\n";
  dashboardContent += "</head>\n";
  dashboardContent += "<body>\n";

  // Displaying nav
  dashboardContent += "<div class=\"container container-nav\"><nav><h1><a href=\"/\">ESP32</a></h1> <div style=\"display: flex; gap: 6px;\"><a href=\"/dashboard\">Charts</a><a href=\"/\">Log Out</a></div></nav></div>\n";

  // Displaying body
  dashboardContent += "<div class=\"container container-body\">";
  dashboardContent += "<h2>ESP32 Dashboard</h2>";

  // Displaying controls
  dashboardContent += "<div class=\"button-container\">";
  dashboardContent += "<button onclick=\"refresh()\">Refresh values</button>";
  dashboardContent += "</div>";

  // Add an input field for customizing the access point IP
  dashboardContent += "<label for=\"ap_ip\">Access Point IP:</label>";
  dashboardContent += "<input type=\"text\" id=\"ap_ip\" name=\"ap_ip\" value=\"" + WiFi.softAPIP().toString() + "\">";

  // Add a button to apply the new access point IP
  dashboardContent += "<button onclick=\"applyAPConfig()\">Apply AP Config</button>";
  dashboardContent += "</div>";

  dashboardContent += "<div class=\"info\">";
  // Displaying system information
  dashboardContent += "<div>";
  dashboardContent += "<h3>System Information</h3>";
  dashboardContent += "<p><strong>Free Heap Memory:</strong> " + String(ESP.getFreeHeap()) + " bytes</p>";
  dashboardContent += "<p><strong>Free Psram Memory:</strong> " + String(ESP.getFreePsram()) + "</p>";
  dashboardContent += "<p><strong>Chip Model:</strong> " + String(ESP.getChipModel()) + "</p>";
  dashboardContent += "<p><strong>ESP Cores:</strong> " + String(ESP.getChipCores()) + "</p>";
  dashboardContent += "</div>";

  // Displaying network information
  dashboardContent += "<div>";
  dashboardContent += "<h3>Network Information</h3>";
  dashboardContent += "<p><strong>Device Name:</strong> ESP32 Node</p>";
  dashboardContent += "<p><strong>MAC Address:</strong> " + getMacAddress() + "</p>";
  dashboardContent += "<p><strong>IP Address (WiFi):</strong> " + WiFi.localIP().toString() + "</p>";
  dashboardContent += "<p><strong>IP Address (Access Point):</strong> " + WiFi.softAPIP().toString() + "</p>";
  dashboardContent += "</div>";

  // Displaying sensor data
  dashboardContent += "<div>";
  dashboardContent += "<h3>Sensor Data</h3>";
  dashboardContent += "<p><strong>Temperature:</strong> " + String(temperatureC) + " &deg;C</p>";
  dashboardContent += "<p><strong>Distance:</strong> " + String(distanceCm) + " cm</p>";
  dashboardContent += "<p><strong>Gas Level:</strong> " + String(gasValue) + " ppm</p>";
  dashboardContent += "</div>";
  dashboardContent += "</div>";
  dashboardContent += "</div>";
  dashboardContent += "</div>";

  dashboardContent += "<div class=\"container container-footer\"><p>V 1.0.0 Copyright Smart Solutions Technology 2023</p></div>\n";
  dashboardContent += "</body>\n";
  dashboardContent += "</html>\n";

  server.send(200, "text/html", dashboardContent);
}

String getMacAddress() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[18] = { 0 };
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

void handle_Error() {
  // Add code to display content for the error page
  server.send(200, "text/html", "Invalid username or password. Please try again.");
}


void handle_OnTest() {
  server.send(200, "text/plain", "Hello from test page.");
}

void handle_NotFound() {
  server.send(404, "text/plain", "Page not found.");
}

String SendHomePage() {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Web Server - ESP32 | SST</title>\n";
  ptr += "<style> html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body { font-family: Arial, sans-serif; background-color: #121212; color: #f4f4ff;} h3 {color: #00b8e6;}\n";
  ptr += "p {font-size: 16px;color: #bbb;margin-bottom: 10px;}\n";
  ptr += "a{color: inherit;} h1 {color: #00b8e6;} nav{display: flex;justify-content: space-between;width: 100%;} .container{padding: 1rem; display: flex;flex-direction: column;justify-content: center;align-items: center;}";
  ptr += ".container-nav{height: 10vh;} .container-body{width: 100%; height: 80vh; display: flex; align-items: center; justify-content: center;} .container-footer{height: 10vh;} input{margin: 10px;padding: 10px; width: 80%; border-radius: 5px; font-size: 16px;} .info{border: 1px solid #555; border-radius: 10px; padding: 1rem; margin-top: 20px;}";
  ptr += ".button-container { margin-top: 20px; } button { background-color: #00b8e6; color: #fff; padding: 15px 30px; font-size: 18px; border: none; border-radius: 5px; cursor: pointer; }";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";

  // Displaying nav
  ptr += "<div class=\"container container-nav\"><nav><h1><a href=\"/\">ESP32</a></h1><h2>Not Logged In</h2></nav></div>\n";

  // Displaying body
  ptr += "<div class=\"container container-body\">";
  ptr += "<div class=\"container\">";
  ptr += "<form method=\"POST\">";
  ptr += "<label for=\"username\"><p>Username:</p></label><input type=\"text\" name=\"username\"><br>";
  ptr += "<label for=\"password\"><p>Password:</p></label><input type=\"password\" name=\"password\"><br>";
  ptr += "<div class=\"button-container\"><button type=\"submit\">Log In</button></div>";
  ptr += "</form>";
  ptr += "</div>";
  ptr += "</div>";

  // Displaying footer
  ptr += "<div class=\"container container-footer\"><p>V 1.0.0 Copyright Smart Solutions Technology 2023</p></div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";

  return ptr;
}
