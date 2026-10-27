const int pumpPin = 7;   // HUMI output

void setup() {
  digitalWrite(pumpPin, LOW);  // Preload OFF before enabling the output driver.
  pinMode(pumpPin, OUTPUT);
}

void loop() {
  digitalWrite(pumpPin, HIGH);  // Pump ON
  delay(3000);  // Slide 59: supervised 3 s ON, 3 s OFF; keep intake in water.

  digitalWrite(pumpPin, LOW);   // Pump OFF
  delay(3000);
}
