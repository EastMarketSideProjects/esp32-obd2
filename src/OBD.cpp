#include "OBD.h"

bool OBD::begin(gpio_num_t txPin, gpio_num_t rxPin, uint32_t speedKbps) {
  // Zero out all slots
  for (uint8_t i = 0; i < OBD_MAX_PENDING; i++) {
    _pending[i].active = false;
  }
  for (uint8_t i = 0; i < OBD_MAX_SUBSCRIPTIONS; i++) {
    _subscriptions[i].active = false;
  }

  ESP32Can.setPins(txPin, rxPin);
  ESP32Can.setRxQueueSize(20);
  ESP32Can.setTxQueueSize(10);
  ESP32Can.setSpeed(ESP32Can.convertSpeed(speedKbps));

  bool result = ESP32Can.begin();
  Serial.printf("CAN begin %s at %u kbps\n", result ? "succeeded" : "failed",
                speedKbps);
  return result;
}

void OBD::_send(uint8_t pid) {
  CanFrame frame;
  frame.identifier = OBD_REQUEST_ID;
  frame.extd = 0;
  frame.data_length_code = 8;
  frame.data[0] = 0x02;
  frame.data[1] = 0x01;
  frame.data[2] = pid;
  frame.data[3] = 0x00;
  frame.data[4] = 0x00;
  frame.data[5] = 0x00;
  frame.data[6] = 0x00;
  frame.data[7] = 0x00;
  ESP32Can.writeFrame(frame);
}

void OBD::_send(uint8_t pid, uint8_t* data) {
  CanFrame frame;
  frame.identifier = OBD_REQUEST_ID;
  frame.extd = 0;
  frame.data_length_code = 8;
  frame.data[0] = 0x02;
  frame.data[1] = 0x01;
  frame.data[2] = pid;
  memcpy(&frame.data[3], data, 5);  // copy up to 5 bytes of data
  ESP32Can.writeFrame(frame);
}

int8_t OBD::_freePendingSlot() const {
  for (uint8_t i = 0; i < OBD_MAX_PENDING; i++) {
    if (!_pending[i].active) return (int8_t)i;
  }
  return -1;
}

int8_t OBD::_freeSubSlot() const {
  for (uint8_t i = 0; i < OBD_MAX_SUBSCRIPTIONS; i++) {
    if (!_subscriptions[i].active) return (int8_t)i;
  }
  return -1;
}

int8_t OBD::_findSub(PID pid) const {
  for (uint8_t i = 0; i < OBD_MAX_SUBSCRIPTIONS; i++) {
    if (_subscriptions[i].active && _subscriptions[i].pid == pid) {
      return (int8_t)i;
    }
  }
  return -1;
}

void OBD::request(PID pid, OBDCallback callback) {
  int8_t slot = _freePendingSlot();
  if (slot < 0) {
    Serial.println("OBD: pending queue full, dropping request");
    return;
  }
  _send(static_cast<uint8_t>(pid));
  _pending[slot].pid = pid;
  _pending[slot].callback = callback;
  _pending[slot].sentAt = millis();
  _pending[slot].active = true;
}

void OBD::scanSupportedPIDs(OBDScanCallback callback) {
  _scanCallback = callback;
  _scanPending = true;
  _send(0x00);
}

void OBD::subscribe(PID pid, uint32_t interval_ms, OBDCallback callback) {
  // Update existing subscription if present
  int8_t existing = _findSub(pid);
  if (existing >= 0) {
    _subscriptions[existing].interval_ms = interval_ms;
    _subscriptions[existing].callback = callback;
    _subscriptions[existing].lastSentAt = 0;
    _subscriptions[existing].awaitingReply = false;
    return;
  }

  int8_t slot = _freeSubSlot();
  if (slot < 0) {
    Serial.println("OBD: subscription table full");
    return;
  }
  _subscriptions[slot].pid = pid;
  _subscriptions[slot].interval_ms = interval_ms;
  _subscriptions[slot].callback = callback;
  _subscriptions[slot].lastSentAt = 0;  // fire immediately on first tick
  _subscriptions[slot].awaitingReply = false;
  _subscriptions[slot].active = true;
}

void OBD::unsubscribe(PID pid) {
  for (uint8_t i = 0; i < OBD_MAX_SUBSCRIPTIONS; i++) {
    if (_subscriptions[i].active && _subscriptions[i].pid == pid) {
      _subscriptions[i].active = false;
    }
  }
}

OBDResult OBD::_decode(PID pid, const CanFrame& frame) {
  OBDResult result;
  result.pid = pid;

  switch (pid) {
    case PID::GEAR_RCMD:
      result.value = (A >> 4) & 0x0F;
      strncpy(result.unit, "gear", sizeof(result.unit));
      break;
    case PID::SPEED:
      result.value = A * 0.621371f;
      strncpy(result.unit, "mph", sizeof(result.unit));
      break;
    case PID::RPM:
      result.value = ((256.0f * A) + B) / 4.0f;
      strncpy(result.unit, "rpm", sizeof(result.unit));
      break;
    case PID::ENGINE_LOAD:
      result.value = A / 2.55f;
      strncpy(result.unit, "%", sizeof(result.unit));
      break;
    case PID::ENGINE_ODOMETER:
      result.value = (((uint32_t)A << 24) | ((uint32_t)B << 16) |
                      ((uint32_t)C << 8) | ((uint32_t)D)) /
                     10.0f;
      strncpy(result.unit, "km", sizeof(result.unit));
      break;
    case PID::COOLANT_TEMP:
      result.value = A - 40.0f;
      strncpy(result.unit, "C", sizeof(result.unit));
      break;
    case PID::INTAKE_TEMP:
      result.value = A - 40.0f;
      strncpy(result.unit, "C", sizeof(result.unit));
      break;
    case PID::OIL_TEMP:
      result.value = A - 40.0f;
      strncpy(result.unit, "C", sizeof(result.unit));
      break;
    case PID::THROTTLE:
    case PID::FUEL_LEVEL:
      result.value = A / 2.55f;
      strncpy(result.unit, "%", sizeof(result.unit));
      break;
    case PID::BATTERY_VOLTAGE:
      result.value = ((uint32_t)A << 8 | B) / 1000.0f;
      strncpy(result.unit, "volts", sizeof(result.unit));
      break;
    default:
      result.value = A;
      strncpy(result.unit, "raw", sizeof(result.unit));
      break;
  }
  result.unit[sizeof(result.unit) - 1] = '\0';
  return result;
}

void OBD::_tickSubscriptions() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < OBD_MAX_SUBSCRIPTIONS; i++) {
    if (!_subscriptions[i].active) continue;
    if (_subscriptions[i].awaitingReply) continue;  // don't flood the bus

    if ((now - _subscriptions[i].lastSentAt) >= _subscriptions[i].interval_ms ||
        _subscriptions[i].lastSentAt == 0) {
      PID pid = _subscriptions[i].pid;

      int8_t slot = _freePendingSlot();
      if (slot < 0) {
        Serial.println("OBD: pending queue full, skipping subscription tick");
        continue;
      }

      _send(static_cast<uint8_t>(pid));
      _subscriptions[i].lastSentAt = now;
      _subscriptions[i].awaitingReply = true;

      _pending[slot].pid = pid;
      _pending[slot].callback = _subscriptions[i].callback;
      _pending[slot].sentAt = now;
      _pending[slot].active = true;

      _pending[slot].subIndex = i;
      _pending[slot].isSubRequest = true;
    }
  }
}

void OBD::update() {
  _tickSubscriptions();

  uint32_t now = millis();
  for (uint8_t i = 0; i < OBD_MAX_PENDING; i++) {
    if (!_pending[i].active) continue;

    if ((now - _pending[i].sentAt) > TIMEOUT_MS) {
      framesDropped++;
      if (_pending[i].isSubRequest) {
        uint8_t si = _pending[i].subIndex;
        if (si < OBD_MAX_SUBSCRIPTIONS && _subscriptions[si].active) {
          _subscriptions[si].awaitingReply = false;
        }
      }
      _pending[i].active = false;
    }
  }

  CanFrame frame;
  if (!ESP32Can.readFrame(frame, 0)) return;  // non-blocking

  if (frame.identifier != ECU_RESPONSE_ID) return;

  uint8_t mode = frame.data[1];
  uint8_t pid = frame.data[2];

  if (mode != 0x41) return;  // only handle current data for now

  if (pid == 0x00) {
    _handleScanResponse(frame);
  }

  _checkPending(pid, frame);
}

void OBD::_checkPending(uint8_t pid, const CanFrame& frame) {
  for (uint8_t i = 0; i < OBD_MAX_PENDING; i++) {
    if (!_pending[i].active) continue;
    if (static_cast<uint8_t>(_pending[i].pid) != pid) continue;

    OBDResult result = _decode(_pending[i].pid, frame);

    if (_pending[i].isSubRequest) {
      uint8_t si = _pending[i].subIndex;
      if (si < OBD_MAX_SUBSCRIPTIONS && _subscriptions[si].active) {
        _subscriptions[si].awaitingReply = false;
      }
    }

    if (_pending[i].callback) _pending[i].callback(result);
    _pending[i].active = false;
    return;
  }
}

void OBD::_handleScanResponse(const CanFrame& frame) {
  if (_scanPending) {
    _scanPending = false;
    uint32_t bitmask =
        ((uint32_t)frame.data[3] << 24) | ((uint32_t)frame.data[4] << 16) |
        ((uint32_t)frame.data[5] << 8) | ((uint32_t)frame.data[6]);

    for (uint8_t i = 0; i < 32 && _supportedPIDsCount < OBD_MAX_SCAN_PIDS;
         i++) {
      if (bitmask & (1UL << (31 - i))) {
        _supportedPIDs[_supportedPIDsCount++] = i + 1;
      }
    }
    if (_scanCallback) _scanCallback(_supportedPIDs, _supportedPIDsCount);
    return;
  }
}

const char* OBD::pidToString(PID pid) {
  switch (pid) {
    case PID::SPEED:
      return "Speed (mph)";
    case PID::RPM:
      return "rpm";
    case PID::ENGINE_LOAD:
      return "Engine Load (%)";
    case PID::COOLANT_TEMP:
      return "Coolant Temp (°C)";
    case PID::INTAKE_TEMP:
      return "Intake Temp (°C)";
    case PID::OIL_TEMP:
      return "Oil Temp (°C)";
    case PID::THROTTLE:
      return "Throttle (%)";
    case PID::FUEL_LEVEL:
      return "Fuel Level (%)";
    case PID::GEAR_RCMD:
      return "Gear Command";
    default:
      return "Unknown";
  }
}

void OBD::printPending() const {
  Serial.println("Pending requests:");
  for (uint8_t i = 0; i < OBD_MAX_PENDING; i++) {
    if (_pending[i].active) {
      Serial.printf("  PID 0x%02X sent at %lu ms\n",
                    static_cast<uint8_t>(_pending[i].pid), _pending[i].sentAt);
    }
  }
}

void OBD::printSubscriptions() const {
  Serial.println("Active subscriptions:");
  for (uint8_t i = 0; i < OBD_MAX_SUBSCRIPTIONS; i++) {
    if (_subscriptions[i].active) {
      Serial.printf("  PID 0x%02X interval %lu ms\n",
                    static_cast<uint8_t>(_subscriptions[i].pid),
                    _subscriptions[i].interval_ms);
    }
  }
}

void OBD::printSupportedPIDs() const {
  Serial.println("Supported PIDs:");
  for (uint8_t i = 0; i < _supportedPIDsCount; i++) {
    Serial.printf("  PID 0x%02X\n", _supportedPIDs[i]);
  }
}