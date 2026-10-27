#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int soilPin = A1;  // Same input as slide 55 and the integrated Nano sketch.

void setup() {
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("Soil Moisture");
}

void loop() {

  int value = analogRead(soilPin);

  lcd.setCursor(0,1);
  lcd.print("Value:");
  lcd.print(value);
  lcd.print("    ");

  delay(500);
}
