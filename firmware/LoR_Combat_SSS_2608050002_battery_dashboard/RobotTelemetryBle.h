#pragma once

#include <Arduino.h>

// Fixed 20-byte packets so notifications work with the default BLE ATT MTU.
struct __attribute__((packed)) RobotTelemetryPacket {
  uint8_t version;
  uint8_t flags;
  uint8_t sequence;
  uint8_t loopHz;
  uint16_t batteryMillivolts;
  uint16_t adcMillivolts;
  int16_t driveLeft;
  int16_t driveRight;
  int16_t weaponCommand;
  uint16_t weaponDshot;
  uint16_t controllerAgeMs;
  uint8_t batteryPercent;
  uint8_t reserved;
};

static_assert(sizeof(RobotTelemetryPacket) == 20, "BLE telemetry packet must remain 20 bytes");

struct __attribute__((packed)) ControllerTelemetryPacket {
  uint8_t version;
  uint8_t sequence;
  uint8_t dpad;
  uint8_t battery;
  int16_t axisX;
  int16_t axisY;
  int16_t axisRX;
  int16_t axisRY;
  uint16_t throttle;
  uint16_t brake;
  uint16_t buttons;
  uint16_t miscButtons;
};

static_assert(sizeof(ControllerTelemetryPacket) == 20,
              "BLE controller packet must remain 20 bytes");

enum RobotTelemetryFlags : uint8_t {
  TELEMETRY_GAMEPAD_CONNECTED = 1u << 0,
  TELEMETRY_FAILSAFE_LATCHED = 1u << 1,
  TELEMETRY_DRIVE_ENABLED = 1u << 2,
  TELEMETRY_WEAPON_ENABLED = 1u << 3,
  TELEMETRY_PERSISTENT_FAULT = 1u << 4,
  TELEMETRY_WEAPON_REVERSE = 1u << 5,
  TELEMETRY_WEAPON_ARMING = 1u << 6,
  TELEMETRY_DISCONNECT_ALERT = 1u << 7,
};

// Call after BP32.setup(). The service is read/notify-only; phone writes are ignored.
void RobotTelemetryBleBegin();

// Copies one snapshot into a single overwrite buffer. This function never waits on BLE.
void RobotTelemetryBlePublish(const RobotTelemetryPacket& robotPacket,
                              const ControllerTelemetryPacket& controllerPacket,
                              bool gamepadConnected);

bool RobotTelemetryBlePhoneConnected();
