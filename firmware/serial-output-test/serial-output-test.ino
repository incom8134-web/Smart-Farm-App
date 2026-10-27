/*
  Nano output test: Serial Monitor, 9600 baud, Newline or Both NL & CR.
  Commands: LED_ON, LED_OFF, FAN_ON, FAN_OFF, PUMP_ON, PUMP_OFF, ALL_OFF.
  Assumed PCB controls: LED D4, fan D6, pump D7; HIGH = ON, LOW = OFF.

  Bench test only: hardware Serial on D0/D1 is deliberately enabled here.
  Do NOT upload ESP32 firmware while this sketch is running on the shared PCB.
  No Bluetooth, LCD, sensors, automatic cycling, or periodic Serial printing.
  Outputs stay commanded ON until OFF, ALL_OFF, reset, or power removal.
  Use a fan as the pump test load; there is no automatic pump protection.
  An OK reply confirms a pin write, NOT that the connected load obeyed it.
*/
#include <Arduino.h>
#include <string.h>

constexpr uint8_t LED_PIN = 4;
constexpr uint8_t FAN_PIN = 6;
constexpr uint8_t PUMP_PIN = 7;

char commandBuffer[16] = {};
uint8_t commandLength = 0;
bool overflow = false;

void allOff() {
  digitalWrite(LED_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(PUMP_PIN, LOW);
}

bool executeCommand(const char* command) {
  // Direct pin writes isolate output control from the classroom app firmware.
  if (strcmp(command, "LED_ON") == 0) digitalWrite(LED_PIN, HIGH);
  else if (strcmp(command, "LED_OFF") == 0) digitalWrite(LED_PIN, LOW);
  else if (strcmp(command, "FAN_ON") == 0) digitalWrite(FAN_PIN, HIGH);
  else if (strcmp(command, "FAN_OFF") == 0) digitalWrite(FAN_PIN, LOW);
  else if (strcmp(command, "PUMP_ON") == 0) digitalWrite(PUMP_PIN, HIGH);
  else if (strcmp(command, "PUMP_OFF") == 0) digitalWrite(PUMP_PIN, LOW);
  else if (strcmp(command, "ALL_OFF") == 0) allOff();
  else return false;
  return true;
}

void setup() {
  allOff();  // Preload LOW before enabling output drivers.
  pinMode(LED_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  Serial.begin(9600);
  Serial.println(F("OUTPUT TEST: 9600 baud; Newline or Both NL & CR."));
  Serial.println(F("LED_ON / LED_OFF / FAN_ON / FAN_OFF / PUMP_ON / PUMP_OFF / ALL_OFF"));
}

void loop() {
  // ponytail: one fixed buffer; execute only a complete newline-ended command.
  while (Serial.available()) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\r' || ch == '\n') {
      if (overflow) Serial.println(F("ERR: command too long; nothing changed."));
      else if (commandLength > 0) {
        commandBuffer[commandLength] = '\0';
        if (executeCommand(commandBuffer)) {
          Serial.print(F("OK: "));
          Serial.println(commandBuffer);
        } else Serial.println(F("ERR: unknown command; nothing changed."));
      }
      commandLength = 0;
      overflow = false;
    } else if (!overflow) {
      if (commandLength < sizeof(commandBuffer) - 1) {
        commandBuffer[commandLength++] = ch;
      } else overflow = true;  // Reject the entire line, not just its prefix.
    }
  }
}
