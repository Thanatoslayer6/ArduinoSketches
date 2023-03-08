#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "secrets.h"
#define UVLIGHT_PIN D4
// Necessary global variables
bool hasConnected = false;

// Create secure wifi client
WiFiClientSecure wemos;
// Create mqtt client
PubSubClient client(wemos);


void setup() {
  Serial.begin(9600);
  EEPROM.begin(96);
  // Use pin 4 for the uv light (relay)
  pinMode(UVLIGHT_PIN, OUTPUT);
  // Tries to check for credentials in EEPROM, if not it will just inform user from serial
  connectToWifi();
  if (WiFi.status() == WL_CONNECTED) {
    ConnectToMQTT();
  }
}

void loop() {
  while (Serial.available() > 0) {
    // Retrieve credentials from serial... store in EEPROM, then restart device
    if (hasConnected == false) {
      String credentials = Serial.readStringUntil('\0');
      int delimiterIndex = credentials.indexOf(';');
      String ssid = credentials.substring(0, delimiterIndex);
      String pwd = credentials.substring(delimiterIndex + 1);
      // Store SSID and PSK, then restart device
      Serial.println("Writing ssid to EEPROM... -> ");
      for (int i = 0; i < ssid.length(); i++) {
        EEPROM.write(i, ssid[i]);
        Serial.print(ssid[i]);
      }
      Serial.println("Writing psk to EEPROM... -> ");
      for (int i = 0; i < pwd.length(); i++) {
        EEPROM.write(i + 32, pwd[i]);
        Serial.print(pwd[i]);
      }
      EEPROM.commit();
      // Reset the device (this is important for MQTT to work idk why)
      Serial.println("Restarting device after smart config");
      ESP.restart();
    }
  }
  client.loop();
}

void connectToWifi() {
  // Read from EEPROM
  String ssid;
  String pwd = "";
  for (int i = 0; i < 32; i++) { // Read ssid
    //ssid += char(EEPROM.read(i));
    char c = EEPROM.read(i);
    if (c == 0) {
      break; // End of string
    }
    ssid += c;
  } -
  Serial.println("Reading SSID -> " + ssid);
  for (int i = 32; i < 96; i++) {
    //pwd += char(EEPROM.read(i));
    char c = EEPROM.read(i);
    if (c == 0) {
      break; // End of string
    }
    pwd += c;
  }
  Serial.println("Reading PSK -> " + pwd);
  if (!ssid.isEmpty() && !pwd.isEmpty()) {
    WiFi.begin(ssid.c_str(), pwd.c_str());
    Serial.println("Connecting to access point");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("WiFi successfully connected");
    hasConnected = true;
  } else {
    Serial.println("No stored wifi credentials found...");
  }
}

void ConnectToMQTT() {
  // First set root certificate
  wemos.setCACert(root_ca);
  client.setBufferSize(32768); // around 32kb
  client.setServer(MQTTserver, MQTTport);
  client.setCallback(messageReceived);
  while (!client.connected()) {
    Serial.println("Connecting to MQTT broker...");
    String clientName = PRODUCT_ID + String("-wemosd1");
    if (client.connect(clientName, MQTTusername, MQTTpassword)) {
      Serial.println("Connected to MQTT broker"); 
    } else {
      Serial.print("Failed to connect to MQTT broker, restarting device");
      delay(2000);
      ESP.restart();
    } 
  }
  
  // Subscribe to the given topics
  client.subscribe(UVLIGHT_DURATION_TOPIC, 1);
  // TODO: Audio
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

  if (strcmp(topic, UVLIGHT_DURATION_TOPIC) == 0) {
    int duration = atoi(message);
    toggleUVLight(duration);
    // Publish something to inform client that action is successful
    client.publish(UVLIGHT_DURATION_RESPONSE_TOPIC, "true");
  } else if (strcmp(topic, RESET_WEMOS_TOPIC) == 0) {
    if (String(message) == "reset") {
      clearEEPROM();
    }
  }
  return;
}

void clearEEPROM() {
  for (int i = 0; i < 96; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("Cleared EEPROM contents, will reset now...");
  ESP.restart();
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
