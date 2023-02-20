// The arduino recieves from the esp32-cam
// We include the servo library
#include <Servo.h>

Servo dispenser;

void setup() {
  Serial.begin(9600);
  // We use the PWM pin 3 for the servo motor
  dispenser.attach(3);
  dispenser.write(0); // Reset servo first
  Serial.print("Resetted servo to 0 degrees");

}

void loop() {
    while (Serial.available()) {
        int duration = Serial.parseInt();
        Serial.print("Received message: ");
        Serial.println(duration);
        moveServoMotor(duration); 
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
