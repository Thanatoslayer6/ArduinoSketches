int waitTime = 5000;

void setup() {
  // put your setup code here, to run once:
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  //pinMode(4, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  for (int i = 2; i <= 4; i++) {
    digitalWrite(i, HIGH);
    delay(waitTime);
    digitalWrite(i, LOW);
    delay(waitTime);
  }
}
