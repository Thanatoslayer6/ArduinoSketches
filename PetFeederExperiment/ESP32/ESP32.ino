#include <EEPROM.h>
#include <WiFi.h>
#include "time.h"
#include <HardwareSerial.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>
#include <neotimer.h>
#include <ezButton.h>
#include "esp_camera.h"
#include "secrets.h"
#include "ArduinoJson.h"

// Default camera pin definitions (Taken from ESP32CameraWebserver code)
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
//#define GPIO_LED_NUM      33

// Set up hardware serial to send data to the arduino
HardwareSerial sendToArduino(1);

// Create secure wifi client
WiFiClientSecure esp32cam;
// Create mqtt client
PubSubClient client(esp32cam);
Servo dispenser;
Neotimer servoTimer = Neotimer();
Neotimer uvTimer = Neotimer();

// Necessary variables and definitions
bool streamToggled = false;
bool isThereStoredSchedules = false;
#define UVLIGHT_PIN 15
#define DISPENSER_PIN 14
#define LED_INDICATOR_PIN 33
#define RESETBTN_PIN 2
ezButton button(RESETBTN_PIN);  // create ezButton object that attach to pin 2

// Struct for storing schedule
// 0 - 31 -> 32 bytes = SSID
// 32 - 95 -> 64 bytes = PSK
// 96 - 2047 -> 1952 bytes = Schedule
struct TimeData {
  int h = -1; // hour
  int m = -1; // minute
  int d = -1; // duration
};

TimeData feeding_schedules[10];

void setup() { // TODO: EEPROM for wifi and ssid, also connection to database...
  Serial.begin(115200);
  button.setDebounceTime(50);
  pinMode(UVLIGHT_PIN, OUTPUT);
  pinMode(LED_INDICATOR_PIN, OUTPUT); // Built-in led
  dispenser.attach(DISPENSER_PIN); // Use GPIO14 of ESP32-Cam for servo motor
  EEPROM.begin(2048); // Use about 2KB of EEPROM

  sendToArduino.begin(9600, SERIAL_8N1, 2, 3);
  // Get stored feeding schedules
  readScheduleFromEEPROM();
  // Initialize Camera first
  CameraInit();

  ConnectToWifi();
  // Init NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if (WiFi.status() == WL_CONNECTED) {
    // Connect to MQTT
    ConnectToMQTT();
  }
  dispenser.write(0); // Reset servo first
  Serial.println("Resetted dispenser to 0 degrees");
  digitalWrite(LED_INDICATOR_PIN, LOW);
}

void saveScheduleToEEPROM() {
  // First clear the schedules from the eeprom 96-2047
  Serial.println("Erasing schedules from the EEPROM");
  for (int i = 96; i < 2048; i++) {
    EEPROM.write(i, 0);
  }
  Serial.println("Schedules are now cleared (not commited)");
  // Set the flag
  int eepromAddr = 96;
  EEPROM.write(eepromAddr, 0xFF); // Set a flag or something...
  eepromAddr++; // start writing data at 97
  for (int i = 0; i < 10; i++) {
    EEPROM.put(eepromAddr, feeding_schedules[i]);
    eepromAddr += sizeof(TimeData);
  }
  EEPROM.commit();
  isThereStoredSchedules = true;
  Serial.println("Latest schedule is now stored...");
}

void readScheduleFromEEPROM() {
  int eepromAddr = 96;
  if (EEPROM.read(eepromAddr) == 0xFF) {
    isThereStoredSchedules = true;
    eepromAddr++;
    for (int i = 0; i < 10; i++) {
      EEPROM.get(eepromAddr, feeding_schedules[i]);
      eepromAddr += sizeof(TimeData);
    }
  } else {
    Serial.println("There are no schedules set...");
  }
}

// Pass number of seconds in milliseconds
void moveServoMotor(uint16_t duration) {
  servoTimer.set(duration);
  Serial.print("Moving servo motor by 90 degrees for ");
  Serial.print(duration);
  Serial.print(" milliseconds");
  Serial.println();
  dispenser.write(145);
  servoTimer.start();
}

// Pass number of seconds in milliseconds
void toggleUVLight(unsigned long duration) {
  uvTimer.set(duration);
  Serial.print("UV light is on for ");
  Serial.print(duration);
  Serial.print(" milliseconds");
  Serial.println();
  digitalWrite(UVLIGHT_PIN, HIGH);
  uvTimer.start();
}


void ConnectToMQTT() {
  // First set root certificate
  esp32cam.setCACert(root_ca);
  client.setBufferSize(32768); // around 32kb
  //client.setBufferSize(65536); // around 64kb
  client.setServer(MQTTserver, MQTTport);
  client.setCallback(messageReceived);
  while (!client.connected()) {
    Serial.println("Connecting to MQTT broker...");
    String clientName = PRODUCT_ID + String("-esp32");
    if (client.connect(clientName.c_str(), MQTTusername, MQTTpassword)) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed to connect to MQTT broker, restarting device");
      delay(2000);
      ESP.restart();
    }
  }
  // Subscribe to the given topics
  client.subscribe(AUTH_TOPIC, 1);
  client.subscribe(FEED_DURATION_TOPIC, 1);
  client.subscribe(FEED_SCHEDULE_TOPIC, 1);
  client.subscribe(UVLIGHT_DURATION_TOPIC, 1);
  client.subscribe(TOGGLE_STREAM_TOPIC, 1);
  return;
}

void CameraInit() {
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
  config.frame_size = FRAMESIZE_VGA; // 640 x 480
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 32; // From 10-63 (lower means higher quality)
  config.fb_count = 2;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(2000); // Add 2 seconds delay probably
    ESP.restart(); // Restart the device
  }
  Serial.println("Initialized camera successfully");
  // Flip the camera
  sensor_t * s = esp_camera_sensor_get(); 
  s->set_vflip(s, 1);
}

void getImage() {
  // Make sure to turn off LED indicator light
  camera_fb_t *fb = esp_camera_fb_get();
  // Check if there's an image with jpeg format, and is less than 32kb
  if (fb != NULL && fb->format == PIXFORMAT_JPEG && fb->len < 32768) {
    Serial.print("Image Length: ");
    Serial.print(fb->len);
    Serial.println();
    bool result = client.publish(STREAM_TOPIC, fb->buf, fb->len);
    //Serial.println(result);
    if (!result) {
      ESP.restart();
    }
  } else {
    Serial.println("Failed to take image");
    return;
  }
  // Release the frame buffer
  esp_camera_fb_return(fb);
}

void ConnectToWifi() {
  String ssid;
  String pwd = "";
  //Serial.println("Reading SSID -> ");
  for (int i = 0; i < 32; i++) { // Read ssid
    //ssid += char(EEPROM.read(i));
    char c = EEPROM.read(i);
    if (c == 0) {
      break; // End of string
    }
    ssid += c;
  }
  Serial.print("SSID: ");
  Serial.print(ssid);
  //Serial.println("Reading PSK ->");
  for (int i = 32; i < 96; i++) {
    //pwd += char(EEPROM.read(i));
    char c = EEPROM.read(i);
    if (c == 0) {
      break; // End of string
    }
    pwd += c;
  }
  Serial.print("PSK: ");
  Serial.print(pwd);

  // MANUAL WIFI SETUP (If there is some password)
  if (!ssid.isEmpty() && !pwd.isEmpty()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pwd.c_str());
    Serial.print("Connecting to WiFi using SSID: ");
    Serial.print(ssid);
    Serial.print(" and password: ");
    Serial.println(pwd);

    // Wait for WiFi to connect to AP
    Serial.println("Waiting for WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      // RESET HANDLER IF THERE IS NO WIFI
      button.loop(); // MUST call the loop() function first

      if (button.isPressed()) {
        Serial.println("Erasing ESP32 EEPROM...");
        delay(1000);
        for (int i = 0; i < 2048; i++) { // Erase EEPROM by writing 0 to each byte
          digitalWrite(LED_INDICATOR_PIN, LOW);
          delay(50);
          EEPROM.write(i, 0);
          digitalWrite(LED_INDICATOR_PIN, HIGH);
          delay(50);
        }
        EEPROM.commit(); // Save changes to EEPROM
        delay(3000); // Wait for 3 seconds to avoid multiple erasures
        EEPROM.end();
        ESP.restart();
      }
      digitalWrite(LED_INDICATOR_PIN, LOW);
      delay(500);
      Serial.print(".");
      digitalWrite(LED_INDICATOR_PIN, HIGH);
      delay(500);
    }
    Serial.println("WiFi successfully connected");
    // Send in ssid and password to the other microcontroller via Serial communication
    sendToArduino.print(ssid + ";" + pwd);

  } else { // SMART WIFI CONFIG
    // No SSID and password stored in EEPROM, start SmartConfig
    Serial.println("SSID and Password not found. Starting SmartConfig...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.beginSmartConfig(SC_TYPE_ESPTOUCH_V2);

    // Wait for SmartConfig packet from mobile
    Serial.println("Waiting for SmartConfig.");
    while (!WiFi.smartConfigDone()) {
      digitalWrite(LED_INDICATOR_PIN, LOW);
      delay(2000);
      Serial.print(".");
      digitalWrite(LED_INDICATOR_PIN, HIGH);
      delay(2000);
    }

    Serial.println("");
    Serial.println("SmartConfig received.");

    // Wait for WiFi to connect to AP
    Serial.println("Waiting for WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      digitalWrite(LED_INDICATOR_PIN, LOW);
      delay(500);
      Serial.print(".");
      digitalWrite(LED_INDICATOR_PIN, HIGH);
      delay(500);
    }
    Serial.println("WiFi connected: ");

    String receivedSsid = WiFi.SSID();
    String receivedPass = WiFi.psk();

    // Store SSID
    Serial.println("Writing ssid to EEPROM...");
    for (int i = 0; i < receivedSsid.length(); i++) {
      EEPROM.write(i, receivedSsid[i]);
      //Serial.print(receivedSsid[i]);
    }
    Serial.println("Writing psk to EEPROM...");
    for (int i = 0; i < receivedPass.length(); i++) {
      EEPROM.write(i + 32, receivedPass[i]);
      //Serial.print(receivedPass[i]);
    }
    EEPROM.commit();
    // Reset the device (this is important for MQTT to work idk why)
    Serial.println("Restarting device after smart config");
    EEPROM.end();
    ESP.restart();
  }
  return;
}

void messageReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);

  // ~~~~~~~~~~~ Make the message as a char[]
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  // ~~~~~~~~~~~ End

  if (strcmp(topic, FEED_DURATION_TOPIC) == 0) {
    uint16_t duration = atoi(message);
    moveServoMotor(duration);
    // Publish something to inform client that action is successful
    client.publish(FEED_DURATION_RESPONSE_TOPIC, "true");
    client.publish(NOTIFICATION_TOPIC, "feed-success");
  } else if (strcmp(topic, TOGGLE_STREAM_TOPIC) == 0) {
    Serial.print(message);
    // On and Off
    if (String(message) == "on") {
      streamToggled = true;
      delay(1000);
    } else if (String(message) == "off") {
      streamToggled = false;
      delay(1000);
    }
  } else if (strcmp(topic, AUTH_TOPIC) == 0) {
    StaticJsonDocument<32> doc;
    DeserializationError error = deserializeJson(doc, message, length);
    // Check for parsing errors
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    // Access the parsed data
    const char* productId = doc["id"];
    const char* productPwd = doc["pwd"];
    if (strcmp(productId, PRODUCT_ID) == 0 && strcmp(productPwd, PRODUCT_PWD) == 0) {
      Serial.println("Product is valid!");
      client.publish(AUTH_RESPONSE_TOPIC, "valid");
    } else {
      Serial.println("Product is invalid!");
    }
  } else if (strcmp(topic, FEED_SCHEDULE_TOPIC) == 0) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, message, length);
    // Check for parsing errors
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    Serial.print(message);
    // Schedules, in this case is only limited to 10
    // Access the parsed data
    int i = 0;
    for (JsonObject item : doc.as<JsonArray>()) {
      feeding_schedules[i].h = item["h"];
      feeding_schedules[i].m = item["m"];
      feeding_schedules[i].d = item["d"];
      i++;
      if (i < 10) {
        break;
      }
    }
    saveScheduleToEEPROM();
  } else if (strcmp(topic, UVLIGHT_DURATION_TOPIC) == 0) {
    if (String(message) == "stop") {
      Serial.println("Cancelled sterilization... turning off uv light now");
      uvTimer.stop();
      digitalWrite(UVLIGHT_PIN, LOW);
      uvTimer.reset();
    } else {
      unsigned long duration = atoi(message);
      toggleUVLight(duration);
      // Publish something to inform client that action is successful
      client.publish(UVLIGHT_DURATION_RESPONSE_TOPIC, "true");
      client.publish(NOTIFICATION_TOPIC, "uv-success");
    }
  } 
  /*
  else if (strcmp(topic, RESET_ESP_TOPIC) == 0) {
    if (String(message) == "reset") {
      Serial.println("Erasing EEPROM...");
      delay(1000);
      for (int i = 0; i < 2048; i++) { // Erase EEPROM by writing 0 to each byte
        EEPROM.write(i, 0);
      }
      EEPROM.commit(); // Save changes to EEPROM
      delay(3000); // Wait for 3 seconds to avoid multiple erasures
      ESP.restart();
    }
  }*/
  return;
}


void scheduleLoop() {
  if (isThereStoredSchedules == true) {
    time_t now = time(nullptr); // Get current time
    struct tm* timeinfo = localtime(&now); // Convert to local time
    for (int currentScheduleIndex = 0; currentScheduleIndex < 10; currentScheduleIndex++) { // Loop over the stored schedule array to check which is the nearest time, break when empty...
      if (feeding_schedules[currentScheduleIndex].h == -1 && feeding_schedules[currentScheduleIndex].m == -1) {
        break;
      } else {
        // If we want the function to only execute once, then we actually check for the exact time! (include seconds) in comparison
        if (timeinfo->tm_hour == feeding_schedules[currentScheduleIndex].h && timeinfo->tm_min == feeding_schedules[currentScheduleIndex].m && timeinfo->tm_sec == 0) {
          //Serial.println("Sending duration for dispenser coming from ESP32");
          moveServoMotor(feeding_schedules[currentScheduleIndex].d);

          // Post data on database, first declare the necessary variables
          StaticJsonDocument<256> doc;
          String json;
          char timeStr[32];

          strftime(timeStr, sizeof(timeStr), "%a %d, %b, %I:%M %p", timeinfo);
          doc["type"] = "Feed Log";
          doc["didFail"] = false;
          doc["duration"] = feeding_schedules[currentScheduleIndex].d / 1000;
          doc["dateFinished"] = timeStr;

          // Serialize the JSON object
          serializeJson(doc, json);

          // Define the HTTP client
          HTTPClient http;
          http.begin(CRUD_API + String("/api/logs/client/") + PRODUCT_ID); // Replace with your server URL
          http.addHeader("Content-Type", "application/json");

          // Send the POST request with JSON data
          int httpResponseCode = http.POST(json);

          // Check for response
          if (httpResponseCode > 0) {
            //String response = http.getString();
            Serial.println("Successfully published to feeding log to database - Status Code: " + httpResponseCode);
            //Serial.println(response);
          } else {
            Serial.println("Error sending feeding log to database! - Status Code: " + httpResponseCode);
          }
          http.end();
        }
      }
    }
  }
}

void loop() {
  button.loop(); // MUST call the loop() function first

  if (button.isPressed()) {
    Serial.println("Erasing ESP32 EEPROM...");
    delay(1000);
    for (int i = 0; i < 2048; i++) { // Erase EEPROM by writing 0 to each byte
      digitalWrite(LED_INDICATOR_PIN, LOW);
      delay(50);
      EEPROM.write(i, 0);
      digitalWrite(LED_INDICATOR_PIN, HIGH);
      delay(50);
    }
    EEPROM.commit(); // Save changes to EEPROM
    delay(3000); // Wait for 3 seconds to avoid multiple erasures
    EEPROM.end();
    ESP.restart();
  }

  if (servoTimer.done()) {
    Serial.println("Done dispensing food... restoring back to original position");
    dispenser.write(0);
    servoTimer.reset();
  }

  if (uvTimer.done()) {
    Serial.println("Done sterilization... turning off uv light now");
    digitalWrite(UVLIGHT_PIN, LOW);
    uvTimer.reset();
  }

  scheduleLoop();

  // RESET DEVICE HANDLER
  if (streamToggled == true && client.connected() == true) {
    getImage();
  } 
  if (WiFi.status() != WL_CONNECTED) {
    // Connect to MQTT
    ESP.restart();
  }
  client.loop();
}
