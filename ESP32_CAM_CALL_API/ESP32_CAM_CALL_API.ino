#include <Arduino.h>
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"

// const char* ssid = "Team . NET";
// const char* password = "Nepo913913";

const char* ssid = "Smart Solutions";
const char* password = "913niraj913913";

// String serverName = "192.168.18.98";
// String serverName = "192.168.1.242";
// String serverName = "192.168.18.157";
// String serverName = "192.168.1.219";
String serverName = "192.168.1.68";
String serverPath = "/v1/api/sensors/upload";
const int serverPort = 9990;

WiFiClient client;

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const int timerInterval = 30000;    // time between each HTTP POST image
unsigned long previousMillis = 0;   // last time image was sent

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(9600);
  // Connecting to WiFi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());

  // Camera Configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }
}

void loop() {
    while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print("WiFi Disconnected in LOOP!!!");
  }
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= timerInterval) {
    sendPhoto();
    previousMillis = currentMillis;
  }
}

void sendPhoto() {
 camera_fb_t* fb = esp_camera_fb_get();
 if (!fb) {
 Serial.println("Camera capture failed");
 delay(1000);
 ESP.restart();
 }

 if (client.connect(serverName.c_str(), serverPort)) {
 String head = "--MyBoundary\r\nContent-Disposition: form-data; name=\"file\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
 String tail = "\r\n--MyBoundary--\r\n";

 uint32_t imageLen = fb->len;
 uint32_t extraLen = head.length() + tail.length();
 uint32_t totalLen = imageLen + extraLen;

 client.println("POST " + serverPath + " HTTP/1.1");
 client.println("Host: " + serverName);
 client.println("Content-Length: " + String(totalLen));
 client.println("Content-Type: multipart/form-data; boundary=MyBoundary");
 client.println();
 client.print(head);

 size_t fbLen = fb->len;
 for (size_t n = 0; n < fbLen; n = n + 1024) {
 if (n + 1024 < fbLen) {
 client.write(fb->buf, 1024);
 fb->buf += 1024;
 } else if (fbLen % 1024 > 0) {
 size_t remainder = fbLen % 1024;
 client.write(fb->buf, remainder);
 }
 }

 client.print(tail);

 esp_camera_fb_return(fb);

 while (client.connected()) {
 String line = client.readStringUntil('\n');
 Serial.println(line);
 }

 client.stop();
 Serial.println("Image uploaded successfully");
 } else {
 Serial.println("Connection to server failed");
 }
}

// String sendPhoto() {
//   String getAll;
//   String getBody;

//   camera_fb_t* fb = NULL;
//   fb = esp_camera_fb_get();
//   if (!fb) {
//     Serial.println("Camera capture failed");
//     delay(1000);
//     ESP.restart();
//   }

//   Serial.println("Connecting to server: " + serverName);

//   if (client.connect(serverName.c_str(), serverPort)) {
//     Serial.println("Connection successful!");
//     String head = "--BishalShrestha\r\nContent-Disposition: form-data; name=\"file\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
//     String tail = "\r\n--BishalShrestha--\r\n";

//     uint32_t imageLen = fb->len;
//     uint32_t extraLen = head.length() + tail.length();
//     uint32_t totalLen = imageLen + extraLen;

//     client.println("POST " + serverPath + " HTTP/1.1");
//     client.println("Host: " + serverName);
//     client.println("Content-Length: " + String(totalLen));
//     client.println("Content-Type: multipart/form-data; boundary=BishalShrestha");
//     client.println();
//     client.print(head);

//     uint8_t* fbBuf = fb->buf;
//     size_t fbLen = fb->len;
//     for (size_t n = 0; n < fbLen; n = n + 1024) {
//       if (n + 1024 < fbLen) {
//         client.write(fbBuf, 1024);
//         fbBuf += 1024;
//       } else if (fbLen % 1024 > 0) {
//         size_t remainder = fbLen % 1024;
//         client.write(fbBuf, remainder);
//       }
//     }

//     client.print(tail);

//     esp_camera_fb_return(fb);

//     int timoutTimer = 10000;
//     long startTimer = millis();
//     boolean state = false;

//     while ((startTimer + timoutTimer) > millis()) {
//       Serial.print(".");
//       delay(100);
//       while (client.available()) {
//         char c = client.read();
//         if (c == '\n') {
//           if (getAll.length() == 0) {
//             state = true;
//           }
//           getAll = "";
//         } else if (c != '\r') {
//           getAll += String(c);
//         }
//         if (state == true) {
//           getBody += String(c);
//         }
//         startTimer = millis();
//       }
//       if (getBody.length() > 0) {
//         break;
//       }
//     }
//     Serial.println();
//     client.stop();
//     Serial.println(getBody);
//   } else {
//     getBody = "Connection to " + serverName + " failed.";
//     Serial.println(getBody);
//   }
//   return getBody;
// }