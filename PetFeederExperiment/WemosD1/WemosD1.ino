#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <Servo.h>
#include "ArduinoJson.h"
#include "time.h"
#include "secrets.h"

#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"

#define DISPENSER_PIN D3
// Necessary global variables
bool audioFlag = false;
bool hasConnected = false;
bool isThereStoredSchedules = false;
Servo dispenser;

AudioGeneratorMP3 *mp3 = NULL;
AudioFileSourceHTTPStream *file = NULL;
AudioFileSourceBuffer *buff = NULL;
AudioOutputI2SNoDAC *out = NULL;
void *preallocateBuffer = NULL;

// Create secure wifi client
//WiFiClientSecure wemos;
WiFiClient wemos;
// Create mqtt client
PubSubClient client(wemos);
// Set up the necessary certificate
//X509List lets_encrypt_cert(root_ca);

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

void setup() {
  Serial.begin(9600);
  EEPROM.begin(2048);

  dispenser.attach(DISPENSER_PIN);
  dispenser.write(0);
  // Define audio variable
  out = new AudioOutputI2SNoDAC();
  Serial.println("Resetting servo motor location to 0 degrees");
  readScheduleFromEEPROM();
  // Tries to check for credentials in EEPROM, if not it will just inform user from serial
  connectToWifi();
  if (WiFi.status() == WL_CONNECTED) {
    // Configure NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    // Connect to MQTT
    ConnectToMQTT();
  }
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
    //WiFi.hostname("Wemos-D1");
    WiFi.mode(WIFI_STA);
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
    Serial.swap(); // Switch to GPIO 15 (TX) and GPIO 13 (RX) to get credentials
  }
}

/*
  unsigned long getEpochTimeInSeconds() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time since Epoch");
    return 0;
  }
  time(&now);
  return now;
  }
*/

void ConnectToMQTT() {
  // Make sure the time is correct when connecting to the broker
  //time_t timeSinceEpoch = getEpochTimeInSeconds();
  //wemos.setTrustAnchors(&lets_encrypt_cert);
  //wemos.setX509Time(timeSinceEpoch);
  // Set proper settings for mqtt client
  client.setBufferSize(128);
  client.setServer(MQTTserver, MQTTport);
  client.setCallback(messageReceived);
  while (!client.connected()) {
    Serial.println("Connecting to MQTT broker...");
    String clientName = PRODUCT_ID + String("-wemosd1");
    if (client.connect(clientName.c_str())) {
      //if (client.connect(clientName.c_str(), MQTTusername, MQTTpassword)) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed to connect to MQTT broker, restarting device...");
      delay(2000);
      ESP.restart();
    }
  }

  // Subscribe to the given topics
  //client.subscribe(UVLIGHT_DURATION_TOPIC, 1);
  client.subscribe(FEED_DURATION_TOPIC, 1);
  client.subscribe(FEED_SCHEDULE_TOPIC, 1);
  client.subscribe(RESET_WEMOS_TOPIC, 1);
  // TODO: Audio
  client.subscribe(AUDIO_TOPIC, 1); // This topic will send in the filename
  return;
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

void messageReceived(char* topic, byte * payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);

  // ~~~~~~~~~~~ Make the message as a char[]
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  // ~~~~~~~~~~~ End

  if (strcmp(topic, FEED_DURATION_TOPIC) == 0) {
    int duration = atoi(message);
    moveServoMotor(duration);
    // Publish something to inform client that action is successful
    client.publish(FEED_DURATION_RESPONSE_TOPIC, "true");
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
  } else if (strcmp(topic, AUDIO_TOPIC) == 0) {
    if (String(message) == "stop") {
      Serial.println("Stopping audio");
      audioFlag = false;
      mp3->stop();
      delete mp3;
      mp3 = NULL;
    } else {
      playAudio(message);
    }
  } else if (strcmp(topic, RESET_WEMOS_TOPIC) == 0) {
    if (String(message) == "reset") {
      clearEEPROM();
    }
  }

  return;
}

void clearEEPROM() {
  for (int i = 0; i < 2048; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("Cleared EEPROM contents, will reset now...");
  ESP.restart();
}

void playAudio(const char* URL) {
  file = new AudioFileSourceHTTPStream();
  if (file->open(URL)) {
    buff = new AudioFileSourceBuffer(file, preallocateBuffer, 8192);
    mp3 = new AudioGeneratorMP3();
    mp3->begin(buff, out);
    audioFlag = true;
  } else {
    Serial.println("cannot open link");
  }
}

void moveServoMotor(uint16_t duration) {
  Serial.print("Moved servo motor by 90 degrees for ");
  Serial.print(duration);
  Serial.print(" milliseconds");
  Serial.println();
  dispenser.write(180);
  delay(duration);
  dispenser.write(0);
}
/*
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
*/
/*
void audioLoop() {
  // Audio
  if (audioFlag == true) {
    static int lastms = 0;
    if (mp3->isRunning()) {
      if (millis() - lastms > 1000) {
        lastms = millis();
        Serial.printf("Running for %d ms...\n", lastms);
        Serial.flush();
      }
      if (!mp3->loop()) {
        audioFlag = false;
        mp3->stop();
        delete mp3;
        mp3 = NULL;
      }
    } else {
      Serial.println("Stopping radio");
      audioFlag = false;
      mp3->stop();
      delete mp3;
      mp3 = NULL;
    }
  }
  return;
}
*/
void startingLoop() {
  while (Serial.available() > 0 && hasConnected == false) {
    // Retrieve credentials from serial... store in EEPROM, then restart device
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
          http.begin(wemos, CRUD_API + String("/api/logs/client/") + PRODUCT_ID); // Replace with your server URL
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
  return;
}


void loop() {
  static int lastms = 0;
  if (audioFlag == true) {
    if (mp3->isRunning()) {
      if (millis() - lastms > 1000) {
        lastms = millis();
        Serial.printf("Running for %d ms...\n", lastms);
        Serial.flush();
      }
      if (!mp3->loop()) {
        mp3->stop();
        audioFlag = false;
        delete mp3;
        mp3 = NULL;
      }
    } else {
      Serial.println("Stopping audio");
      audioFlag = false;
      mp3->stop();
      delete mp3;
      mp3 = NULL;
    }
  }
  startingLoop();
  scheduleLoop();
  client.loop();
}
