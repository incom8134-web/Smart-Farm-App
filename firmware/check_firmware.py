"""Execute the real Nano sketch and camera HTTP code with simulated hardware.

Run: uv run --with ziglang==0.15.1 python firmware/check_firmware.py
No Arduino is flashed. This checks logic, not wiring, power or radio timing.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent

COMMON = r'''
#pragma once
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <sstream>
#include <string>
#include <vector>
using std::isnan;
#define F(x) x
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define A0 14
#define A1 15
#define DHT11 11
static unsigned long clockMs = 0;
static int levels[32] = {}, modes[32] = {}, analogValues[32] = {};
unsigned long millis() { return clockMs; }
void delay(unsigned long ms) { clockMs += ms; }
void digitalWrite(uint8_t pin, int value) { levels[pin] = value; }
void pinMode(uint8_t pin, int value) { modes[pin] = value; }
int analogRead(uint8_t pin) { return analogValues[pin]; }
struct Printer {
  std::string output;
  void print(const char* s) { output += s; }
  void print(char ch) { output += ch; }
  void print(int value) { output += std::to_string(value); }
  void print(float value, int places) {
    char buf[32]; snprintf(buf, sizeof(buf), "%.*f", places, value); output += buf;
  }
};
// No hardware Serial object: accidentally reintroducing it fails compilation.
struct SoftwareSerial : Printer {
  std::deque<char> input;
  SoftwareSerial(int, int) {}
  void begin(int baud) { assert(baud == 9600); }
  int available() { return input.size(); }
  int read() { char ch = input.front(); input.pop_front(); return ch; }
};
struct WireType {
  void begin() {}
  void setWireTimeout(unsigned long, bool) {}
} Wire;
struct LiquidCrystal_I2C : Printer {
  LiquidCrystal_I2C(int, int, int) {}
  void init() {} void backlight() {} void setCursor(int, int) {}
};
struct DHT {
  float temperature = 24.5f, humidity = 60.0f;
  DHT(int, int) {} void begin() {}
  float readHumidity() { return humidity; }
  float readTemperature() { return temperature; }
};
'''

NANO_CHECK = r'''
void feed(const std::string& text) {
  for (char ch : text) bluetooth.input.push_back(ch);
  readBluetooth();
}
int main() {
  setup();
  assert(!ledOn && !fanOn && !pumpOn);
  assert(levels[LED_PIN] == LOW && levels[FAN_PIN] == LOW && levels[PUMP_PIN] == LOW);
  assert(modes[LED_PIN] == OUTPUT && modes[FAN_PIN] == OUTPUT && modes[PUMP_PIN] == OUTPUT);
  feed("LED_"); assert(!ledOn);
  clockMs += 50;
  feed("ON"); assert(ledOn && levels[LED_PIN] == HIGH);
  feed("LED_OFFFAN_ONPUMP_ON"); assert(!ledOn && fanOn && pumpOn);
  feed("fan_off;\r\nPUMP_OFF;"); assert(!fanOn && !pumpOn);
  assert(!executeCommand("AUTO") && !executeCommand("PROTO2"));
  feed(std::string(30, 'X') + "PUMP_ON;"); assert(!pumpOn);
  feed("?PUMP_ON;"); assert(!pumpOn);
  feed("LED_"); clockMs += COMMAND_GAP_MS; feed("PUMP_ON"); assert(pumpOn);
  feed("PUMP_OFF;");
  // Invalid prefixes cannot absorb the next command after an idle timeout.
  feed("JUNK"); clockMs += COMMAND_GAP_MS; feed("LED_ON"); assert(ledOn);
  feed("LED_OFF;");
  feed(std::string(100, 'X')); assert(bluetooth.available() == 36);
  readBluetooth(); assert(bluetooth.available() == 0);
  feed(";");
  clockMs = 1999; loop(); assert(bluetooth.output.empty());
  analogValues[A0] = 1000; analogValues[A1] = 12;
  clockMs = 2000; loop();
  assert(bluetooth.output == "24.5,60.0,1000,12;");
  bluetooth.output.clear();
  dht.temperature = NAN; dht.humidity = NAN;
  clockMs = 4000; loop();
  assert(bluetooth.output == "-99.0,-1.0,1000,12;");
  assert(isnan(temperatureC) && isnan(humidityPercent));
  lcdPage = 0; lcd.output.clear(); updateLcd();
  assert(lcd.output.find("ERR") != std::string::npos);
  // This firmware deliberately keeps manual outputs latched after sensor loss.
  feed("PUMP_ON"); clockMs += 60000; loop(); assert(pumpOn);
  feed("PUMP_OFF"); assert(!pumpOn);
  writeOutput(LED_PIN, false, false); assert(levels[LED_PIN] == HIGH);
  writeOutput(LED_PIN, true, false); assert(levels[LED_PIN] == LOW);
  // Unsigned elapsed-time arithmetic across a native unsigned-long wrap.
  lastSampleAt = ULONG_MAX - 999; clockMs = 1000;
  bluetooth.output.clear(); loop(); assert(!bluetooth.output.empty());
  puts("PASS Nano: commands, framing limits, timeout, sensors, polarity, rollover");
}
'''

SERIAL_OUTPUT_CHECK = r'''
void feedSerial(const std::string& text) {
  for (char ch : text) Serial.input.push_back(ch);
  loop();
}
int main() {
  setup();
  assert(levels[4] == LOW && levels[6] == LOW && levels[7] == LOW);
  assert(modes[4] == OUTPUT && modes[6] == OUTPUT && modes[7] == OUTPUT);
  Serial.output.clear(); loop(); assert(Serial.output.empty());
  feedSerial("LED_ON"); assert(levels[4] == LOW);  // Wait for newline.
  feedSerial("\r\n"); assert(levels[4] == HIGH && Serial.output == "OK: LED_ON\n");
  feedSerial("FAN_ON\nPUMP_ON\n");
  assert(levels[4] == HIGH && levels[6] == HIGH && levels[7] == HIGH);
  feedSerial("PUMP_OFF\n");
  assert(levels[4] == HIGH && levels[6] == HIGH && levels[7] == LOW);
  feedSerial(std::string(30, 'X') + "PUMP_ON\n"); assert(levels[7] == LOW);
  feedSerial("UNKNOWN\n"); assert(levels[7] == LOW);
  feedSerial("LED_OFF\nFAN_OFF\n"); assert(levels[4] == LOW && levels[6] == LOW);
  feedSerial("LED_ON\nFAN_ON\nPUMP_ON\nALL_OFF\n");
  assert(levels[4] == LOW && levels[6] == LOW && levels[7] == LOW);
  Serial.output.clear(); clockMs += 60000; loop(); assert(Serial.output.empty());
  puts("PASS Serial output test: boot OFF, six commands, ALL_OFF, framing, no idle printing");
}
'''

HTTP = r'''
#pragma once
#include "Arduino.h"
using esp_err_t = int;
using httpd_handle_t = void*;
constexpr int ESP_OK = 0, ESP_FAIL = -1, PIXFORMAT_JPEG = 1, HTTP_GET = 0;
constexpr int HTTPD_RESP_USE_STRLEN = -1;
struct httpd_req_t {};
struct camera_fb_t { uint8_t* buf; size_t len; int format; };
static uint8_t bytes[] = {1, 2, 3};
static camera_fb_t testFrame = {bytes, 3, PIXFORMAT_JPEG};
static int frameGets = 0, returns = 0, sends = 0, chunkFailure = 1;
static bool noFrame = false, failSend = false;
camera_fb_t* esp_camera_fb_get() { ++frameGets; return noFrame ? nullptr : &testFrame; }
void esp_camera_fb_return(camera_fb_t* frame) { assert(frame == &testFrame); ++returns; }
int httpd_resp_set_type(httpd_req_t*, const char*) { return ESP_OK; }
int httpd_resp_set_hdr(httpd_req_t*, const char*, const char*) { return ESP_OK; }
int httpd_resp_send_500(httpd_req_t*) { return ESP_FAIL; }
int httpd_resp_send(httpd_req_t*, const char*, int) { return failSend ? ESP_FAIL : ESP_OK; }
int httpd_resp_send_chunk(httpd_req_t*, const char*, int) {
  return ++sends == chunkFailure ? ESP_FAIL : ESP_OK;
}
struct httpd_config_t {
  int server_port = 80, ctrl_port = 32768, max_uri_handlers = 8;
  int max_open_sockets = 7, recv_wait_timeout = 5, send_wait_timeout = 5;
  bool lru_purge_enable = false;
};
#define HTTPD_DEFAULT_CONFIG() httpd_config_t{}
struct httpd_uri_t { const char* uri; int method; int (*handler)(httpd_req_t*); };
static int starts = 0, stops = 0, registrations = 0, failStart = 0, failRegister = 0;
static std::vector<httpd_config_t> configs;
int httpd_start(httpd_handle_t* handle, const httpd_config_t* config) {
  configs.push_back(*config);
  if (++starts == failStart) return ESP_FAIL;
  *handle = reinterpret_cast<void*>(static_cast<uintptr_t>(starts)); return ESP_OK;
}
int httpd_stop(httpd_handle_t) { ++stops; return ESP_OK; }
int httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t*) {
  return ++registrations == failRegister ? ESP_FAIL : ESP_OK;
}
'''

HTTP_CHECK = r'''
void resetServer() {
  starts = stops = registrations = failStart = failRegister = 0;
  configs.clear();
}
int main() {
  httpd_req_t request;
  assert(captureHandler(&request) == ESP_OK && frameGets == returns);
  failSend = true;
  assert(captureHandler(&request) == ESP_FAIL && frameGets == returns);
  for (int fail = 1; fail <= 3; ++fail) {
    sends = 0; chunkFailure = fail;
    assert(streamHandler(&request) == ESP_FAIL && frameGets == returns);
  }
  testFrame.format = 99;
  assert(streamHandler(&request) == ESP_FAIL && frameGets == returns);
  assert(captureHandler(&request) == ESP_FAIL && frameGets == returns);
  noFrame = true;
  assert(streamHandler(&request) == ESP_FAIL);
  assert(captureHandler(&request) == ESP_FAIL);
  resetServer(); assert(startCameraServer());
  assert(configs[0].server_port == 80 && configs[1].server_port == 81);
  assert(configs[0].ctrl_port != configs[1].ctrl_port);
  for (int fail = 1; fail <= 2; ++fail) {
    resetServer(); failStart = fail;
    assert(!startCameraServer()); assert(stops == fail - 1);
  }
  for (int fail = 1; fail <= 4; ++fail) {
    resetServer(); failRegister = fail;
    assert(!startCameraServer()); assert(stops == (fail < 4 ? 1 : 2));
    assert(pageServer == nullptr);
  }
  puts("PASS HTTP: frame ownership on send/capture errors, ports, startup cleanup");
}
'''


def check(source: Path, extra: str, files: dict[str, str]) -> None:
    with tempfile.TemporaryDirectory(prefix="smartfarm-check-") as folder:
        work = Path(folder)
        for name, body in files.items():
            (work / name).write_text(body, encoding="utf-8")
        test = work / "check.cpp"
        test.write_text(f'#include "{source.as_posix()}"\n{extra}', encoding="utf-8")
        executable = work / ("check.exe" if sys.platform == "win32" else "check")
        subprocess.run([sys.executable, "-m", "ziglang", "c++", "-std=c++11",
                        "-Wall", "-Wextra", "-I", str(work), str(test),
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    headers = {"Arduino.h": COMMON}
    headers.update({name: '#include "Arduino.h"\n' for name in
                    ("SoftwareSerial.h", "Wire.h", "LiquidCrystal_I2C.h", "DHT.h")})
    check(ROOT / "nano-controller/nano-controller.ino", NANO_CHECK, headers)
    serial_header = COMMON + r'''
struct HardwareSerial : SoftwareSerial {
  HardwareSerial() : SoftwareSerial(0, 1) {}
  void println(const char* text) { print(text); print('\n'); }
} Serial;
'''
    check(ROOT / "serial-output-test/serial-output-test.ino", SERIAL_OUTPUT_CHECK,
          {"Arduino.h": serial_header})
    check(ROOT / "esp32-cam/app_httpd.cpp", HTTP_CHECK,
          {"Arduino.h": COMMON, "esp_camera.h": HTTP,
           "esp_http_server.h": '#include "esp_camera.h"\n'})
