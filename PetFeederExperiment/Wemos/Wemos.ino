// The arduino recieves from the esp32-cam
// We include the servo library
#include <Servo.h>
#include <ESP8266WiFi.h>

Servo dispenser;
#define UVLIGHT_PIN D4
#define DISPENSER_PIN D3
bool hasSSID = false;
bool hasPassword = false;
bool isConnectedToInternet = false;
String ssid, password;
//char ssid[32], password[64];
//int msgIndex = 0;

void setup() {
  Serial.begin(9600);
  
  // We use the PWM pin 3 for the servo motor
  dispenser.attach(DISPENSER_PIN);
  // Use pin 4 for the uv light (relay)
  pinMode(UVLIGHT_PIN, OUTPUT);
          
  dispenser.write(0); // Reset servo first
  Serial.print("Resetted servo to 0 degrees");

  // WIFI SETUP (wait for serial communication)
  //WifiConfiguration();    

}

void WifiConfiguration() {

  /*
  while (hasSSID == false) {
    while (Serial.available()) { // Get ssid
      ssid = Serial.readStringUntil('\n');
      hasSSID = true;
      
      char c = Serial.read();
      if (c == '\n' || index >= 31) { // From 0 - 31 (32 chars)
        ssid[index] = '\0';
        hasSSID = true;
        break;
      }
      ssid[index] = c;
      index++;
      
    }
  }
  Serial.println(ssid);
  //index = 0;
  while (hasPassword == false) {
    while (Serial.available()) { // Get password
      password = Serial.readStringUntil('\n');
      hasPassword = true;
      //char c = Serial.read();
      
      if (c == '\n' || index >= 63) {
        password[index] = '\0';
        hasPassword = true;
        break;
      }
      password[index] = c;
      index++;
      
    }
  }
  */
  // Start connecting to wifi as soon as possible...
  Serial.println("Acquired Wifi ssid: ");
  Serial.print(ssid);
  Serial.println("Acquired Wifi password: ");
  Serial.print(password);
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  isConnectedToInternet = true;
}


void loop() {
  // TODO: MQTT Handling (Microphone/Audio, UV-Light)
  // ESP32 -> Camera, and Servo motor scheduling....
  // WEMOS D1 -> Audio and UV-Light
  while(Serial.available()) {
    // wifi credentials format are "SSID;PSK"
    if (isConnectedToInternet == false) {
      String credentials = Serial.readString();
      int delimiterIndex = credentials.indexOf(';');
      ssid = credentials.substring(0, delimiterIndex);
      password = credentials.substring(delimiterIndex + 1);
      WifiConfiguration();
      /*
      if (hasSSID == false) {
        ssid = Serial.readString();
      }
      if (hasSSID == true && hasPassword == false) {
        password = Serial.readString();
        hasPassword = true; 
      }
      if (hasSSID == true && hasPassword == true) {
        WifiConfiguration();  
      }
      hasSSID = true;
      */
    }
  }
  
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
