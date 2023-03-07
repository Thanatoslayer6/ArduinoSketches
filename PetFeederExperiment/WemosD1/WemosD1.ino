#include <ESP8266WiFi.h>
#include <EEPROM.h>
#define UVLIGHT_PIN D4

void setup() {
  Serial.begin(9600);
  EEPROM.begin(96);
  // Use pin 4 for the uv light (relay)
  pinMode(UVLIGHT_PIN, OUTPUT);

  ConnectToWifi();
}

void loop() {

}

void ConnectToWifi() {
  String ssid = "";
  String pwd = "";
  for (int i = 0; i < 32; i++) { // Read ssid
    //ssid += char(EEPROM.read(i));
    char c = EEPROM.read(i);
    if (c == 0) {
      break; // End of string
    }
    ssid += c;
  }
  Serial.println("SSID: " + ssid);
  //Serial.print(ssid);
  for (int i = 32; i < 96; i++) {
    //pwd += char(EEPROM.read(i));
    char c = EEPROM.read(i);
    if (c == 0) {
      break; // End of string
    }
    pwd += c;
  }
  Serial.println("PSK: " + pwd);
  //Serial.print(pwd);
  
  // MANUAL WIFI SETUP (If there is some password)
  if (!ssid.isEmpty() && !pwd.isEmpty()) { 
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pwd.c_str());
    Serial.print("Connecting to WiFi using stored credentials");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("WiFi connected successfully!");
    
  } else { // SMART WIFI CONFIG
    // No SSID and password stored in EEPROM, start SmartConfig
    Serial.println("Stored SSID and PSK not found. Using SmartConfig");
    WiFi.mode(WIFI_AP_STA);
    WiFi.beginSmartConfig(); // Uses esptouch v1

    while (!WiFi.smartConfigDone()) {
      delay(500);
      Serial.print(".");
    }

    // Wait for WiFi to connect to AP
    Serial.println("SmartConfig received, now connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("WiFi connected successfully!");
 
    String receivedSsid = WiFi.SSID();
    String receivedPass = WiFi.psk();

    // Store SSID
    Serial.println("Writing ssid to EEPROM... -> " + receivedSsid);
    for (int i = 0; i < receivedSsid.length(); i++) {
       EEPROM.write(i, receivedSsid[i]);
    }
    Serial.println("Writing psk to EEPROM... -> " + receivedPass);
    for (int i = 0; i < receivedPass.length(); i++) {
       EEPROM.write(i + 32, receivedPass[i]);
    }
    EEPROM.commit();
    // Reset the device (this is important for MQTT to work idk why)
    Serial.println("Restarting device after smart config");
    ESP.restart();
  }
  return;
}


// Pass number of seconds in milliseconds
void toggleUVLight(uint16_t duration) {
  Serial.print("UV light is on for ");
  Serial.print(duration);
  Serial.print(" milliseconds");
  Serial.println();
  digitalWrite(UVLIGHT_PIN, HIGH);
  delay(duration);
  digitalWrite(UVLIGHT_PIN, LOW);
}
