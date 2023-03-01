// The esp32-cam has to send in data to the arduino
// The esp32-cam needs to be powered via usb
// Include wifi and time library to get the time via NTP
#include <EEPROM.h>
#include <WiFi.h>
#include "time.h"
#include <HardwareSerial.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
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
#define LED_GPIO_NUM      33

// Set up hardware serial to send data to the arduino
HardwareSerial sendToArduino(1);

// Create secure wifi client
WiFiClientSecure esp32cam;
// Create mqtt client 
PubSubClient client(esp32cam);

// Necessary variables
bool streamToggled = false;
// Declare necessary variables

void setup(){ // TODO: EEPROM for wifi and ssid, also connection to database...
  Serial.begin(115200);
  pinMode(RESETBTN_PIN, INPUT); // Use GPIO2 of ESP32-Cam which will act as reset button
  EEPROM.begin(96);

  //sendToArduino.begin(9600, SERIAL_8N1, 2, 3);
  // Initialize Camera first
  //CameraInit();
  //delay(2000); // Add some delay just in case...
  // Init Wifi
  ConnectToWifi();
  //ValidateProduct(); // This will only be called once (for setting up via application)
  // Init NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if (WiFi.status() == WL_CONNECTED) {
    // Connect to MQTT
    ConnectToMQTT();
  }
}

void ConnectToMQTT() {
  // First set root certificate
  esp32cam.setCACert(root_ca);
  client.setBufferSize(32768); // around 32kb
  client.setServer(MQTTserver, MQTTport);
  client.setCallback(messageReceived);
  while (!client.connected()) {
    Serial.println("Connecting to MQTT broker...");
    if (client.connect(PRODUCT_ID, MQTTusername, MQTTpassword)) {
      Serial.println("Connected to MQTT broker"); 
    } else {
      Serial.print("Failed to connect to MQTT broker, restarting device");
      delay(2000);
      ESP.restart();
    } 
  }
  // Subscribe to the given topics
  client.subscribe(FEED_DURATION_TOPIC, 1);
  client.subscribe(TOGGLE_STREAM_TOPIC, 1);
  client.subscribe(TOGGLE_UVLIGHT_TOPIC, 1);
  client.subscribe(AUTH_TOPIC, 1);
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
    ESP.restart(); // Restart the device
    return;
  }
  Serial.println("Initialized camera successfully");
}

void getImage() {
  camera_fb_t *fb = esp_camera_fb_get();
  // Check if there's an image, it is a jpeg, and is less than 32kb
  if (fb != NULL && fb->format == PIXFORMAT_JPEG && fb->len < 32768) {
    Serial.print("Image Length: ");
    Serial.print(fb->len);
    Serial.println();
    bool result = client.publish(STREAM_TOPIC, fb->buf, fb->len);
    Serial.println(result);
    
    if (!result) {
      ESP.restart();
    }
  }
  // Release the frame buffer
  esp_camera_fb_return(fb);
  delay(50); // Miniscule duration for the esp32 to gain strength
}



void ConnectToWifi() {
  String ssid;
  String pwd = "";
  Serial.println("Reading SSID");
  for (int i = 0; i < 32; i++) { // Read ssid
    //ssid += char(EEPROM.read(i));
    char c = EEPROM.read(i);
    if (c == 0) {
      break; // End of string
    }
    ssid += c;
  }
  Serial.println("SSID: ");
  Serial.print(ssid);
  Serial.println("Reading PWD");
  for (int i = 32; i < 96; i++) {
    //pwd += char(EEPROM.read(i));
    char c = EEPROM.read(i);
    if (c == 0) {
      break; // End of string
    }
    pwd += c;
  }
  Serial.print("PWD: ");
  Serial.println(pwd);
  
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
      delay(500);
      Serial.print(".");
    }
    Serial.println("WiFi successfully connected");
    
  } else { // SMART WIFI CONFIG
    // No SSID and password stored in EEPROM, start SmartConfig
    Serial.println("SSID and Password not found. Starting SmartConfig...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.beginSmartConfig(SC_TYPE_ESPTOUCH_V2); 

    // Wait for SmartConfig packet from mobile
    Serial.println("Waiting for SmartConfig.");
    while (!WiFi.smartConfigDone()) {
      delay(500);
      Serial.print(".");
    }

    Serial.println("");
    Serial.println("SmartConfig received.");

    // Wait for WiFi to connect to AP
    Serial.println("Waiting for WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("WiFi connected: ");
 
    String receivedSsid = WiFi.SSID();
    String receivedPass = WiFi.psk();

    // Store SSID
    Serial.println("Writing ssid to EEPROM...");
    for (int i = 0; i < receivedSsid.length(); i++) {
       EEPROM.write(i, receivedSsid[i]);
       Serial.println(receivedSsid[i]);
    }
    Serial.println("Writing psk to EEPROM...");
    for (int i = 0; i < receivedPass.length(); i++) {
       EEPROM.write(i + 32, receivedPass[i]);
       Serial.println(receivedPass[i]);
    }
    EEPROM.commit();
    // Reset the device (this is important for MQTT to work idk why)
    Serial.println("Restarting device after smart config");
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
      int duration = atoi(message);
      sendToArduino.print(duration);
      // Publish something to inform client that action is successful
      client.publish(FEED_DURATION_RESPONSE_TOPIC, "true");
  } else if (strcmp(topic, TOGGLE_STREAM_TOPIC) == 0) {
      Serial.print(message);
      // On and Off
      if (String(message) == "on") {
        streamToggled = true;
      } else if (String(message) == "off") {
        streamToggled = false;
      }
  } else if (strcmp(topic, AUTH_TOPIC) == 0) {
    StaticJsonDocument<32> doc;
    DeserializationError error = deserializeJson(doc, message, length);
    //Serial.println(message);
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
  } 
  return;
}


void loop(){
  /*
  time_t now = time(nullptr); // Get current time
  struct tm* timeinfo = localtime(&now); // Convert to local time

  // If we want to like only execute once, then we actually check for the exact time! (include seconds) in comparison
  
  if (timeinfo->tm_hour == targetHour && timeinfo->tm_min == targetMinute && timeinfo->tm_sec == 0) { // Check if minute is divisible by 5
    Serial.println("Sending duration for dispenser coming from ESP32");
    sendToArduino.print(6);
    delay(1200); // Just wait for 1.2 seconds to be safe
   }
  
   if ((timeinfo->tm_min % 2 == 0) && timeinfo->tm_sec == 0) {
    Serial.println("Sending duration for dispenser coming from ESP32");
    sendToArduino.print(8000);
    //delay(1200); // Just wait for 1.2 seconds to be safe
  
   }
  */
  // RESET DEVICE HANDLER
   if (digitalRead(RESETBTN_PIN) == HIGH) { // If the button is pressed
    Serial.println("Erasing EEPROM...");
    for (int i = 0; i < 96; i++) { // Erase EEPROM by writing 0 to each byte
      EEPROM.write(i, 0);
    }
    EEPROM.commit(); // Save changes to EEPROM
    Serial.println("EEPROM erased... Restarting device in 5s");
    delay(5000); // Wait for 1 second to avoid multiple erasures
    ESP.restart();
  }
  client.loop();
  if (streamToggled == true && client.connected() == true) {
    getImage();
  }
}
