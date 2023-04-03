#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <ezButton.h>
#include "secrets.h"
#include "boot_sound.h"

#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioFileSourcePROGMEM.h"
#include "AudioOutputI2SNoDAC.h"

// Necessary global variables
bool hasConnected = false;
bool isAudioPlaying = false;
#define RESETBTN_PIN D3
ezButton button(RESETBTN_PIN);

AudioGeneratorMP3 *mp3 = NULL;
AudioGeneratorWAV *wav = NULL;
AudioFileSourcePROGMEM *file_progmem = NULL;
AudioFileSourceHTTPStream *file = NULL;
AudioFileSourceBuffer *buff = NULL;
AudioOutputI2SNoDAC *out = NULL;
void *preallocateBuffer = NULL;

// Create wifi client
WiFiClient wemos;

// Create mqtt client
PubSubClient client(wemos);

void setup() {
  Serial.begin(9600);
  button.setDebounceTime(50);
  //Serial.setDebugOutput(true);
  EEPROM.begin(96);

  // Tries to check for credentials in EEPROM, if not it will just inform user from serial
  connectToWifi();
  if (WiFi.status() == WL_CONNECTED) {
    // Connect to MQTT
    ConnectToMQTT();
    // Play sound reminder
    playBootSound();
  }
}

void playBootSound() {
  file_progmem = new AudioFileSourcePROGMEM(boot_sound, sizeof(boot_sound));
  out = new AudioOutputI2SNoDAC();
  wav = new AudioGeneratorWAV();
  wav->begin(file_progmem, out);
}

void connectToWifi() {
  // Read from EEPROM
  String ssid;
  String pwd = "";
  for (int i = 0; i < 32; i++) { // Read ssid
    char c = EEPROM.read(i);
    if (c == 0) {
      break; // End of string
    } else {
      ssid += c;
    }
  } -
  Serial.println("Reading SSID -> " + ssid);
  for (int i = 32; i < 96; i++) {
    char c = EEPROM.read(i);
    if (c == 0) {
      break; // End of string
    } else {
      pwd += c;
    }
  }
  Serial.println("Reading PSK -> " + pwd);
  if (!ssid.isEmpty() && !pwd.isEmpty()) {

    IPAddress ip(192, 168, 8, 32); // where xx is the desired IP Address
    IPAddress gateway(192, 168, 8, 1); // set gateway to match your network
    Serial.print(F("Setting static ip to : "));
    Serial.println(ip);
    IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your
    IPAddress primaryDns(192, 168, 8, 1); 
    WiFi.config(ip, gateway, subnet, primaryDns);
    
    WiFi.hostname("Wemos-D1");
    WiFi.persistent(false); //These 3 lines are a required work around
    /*
    
    WiFi.mode(WIFI_OFF);    //otherwise the module will not reconnect
    WiFi.forceSleepWake();
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    */
    WiFi.setPhyMode(WIFI_PHY_MODE_11B);
    WiFi.mode(WIFI_STA);
    Serial.println("Connecting to access point");

    WiFi.begin(ssid.c_str(), pwd.c_str());
    while (WiFi.status() != WL_CONNECTED) {
      button.loop(); // MUST call the loop() function first

      if (button.isPressed()) {
        Serial.println("Erasing WEMOS-D1 EEPROM...");
        delay(1000);
        for (int i = 0; i < 2048; i++) { // Erase EEPROM by writing 0 to each byte
          delay(50);
          EEPROM.write(i, 0);
        }
        EEPROM.commit(); // Save changes to EEPROM
        delay(3000); // Wait for 3 seconds to avoid multiple erasures
        EEPROM.end();
        ESP.restart();
      }
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

void ConnectToMQTT() {
  // Make sure the time is correct when connecting to the broker
  // Set proper settings for mqtt client
  client.setBufferSize(128);
  client.setServer(MQTTserver, MQTTport);
  client.setCallback(messageReceived);
  while (!client.connected()) {
    Serial.println("Connecting to MQTT broker...");
    String clientName = PRODUCT_ID + String("-wemosd1");
    if (client.connect(clientName.c_str())) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed to connect to MQTT broker, restarting device...");
      delay(2000);
      ESP.restart();
    }
  }

  // Subscribe to the given topics
  //client.subscribe(RESET_WEMOS_TOPIC, 1);
  client.subscribe(AUDIO_TOPIC, 1); // This topic will send in the url to play
  return;
}

void messageReceived(char* topic, byte * payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);

  // ~~~~~~~~~~~ Make the message as a char[]
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  // ~~~~~~~~~~~ End

  if (strcmp(topic, AUDIO_TOPIC) == 0) {
    if (String(message) == "stop") {
      Serial.println("Stopping audio manually");
      stopPlaying();
    } else {
      Serial.print("About to play: ");
      Serial.println(message);
      playAudio(message);
    }
  }
  /*
    else if (strcmp(topic, RESET_WEMOS_TOPIC) == 0) {
    if (String(message) == "reset") {
      clearEEPROM();
    }
    }
  */
  return;
}

void clearEEPROM() {
  for (int i = 0; i < 96; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("Cleared EEPROM contents, will reset now...");
  EEPROM.end();
  delay(3000);
  ESP.restart();
}

void playAudio(const char* URL) {
  isAudioPlaying = true;
  file = new AudioFileSourceHTTPStream();
  if (file->open(URL)) {
    buff = new AudioFileSourceBuffer(file, preallocateBuffer, 8196);
    mp3 = new AudioGeneratorMP3();
    mp3->begin(buff, out);
  } else {
    Serial.println("cannot open link");
    ESP.restart();
  }
}

void startingLoop() {
  while (hasConnected == false && Serial.available() > 0) {
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

void stopPlaying() {
  isAudioPlaying = false;
  // File formats
  if (mp3) {
    Serial.println("Stopping .mp3 audio");
    mp3->stop();
    delete mp3;
    mp3 = NULL;
  }
  if (wav) {
    Serial.println("Stopping .wav audio");
    wav->stop();
    delete wav;
    wav = NULL;
  }
  // Audio variables
  if (buff) {
    buff->close();
    delete buff;
    buff = NULL;
  }
  if (file) {
    file->close();
    delete file;
    file = NULL;
  }
  if (file_progmem) {
    file_progmem->close();
    delete file_progmem;
    file_progmem = NULL;
  }

}

void resetButtonHandler() {
  if (isAudioPlaying == false) {
    button.loop(); // MUST call the loop() function first
    if (button.isPressed()) {
      Serial.println("Erasing Wemos D1 EEPROM...");
      delay(1000);
      for (int i = 0; i < 96; i++) { // Erase EEPROM by writing 0 to each byte
        EEPROM.write(i, 0);
      }
      EEPROM.commit(); // Save changes to EEPROM
      delay(3000); // Wait for 3 seconds to avoid multiple erasures
      EEPROM.end();
      ESP.restart();
    }
  }
}

void loop() {
  startingLoop();
  resetButtonHandler();
  client.loop();
  if (mp3 && !mp3->loop()) stopPlaying();
  if (wav && !wav->loop()) stopPlaying();
}
