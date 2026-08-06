# System architecture

## Data and control paths

```text
Xbox gamepad --Bluetooth HID--> Bluepad32 / ESP32-S3
                                      |
                         100 Hz control and safety loop
                         /            |             \
                  drive PWM       DShot300       LED state machine
                  GPIO4/5          GPIO6          GPIO10/21
                                      |
GPIO8 divider --> filtered battery telemetry
                                      |
                         read/notify-only BLE service
                                      |
                            Android dashboard
```

The robot control loop owns all motor decisions. Phone telemetry runs through a separate BTstack timer and only copies compact state snapshots. There is no BLE command characteristic, so connecting a phone does not enter the motion-control path.

## Firmware scheduling

| Work | Nominal rate | Notes |
|---|---:|---|
| Control and safety loop | 100 Hz | Gamepad processing, output commands, watchdog service |
| Battery sampling | 10 Hz | GPIO8 millivolt reading and low-pass filtering |
| LED animation | 30 Hz | Non-blocking internal and external LED state machines |
| Serial diagnostics | 5 Hz | State, input, motor output, battery, and loop information |
| Each BLE characteristic, gamepad connected | 10 Hz | Robot and controller packets alternate every 50 ms |
| Each BLE characteristic, waiting | 2 Hz | Robot and controller packets alternate every 250 ms |

## Motion state sequence

1. Boot begins with both drive outputs detached and weapon DShot disabled.
2. A connected controller must produce at least one real HID report.
3. Drive, steering, and both weapon triggers must stay inside their neutral thresholds for 500 ms.
4. The firmware attaches the drive outputs and enables the weapon ESC with a DShot STOP arming dwell.
5. A controller disconnect, DShot initialization failure, or persistent reset fault enters the fail-safe state and clears all requested and ramped commands.
6. Reconnection repeats the first-report and neutral-hold sequence; previous commands are never restored.

## LED behavior

- Internal GPIO21 RGB LED follows weapon-output enable with inverted physical LED logic handled by the firmware.
- External four-pixel GPIO10 strip flashes red for three seconds after disconnect.
- While waiting for a gamepad, it shows a fast smooth blue/white animation.
- When connected with the weapon stopped, it shows a green bouncing orb with eased ends and a broad glow.
- With the weapon moving, it shows a two-color rainbow wash whose rate follows weapon speed. Reverse rotation adds a 2 Hz flash.

## Android dashboard

The Android app is a single native Java activity with custom views for the graph, robot, gamepad, and safety panel. It subscribes to two fixed 20-byte notifications, maintains the rolling battery history locally, and marks telemetry stale after 1.6 seconds without a robot packet. The interface is display-only and uses responsive weighted regions so the complete dashboard fits one screen on phones and tablets.
