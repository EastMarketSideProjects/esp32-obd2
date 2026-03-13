#pragma once
#include <Arduino.h>

#include <ESP32-TWAI-CAN.hpp>

#define A frame.data[3]
#define B frame.data[4]
#define C frame.data[5]
#define D frame.data[6]
#define E frame.data[7]

static const uint32_t ECU_RESPONSE_ID = 0x7E8;
static const uint32_t OBD_REQUEST_ID = 0x7E0;
static const uint32_t TIMEOUT_MS = 500;

// Capacities
static const uint8_t OBD_MAX_PENDING = 8;
static const uint8_t OBD_MAX_SUBSCRIPTIONS = 8;
static const uint8_t OBD_MAX_SCAN_PIDS = 32;

// Known parameter IDs
enum class PID : uint8_t {
  SUPPORTED_PIDS_01_20 = 0x00,
  ENGINE_LOAD = 0x04,
  ENGINE_ODOMETER = 0xD3,
  COOLANT_TEMP = 0x05,
  FUEL_PRESSURE = 0x0A,
  RPM = 0x0C,
  SPEED = 0x0D,
  INTAKE_TEMP = 0x0F,
  THROTTLE = 0x11,
  FUEL_LEVEL = 0x2F,
  OIL_TEMP = 0x5C,
  GEAR_RCMD = 0x65,
  BATTERY_VOLTAGE = 0x42,
};

struct OBDResult {
  PID pid;
  float value;
  char unit[8];  // "rpm", "mph", "%", "°C", "gear", "raw"
};

typedef void (*OBDCallback)(const OBDResult&);

typedef void (*OBDScanCallback)(const uint8_t* pids, uint8_t count);

class OBD {
 public:
  bool begin(gpio_num_t txPin, gpio_num_t rxPin, uint32_t speedKbps = 500);

  void request(PID pid, OBDCallback callback);

  void scanSupportedPIDs(OBDScanCallback callback);

  void subscribe(PID pid, uint32_t interval_ms, OBDCallback callback);
  void unsubscribe(PID pid);

  void update();

  const char* pidToString(PID pid);

  void printPending() const;
  void printSubscriptions() const;
  void printSupportedPIDs() const;

 private:
  struct PendingRequest {
    PID pid;
    OBDCallback callback;
    uint32_t sentAt;
    bool active;
    bool isSubRequest;
    uint8_t subIndex;
  };

  struct Subscription {
    PID pid;
    uint32_t interval_ms;
    OBDCallback callback;
    uint32_t lastSentAt;
    bool awaitingReply;
    bool active;
  };

  unsigned long framesDropped = 0;
  PendingRequest _pending[OBD_MAX_PENDING] = {};
  Subscription _subscriptions[OBD_MAX_SUBSCRIPTIONS] = {};
  uint8_t _supportedPIDs[OBD_MAX_SCAN_PIDS] = {};
  uint8_t _supportedPIDsCount = 0;

  OBDScanCallback _scanCallback = nullptr;
  bool _scanPending = false;

  void _send(uint8_t pid);
  void _send(uint8_t pid, uint8_t* data);
  OBDResult _decode(PID pid, const CanFrame& frame);
  void _tickSubscriptions();
  void _checkPending(uint8_t pid, const CanFrame& frame);
  void _handleScanResponse(const CanFrame& frame);

  int8_t _freePendingSlot() const;
  int8_t _freeSubSlot() const;
  int8_t _findSub(PID pid) const;
};