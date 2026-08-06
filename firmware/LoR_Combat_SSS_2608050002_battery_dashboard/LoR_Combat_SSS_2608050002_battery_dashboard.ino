//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Lord of Robots - Spinning Shell SSS combat robot - final release 2026-08-05
// Target: Waveshare ESP32-S3-Zero with Bluepad32 gamepad control and BLE telemetry

///////////////////////////////////////////////////////////////////////////////////////////
//            --- Environment Setup Prerequisites and Important Notes: ---               //
///////////////////////////////////////////////////////////////////////////////////////////
// 1. Add the following custom ULRs to the arduino IDE - File / Preferences / Additional Boards Manager URLs
//  - https://dl.espressif.com/dl/package_esp32_index.json
//  - https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json
// 2. Install the following boards in Board Manager:
//  - "esp32" by Espressif Systems
//  - "esp32_bluepad32" by Ricardo Quesada
// 3. Install the following libraries using the Library Manager:
//  - "FastLED" (Latest version) by Danial Garcia
//  - "ESP32Servo"(VERSION 3.0.7) by Kevin Harrington, John K. Bennett (Version 3.0.8 does not work due to standard ESP32 board update, ledc handling)
// 4. If needed, Install USB Driver: CH340. Check Device manager for connection in COM ports
//  - https://www.wch-ic.com/download/file?id=65
// 5. Select Target Board:
//  - USE "esp32_bluepad32 / ESP32S3 Dev Module"
//  - DO NOT use the non-Bluepad32 Espressif board core for this firmware
// 6. Select COM Port and Upload
//  - Process is automatic, Boot button is not reqired
//  - If auto load fail, Manually enter Boot mode: Press and hold Boot Button, press and release the RST button, then release Boot Button
// 7. Pair GamePad with LoR Core V3 (FIRST TIME ONLY)
//  - Turn on LoR Core V3
//  - Press and Hold User Button A + D. Press and release RST Button.
//  - When LEDs Flash BLUE and WHITE, Release User Button A + D.
//  - Set GamePad to Bluetooth Pair Mode.
//  - Wait for Pairing to Complete. Indicated with GREEN Flash and automatically returns to Normal mode.
//  - Will automatically connect on next power up or LoR Core V3 and GamePad.
//  - will remeber bluetooth keys between power cycles and uploads.
// 8. LED States
//  - Internal GREEN = Weapon ESC output disabled
//  - Internal RED = Weapon ESC output enabled
//  - External BLUE/WHITE wave = Waiting for a GamePad
//  - External flashing RED for 3 seconds = GamePad disconnected
//  - External two-colour rainbow wash = GamePad connected

////////////////////////////////////////////////////////////////////////////////////////////
//                            Libraries                                                   //
////////////////////////////////////////////////////////////////////////////////////////////
#include <Bluepad32.h>     // Gamepad Core
#include "esp_task_wdt.h"  // System stability
#include <ESP32Servo.h>    // Servo PWM Core
#include <FastLED.h>       // Addressable LED Core
#include "WeaponDShot300.h"  // DShot300 weapon ESC output
#include "RobotTelemetryBle.h"  // Read/notify-only phone telemetry over Bluepad32 BTstack
#include "esp_rom_sys.h"  // ESP32-S3 native USB reset reason

// IO Interface Definitions



////////////////////////////////////////////////////////////////////////////////////////////
//                            AUX Port Config                                             //
////////////////////////////////////////////////////////////////////////////////////////////
// --- AUX_Port Pins ---
const uint8_t AUX_PINS[9] = { 0, 5, 18, 23, 19, 22, 21, 1, 3 };  // AUX_PIN[slot_number]note: slot 0 is imaginary

////////////////////////////////////////////////////////////////////////////////////////////
//                            IO Port Config                                              //
////////////////////////////////////////////////////////////////////////////////////////////
// --- IO_Port Pins ---
const uint8_t IO_PINS[13] = { 0, 4, 5, 6, 22, 14, 12, 13, 15, 2, 4, 22, 21 };  // IO_PIN[slot_number]   note: slot 0 is imaginary

////////////////////////////////////////////////////////////////////////////////////////////
//                         User Button and Switch Config                                  //
////////////////////////////////////////////////////////////////////////////////////////////
// --- User Inputs ---
#define User_BTN_A 35
#define User_BTN_B 39
#define User_BTN_C 38
#define User_BTN_D 37
#define User_SW 36

////////////////////////////////////////////////////////////////////////////////////////////
//                      Input Voltage Monitor Config                                      //
////////////////////////////////////////////////////////////////////////////////////////////
// --- Input Voltage Sensor Input ---
#define VIN_SENSE 8

// Corrected live calibration on 2026-08-05: 12.000V supply produced 2.621V ADC.
// Effective divider conversion is 12.000 / 2.621 = 4.5784.
constexpr float VIN_DIVIDER_RATIO = 4.5784f;
constexpr float VIN_CALIBRATION = 1.00000f;
constexpr float BATTERY_EMPTY_VOLTS = 9.90f;   // 3.30V/cell
constexpr float BATTERY_FULL_VOLTS = 13.05f;   // 4.35V/cell HV charge
constexpr float BATTERY_PRESENT_VOLTS = 6.0f;  // Reject a floating/unwired ADC as no pack.
constexpr uint32_t BATTERY_SAMPLE_PERIOD_MS = 100;

////////////////////////////////////////////////////////////////////////////////////////////
//                            LED Config                                                  //
////////////////////////////////////////////////////////////////////////////////////////////
// --- Addressable LED Data Outputs ---
#define LED_PIN 10           // External 4-LED strip
#define INTERNAL_LED_PIN 21  // Internal RGB status LED

#define LED_COUNT 4
#define BRIGHTNESS 255
#define COLOR_ORDER GRB
#define CHIPSET WS2812B
CRGB leds[LED_COUNT];
CRGB internalLed[1];
uint8_t rainbowHue = 0;
int16_t GetWeaponCommandDeg();  // Shared with the LED and debug state machines

constexpr uint32_t LED_UPDATE_PERIOD_MS = 33;  // Approximately 30 Hz
constexpr uint32_t DISCONNECT_ALERT_MS = 3000;
constexpr float EXTERNAL_LED_SPACING_IN = 0.625f;
constexpr uint8_t WAITING_PHASE_PER_LED = 42;  // Smooth wave across the 1.875-inch LED span
constexpr uint32_t DEBUG_UPDATE_PERIOD_MS = 200;  // 5 Hz telemetry
constexpr uint32_t WEAPON_ESC_ARM_MS = 3000;
constexpr uint16_t WEAPON_DSHOT_STOP = 0;
constexpr uint32_t NEUTRAL_RELEASE_HOLD_MS = 500;
constexpr uint16_t DRIVE_NEUTRAL_US = 1525;  // Existing slot midpoint: 1050..2000 us
constexpr int16_t DRIVE_MAX_OUTPUT_DEG = 60;  // 75% of the previous +/-80 degree drive range
constexpr int16_t DRIVE_MIN_COMMAND_DEG = 90 - DRIVE_MAX_OUTPUT_DEG;
constexpr int16_t DRIVE_MAX_COMMAND_DEG = 90 + DRIVE_MAX_OUTPUT_DEG;
constexpr uint8_t WEAPON_STOP_BURST_FRAMES = 10;
static uint32_t lastLedUpdateMs = 0;
static uint32_t disconnectAlertStartMs = 0;
static bool disconnectAlertActive = false;
static bool persistentSystemFault = false;
static uint8_t waitingPhase = 0;
static uint32_t loopCounter = 0;
static uint32_t gamepadUpdateCounter = 0;
static uint32_t lastGamepadDataMs = 0;
static uint32_t lastBatterySampleMs = 0;
static uint16_t batteryAdcMillivolts = 0;
static uint16_t batteryMillivolts = 0;
static uint8_t batteryPercent = 0;
static float filteredBatteryVolts = 0.0f;
static bool weaponEscEnabled = false;
static bool weaponDshotInitialized = false;
static uint32_t weaponEscEnabledAtMs = 0;
static uint16_t weaponDshotValue = WEAPON_DSHOT_STOP;
static bool driveOutputsEnabled = false;
static bool motionFailsafeLatched = true;
static uint32_t neutralControlsSinceMs = 0;

WeaponDShot300 WeaponESC(6);

////////////////////////////////////////////////////////////////////////////////////////////
//                            Initalize Interal features                                  //
////////////////////////////////////////////////////////////////////////////////////////////
#define WDT_TIMEOUT 1   // Runtime watchdog: reset within about one second
Servo MotorOutput[13];  // example: servos[slot_number].write(90);

void INIT_InternalFeatures() {
  // The Arduino core enrolls this task in its watchdog before setup().
  // Bluepad32 initialization can legitimately exceed that timeout.
  esp_task_wdt_delete(NULL);

  // --- Inputs ---
  // pinMode(User_BTN_A, INPUT);
  // pinMode(User_BTN_B, INPUT);
  // pinMode(User_BTN_C, INPUT);
  // pinMode(User_BTN_D, INPUT);
  // pinMode(User_SW, INPUT);
  pinMode(VIN_SENSE, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(VIN_SENSE, ADC_11db);

  // --- FastLED Setup ---
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, LED_COUNT);
  // The Waveshare onboard XL-0807 RGB LED uses RGB byte order; the external strip uses GRB.
  FastLED.addLeds<CHIPSET, INTERNAL_LED_PIN, RGB>(internalLed, 1);
  FastLED.setBrightness(255);  // Maximum brightness
  FastLED.clear();
  FastLED.show();
}

void INIT_Watchdog() {
  // Start runtime protection only after the potentially slow Bluetooth setup.
  esp_task_wdt_init(WDT_TIMEOUT, true);  // Enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);
  esp_task_wdt_reset();
}




////////////////////////////////////////////////////////////////////////////////////////////
//                            bluepad 32 Config                                           //
////////////////////////////////////////////////////////////////////////////////////////////
ControllerPtr myController = nullptr;  // Define a single controller pointer

// Immediately neutralize all active outputs and clear motion state.
void StopAllMotionAndResetState();
bool EnableWeaponESC();
void DisableWeaponESC();
bool EnableDriveOutputs();
void DisableDriveOutputs();
void EnterMotionFailsafe(const char* reason, bool persistentFault = false);
void UpdateMotionSafetyState();
void UpdateWirelessTelemetry();

// --- Gamepad Connected ---
void onConnectedController(ControllerPtr ctl) {
  if (myController == nullptr) {  // check if not already connected to a GamePad
    Serial.println("! GamePad connected !");
    myController = ctl;  //establish pointer
    lastGamepadDataMs = 0;  // Require a real input report before enabling motion.
    neutralControlsSinceMs = 0;
    Serial.println("Motion remains disabled until controls are neutral for 500ms");

    ctl->playDualRumble(0x00, 0xc0, 0xc0, 0xc0);  // give a lil shake
    ctl->setColorLED(0, 255, 0);                  //set GamePad LED colour

    BP32.enableNewBluetoothConnections(false);  // Disable Pairing
    disconnectAlertActive = false;

  } else {
    Serial.println("Another controller tried to connect but is rejected");
  }
}

// --- Gamepad Disconnected ---
void onDisconnectedController(ControllerPtr ctl) {
  if (myController == ctl) {
    Serial.println("! GamePad disconnected !");
    myController = nullptr;  // Reset the controller pointer when disconnected
    lastGamepadDataMs = 0;

    EnterMotionFailsafe("GamePad disconnected");
    disconnectAlertStartMs = millis();
    disconnectAlertActive = true;

    // Re-open Bluetooth connections so the paired gamepad can reconnect.
    // onConnectedController() closes this again after a controller connects.
    BP32.enableNewBluetoothConnections(true);
    Serial.println("Bluetooth connections re-enabled; waiting for GamePad");
  }
}

// --- Parallel, non-blocking LED state machine (approximately 30 Hz) ---
void UpdateLEDStateMachine() {
  uint32_t now = millis();
  if ((uint32_t)(now - lastLedUpdateMs) < LED_UPDATE_PERIOD_MS) return;
  lastLedUpdateMs = now;

  bool connected = myController && myController->isConnected();
  bool disconnectAlert = false;

  if (disconnectAlertActive) {
    uint32_t alertElapsed = (uint32_t)(now - disconnectAlertStartMs);
    if (alertElapsed < DISCONNECT_ALERT_MS) {
      disconnectAlert = true;
    } else {
      disconnectAlertActive = false;
    }
  }

  // Internal LED directly follows the weapon ESC output state.
  // Green = weapon output disabled; red = weapon output enabled.
  internalLed[0] = weaponEscEnabled ? CRGB::Red : CRGB::Green;

  if (connected) {
    uint16_t weaponSpeed = abs(GetWeaponCommandDeg());

    if (weaponSpeed <= 2) {
      // Sinusoidal motion naturally accelerates away from each end and
      // decelerates as the green orb approaches the opposite end.
      constexpr int16_t ORB_OVERSHOOT = 256;
      constexpr uint16_t ORB_GLOW_RADIUS = 384;
      uint16_t orbTravel = (LED_COUNT - 1) * 256 + (2 * ORB_OVERSHOOT);
      int32_t orbPosition = (int32_t)beatsin16(45, 0, orbTravel) - ORB_OVERSHOOT;
      for (uint8_t i = 0; i < LED_COUNT; i++) {
        int32_t pixelPosition = i * 256;
        uint16_t distance = abs(pixelPosition - orbPosition);
        uint8_t orbBrightness = (distance >= ORB_GLOW_RADIUS)
                                  ? 0
                                  : 255 - ((uint32_t)distance * 255 / ORB_GLOW_RADIUS);
        leds[i] = CRGB(0, orbBrightness, 0);
      }
    } else {
      // Smooth linear rainbow wash using two source hues at a time.
      CRGB colorA = CHSV(rainbowHue, 255, 255);
      CRGB colorB = CHSV(rainbowHue + 32, 255, 255);
      for (uint8_t i = 0; i < LED_COUNT; i++) {
        uint8_t position = (uint16_t)i * 255 / (LED_COUNT - 1);
        leds[i] = blend(colorA, colorB, position);
      }

      // Idle weapon speed (25) keeps the current animation rate. The wash
      // progressively accelerates to three times the previous high-end rate.
      uint8_t rainbowStep = 2;
      if (weaponSpeed > 25) {
        rainbowStep += ((weaponSpeed - 25) * 28) / 65;
      }
      rainbowHue += constrain(rainbowStep, 2, 30);

      // Reverse weapon direction flashes the moving rainbow at 2 Hz.
      if (GetWeaponCommandDeg() < -2 && ((now / 250) % 2 != 0)) {
        fill_solid(leds, LED_COUNT, CRGB::Black);
      }
    }
  } else if (disconnectAlert) {
    // Fast red flash for three seconds after a fresh disconnect.
    bool flashOn = ((uint32_t)(now - disconnectAlertStartMs) / 150) % 2 == 0;
    fill_solid(leds, LED_COUNT, flashOn ? CRGB::Red : CRGB::Black);
  } else {
    // Fast, smooth blue/white wave while waiting for a gamepad.
    for (uint8_t i = 0; i < LED_COUNT; i++) {
      uint8_t blendAmount = sin8(waitingPhase - (i * WAITING_PHASE_PER_LED));
      leds[i] = blend(CRGB::Blue, CRGB::White, blendAmount);
    }
    waitingPhase += 10;
  }

  FastLED.show();
}

// --- Initalize and Pair mode --- //
void INIT_BluetoothGamepad_PairMode() {  // Setup the Bluepad32 callbacks

  //Pair Mode Handling
  if (!digitalRead(User_BTN_A) && !digitalRead(User_BTN_D)) {
    BP32.forgetBluetoothKeys();  // Forgetting Bluetooth keys resets to "factory" and prevents "paired" gamepads to reconnect automatically.
    Serial.println("Gamepad Unpaird!");
    BP32.enableNewBluetoothConnections(true);                       // Allow Game pads to pair
    BP32.setup(&onConnectedController, &onDisconnectedController);  // start bluetooth Gamepage functions
  } else BP32.setup(&onConnectedController, &onDisconnectedController);  // start bluetooth Gamepage functions

  BP32.enableVirtualDevice(false);  // Stop Virtual devices from pairing
}


////////////////////////////////////////////////////////////////////////////////////////////
//                            Input voltage / Battery monitor                             //
////////////////////////////////////////////////////////////////////////////////////////////
void UpdateBatteryMonitor() {
  uint32_t now = millis();
  if ((uint32_t)(now - lastBatterySampleMs) < BATTERY_SAMPLE_PERIOD_MS) return;
  lastBatterySampleMs = now;

  // analogReadMilliVolts uses the ESP32-S3 ADC calibration supplied by the Arduino core.
  uint16_t adcMv = (uint16_t)analogReadMilliVolts(VIN_SENSE);
  float packVolts = ((float)adcMv / 1000.0f) * VIN_DIVIDER_RATIO * VIN_CALIBRATION;

  // A disconnected divider reads near zero. Start the filter immediately when a pack appears.
  if (filteredBatteryVolts < 1.0f || packVolts < 1.0f) {
    filteredBatteryVolts = packVolts;
  } else {
    filteredBatteryVolts += 0.18f * (packVolts - filteredBatteryVolts);
  }

  batteryAdcMillivolts = adcMv;
  bool batteryPresent = filteredBatteryVolts >= BATTERY_PRESENT_VOLTS;
  batteryMillivolts = batteryPresent
                       ? (uint16_t)constrain(lroundf(filteredBatteryVolts * 1000.0f), 0L, 65535L)
                       : 0;
  float percent = 100.0f * (filteredBatteryVolts - BATTERY_EMPTY_VOLTS) /
                  (BATTERY_FULL_VOLTS - BATTERY_EMPTY_VOLTS);
  batteryPercent = !batteryPresent
                     ? 0
                     : (uint8_t)constrain(lroundf(percent), 0L, 100L);
}

////////////////////////////////////////////////////////////////////////////////////////////
//                            Power up Diagnostics                                        //
////////////////////////////////////////////////////////////////////////////////////////////
void Powerup_Diagnostics_LED() {  // System check on boot cause and report
  esp_reset_reason_t reason = esp_reset_reason();
  soc_reset_reason_t romReason = esp_rom_get_reset_reason(0);
  Serial.printf("BOOT Condition (%d, ROM 0x%02x): ",
                static_cast<int>(reason), static_cast<unsigned>(romReason));

  switch (reason) {
    case ESP_RST_TASK_WDT:
    case ESP_RST_INT_WDT:
    case ESP_RST_WDT:
      Serial.println("Watchdog Reset Detected");
      persistentSystemFault = true;
      break;
    case ESP_RST_BROWNOUT:
      Serial.println("Brownout Reset Detected");
      persistentSystemFault = true;
      break;
    case ESP_RST_PANIC:
      Serial.println("Panic Reset Detected");
      persistentSystemFault = true;
      break;
    case ESP_RST_POWERON:
      Serial.println("Power-on Reset Detected");
      persistentSystemFault = false;
      break;
    case ESP_RST_SW:
      Serial.println("Software Reset Detected");
      persistentSystemFault = false;
      break;
    case ESP_RST_EXT:
      Serial.println("External/USB Reset Detected");
      persistentSystemFault = false;
      break;
    case ESP_RST_DEEPSLEEP:
      Serial.println("Deep-sleep Wake Detected");
      persistentSystemFault = false;
      break;
    case ESP_RST_SDIO:
      Serial.println("SDIO Reset Detected");
      persistentSystemFault = false;
      break;
    default:
      if (reason == ESP_RST_UNKNOWN &&
          (romReason == RESET_REASON_CORE_USB_UART ||
           romReason == RESET_REASON_CORE_USB_JTAG)) {
        // IDF 4.4 maps the ESP32-S3 native USB reset to ESP_RST_UNKNOWN.
        Serial.println("Native USB Reset Detected");
        persistentSystemFault = false;
      } else {
        Serial.println("Unknown Reset Detected");
        persistentSystemFault = true;
      }
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////
//                               Servo/ Motor Config                                      //
////////////////////////////////////////////////////////////////////////////////////////////

//Names of Predefined Motor types
enum MotorType {
  MG90_CR,
  MG90_Degree,
  N20Plus,
  STD_SERVO,
  Victor_SPX,
  Talon_SRX,
  ESC,
  DualBrushed_Driver,
  CUSTOM
};

struct MotorTypeConfig {  // define the config structure
  MotorType type;
  float pwmFreq;
  int minPulseUs;
  int maxPulseUs;
  float inputMin;
  float inputMax;
};

MotorTypeConfig motorTypeConfigs[] = {
  // define specific motor config parameters {NAME, FREQ, MINms, MAXms}
  { MG90_CR, 50, 500, 2500, -1, 1 },
  { MG90_Degree, 50, 500, 2500, 1, 180 },
  { N20Plus, 50, 1000, 2000, -1, 1 },
  { Victor_SPX, 50, 1000, 2000, -1, 1 },
  { Talon_SRX, 50, 1000, 2000, -1, 1 },
  { STD_SERVO, 50, 1000, 2000 },
  { ESC, 50, 1000, 2000 },
  { DualBrushed_Driver, 50, 1050, 2000, -1, 1 },

  // add more types as needed
};

//--- Configure motors function --- //
void ConfigureMotorOutput(uint8_t slot, MotorType motorType, int startupPositionDeg = 90) {
  // Lookup the config
  float pwmFreq = 50;
  int minPulseUs = 1000;
  int maxPulseUs = 2000;

  for (auto &cfg : motorTypeConfigs) {
    if (cfg.type == motorType) {
      pwmFreq = cfg.pwmFreq;
      minPulseUs = cfg.minPulseUs;
      maxPulseUs = cfg.maxPulseUs;
      break;
    }
  }

  uint8_t pin = IO_PINS[slot];
  pinMode(pin, OUTPUT);

  MotorOutput[slot].setPeriodHertz(pwmFreq);
  MotorOutput[slot].attach(pin, minPulseUs, maxPulseUs);
  MotorOutput[slot].write(startupPositionDeg);

  Serial.printf(
    "Motor slot %d configured on pin %d as type %d: freq=%.1f Hz, pulse=%d-%d us, start=%d deg\n",
    slot, pin, motorType, pwmFreq, minPulseUs, maxPulseUs, startupPositionDeg);
}

bool EnableDriveOutputs() {
  if (driveOutputsEnabled) return true;

  ConfigureMotorOutput(1, DualBrushed_Driver, 90);
  ConfigureMotorOutput(2, DualBrushed_Driver, 90);
  if (!MotorOutput[1].attached() || !MotorOutput[2].attached()) {
    persistentSystemFault = true;
    Serial.println("ERROR: Drive PWM failed to attach");
    DisableDriveOutputs();
    return false;
  }

  MotorOutput[1].writeMicroseconds(DRIVE_NEUTRAL_US);
  MotorOutput[2].writeMicroseconds(DRIVE_NEUTRAL_US);
  driveOutputsEnabled = true;
  Serial.println("Drive outputs enabled at verified neutral midpoint");
  return true;
}

void DisableDriveOutputs() {
  if (MotorOutput[1].attached()) {
    MotorOutput[1].writeMicroseconds(DRIVE_NEUTRAL_US);
    MotorOutput[1].detach();
  }
  if (MotorOutput[2].attached()) {
    MotorOutput[2].writeMicroseconds(DRIVE_NEUTRAL_US);
    MotorOutput[2].detach();
  }

  pinMode(IO_PINS[1], INPUT);
  pinMode(IO_PINS[2], INPUT);
  driveOutputsEnabled = false;
}

bool EnableWeaponESC() {
  if (weaponEscEnabled) return true;

  if (!weaponDshotInitialized) {
    if (!WeaponESC.begin()) {
      persistentSystemFault = true;
      Serial.println("ERROR: Weapon ESC DShot300 initialization failed");
      return false;
    }
    weaponDshotInitialized = true;
  }

  weaponDshotValue = WEAPON_DSHOT_STOP;
  WeaponESC.send(WEAPON_DSHOT_STOP);
  weaponEscEnabled = true;
  weaponEscEnabledAtMs = millis();
  Serial.println("Weapon ESC enabled; sending DShot300 STOP for 3 seconds");
  return true;
}

void DisableWeaponESC() {
  if (!weaponEscEnabled) return;

  weaponDshotValue = WEAPON_DSHOT_STOP;
  for (uint8_t i = 0; i < WEAPON_STOP_BURST_FRAMES; i++) {
    WeaponESC.send(WEAPON_DSHOT_STOP);
    delayMicroseconds(100);
  }
  weaponEscEnabled = false;
  weaponEscEnabledAtMs = 0;
  Serial.println("Weapon ESC disabled after DShot STOP burst");
}


////////////////////////////////////////////////////////////////////////////////////////////
//                            Initialize LoR Core                                         //
////////////////////////////////////////////////////////////////////////////////////////////

// Initalizes core features of the LoRcore
void INIT_LoRcore() {
  INIT_InternalFeatures();
  Powerup_Diagnostics_LED();
  INIT_BluetoothGamepad_PairMode();
  // Must be initialized after BP32.setup() so it shares Bluepad32's BTstack.
  RobotTelemetryBleBegin();
  INIT_Watchdog();
}

////////////////////////////////////////////////////////////////////////////////////////////
//                            Weapon system functions                                        //
////////////////////////////////////////////////////////////////////////////////////////////

// --- Weapon slew/dwell state ---
static int16_t  WeaponCmd_deg = 0;            // centered: -90..+90, where 0 => 90° output
static uint32_t weapon_lastMs = 0;
static uint32_t dwellUntil_Weapon = 0;
static float WeaponCmd_precise = 0.0f;  // retains sub-degree progress at 100 Hz

int16_t GetWeaponCommandDeg() {
  return WeaponCmd_deg;
}

// Tuning
constexpr uint16_t WEAPON_REV_DWELL_MS     = 200;   // pause at 90° during reversals
constexpr int16_t  WEAPON_REV_TGT_MIN_DEG  = 5;     // ignore tiny targets for dwell
constexpr float    WEAPON_SLEW_UP_DEG_PER_S   = 45.0f;   // accel rate (toward target)
constexpr float    WEAPON_SLEW_DOWN_DEG_PER_S = 90.0f;   // brake rate (toward 0)

static inline float rampTowardFloat(float current, float target, float maxStep) {
  if (target > current) return min(current + maxStep, target);
  if (target < current) return max(current - maxStep, target);
  return current;
}

static inline float updateWeaponWithDwell(
    float current, float target,
    float stepUp, float stepDown,
    uint32_t& dwellUntil, uint32_t now) {

  if (dwellUntil && now < dwellUntil) return 0;        // hold at zero (center)
  if (dwellUntil && now >= dwellUntil) dwellUntil = 0;

  int sc = (current > 0.0f) - (current < 0.0f);
  int st = (target > WEAPON_REV_TGT_MIN_DEG) - (target < -WEAPON_REV_TGT_MIN_DEG);

  if (sc != 0 && st != 0 && sc != st) {
    // Reversal: brake to zero first
    float x = rampTowardFloat(current, 0.0f, stepDown);
    if (x == 0.0f) dwellUntil = now + WEAPON_REV_DWELL_MS;
    return x;
  }

  float step = (fabsf(target) >= fabsf(current)) ? stepUp : stepDown;
  return rampTowardFloat(current, target, step);
}
// Weapon mode state machine
int Weapon_Mode = 0;
void Weapon_StateMachine() {
  if (motionFailsafeLatched || !weaponEscEnabled) return;

  // Send repeated digital STOP frames while the brushless ESC initializes.
  if ((uint32_t)(millis() - weaponEscEnabledAtMs) < WEAPON_ESC_ARM_MS) {
    WeaponCmd_precise = 0.0f;
    WeaponCmd_deg = 0;
    dwellUntil_Weapon = 0;
    weapon_lastMs = millis();
    weaponDshotValue = WEAPON_DSHOT_STOP;
    WeaponESC.send(WEAPON_DSHOT_STOP);
    return;
  }

  // dt
  if (weapon_lastMs == 0) weapon_lastMs = millis();
  uint32_t now = millis();
  float dt = (now - weapon_lastMs) * 0.001f;
  weapon_lastMs = now;
  if (dt < 0) dt = 0;
  if (dt > 0.100f) dt = 0.100f;  // cap long gaps

  // Inputs (clamp diff before mapping to avoid overshoot)
  int raw = myController->throttle() - myController->brake();   // range may be -1024..+1024
  raw = constrain(raw, -1024, 1024);
  int Weapon_Throttle = map(raw, -1024, 1024, -90, 90);         // full trigger range, centered

  // Mode -> idle bias (centered)
  int Weapon_Idle = 0;
  if      (myController->b()) Weapon_Mode = 0;
  else if (myController->a()) Weapon_Mode = 1;
  else if (myController->x()) Weapon_Mode = 2;

  switch (Weapon_Mode) {
    case 1:  Weapon_Idle =  25; break;  // A: forward idle
    case 2:  Weapon_Idle = -25; break;  // X: reverse idle
    default: Weapon_Idle =   0;  break;
  }

  // Target (centered), clamp to ±90 so output stays 0..180
  int16_t targetCentered = constrain(Weapon_Idle + Weapon_Throttle, -90, 90);

  // Steps this frame
  float stepUp   = WEAPON_SLEW_UP_DEG_PER_S   * dt;
  float stepDown = WEAPON_SLEW_DOWN_DEG_PER_S * dt;

  // Slew with 100 ms dwell at 0 during reversals
  WeaponCmd_precise = updateWeaponWithDwell(WeaponCmd_precise, targetCentered,
                                            stepUp, stepDown,
                                            dwellUntil_Weapon, now);
  WeaponCmd_deg = (int16_t)roundf(WeaponCmd_precise);

  // BLHeli 3D DShot ranges:
  //   0 = stop, 48..1047 = reverse, 1048..2047 = forward.
  if (WeaponCmd_deg < 0) {
    weaponDshotValue = map(abs(WeaponCmd_deg), 1, 90, 48, 1047);
    WeaponESC.send(weaponDshotValue);
  } else if (WeaponCmd_deg > 0) {
    weaponDshotValue = map(WeaponCmd_deg, 1, 90, 1048, 2047);
    WeaponESC.send(weaponDshotValue);
  } else {
    weaponDshotValue = WEAPON_DSHOT_STOP;
    WeaponESC.send(WEAPON_DSHOT_STOP);
  }
}


////////////////////////////////////////////////////////////////////////////////////////////
//                            Drive system functions                                        //
////////////////////////////////////////////////////////////////////////////////////////////
// Existing (from earlier)
static int16_t MotorOutput_Left  = 0;
static int16_t MotorOutput_Right = 0;
static uint32_t drive_lastMs = 0;

// New: dwell timers per side
static uint32_t dwellUntil_Left  = 0;
static uint32_t dwellUntil_Right = 0;

// Tuning
constexpr uint16_t REV_DWELL_MS   = 100;  // pause at zero on reversals
constexpr int16_t  REV_TGT_MIN    = 80;   // ignore tiny “targets” to avoid flicker
constexpr float    SLEW_UP_UNITS_PER_S   = 3500.0f;
constexpr float    SLEW_DOWN_UNITS_PER_S = 7000.0f;


static inline int16_t rampToward(int16_t current, int16_t target, float maxStep) {
  if (target > current) {
    float next = current + maxStep;
    return (next > target) ? target : (int16_t)next;
  } else if (target < current) {
    float next = current - maxStep;
    return (next < target) ? target : (int16_t)next;
  }
  return current;
}

// sign with threshold
static inline int8_t sgn_thresh(int16_t x, int16_t thr) {
  return (x > thr) - (x < -thr);
}

// One side update with 0-dwell on reversal
static inline int16_t updateSideWithDwell(
    int16_t current, int16_t target,
    float stepUp, float stepDown,
    uint32_t& dwellUntil, uint32_t now) {

  // If currently dwelling at zero, hold until timer expires
  if (dwellUntil && now < dwellUntil)
    return 0;
  if (dwellUntil && now >= dwellUntil)
    dwellUntil = 0;  // dwell finished

  // Decide if this is a real reversal (non-trivial target of opposite sign)
  int sc = sgn_thresh(current, 0);           // current sign
  int st = sgn_thresh(target,  REV_TGT_MIN); // target sign (ignore tiny)

  if (sc != 0 && st != 0 && sc != st) {
    // Reversing: brake to zero first
    int16_t x = rampToward(current, 0, stepDown);
    if (x == 0) {
      // Start dwell
      dwellUntil = now + REV_DWELL_MS;
    }
    return x;
  }

  // Same sign (or one is near zero): normal slew toward target
  float step = (abs(target) >= abs(current)) ? stepUp : stepDown;
  return rampToward(current, target, step);
}

void Drive() {
  if (motionFailsafeLatched || !driveOutputsEnabled) return;

  if (drive_lastMs == 0) drive_lastMs = millis();
  uint32_t now = millis();
  float dt = (now - drive_lastMs) * 0.001f;
  drive_lastMs = now;
  if (dt < 0) dt = 0;
  if (dt > 0.100f) dt = 0.100f;  // cap dt

  // Inputs
  int Forward_Backward = myController->axisY();
  int Turn             = -myController->axisRX() / 2;  // 50% steering rate
  if (abs(Forward_Backward) < 60) Forward_Backward = 0;
  if (abs(Turn) < 60) Turn = 0;

  // Targets
  int TargetLeft  = constrain(Forward_Backward - Turn, -512, 512);
  int TargetRight = constrain(Forward_Backward + Turn, -512, 512);

  // Per-frame steps
  float stepUp   = SLEW_UP_UNITS_PER_S   * dt;
  float stepDown = SLEW_DOWN_UNITS_PER_S * dt;

  // Update with dwell at zero on reversals
  MotorOutput_Left  = updateSideWithDwell(MotorOutput_Left,  TargetLeft,  stepUp, stepDown, dwellUntil_Left,  now);
  MotorOutput_Right = updateSideWithDwell(MotorOutput_Right, TargetRight, stepUp, stepDown, dwellUntil_Right, now);

  // Map to servo range
  int MappedLeft  = map(MotorOutput_Left,  -512, 512, DRIVE_MIN_COMMAND_DEG, DRIVE_MAX_COMMAND_DEG);
  int MappedRight = map(MotorOutput_Right, -512, 512, DRIVE_MIN_COMMAND_DEG, DRIVE_MAX_COMMAND_DEG);

  MotorOutput[1].write(MappedRight);
  MotorOutput[2].write(MappedLeft);
}

void StopAllMotionAndResetState() {
  // Neutralize every configured motor output immediately.
  if (MotorOutput[1].attached()) MotorOutput[1].writeMicroseconds(DRIVE_NEUTRAL_US);
  if (MotorOutput[2].attached()) MotorOutput[2].writeMicroseconds(DRIVE_NEUTRAL_US);
  if (weaponEscEnabled) {
    weaponDshotValue = WEAPON_DSHOT_STOP;
    WeaponESC.send(WEAPON_DSHOT_STOP);
  }

  // Keep the software ramp state synchronized with the physical outputs.
  MotorOutput_Left = 0;
  MotorOutput_Right = 0;
  dwellUntil_Left = 0;
  dwellUntil_Right = 0;
  drive_lastMs = 0;

  WeaponCmd_deg = 0;
  WeaponCmd_precise = 0.0f;
  Weapon_Mode = 0;
  dwellUntil_Weapon = 0;
  weapon_lastMs = 0;
}

bool GamepadMotionControlsNeutral() {
  if (!(myController && myController->isConnected())) return false;

  return abs(myController->axisY()) <= 60 &&
         abs(myController->axisRX()) <= 60 &&
         myController->throttle() <= 30 &&
         myController->brake() <= 30 &&
         !myController->a() &&
         !myController->x();
}

void EnterMotionFailsafe(const char* reason, bool persistentFault) {
  bool wasLatchedAndDisabled = motionFailsafeLatched &&
                               !driveOutputsEnabled &&
                               !weaponEscEnabled;

  motionFailsafeLatched = true;
  neutralControlsSinceMs = 0;
  if (persistentFault) persistentSystemFault = true;

  StopAllMotionAndResetState();
  DisableDriveOutputs();
  DisableWeaponESC();

  if (!wasLatchedAndDisabled) {
    Serial.printf("MOTION FAILSAFE LATCHED: %s\n", reason);
  }
}

void UpdateMotionSafetyState() {
  bool connected = myController && myController->isConnected();

  if (!connected) {
    EnterMotionFailsafe("No connected GamePad");
    return;
  }

  // Bluepad32 controllers can be event-driven and may not repeat an unchanged
  // command. Require one genuine input report after each connection, but do not
  // treat the age of a held command as a transport timeout.
  if (lastGamepadDataMs == 0) {
    EnterMotionFailsafe("Waiting for first controller report");
    return;
  }

  if (persistentSystemFault) {
    EnterMotionFailsafe("Persistent system fault", true);
    return;
  }

  if (!motionFailsafeLatched) return;

  if (!GamepadMotionControlsNeutral()) {
    neutralControlsSinceMs = 0;
    return;
  }

  uint32_t now = millis();
  if (neutralControlsSinceMs == 0) {
    neutralControlsSinceMs = now;
    Serial.println("Neutral controls detected; holding for 500ms");
    return;
  }

  if ((uint32_t)(now - neutralControlsSinceMs) < NEUTRAL_RELEASE_HOLD_MS) return;

  if (!EnableDriveOutputs()) {
    EnterMotionFailsafe("Drive output enable failed", true);
    return;
  }
  if (!EnableWeaponESC()) {
    EnterMotionFailsafe("Weapon DShot enable failed", true);
    return;
  }

  motionFailsafeLatched = false;
  neutralControlsSinceMs = 0;
  Serial.println("MOTION FAILSAFE CLEARED: outputs enabled");
}

void PrintDebugTelemetry() {
  static uint32_t lastDebugMs = 0;
  static uint32_t lastLoopCount = 0;
  static uint32_t lastGamepadUpdateCount = 0;

  uint32_t now = millis();
  uint32_t elapsed = (uint32_t)(now - lastDebugMs);
  if (elapsed < DEBUG_UPDATE_PERIOD_MS) return;

  uint32_t loopHz = ((loopCounter - lastLoopCount) * 1000UL) / elapsed;
  uint32_t gamepadHz = ((gamepadUpdateCounter - lastGamepadUpdateCount) * 1000UL) / elapsed;
  lastDebugMs = now;
  lastLoopCount = loopCounter;
  lastGamepadUpdateCount = gamepadUpdateCounter;

  bool connected = myController && myController->isConnected();
  bool disconnectFault = disconnectAlertActive &&
                         ((uint32_t)(now - disconnectAlertStartMs) < DISCONNECT_ALERT_MS);
  bool healthy = !persistentSystemFault && !disconnectFault;

  const char* ledState;
  if (connected) {
    ledState = (abs(WeaponCmd_deg) <= 2) ? "GREEN_ORB" : "WEAPON_RAINBOW";
  } else {
    ledState = disconnectFault ? "DISCONNECT_FLASH" : "WAITING_WAVE";
  }

  int mappedLeft = map(MotorOutput_Left, -512, 512, DRIVE_MIN_COMMAND_DEG, DRIVE_MAX_COMMAND_DEG);
  int mappedRight = map(MotorOutput_Right, -512, 512, DRIVE_MIN_COMMAND_DEG, DRIVE_MAX_COMMAND_DEG);

  Serial.printf(
    "DBG state ms=%lu conn=%d age=%lums health=%s failsafe=%s drive=%s led=%s loop=%luHz pad=%luHz batt=%.3fV adc=%umV wmode=%d esc=%s dwell[L=%d R=%d W=%d]\n",
    now, connected,
    connected && lastGamepadDataMs != 0 ? (uint32_t)(now - lastGamepadDataMs) : 65535UL,
    healthy ? "OK" : "FAULT", motionFailsafeLatched ? "LATCHED" : "CLEAR",
    driveOutputsEnabled ? "ENABLED" : "DISABLED", ledState, loopHz, gamepadHz,
    batteryMillivolts / 1000.0f, batteryAdcMillivolts, Weapon_Mode,
    weaponEscEnabled ? (((uint32_t)(now - weaponEscEnabledAtMs) < WEAPON_ESC_ARM_MS) ? "ARMING" : "ENABLED") : "DISABLED",
    dwellUntil_Left != 0, dwellUntil_Right != 0, dwellUntil_Weapon != 0);

  if (connected) {
    Serial.printf(
      "DBG input y=%d rx=%d throttle=%d brake=%d btn[A=%d B=%d X=%d] cmd[L=%d R=%d W=%d] out[slot1_R=%d slot2_L=%d weapon_DShot=%u]\n",
      myController->axisY(), myController->axisRX(), myController->throttle(), myController->brake(),
      myController->a(), myController->b(), myController->x(),
      MotorOutput_Left, MotorOutput_Right, WeaponCmd_deg,
      mappedRight, mappedLeft, weaponDshotValue);
  } else {
    Serial.println("DBG input controller=NONE cmd[L=0 R=0 W=0] out[drive=DETACHED weapon_DShot=OFF]");
  }
}

void UpdateWirelessTelemetry() {
  static uint32_t lastTelemetryMs = 0;
  static uint32_t lastTelemetryLoopCount = 0;
  static uint8_t robotSequence = 0;
  static uint8_t controllerSequence = 0;

  uint32_t now = millis();
  uint32_t elapsed = (uint32_t)(now - lastTelemetryMs);
  bool connected = myController && myController->isConnected();
  uint32_t productionPeriodMs = connected ? 100 : 500;
  if (elapsed < productionPeriodMs) return;

  uint32_t measuredLoopHz = ((loopCounter - lastTelemetryLoopCount) * 1000UL) / elapsed;
  lastTelemetryMs = now;
  lastTelemetryLoopCount = loopCounter;

  bool disconnectAlert = disconnectAlertActive &&
                         ((uint32_t)(now - disconnectAlertStartMs) < DISCONNECT_ALERT_MS);

  RobotTelemetryPacket packet = {};
  packet.version = 2;
  packet.sequence = robotSequence++;
  packet.loopHz = (uint8_t)min(measuredLoopHz, (uint32_t)255);

  if (connected) packet.flags |= TELEMETRY_GAMEPAD_CONNECTED;
  if (motionFailsafeLatched) packet.flags |= TELEMETRY_FAILSAFE_LATCHED;
  if (driveOutputsEnabled) packet.flags |= TELEMETRY_DRIVE_ENABLED;
  if (weaponEscEnabled) packet.flags |= TELEMETRY_WEAPON_ENABLED;
  if (persistentSystemFault) packet.flags |= TELEMETRY_PERSISTENT_FAULT;
  if (WeaponCmd_deg < 0) packet.flags |= TELEMETRY_WEAPON_REVERSE;
  if (weaponEscEnabled && (uint32_t)(now - weaponEscEnabledAtMs) < WEAPON_ESC_ARM_MS) {
    packet.flags |= TELEMETRY_WEAPON_ARMING;
  }
  if (disconnectAlert) packet.flags |= TELEMETRY_DISCONNECT_ALERT;

  packet.batteryMillivolts = batteryMillivolts;
  packet.adcMillivolts = batteryAdcMillivolts;
  packet.driveLeft = MotorOutput_Left;
  packet.driveRight = MotorOutput_Right;
  packet.weaponCommand = WeaponCmd_deg;
  packet.weaponDshot = weaponDshotValue;
  packet.controllerAgeMs = connected
                             ? (uint16_t)min((uint32_t)(now - lastGamepadDataMs), (uint32_t)65535)
                             : 65535;
  packet.batteryPercent = batteryPercent;

  ControllerTelemetryPacket controllerPacket = {};
  controllerPacket.version = 1;
  controllerPacket.sequence = controllerSequence++;
  controllerPacket.battery = 0;
  if (connected) {
    controllerPacket.dpad = myController->dpad();
    controllerPacket.battery = myController->battery();
    controllerPacket.axisX = myController->axisX();
    controllerPacket.axisY = myController->axisY();
    controllerPacket.axisRX = myController->axisRX();
    controllerPacket.axisRY = myController->axisRY();
    controllerPacket.throttle = myController->throttle();
    controllerPacket.brake = myController->brake();
    controllerPacket.buttons = myController->buttons();
    controllerPacket.miscButtons = myController->miscButtons();
  }

  // This is a fixed-size overwrite, never a wait or queue operation.
  RobotTelemetryBlePublish(packet, controllerPacket, connected);
}



////////////////////////////////////////////////////////////////////////////////////////////
//                            Setup LOOP                                                  //
////////////////////////////////////////////////////////////////////////////////////////////
// Set up pins, LED PWM functionalities and begin GamePad, Serial and Serial2 communication


void setup() {

  Serial.begin(115200);

  // --- All motion outputs start disabled ---
  DisableDriveOutputs();
  DisableWeaponESC();
  motionFailsafeLatched = true;
  Serial.println("Motor outputs disabled; motion failsafe latched");

  INIT_LoRcore();

  // --- System Start Complete ---
  Serial.println("System ready: control=100Hz LEDs=30Hz debug=5Hz");
}

////////////////////////////////////////////////////////////////////////////////////////////
//                            Main LOOP                                                   //
////////////////////////////////////////////////////////////////////////////////////////////

void loop() {

  loopCounter++;

  // --- Watch Dog ---
  esp_task_wdt_reset();  // Feed the watchdog

  // --- Check and Collect Gamepad Data ---
  if (BP32.update()) {
    gamepadUpdateCounter++;
    if (myController && myController->isConnected() && myController->hasData()) {
      lastGamepadDataMs = millis();
    }
  }

  // --- Enforce the global motion fail-safe and neutral-release interlock ---
  UpdateMotionSafetyState();

  // --- Sample and filter the 3S HV main battery monitor on GPIO8 ---
  UpdateBatteryMonitor();

  // --- Gamepad Connected ---
  if (myController && myController->isConnected() && !motionFailsafeLatched) {

    Drive();

    Weapon_StateMachine();

    //Serial.println("Left:t/" + String(MappedLeft) + " \tRight:\t" + String(MappedRight) + "\t Weapon:\t" + String(MappedWeapon));

  }

  // --- Update internal and external LEDs independently at approximately 30 Hz ---
  UpdateLEDStateMachine();

  // --- Rate-limited development telemetry ---
  PrintDebugTelemetry();

  // --- Low-priority, nonblocking BLE phone telemetry snapshot ---
  UpdateWirelessTelemetry();

  // --- Hold approximately 100 Hz after BLE, ADC, safety, and LED processing overhead ---
  delay(8);
}
