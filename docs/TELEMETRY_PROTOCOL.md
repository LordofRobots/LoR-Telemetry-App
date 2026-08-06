# LoR SSS BLE Telemetry Protocol v2

Service UUID: `8b7d0001-3f9b-4f6f-8d6a-11f6a3c80001`

Both characteristics are read/notify-only. The phone only writes their standard
notification-subscription descriptors. Each packet is fixed at 20 bytes. Robot
and controller packets each update at 10 Hz with a gamepad connected and 2 Hz
while waiting.

## Robot characteristic

UUID: `8b7d0002-3f9b-4f6f-8d6a-11f6a3c80001`

| Offset | Type | Field |
|---:|---|---|
| 0 | `uint8` | Protocol version (`2`) |
| 1 | `uint8` | State flags (unchanged from v1) |
| 2 | `uint8` | Sequence |
| 3 | `uint8` | Control-loop Hz |
| 4 | `uint16` | Main battery millivolts |
| 6 | `uint16` | GPIO8 ADC millivolts |
| 8 | `int16` | Left drive command |
| 10 | `int16` | Right drive command |
| 12 | `int16` | Weapon command |
| 14 | `uint16` | DShot value |
| 16 | `uint16` | Latest controller-data age in ms (`65535` disconnected) |
| 18 | `uint8` | Estimated 3S HV battery percentage |
| 19 | `uint8` | Reserved |

## Controller characteristic

UUID: `8b7d0003-3f9b-4f6f-8d6a-11f6a3c80001`

| Offset | Type | Field |
|---:|---|---|
| 0 | `uint8` | Controller packet version (`1`) |
| 1 | `uint8` | Sequence |
| 2 | `uint8` | D-pad bitfield |
| 3 | `uint8` | Controller battery (`0` unknown, `255` full) |
| 4 | `int16` | Left stick X |
| 6 | `int16` | Left stick Y |
| 8 | `int16` | Right stick X |
| 10 | `int16` | Right stick Y |
| 12 | `uint16` | Right trigger / throttle |
| 14 | `uint16` | Left trigger / brake |
| 16 | `uint16` | Standard button bitfield |
| 18 | `uint16` | Misc button bitfield |

## GPIO8 battery divider

The phone service is read-only and is not part of the control path. After each
gamepad connection, motion remains disabled until at least one real input report
has arrived and neutral controls have then been held for **500 ms**. Controller age
is diagnostic only because Xbox HID reports may be event-driven while held steady.

- Corrected live calibration: **12.000 V supply = 2.621 V ADC**.
- The measured effective divider conversion is **4.5784:1**.
- A fully charged 13.05 V 3S HV pack produces approximately 2.85 V at GPIO8.
- Battery ground and ESP32 ground must be common.
- Keep the **0.1 µF** ceramic capacitor from GPIO8 to ground close to the ESP32-S3.
