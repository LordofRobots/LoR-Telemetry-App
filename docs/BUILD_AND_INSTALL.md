# Build, flash, and install

## Firmware

### Requirements

- Arduino CLI or Arduino IDE
- Bluepad32 ESP32 board package (the verified local build used 4.1.0)
- FastLED
- ESP32Servo (the verified local build used 3.2.1)
- A data-capable USB cable

From the repository root:

```powershell
arduino-cli compile --fqbn esp32-bluepad32:esp32:esp32s3 .\firmware\LoR_Combat_SSS_2608050002_battery_dashboard
arduino-cli upload -p COM10 --fqbn esp32-bluepad32:esp32:esp32s3 .\firmware\LoR_Combat_SSS_2608050002_battery_dashboard
```

Change `COM10` if Windows assigns another port. If automatic upload fails, hold **BOOT**, tap **RESET**, release **BOOT**, and retry. Keep the weapon power link removed while flashing and validating outputs.

At 115200 baud, the serial console prints state, connection, controller input, requested commands, physical output values, battery ADC values, and loop rates at 5 Hz.

## Android dashboard

### Requirements

- Windows PowerShell
- Android SDK platform and build tools (API 36 used for the verified build)
- JDK 8 or newer; Android Studio's bundled JBR works

Set `ANDROID_SDK_ROOT` (or `ANDROID_HOME`) and optionally `JAVA_HOME`, then run:

```powershell
powershell -ExecutionPolicy Bypass -File .\android-dashboard\build.ps1
```

The script discovers the newest installed Android platform/build-tools version and creates:

```text
android-dashboard\output\LoR-Telemetry-debug.apk
```

The debug keystore is generated locally and intentionally excluded from Git. A prebuilt debug APK is included for convenient testing; production releases should use a protected release signing key.

Install over USB with Android Debug Bridge:

```powershell
adb install -r .\android-dashboard\output\LoR-Telemetry-debug.apk
```

If Android reports a signing mismatch with an older build, uninstall the existing app first. This removes that app's local data.

## First connection

1. Power the robot with the weapon physically disabled.
2. Open **LoR Combat Telemetry** and grant Nearby devices/Bluetooth permission.
3. Tap the connection control; the app scans for `LoR SSS Telemetry`.
4. Verify plausible battery voltage, zero motor commands, drive disabled, and ESC disabled before connecting the gamepad.
5. Connect the gamepad and keep all motion controls neutral through the 500 ms release interlock.

Only one phone or tablet telemetry connection is supported at a time.
