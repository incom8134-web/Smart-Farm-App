#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Change A0 only if the board LIGHT_S signal uses another analog pin
const int lightSensorPin = A0;

void setup() {
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Light Sensor");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(1500);

  lcd.clear();
}

void loop() {
  int lightValue = analogRead(lightSensorPin);

  lcd.setCursor(0, 0);
  lcd.print("Light Value:");

  lcd.setCursor(0, 1);
  lcd.print(lightValue);
  lcd.print("        ");

  delay(300);
}