#include "RobotTelemetryBle.h"

#include "btstack.h"

namespace {

// Alternate robot and controller notifications. Each characteristic updates at
// 10 Hz while connected and 2 Hz while waiting, without touching the control loop.
constexpr uint32_t TELEMETRY_CONNECTED_PERIOD_MS = 50;
constexpr uint32_t TELEMETRY_WAITING_PERIOD_MS = 250;

// 8b7d0001-3f9b-4f6f-8d6a-11f6a3c80001
const uint8_t kServiceUuid[16] = {
  0x8b, 0x7d, 0x00, 0x01, 0x3f, 0x9b, 0x4f, 0x6f,
  0x8d, 0x6a, 0x11, 0xf6, 0xa3, 0xc8, 0x00, 0x01
};

// 8b7d0002-3f9b-4f6f-8d6a-11f6a3c80001
const uint8_t kTelemetryUuid[16] = {
  0x8b, 0x7d, 0x00, 0x02, 0x3f, 0x9b, 0x4f, 0x6f,
  0x8d, 0x6a, 0x11, 0xf6, 0xa3, 0xc8, 0x00, 0x01
};

// 8b7d0003-3f9b-4f6f-8d6a-11f6a3c80001
const uint8_t kControllerUuid[16] = {
  0x8b, 0x7d, 0x00, 0x03, 0x3f, 0x9b, 0x4f, 0x6f,
  0x8d, 0x6a, 0x11, 0xf6, 0xa3, 0xc8, 0x00, 0x01
};

// Flags + complete 128-bit service UUID (little-endian on the air) + short name.
uint8_t kAdvertisingData[] = {
  0x02, 0x01, 0x06,
  0x11, 0x07,
  0x01, 0x00, 0xc8, 0xa3, 0xf6, 0x11, 0x6a, 0x8d,
  0x6f, 0x4f, 0x9b, 0x3f, 0x01, 0x00, 0x7d, 0x8b,
  0x08, 0x09, 'L', 'o', 'R', ' ', 'S', 'S', 'S'
};

portMUX_TYPE telemetryMux = portMUX_INITIALIZER_UNLOCKED;
RobotTelemetryPacket latestPacket = {};
ControllerTelemetryPacket latestControllerPacket = {};
volatile bool gamepadConnectedForRate = false;
volatile bool robotNotificationsEnabled = false;
volatile bool controllerNotificationsEnabled = false;
volatile hci_con_handle_t phoneConnectionHandle = HCI_CON_HANDLE_INVALID;
uint16_t telemetryValueHandle = 0;
uint16_t telemetryCccdHandle = 0;
uint16_t controllerValueHandle = 0;
uint16_t controllerCccdHandle = 0;
bool sendControllerNext = false;
btstack_timer_source_t telemetryTimer;
btstack_packet_callback_registration_t hciEventRegistration;

uint16_t ReadCallback(hci_con_handle_t connectionHandle,
                      uint16_t attributeHandle,
                      uint16_t offset,
                      uint8_t* buffer,
                      uint16_t bufferSize) {
  (void)connectionHandle;

  if (attributeHandle == telemetryValueHandle) {
    RobotTelemetryPacket snapshot;
    portENTER_CRITICAL(&telemetryMux);
    snapshot = latestPacket;
    portEXIT_CRITICAL(&telemetryMux);
    return att_read_callback_handle_blob(reinterpret_cast<const uint8_t*>(&snapshot),
                                         sizeof(snapshot), offset, buffer, bufferSize);
  }

  if (attributeHandle == telemetryCccdHandle) {
    uint8_t cccd[2] = { static_cast<uint8_t>(robotNotificationsEnabled ? 1 : 0), 0 };
    return att_read_callback_handle_blob(cccd, sizeof(cccd), offset, buffer, bufferSize);
  }

  if (attributeHandle == controllerValueHandle) {
    ControllerTelemetryPacket snapshot;
    portENTER_CRITICAL(&telemetryMux);
    snapshot = latestControllerPacket;
    portEXIT_CRITICAL(&telemetryMux);
    return att_read_callback_handle_blob(reinterpret_cast<const uint8_t*>(&snapshot),
                                         sizeof(snapshot), offset, buffer, bufferSize);
  }

  if (attributeHandle == controllerCccdHandle) {
    uint8_t cccd[2] = { static_cast<uint8_t>(controllerNotificationsEnabled ? 1 : 0), 0 };
    return att_read_callback_handle_blob(cccd, sizeof(cccd), offset, buffer, bufferSize);
  }

  return 0;
}

int WriteCallback(hci_con_handle_t connectionHandle,
                  uint16_t attributeHandle,
                  uint16_t transactionMode,
                  uint16_t offset,
                  uint8_t* buffer,
                  uint16_t bufferSize) {
  (void)transactionMode;

  // The only accepted phone write is the standard notification subscription descriptor.
  if (attributeHandle == telemetryCccdHandle && offset == 0 && bufferSize == 2) {
    uint16_t configuration = little_endian_read_16(buffer, 0);
    phoneConnectionHandle = connectionHandle;
    robotNotificationsEnabled =
      (configuration & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION) != 0;
  } else if (attributeHandle == controllerCccdHandle && offset == 0 && bufferSize == 2) {
    uint16_t configuration = little_endian_read_16(buffer, 0);
    phoneConnectionHandle = connectionHandle;
    controllerNotificationsEnabled =
      (configuration & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION) != 0;
  }
  return 0;
}

void HciPacketHandler(uint8_t packetType, uint16_t channel, uint8_t* packet, uint16_t size) {
  (void)channel;
  (void)size;
  if (packetType != HCI_EVENT_PACKET) return;

  uint8_t eventType = hci_event_packet_get_type(packet);
  if (eventType == HCI_EVENT_LE_META &&
      hci_event_le_meta_get_subevent_code(packet) == HCI_SUBEVENT_LE_CONNECTION_COMPLETE &&
      hci_subevent_le_connection_complete_get_status(packet) == ERROR_CODE_SUCCESS &&
      hci_subevent_le_connection_complete_get_role(packet) == HCI_ROLE_SLAVE) {
    phoneConnectionHandle = hci_subevent_le_connection_complete_get_connection_handle(packet);
    robotNotificationsEnabled = false;
    controllerNotificationsEnabled = false;
    return;
  }

  if (eventType == HCI_EVENT_DISCONNECTION_COMPLETE) {
    hci_con_handle_t disconnected = hci_event_disconnection_complete_get_connection_handle(packet);
    if (disconnected == phoneConnectionHandle) {
      phoneConnectionHandle = HCI_CON_HANDLE_INVALID;
      robotNotificationsEnabled = false;
      controllerNotificationsEnabled = false;
      gap_advertisements_enable(1);
    }
  }
}

void TelemetryTimerHandler(btstack_timer_source_t* timer) {
  if ((robotNotificationsEnabled || controllerNotificationsEnabled) &&
      phoneConnectionHandle != HCI_CON_HANDLE_INVALID &&
      att_server_can_send_packet_now(phoneConnectionHandle)) {
    bool sendController = controllerNotificationsEnabled &&
                          (sendControllerNext || !robotNotificationsEnabled);
    if (sendController) {
      ControllerTelemetryPacket snapshot;
      portENTER_CRITICAL(&telemetryMux);
      snapshot = latestControllerPacket;
      portEXIT_CRITICAL(&telemetryMux);
      att_server_notify(phoneConnectionHandle, controllerValueHandle,
                        reinterpret_cast<const uint8_t*>(&snapshot), sizeof(snapshot));
    } else {
      RobotTelemetryPacket snapshot;
      portENTER_CRITICAL(&telemetryMux);
      snapshot = latestPacket;
      portEXIT_CRITICAL(&telemetryMux);
      att_server_notify(phoneConnectionHandle, telemetryValueHandle,
                        reinterpret_cast<const uint8_t*>(&snapshot), sizeof(snapshot));
    }
    sendControllerNext = !sendController;
  }

  uint32_t nextPeriod = gamepadConnectedForRate
                          ? TELEMETRY_CONNECTED_PERIOD_MS
                          : TELEMETRY_WAITING_PERIOD_MS;
  btstack_run_loop_set_timer(timer, nextPeriod);
  btstack_run_loop_add_timer(timer);
}

}  // namespace

void RobotTelemetryBleBegin() {
  static uint8_t deviceName[] = "LoR SSS Telemetry";

  att_db_util_init();
  att_db_util_add_service_uuid16(0x1800);  // Generic Access
  att_db_util_add_characteristic_uuid16(0x2a00, ATT_PROPERTY_READ,
                                        ATT_SECURITY_NONE, ATT_SECURITY_NONE,
                                        deviceName, sizeof(deviceName) - 1);
  att_db_util_add_service_uuid16(0x1801);  // Generic Attribute
  att_db_util_add_service_uuid128(kServiceUuid);
  telemetryValueHandle = att_db_util_add_characteristic_uuid128(
    kTelemetryUuid,
    ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY | ATT_PROPERTY_DYNAMIC,
    ATT_SECURITY_NONE, ATT_SECURITY_NONE, nullptr, 0);
  telemetryCccdHandle = telemetryValueHandle + 1;
  controllerValueHandle = att_db_util_add_characteristic_uuid128(
    kControllerUuid,
    ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY | ATT_PROPERTY_DYNAMIC,
    ATT_SECURITY_NONE, ATT_SECURITY_NONE, nullptr, 0);
  controllerCccdHandle = controllerValueHandle + 1;

  att_server_init(att_db_util_get_address(), ReadCallback, WriteCallback);
  att_server_register_packet_handler(HciPacketHandler);

  hciEventRegistration.callback = HciPacketHandler;
  hci_add_event_handler(&hciEventRegistration);

  bd_addr_t nullAddress = { 0, 0, 0, 0, 0, 0 };
  constexpr uint16_t advertisingInterval = 0x0320;  // 500 ms
  gap_advertisements_set_params(advertisingInterval, advertisingInterval,
                                0, 0, nullAddress, 0x07, 0x00);
  gap_advertisements_set_data(sizeof(kAdvertisingData), kAdvertisingData);
  gap_advertisements_enable(1);

  telemetryTimer.process = TelemetryTimerHandler;
  btstack_run_loop_set_timer(&telemetryTimer, TELEMETRY_WAITING_PERIOD_MS);
  btstack_run_loop_add_timer(&telemetryTimer);
}

void RobotTelemetryBlePublish(const RobotTelemetryPacket& robotPacket,
                              const ControllerTelemetryPacket& controllerPacket,
                              bool gamepadConnected) {
  portENTER_CRITICAL(&telemetryMux);
  latestPacket = robotPacket;
  latestControllerPacket = controllerPacket;
  portEXIT_CRITICAL(&telemetryMux);
  gamepadConnectedForRate = gamepadConnected;
}

bool RobotTelemetryBlePhoneConnected() {
  return phoneConnectionHandle != HCI_CON_HANDLE_INVALID;
}
