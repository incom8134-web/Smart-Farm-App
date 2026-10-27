# AI Smart Farm – Android app (MIT App Inventor V11)

**English** | [한국어](README.ko.md)

- [Ai_Smart_Farm_V11.apk](Ai_Smart_Farm_V11.apk?raw=true): Android install package.
- [Ai_Smart_Farm_V11.aia](Ai_Smart_Farm_V11.aia?raw=true): editable MIT App Inventor source.

V11 is the project revision label; the version shown inside the app is 1.3.
Test the app with the phone and the real board before a class.

## Install

Install the APK on an Android phone and allow installation from this source if
Android asks. Pair the HC-06 in the Android Bluetooth settings and allow the
Nearby devices / Bluetooth permission when the app asks.

## Edit and rebuild

1. Sign in to <https://ai2.appinventor.mit.edu/>.
2. Choose **Projects → Import project (.aia) from my computer** and select the V11 file.
   Back up any existing online project with the same name first.
3. Check the Designer and Blocks, then choose **Build → Android App (.apk)**.
4. Install the new APK. Changing the AIA never updates an app that is already
   installed. If the signing key differs, uninstall the old app first.

## Firmware and camera

The board programs are in [../firmware](../firmware); see the
[firmware guide](../firmware/README.md) for versions, libraries and the
ESP32-first / Nano-second upload order.

Connect the phone and the ESP32-CAM to the same router and enter the camera IP
that the ESP32 prints once at startup. The camera (Wi-Fi) and the HC-06
(Bluetooth) are separate connections.

## Phone and board checklist

1. Fresh install: deny, retry and grant the Bluetooth permission; test Bluetooth OFF/ON.
2. Pair and connect the HC-06; check the Unknown states and fresh sensor data every two seconds.
3. Send each ON/OFF command separately, including OFF when a switch already looks off.
   Check the Nano LCD and the real output.
4. Disconnect and reconnect, and interrupt the Nano data: outputs must not be shown as
   physically OFF, and old readings must not stay marked as fresh.
5. Unplug the DHT11: temperature/humidity show ERR while the analog readings still update.
6. Change theme and language: current readings and command states are kept.
7. Test a wrong and a correct camera IP, live video, and leaving/reopening the camera screen.
8. Confirm the PCB DIP routing and power ratings, and that the ESP32 can be uploaded again.

**Manual-control safety:** a sent command is not a confirmation from the hardware.
Disconnecting or closing the app does not stop a running pump, fan or light. Send OFF
and check that the load really stops. Keep the pump under water. There is no automatic
low-water or disconnect cutoff. Disconnect USB **and** external power before changing
DIP switches, and never turn all switches ON.
