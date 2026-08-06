#pragma once

#include <Arduino.h>

class WeaponDShot300 {
 public:
  explicit WeaponDShot300(uint8_t pin) : pin_(pin) {}

  bool begin();
  bool send(uint16_t value);
  bool initialized() const { return initialized_; }

 private:
  static constexpr uint8_t FRAME_BITS = 16;
  static constexpr uint16_t MAX_VALUE = 2047;

  uint8_t pin_;
  bool initialized_ = false;

  static uint16_t makePacket(uint16_t value);
};
