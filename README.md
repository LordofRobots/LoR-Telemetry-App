# LoR Telemetry App

Firmware and a read-only Android telemetry dashboard for a Lord of Robots spinning-shell combat robot. The robot uses a Waveshare ESP32-S3-Zero, an Xbox-compatible gamepad, tank drive, and a bidirectional BLHeli_S weapon ESC running DShot300.

![LoR Combat Telemetry dashboard](docs/images/dashboard-tablet.png)

## What is included

- `firmware/LoR_Combat_SSS_2608050002_battery_dashboard/` — ESP32-S3 robot firmware, safety interlocks, DShot300 weapon output, battery monitoring, LEDs, and BLE telemetry.
- `android-dashboard/` — dependency-free native Android dashboard source and PowerShell build script.
- `android-dashboard/output/LoR-Telemetry-debug.apk` — prebuilt debug APK for Android 12 or newer.
- `docs/` — hardware, architecture, build, BLE protocol, and fail-safe documentation.

The dashboard shows the robot state, all gamepad inputs, drive and weapon commands, ESC state, control-loop health, and a rolling 30-second 3S HV LiPo voltage graph. Its layout scales to phones and tablets without scrolling. The app has no robot-control characteristic: it can observe the robot but cannot enable or command motion.

## Quick start

1. Wire and calibrate the battery divider as described in [Hardware](docs/HARDWARE.md).
2. Compile and upload the firmware using the instructions in [Build and install](docs/BUILD_AND_INSTALL.md).
3. Install `android-dashboard/output/LoR-Telemetry-debug.apk` on an Android 12+ device.
4. Power the robot, grant the app Bluetooth permissions, and tap **Connect**. The app scans for `LoR SSS Telemetry`.

## Robot controls

| Input | Function |
|---|---|
| Left stick Y | Forward/reverse tank drive, limited to 75% of the original top speed |
| Right stick X | Steering, reduced to 50% turn authority |
| Right trigger | Proportional forward weapon command |
| Left trigger | Proportional reverse/brake weapon command |
| A | Forward weapon idle preset |
| B | Stop weapon |
| X | Reverse weapon idle preset |

Motion remains disabled after a gamepad connects until the firmware receives a real controller report and all motion controls remain neutral for 500 ms. See [Safety review](docs/SAFETY_REVIEW.md) for the complete behavior and remaining hardware risks.

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Hardware and wiring](docs/HARDWARE.md)
- [Build, flash, and install](docs/BUILD_AND_INSTALL.md)
- [BLE telemetry protocol](docs/TELEMETRY_PROTOCOL.md)
- [Final fail-safe review](docs/SAFETY_REVIEW.md)

## Important safety note

Combat-robot software is not a substitute for a physical weapon power link, a restrained test fixture, and an accessible master disconnect. Remove weapon power before changing wiring or working near the shell.
