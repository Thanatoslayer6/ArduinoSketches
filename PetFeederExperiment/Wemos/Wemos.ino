// The arduino recieves from the esp32-cam
// We include the servo library
#include <Servo.h>
#include <ESP8266WiFi.h>

Servo dispenser;
#define UVLIGHT_PIN D4
#define DISPENSER_PIN D3

void setup() {
  Serial.begin(9600);
  // We use the PWM pin 3 for the servo motor
  dispenser.attach(DISPENSER_PIN);
  // Use pin 4 for the uv light (relay)
  pinMode(UVLIGHT_PIN, OUTPUT);
          
  dispenser.write(0); // Reset servo first
  Serial.print("Resetted servo to 0 degrees");

  // WIFI SETUP (wait for serial communication)
  WifiConfiguration();    

}

void WifiConfiguration() {
  char ssid[32], password[64];
  int index = 0;
  bool hasSSID = false, hasPassword = false;
  while (hasSSID == false && hasPassword == false) {
    while (hasSSID == false) {
      while (Serial.available()) { // Get ssid
        char c = Serial.read();
        if (c == '\n' || index >= 31) { // From 0 - 31 (32 chars)
          ssid[index] = '\0';
          hasSSID = true;
          break;
        }
        ssid[index++] = c;
      }
    }
    index = 0;
    while (hasPassword == false) {
      while (Serial.available()) { // Get password
        char c = Serial.read();
        if (c == '\n' || index >= 63) {
          password[index] = '\0';
          hasPassword = true;
          break;
        }
        password[index++] = c;
      }
    }
  }

  // Start connecting to wifi as soon as possible...
  Serial.println("Acquired Wifi ssid: ");
  Serial.print(ssid);
  Serial.println("Acquired Wifi password: ");
  Serial.print(password);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  isConnectedToTheInternet = true;
}


void loop() {
  // TODO: MQTT Handling (Microphone/Audio, UV-Light)
  // ESP32 -> Camera, and Servo motor scheduling....
  // WEMOS D1 -> Audio and UV-Light
  /*
    while (Serial.available()) {
        char commandIdentifier = Serial.read(); // Get the first character
        int duration = Serial.parseInt();
        Serial.print("Received integer message: ");
        Serial.println(duration);
        if (commandIdentifier == 's') { // Means we move the servo motor
          Serial.println("Moving the servo motor");
          moveServoMotor(duration);
        } else if (commandIdentifier == 'u') { // Means we toggle the uv light
          Serial.println("Toggling the UV Light");
          toggleUVLight(duration);
        } 
    }  
  */
}

// Pass number of seconds in milliseconds
void moveServoMotor(uint16_t duration) {
    Serial.print("Moved servo motor by 90 degrees for ");
    Serial.print(duration);
    Serial.print(" milliseconds");
    Serial.println();
    dispenser.write(90); // Turn 90 degrees
    delay(duration);
    dispenser.write(0);
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
