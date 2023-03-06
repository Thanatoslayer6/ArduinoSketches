// The arduino recieves from the esp32-cam
// We include the servo library
#include <Servo.h>

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

}

void loop() {
  
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
