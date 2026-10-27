/*
  JINIE EduTech AI Smart Farm - final AI Thinker ESP32-CAM classroom camera

  Uses Espressif's camera driver. Phone/camera join the same 2.4 GHz router.
  / = viewing page, /capture = JPEG, :81/stream = MJPEG, /status = startup health.
  Supplies images; plant-health ML is a separate lesson, not part of firmware.
  Startup messages run once. No application UART output after setup().
  The ESP32 ROM can still print a boot message before this program starts.
*/
#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "esp_log.h"
#include "board_config.h"

// Arduino core log macros can print from background tasks. Enforce the quiet
// build setting rather than relying on Serial.setDebugOutput(false) alone.
#if ARDUHAL_LOG_LEVEL > 0
#error "Select Tools > Core Debug Level > None for the shared-UART PCB."
#endif

// Enter the classroom router's details before uploading.
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "";  // Empty only for an intentionally open router.
constexpr unsigned long WIFI_TIMEOUT_MS = 20000;

bool startCameraServer();  // In app_httpd.cpp; keep all sketch files together.

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  esp_log_level_set("*", ESP_LOG_NONE);

  // Keep the on-board flash LED off. This is not the Nano's grow light.
  digitalWrite(LED_GPIO_NUM, LOW);
  pinMode(LED_GPIO_NUM, OUTPUT);
  camera_config_t config = {};  // Initialize all fields, including driver extras.
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  // ponytail: fixed QVGA (320x240) avoids large buffers and register controls.
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;  // Lower number = better quality, more bytes.
  bool hasPsram = psramFound();
  config.fb_count = hasPsram ? 2 : 1;
  config.fb_location = hasPsram ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  config.grab_mode = hasPsram ? CAMERA_GRAB_LATEST : CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t error = esp_camera_init(&config);
  if (error != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x. Check board, ribbon and power.\n", error);
    Serial.flush();
    Serial.end();
    return;  // Stay idle instead of creating a reset/print loop.
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);
  unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_TIMEOUT_MS) {
    delay(100);  // Yield to Wi-Fi tasks without repeatedly printing dots.
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi timeout. Check 2.4 GHz router/credentials, then reset.");
    WiFi.disconnect(true);
    esp_camera_deinit();
  } else if (!startCameraServer()) {
    Serial.println("HTTP server failed to start. Reset and check power/memory.");
    WiFi.disconnect(true);
    esp_camera_deinit();
  } else {
    // Exactly one IP report per successful boot, never repeated from loop().
    Serial.print("Camera IP: ");
    Serial.println(WiFi.localIP());
  }
  // Release the UART after startup, including on Wi-Fi/server failure.
  Serial.flush();
  Serial.end();
}

void loop() {
  // HTTP/Wi-Fi use their own tasks. No UART output or automatic reset here.
  delay(1000);
}
