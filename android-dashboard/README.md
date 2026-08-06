# LoR Combat Telemetry Dashboard

Native Android BLE dashboard for the telemetry-enabled LoR combat robot firmware.

The branded app scans for `LoR SSS Telemetry`, subscribes to its read-only robot
and controller streams, and displays:

- a rolling 30-second 3S HV battery-voltage graph;
- motor, shell, ESC, safety and connection state on the robot image;
- a compact six-tile safety/output panel with state-colored icons;
- both sticks, triggers, D-pad and all standard/misc buttons on the gamepad image;
- a wireless `LINKED` / `WAITING` gamepad connection badge;
- control-loop rate, packet health and raw controller values.

The portrait dashboard uses weighted visualization regions and capped width-based
scaling, so it fills both phone and tablet screens without scrolling or distorting
the robot and gamepad images.

The launcher uses a custom adaptive LoR Combat Telemetry icon built around the
Lord of Robots flame hexagon, spinning-shell ring, and wireless telemetry arcs.

Run `build.ps1` to produce `output\LoR-Telemetry-debug.apk` using the locally
installed Android SDK. No network dependencies or Gradle download are required.

The dashboard is display-only. It has no command characteristic and cannot arm,
enable, or control the robot. If robot telemetry stops for 1.6 seconds, the app
changes the connection and robot-status displays to red `TELEMETRY STALE`.

## Firmware safety behavior

- Drive PWM and weapon DShot start disabled.
- Any gamepad disconnect immediately commands stop, clears command/ramp state,
  detaches both drive outputs, and stops weapon DShot transmission after a STOP burst.
- After every connection, the controller must deliver a real input report and then
  remain neutral for 500 ms before outputs are enabled.
- A Bluepad32 disconnect callback immediately latches the same motion fail-safe.
  Xbox HID reports are event-driven, so unchanged held commands are not incorrectly
  treated as stale merely because their report age increases.
- The one-second task watchdog resets the MCU if the control loop stops running.
- After a watchdog, panic, brownout, or unknown reset, a persistent fault keeps all
  motion outputs disabled until a clean reset/power cycle.
