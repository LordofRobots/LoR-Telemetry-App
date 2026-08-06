# Hardware and wiring

## Controller board

The firmware targets a [Waveshare ESP32-S3-Zero](https://www.waveshare.com/wiki/ESP32-S3-Zero), compiled with the Bluepad32 ESP32-S3 board target.

| GPIO | Function | Signal |
|---:|---|---|
| 4 | Right drive controller | Servo-style PWM |
| 5 | Left drive controller | Servo-style PWM |
| 6 | LANRC 45 A BLHeli_S weapon ESC | DShot300, bidirectional/3D throttle mapping |
| 8 | Main battery monitor | ADC through voltage divider |
| 10 | External strip | Four WS2812B addressable LEDs |
| 21 | On-board status LED | RGB addressable LED |

Confirm the pinout against the exact board revision before energizing the robot. All controllers, the ESC signal ground, battery-monitor divider, and ESP32 must share a common signal ground.

## GPIO8 battery monitor

The current calibration is based on a measured **12.000 V input producing 2.621 V at GPIO8**, an effective conversion ratio of **4.5784:1**.

```text
Battery positive --- R1 ---+--- R2 --- Battery negative / ESP32 GND
                           |
                         GPIO8
                           |
                        0.1 uF
                           |
                          GND
```

- Choose R1 and R2 so a 13.05 V fully charged 3S HV LiPo stays safely below the ESP32-S3 ADC limit under component tolerance and transients.
- The firmware considers less than 6.0 V to mean the monitor or pack is absent.
- The displayed scale maps 9.90 V (3.30 V/cell) to 0% and 13.05 V (4.35 V/cell) to 100%.
- Place a 0.1 uF ceramic capacitor from GPIO8 to ground close to the ESP32-S3.
- Recalibrate `VIN_DIVIDER_RATIO` if resistor values, wiring, or the ADC board changes.

Battery percentage is a voltage-based estimate, not a fuel gauge. Voltage sag under weapon load can temporarily lower the estimate.

## Weapon ESC

The weapon signal is DShot300 on GPIO6. The mapping uses BLHeli 3D ranges: value 0 is stop, 48–1047 is reverse, and 1048–2047 is forward. The implementation is unidirectional, so ESC telemetry and positive stop confirmation are unavailable.

Use a physical weapon power link and remove it during programming, dashboard testing, and battery-monitor calibration.
