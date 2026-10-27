# Classroom firmware

**English** | [한국어](README.ko.md)

Two programs run a supervised, manual smart-farm lesson. The **Arduino Nano** reads the
sensors and switches three outputs on Bluetooth commands. The **ESP32-CAM** provides
photos and live video through a Wi-Fi router. Neither program waters automatically or
classifies plant health.

Open `nano-controller/nano-controller.ino` and `esp32-cam/esp32-cam.ino` in the Arduino
IDE. Keep the ESP32 sketch's whole folder together: its `.cpp` and `.h` files are part of
the program. Each board has one permanent program.

| Folder / file | Purpose |
|---|---|
| [`nano-controller/`](nano-controller) | Final Arduino Nano program (sensors, LCD, Bluetooth, outputs) |
| [`esp32-cam/`](esp32-cam) | Final ESP32-CAM camera program |
| [`sensor-tests/`](sensor-tests) | Single-part lessons: `lightsensor`, `soilmoisture`, `pump` |
| [`serial-output-test/`](serial-output-test) | Bench test: switch LED/fan/pump from the Serial Monitor |
| [`check_firmware.py`](check_firmware.py) | Offline check of the firmware (developers) |

The camera uses the router method from the teaching deck: the phone and the camera join
the same router, and the app uses the IP address the camera prints at startup.

## Why the Nano never uses hardware Serial

On this custom PCB the Nano's hardware UART shares the line used to program the ESP32, so
Nano serial output can block ESP32 uploads. The Nano program therefore never starts
hardware `Serial`; the HC-06 Bluetooth module uses `SoftwareSerial` on D8/D9.

The ESP32 prints exactly one `Camera IP: <address>` line per successful boot at 115200
baud and then releases its UART with `Serial.end()`. Select **Core Debug Level: None** –
the program refuses to compile with any other debug level. Boot messages from the ESP32
ROM can still appear before the program starts, so the PCB switches must isolate the
boards when the Nano is programmed.

## Upload procedure

1. Disconnect USB and external power before moving modules or changing DIP switches.
   Leave the loads (light, fan, pump) disconnected for the first upload and output tests.
2. If the Nano still runs an old sketch that prints continuously, first upload the
   **final Nano sketch** from this folder with the ESP32 removed. No separate blank sketch is needed.
3. Power off. Re-insert the ESP32 and select its programming route on the DIP switches
   (reported as **1/3/5 ON, 2/4/6 OFF** – check the PCB manual). Use the board's download/reset sequence.
4. Upload the ESP32 **first** with the settings below, then return it to normal boot mode.
5. Open Serial Monitor at 115200 **before** resetting to catch the one-time
   `Camera IP: <address>` line. Write the IP down and close Serial Monitor.
6. Check that a second ESP32 upload through the PCB also works.
7. Power off. Select the Nano programming route (reported as **2/4/6 ON, 1/3/5 OFF**)
   and upload the final Nano sketch **second**.
8. Power off and select the normal operating mode. Do not turn every DIP switch ON to run
   both boards; only the PCB documentation says which paths may be on together.
9. Power on, pair the HC-06 and test the outputs one at a time.

A normal upload replaces the old program. Erasing flash does not fix serial conflicts:
even an erase must talk to the ESP32 bootloader first. For an optional full ESP32 erase,
set **Erase All Flash Before Sketch Upload: Enabled** for one upload, then set it back to
**Disabled**. Do not use Nano **Burn Bootloader** as an erase.

## Arduino IDE settings

| Item | Nano | ESP32-CAM |
|---|---|---|
| Board package | Arduino AVR Boards 1.8.8 | esp32 by Espressif Systems 3.3.2 |
| Board | Arduino Nano | AI Thinker ESP32-CAM |
| Processor | ATmega328P (Old Bootloader only if that Nano needs it) | AI Thinker defaults |
| Upload speed | Board default | 115200 if faster speeds are unreliable through the PCB |
| Debug level | – (no hardware Serial) | Core Debug Level: None |
| PSRAM | – | Enabled when offered |
| Flash | Default | 4 MB AI Thinker module |
| Partition scheme | Default | Included `partitions.csv` (single app, no OTA) |

Libraries: **DHT sensor library 1.4.7**, **Adafruit Unified Sensor 1.1.15** and
**LiquidCrystal I2C 1.1.2**. Other LCD libraries with the same name can use different
functions, so use these versions. Wire and SoftwareSerial come with the AVR package; the
camera, Wi-Fi and HTTP libraries come with the ESP32 package.

## Nano wiring

| Signal | Nano pin | Meaning |
|---|---|---|
| Grow-light driver | D4 | Output, `LED_ON` / `LED_OFF` |
| DHT11 | D5 | Temperature (°C), relative humidity (%) |
| Fan driver | D6 | Output, `FAN_ON` / `FAN_OFF` |
| Pump driver | D7 | Output, `PUMP_ON` / `PUMP_OFF` |
| HC-06 TX | D8 | Nano receives at 9600 baud |
| HC-06 RX | D9 | Nano sends (check the PCB level shifting to the module) |
| Light sensor | A0 | Raw ADC value 0–1023 |
| Water sensor | A1 | Raw ADC value 0–1023 (not calibrated soil moisture) |
| LCD SDA / SCL | A4 / A5 | 16x2 LCD at 0x27 (scan before changing to 0x3F) |

With the loads disconnected, measure each output pin's ON and OFF level against ground,
then follow the signal into the driver stage. Never connect a motor directly to a GPIO pin.

`LED_ACTIVE_HIGH`, `FAN_ACTIVE_HIGH` and `PUMP_ACTIVE_HIGH` are separate settings, all
`true` by default. Set one to `false` only for a driver confirmed to be active-low.

The Nano program has three teaching steps:

1. `readBluetooth()` collects command characters without blocking.
2. Every two seconds `loop()` reads the DHT11 and averages eight ADC readings.
3. `sendSensorPacket()` sends the values, then `updateLcd()` cycles through three LCD pages.

The DHT11 needs at least two seconds between readings. A failed reading shows `ERR` on the
LCD and error values over Bluetooth – an old value is never shown as new.

There is no low-water cutoff or pump timer. `PUMP_ON` stays on until `PUMP_OFF`, a reset or
power-off, even if the phone disconnects. Use it under supervision.

## Bluetooth protocol

Send one of the six commands: `LED_ON`, `LED_OFF`, `FAN_ON`, `FAN_OFF`, `PUMP_ON`,
`PUMP_OFF`. Lowercase is accepted. The app appends `;`; a command without a terminator is
also executed as soon as it is complete. CR or LF also work as terminators.

The receive buffer holds 16 bytes. Too-long or invalid text is discarded up to the next
terminator or a 500 ms pause; an unfinished command also expires after that pause.

Every two seconds the Nano sends four values and a semicolon:

```text
24.5,60.0,410,330;
```

Fields: temperature °C, humidity %, light raw, water raw. A DHT failure sends `-99.0` for
temperature and `-1.0` for humidity. There are no acknowledgements: the app cannot tell the
real state of a load from this packet, so its ON/OFF labels show the last command sent.

SoftwareSerial is half-duplex, and DHT reading can briefly block interrupts, so commands
sent in a quick burst can be lost. Send one command at a time and check the LCD or output.

## Camera

Set `WIFI_SSID` and `WIFI_PASSWORD` at the top of `esp32-cam.ino`. The phone and the
camera must be on the same 2.4 GHz network; a guest network that isolates clients can block
access. Use the IP printed at boot (`192.168.4.1` is not this program's address).

At startup the camera either connects within 20 seconds and starts its web servers, or
prints one error message and waits until reset. After connecting, Wi-Fi reconnects
automatically, but the router may give a new IP – reset the camera to print it again.

| URL | Result |
|---|---|
| `http://<ip>/` | Start/stop live view and photo link |
| `http://<ip>/capture` | JPEG photo |
| `http://<ip>:81/stream` | MJPEG live stream for the app or a browser |
| `http://<ip>/status` | JSON with the camera format |

The camera is fixed at 320x240 JPEG for a lightweight live view. One live viewer is
supported; close old streams before opening a new one. Photos and streaming share the
sensor, so use one at a time on boards without PSRAM. Higher-resolution datasets for
machine learning need a separate lesson.

HTTP has no password or encryption. Use a trusted classroom network and never expose the
camera to the Internet. Use the specified power supply (random resets are often a power
problem), do not hot-plug the camera ribbon and never apply 5 V to an ESP32 GPIO.

## Teacher's check before class

| Check | Expected result |
|---|---|
| Nano power-on/reset, loads disconnected | All three outputs at their inactive level |
| Six individual commands | Each ON and OFF changes the output pin and the LCD |
| Working DHT11, then DHT11 unplugged | Fresh readings, then ERR / error values |
| Cover/uncover the light sensor; wet/dry water sensor | Raw values change |
| Long invalid Bluetooth text, then a valid command | Invalid text ignored, next command works |
| Nano after boot | No hardware-UART output |
| ESP32 startup | One IP or error message |
| Wrong router password | Timeout after about 20 s, then waits for reset |
| Capture and stop/start the stream several times | Valid images, no crash |
| ESP32 upload twice through the PCB, then Nano twice | Works every time with the documented switch sequence |
| Loads connected after electrical checks | Light, fan and pump follow ON/OFF |

## Developer checks

Compiled on 2026-10-19 with the package and library versions above:

| Program | Compile | Flash | Static RAM |
|---|---|---|---|
| Nano controller (ATmega328P Old Bootloader) | Passed | 10,208 / 30,720 bytes | 670 / 2,048 bytes |
| ESP32-CAM camera | Passed | 1,023,163 / 3,145,728 bytes | 55,356 / 327,680 bytes |
| Pump lesson | Passed | 930 / 30,720 bytes | 9 / 2,048 bytes |
| Soil (A1) lesson | Passed | 3,570 / 30,720 bytes | 283 / 2,048 bytes |

Compile without a board using the Arduino CLI bundled with the Arduino IDE, from the
repository root:

```text
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old firmware/nano-controller
arduino-cli compile --fqbn esp32:esp32:esp32cam firmware/esp32-cam
uv run --with ziglang==0.15.1 python firmware/check_firmware.py
```

`check_firmware.py` compiles the real Nano sketch and HTTP code against simulated devices
and tests split/combined commands, invalid input, overflow, timeouts, sensor failures,
active-low outputs, timer wrap-around and camera-buffer cleanup. It does not simulate
electrical timing, the DHT signal or real Wi-Fi clients.

The `sensor-tests/` sketches are single-part lessons: the light test reads A0, the soil test
reads A1, and the pump test runs a supervised 3-second ON / 3-second OFF cycle starting OFF.

`serial-output-test/` is a bench test for the outputs. It uses the Nano's hardware Serial
(9600 baud, Newline) and accepts `LED_ON`, `LED_OFF`, `FAN_ON`, `FAN_OFF`, `PUMP_ON`, `PUMP_OFF`
and `ALL_OFF`. Because it uses hardware Serial, **do not upload the ESP32 while it is running**
on the shared PCB – upload the final Nano program again afterwards.
