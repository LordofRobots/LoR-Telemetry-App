#include "WeaponDShot300.h"

#include "soc/gpio_struct.h"

static portMUX_TYPE dshotMux = portMUX_INITIALIZER_UNLOCKED;

static inline void IRAM_ATTR waitUntilCycle(uint32_t targetCycle) {
  while ((int32_t)(ESP.getCycleCount() - targetCycle) < 0) {
  }
}

bool WeaponDShot300::begin() {
  if (initialized_) return true;

  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, LOW);
  initialized_ = true;
  return true;
}

uint16_t WeaponDShot300::makePacket(uint16_t value) {
  uint16_t payload = (value & 0x07FF) << 1;  // Telemetry request bit remains zero.
  uint16_t checksum = (payload ^ (payload >> 4) ^ (payload >> 8)) & 0x0F;
  return (payload << 4) | checksum;
}

bool WeaponDShot300::send(uint16_t value) {
  if (!initialized_ || value > MAX_VALUE || pin_ >= 32) return false;

  uint16_t packet = makePacket(value);
  uint32_t gpioMask = 1UL << pin_;
  uint32_t cyclesPerUs = getCpuFrequencyMhz();
  uint32_t bitCycles = (cyclesPerUs * 10) / 3;  // 3.333 us
  uint32_t oneHighCycles = (cyclesPerUs * 5) / 2;  // 2.500 us
  uint32_t zeroHighCycles = (cyclesPerUs * 5) / 4; // 1.250 us

  portENTER_CRITICAL(&dshotMux);
  for (uint8_t i = 0; i < FRAME_BITS; i++) {
    bool one = packet & (1U << (15 - i));
    uint32_t bitStart = ESP.getCycleCount();
    GPIO.out_w1ts = gpioMask;
    waitUntilCycle(bitStart + (one ? oneHighCycles : zeroHighCycles));
    GPIO.out_w1tc = gpioMask;
    waitUntilCycle(bitStart + bitCycles);
  }
  portEXIT_CRITICAL(&dshotMux);

  return true;
}
