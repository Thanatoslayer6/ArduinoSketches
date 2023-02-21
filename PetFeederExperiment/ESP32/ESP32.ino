// The esp32-cam has to send in data to the arduino
// The esp32-cam needs to be powered via usb
// Include wifi and time library to get the time via NTP

#include <WiFi.h>
#include "time.h"
#include <HardwareSerial.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "secrets.h"

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

//const int bufferSize = 1024 * 23; // 23552 bytes for the maximum size sending an image

// Set up hardware serial to send data to the arduino
HardwareSerial sendToArduino(1);

// Create secure wifi client
WiFiClientSecure esp32cam;
// Create mqtt client 
PubSubClient client(esp32cam);
// Necessary variables
bool streamToggled = false;

void setup(){
  Serial.begin(115200);
  sendToArduino.begin(9600, SERIAL_8N1, 2, 3);
  // Initialize Camera first
  CameraInit();
  //delay(2000); // Add some delay just in case...
  // Init Wifi
  ConnectToWifi();
  // Init NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  // Connect to MQTT
  ConnectToMQTT();
  /*
  Serial.println("About to take a picture in 5 seconds quick!");
  // Delay and get a quick snapshot
  delay(5000);
  getImage();
  Serial.println("Done taking a picture! enjoy :D");
  */
}

void ConnectToMQTT() {
  // First set root certificate
  esp32cam.setCACert(root_ca);
  client.setBufferSize(32768); // around 32kb
  client.setServer(MQTTserver, MQTTport);
  client.setCallback(messageReceived);
  while (!client.connected()) {
    Serial.println("Connecting to MQTT broker...");
    // Create a random client ID
    String clientId = "ESP32CAMClient-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), MQTTusername, MQTTpassword)) {
      Serial.println("Connected to MQTT broker"); 
    } else {
      Serial.print("Failed to connect to MQTT broker");
      Serial.println(client.state());
      delay(5000); 
    } 
  }
  // Subscribe to the given topics
  client.subscribe("feed_duration_response", 1);
  client.subscribe("toggle_stream", 1);
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
  config.frame_size = FRAMESIZE_VGA; // 640x480
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
    bool result = client.publish("stream", fb->buf, fb->len);
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
  // Connect to Wi-Fi
  Serial.print("Connecting to: ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  
  }
  Serial.println("WiFi connected: ");
  Serial.print(WiFi.localIP());

}

void messageReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);
  /*
  if (strcmp(topic, "feed_duration") == 0) {
    char message[length + 1]; // allocate space for null terminator
    for (int i = 0; i < length; i++) {
      message[i] = (char)payload[i];
    
    }
    message[length] = '\0'; // add null terminator
    int duration = atoi(message);
    sendToArduino.print(duration);
    // Publish something to inform client that action is successful
    client.publish("feed_duration_response", "true");
  
  } else if (strcmp(topic, "toggle_stream") == 0) {
      Serial.println((int)payload);
  }
  */
  // Make the message as a char *
  char message[length + 1]; // allocate space for null terminator
  for (int i = 0; i < length; i++) {
     message[i] = (char)payload[i];
  }
  message[length] = '\0'; // add null terminator
  
  if (String(topic) == "feed_duration") {
    int duration = atoi(message);
    sendToArduino.print(duration);
    // Publish something to inform client that action is successful
    client.publish("feed_duration_response", "true");
  } else if (String(topic) == "toggle_stream") {
      Serial.print(message);
      // On and Off
      if (String(message) == "on") {
        streamToggled = true;
      } else if (String(message) == "off") {
        streamToggled = false;
      }
  }
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
  client.loop();
  if (streamToggled == true && client.connected() == true) {
    getImage();
  }
  /*
  if (client.connected() == true) {
    getImage();
  }
  */
}
