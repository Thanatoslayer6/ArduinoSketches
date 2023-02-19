// The esp32-cam has to send in data to the arduino
// The esp32-cam needs to be powered via usb
// Include wifi and time library to get the time via NTP

#include <WiFi.h>
#include "time.h"
#include <HardwareSerial.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "secrets.h"

// Set up hardware serial to send data to the arduino
HardwareSerial sendToArduino(1);

// Create secure wifi client
WiFiClientSecure esp32cam;
// Create mqtt client 
PubSubClient client(esp32cam);

void setup(){
  Serial.begin(115200);
  sendToArduino.begin(9600, SERIAL_8N1, 2, 3);
  ConnectToWifi();
  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  ConnectToMQTT();
}

void ConnectToMQTT() {
  // First set root certificate
  esp32cam.setCACert(root_ca);
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
  client.subscribe("feed_duration");
}

void ConnectToWifi() {
  // Connect to Wi-Fi
  Serial.println("Connecting to: ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  
  }
  Serial.println("\nWiFi connected.");
  //Serial.println(WiFi.localIP());

}

void messageReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);
  if (strcmp(topic, "feed_duration") == 0) {
    char message[length + 1]; // allocate space for null terminator
    for (int i = 0; i < length; i++) {
      message[i] = (char)payload[i];
    
    }
    message[length] = '\0'; // add null terminator
    int duration = atoi(message);
    sendToArduino.print(duration);
  
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
}
