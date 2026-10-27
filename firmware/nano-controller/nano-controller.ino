/*
  JINIE EduTech AI Smart Farm - final Arduino Nano classroom controller

  Lesson: read four sensors, show values, accept six HC-06 Bluetooth commands.
  Hardware Serial on D0/D1 is NEVER started: the custom PCB shares the
  programming UART. HC-06 uses SoftwareSerial on D8/D9 instead.
  Install ESP32-CAM first, then this Nano program. See firmware/README.md.

  Phone -> Nano: LED_ON, LED_OFF, FAN_ON, FAN_OFF, PUMP_ON, PUMP_OFF
  Nano -> phone: temperatureC,humidityPercent,lightRaw,waterRaw;
  A0/A1 are raw ADC counts (0..1023), NOT lux or moisture percentages.
  DHT failure: -99.0 C / -1.0 %, LCD shows ERR instead of an old reading.

  Outputs start OFF. Commands stay active until OFF or reset/power removal.
  This is supervised manual control: no automatic watering, low-water cutoff
  or Bluetooth-disconnect cutoff. Keep a running pump submerged.
  Verify pin mapping and output polarity on the actual PCB before using loads.
*/
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <string.h>

// Change these only after checking the PCB wiring and measuring its outputs.
constexpr uint8_t LED_PIN = 4;
constexpr uint8_t DHT_PIN = 5;
constexpr uint8_t FAN_PIN = 6;
constexpr uint8_t PUMP_PIN = 7;
constexpr uint8_t BT_RX_PIN = 8;   // HC-06 TX -> Nano RX.
constexpr uint8_t BT_TX_PIN = 9;   // Nano TX -> HC-06 RX; check level shifting.
constexpr uint8_t LIGHT_PIN = A0;
constexpr uint8_t WATER_PIN = A1;
constexpr uint8_t LCD_ADDRESS = 0x27;  // 0x3F only if an I2C scan confirms it.
constexpr uint8_t DHT_TYPE = DHT11;    // Match the sensor fitted to the PCB.
constexpr bool LED_ACTIVE_HIGH = true;
constexpr bool FAN_ACTIVE_HIGH = true;
constexpr bool PUMP_ACTIVE_HIGH = true;
constexpr unsigned long SENSOR_INTERVAL_MS = 2000;  // DHT library minimum.
constexpr unsigned long COMMAND_GAP_MS = 500;       // Abandoned command timeout.

SoftwareSerial bluetooth(BT_RX_PIN, BT_TX_PIN);
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);
bool ledOn = false;
bool fanOn = false;
bool pumpOn = false;
float temperatureC = NAN;
float humidityPercent = NAN;
int lightRaw = 0;
int waterRaw = 0;
unsigned long lastSampleAt = 0;
uint8_t lcdPage = 0;

// ponytail: fixed storage avoids heap fragmentation in the Nano's 2 KB RAM.
char commandBuffer[16] = {};
uint8_t commandLength = 0;
bool discardCommand = false;
unsigned long lastCommandByteAt = 0;

void writeOutput(uint8_t pin, bool on, bool activeHigh) {
  digitalWrite(pin, on == activeHigh ? HIGH : LOW);
}

bool executeCommand(const char* command) {
  // Only these six commands are part of the classroom app's protocol.
  if (strcmp(command, "LED_ON") == 0) ledOn = true;
  else if (strcmp(command, "LED_OFF") == 0) ledOn = false;
  else if (strcmp(command, "FAN_ON") == 0) fanOn = true;
  else if (strcmp(command, "FAN_OFF") == 0) fanOn = false;
  else if (strcmp(command, "PUMP_ON") == 0) pumpOn = true;
  else if (strcmp(command, "PUMP_OFF") == 0) pumpOn = false;
  else return false;  // Unknown text never changes an output.

  writeOutput(LED_PIN, ledOn, LED_ACTIVE_HIGH);
  writeOutput(FAN_PIN, fanOn, FAN_ACTIVE_HIGH);
  writeOutput(PUMP_PIN, pumpOn, PUMP_ACTIVE_HIGH);
  return true;
}

void readBluetooth() {
  unsigned long now = millis();
  // Unsigned subtraction works even when millis() wraps after about 50 days.
  if (now - lastCommandByteAt >= COMMAND_GAP_MS) {
    commandLength = 0;
    discardCommand = false;
  }
  // Bound each pass so continuous bad input cannot starve sensors/LCD updates.
  for (uint8_t count = 0; count < 64 && bluetooth.available(); ++count) {
    char ch = static_cast<char>(bluetooth.read());
    lastCommandByteAt = millis();
    if (ch == ';' || ch == '\r' || ch == '\n') {
      commandLength = 0;
      discardCommand = false;
      continue;
    }
    if (discardCommand) continue;
    if (ch == ' ' && commandLength == 0) continue;
    if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
    if ((ch < 'A' || ch > 'Z') && ch != '_') {
      discardCommand = true;
      continue;
    }
    if (commandLength >= sizeof(commandBuffer) - 1) {
      // Discard the whole oversized message, including any valid-looking tail.
      discardCommand = true;
      continue;
    }
    commandBuffer[commandLength++] = ch;
    commandBuffer[commandLength] = '\0';
    // Existing app sends no delimiter: execute a complete known token at once.
    // New senders should add ';'. A valid token followed by junk still executes;
    // strict whole-frame validation needs a corresponding app protocol change.
    if (executeCommand(commandBuffer)) commandLength = 0;
  }
}

int averageAnalogRead(uint8_t pin) {
  analogRead(pin);  // Discard first conversion after switching ADC channels.
  long total = 0;
  for (uint8_t i = 0; i < 8; ++i) total += analogRead(pin);
  return static_cast<int>(total / 8);
}

void sendSensorPacket() {
  // Preserve the app's four-value, semicolon-framed packet with numeric fields.
  bluetooth.print(isnan(temperatureC) ? -99.0f : temperatureC, 1);
  bluetooth.print(',');
  bluetooth.print(isnan(humidityPercent) ? -1.0f : humidityPercent, 1);
  bluetooth.print(',');
  bluetooth.print(lightRaw);
  bluetooth.print(',');
  bluetooth.print(waterRaw);
  bluetooth.print(';');
}

void updateLcd() {
  // Overwrite both complete rows to remove old digits without clear() flicker.
  for (uint8_t row = 0; row < 2; ++row) {
    lcd.setCursor(0, row);
    lcd.print(F("                "));
  }
  lcd.setCursor(0, 0);
  if (lcdPage == 0) {
    lcd.print(F("Temp: "));
    if (isnan(temperatureC)) lcd.print(F("ERR"));
    else { lcd.print(temperatureC, 1); lcd.print(F(" C")); }
    lcd.setCursor(0, 1);
    lcd.print(F("Hum:  "));
    if (isnan(humidityPercent)) lcd.print(F("ERR"));
    else { lcd.print(humidityPercent, 1); lcd.print('%'); }
  } else if (lcdPage == 1) {
    lcd.print(F("Light raw: ")); lcd.print(lightRaw);
    lcd.setCursor(0, 1);
    lcd.print(F("Water raw: ")); lcd.print(waterRaw);
  } else {
    lcd.print(F("LED:")); lcd.print(ledOn ? F("ON ") : F("OFF"));
    lcd.print(F(" FAN:")); lcd.print(fanOn ? F("ON") : F("OFF"));
    lcd.setCursor(0, 1);
    lcd.print(F("Pump: ")); lcd.print(pumpOn ? F("ON") : F("OFF"));
  }
  lcdPage = (lcdPage + 1) % 3;
}

void setup() {
  // Preload OFF before OUTPUT mode to reduce startup pulses.
  writeOutput(LED_PIN, false, LED_ACTIVE_HIGH);
  writeOutput(FAN_PIN, false, FAN_ACTIVE_HIGH);
  writeOutput(PUMP_PIN, false, PUMP_ACTIVE_HIGH);
  pinMode(LED_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  Wire.begin();
  Wire.setWireTimeout(25000, true);  // A stuck I2C bus must not freeze commands.
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print(F("AI Smart Farm"));
  lcd.setCursor(0, 1); lcd.print(F("Manual control"));
  dht.begin();
  bluetooth.begin(9600);
  lastSampleAt = millis();  // Let the DHT settle before its first measurement.
}

void loop() {
  readBluetooth();
  unsigned long now = millis();
  if (now - lastSampleAt < SENSOR_INTERVAL_MS) return;
  lastSampleAt = now;
  humidityPercent = dht.readHumidity();
  temperatureC = dht.readTemperature();
  lightRaw = averageAnalogRead(LIGHT_PIN);
  waterRaw = averageAnalogRead(WATER_PIN);
  readBluetooth();  // Service commands between sensor work and transmission.
  sendSensorPacket();
  updateLcd();
}
