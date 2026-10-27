// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Simplified for JINIE EduTech classroom use.
// Uses Espressif esp_http_server and esp_camera. JPEG-only teaching server:
// no face detection, sensor-register writes, flash PWM or runtime UART logs.
#include <Arduino.h>
#include "esp_camera.h"
#include "esp_http_server.h"

static httpd_handle_t pageServer = nullptr;
static httpd_handle_t streamServer = nullptr;
static const char VIEW_PAGE[] = R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>AI Smart Farm Camera</title>
<style>body{font:18px sans-serif;max-width:40rem;margin:2rem auto;padding:1rem}
img{width:100%;background:#eee}button,a{margin:.5rem;padding:.5rem}</style>
<h1>AI Smart Farm Camera</h1><p>320 x 240 JPEG camera. One live viewer at a time.</p>
<button onclick="view.src='http://'+location.hostname+':81/stream'">Start live view</button>
<button onclick="view.removeAttribute('src')">Stop live view</button>
<a href="/capture" target="_blank" rel="noopener">Take a photograph</a>
<p><img id="view" alt="Camera live view"></p>
<p>This camera supplies images. It does not classify plant health.</p></html>)HTML";

static esp_err_t pageHandler(httpd_req_t* request) {
  httpd_resp_set_type(request, "text/html");
  return httpd_resp_send(request, VIEW_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t statusHandler(httpd_req_t* request) {
  httpd_resp_set_type(request, "application/json");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  return httpd_resp_send(request, "{\"camera\":\"ready\",\"format\":\"JPEG\",\"width\":320,\"height\":240}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t captureHandler(httpd_req_t* request) {
  camera_fb_t* frame = esp_camera_fb_get();
  if (!frame) return httpd_resp_send_500(request);
  if (frame->format != PIXFORMAT_JPEG) {
    esp_camera_fb_return(frame);
    return httpd_resp_send_500(request);
  }
  httpd_resp_set_type(request, "image/jpeg");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_set_hdr(request, "Content-Disposition", "inline; filename=plant.jpg");
  esp_err_t result = httpd_resp_send(request, reinterpret_cast<const char*>(frame->buf), frame->len);
  // Every successful get MUST have a return, even if sending failed.
  esp_camera_fb_return(frame);
  return result;
}

static esp_err_t streamHandler(httpd_req_t* request) {
  const char* boundary = "\r\n--farm-frame\r\n";
  esp_err_t result = httpd_resp_set_type(request, "multipart/x-mixed-replace;boundary=farm-frame");
  if (result != ESP_OK) return result;
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  // MJPEG is independent JPEGs, each preceded by its byte length.
  while (true) {
    camera_fb_t* frame = esp_camera_fb_get();
    if (!frame) return ESP_FAIL;
    if (frame->format != PIXFORMAT_JPEG) {
      esp_camera_fb_return(frame);
      return ESP_FAIL;
    }
    char header[96];  // Character storage, not an array of pointers.
    int length = snprintf(header, sizeof(header),
        "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
        static_cast<unsigned>(frame->len));
    if (length < 0 || static_cast<size_t>(length) >= sizeof(header)) {
      esp_camera_fb_return(frame);
      return ESP_FAIL;
    }
    result = httpd_resp_send_chunk(request, boundary, strlen(boundary));
    if (result == ESP_OK) result = httpd_resp_send_chunk(request, header, length);
    if (result == ESP_OK) result = httpd_resp_send_chunk(request,
        reinterpret_cast<const char*>(frame->buf), frame->len);
    esp_camera_fb_return(frame);
    if (result != ESP_OK) return result;  // Disconnect or send timeout.
    delay(20);  // Yield between frames rather than busy-spinning.
  }
}

bool startCameraServer() {
  // ponytail: two standard HTTP tasks keep a stream on 81 from blocking still
  // captures and the viewing page on 80. One live viewer per camera.
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.max_uri_handlers = 3;
  config.max_open_sockets = 2;
  config.recv_wait_timeout = 5;
  config.send_wait_timeout = 5;
  config.lru_purge_enable = true;
  httpd_uri_t page = {};
  page.uri = "/"; page.method = HTTP_GET; page.handler = pageHandler;
  httpd_uri_t capture = {};
  capture.uri = "/capture"; capture.method = HTTP_GET; capture.handler = captureHandler;
  httpd_uri_t status = {};
  status.uri = "/status"; status.method = HTTP_GET; status.handler = statusHandler;
  if (httpd_start(&pageServer, &config) != ESP_OK) return false;
  if (httpd_register_uri_handler(pageServer, &page) != ESP_OK ||
      httpd_register_uri_handler(pageServer, &capture) != ESP_OK ||
      httpd_register_uri_handler(pageServer, &status) != ESP_OK) {
    httpd_stop(pageServer);
    pageServer = nullptr;
    return false;
  }
  config.server_port = 81;
  config.ctrl_port += 1;  // A distinct internal control port for each task.
  config.max_uri_handlers = 1;
  config.max_open_sockets = 1;
  httpd_uri_t stream = {};
  stream.uri = "/stream"; stream.method = HTTP_GET; stream.handler = streamHandler;
  if (httpd_start(&streamServer, &config) != ESP_OK) {
    httpd_stop(pageServer);
    pageServer = nullptr;
    return false;
  }
  if (httpd_register_uri_handler(streamServer, &stream) != ESP_OK) {
    httpd_stop(streamServer);
    httpd_stop(pageServer);
    streamServer = nullptr;
    pageServer = nullptr;
    return false;
  }
  return true;
}
