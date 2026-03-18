# ESP32-OBD2

An asynchronus library to handle OBD2/CAN communication on ESP32

Currently only supports reading from the CAN bus not writing

# Usage


This is simple example how to subscribe to an OBD PID (Parameter ID) with a callback function

```cpp
#include <OBD.h>

#define CAN_RX GPIO_NUM_4
#define CAN_TX GPIO_NUM_5
#define CAN_SPEED 500  // kbps

OBD obd;

void on_speed(OBDResult& result) {
  Serial.printf("Speed: %0.0f %s\n", result.value, result.unit);
}

void setup() {
  if (!obd.begin(CAN_TX, CAN_RX, CAN_SPEED)) {
    Serial.println("CAN failed to start!");
  }

  // named handler function
  obd.subscribe(PID::SPEED, REQUEST_INTERVAL, on_speed);

  // inline lambda handler
  obd.subscribe(PID::SPEED, REQUEST_INTERVAL, [](OBDResult& result) {
    Serial.printf("RPM: %0.0f %s\n", result.value, result.unit);
  });
}

void loop() { obd.update(); }
```

# PlatformIO

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps = 
    https://github.com/EastMarketSideProjects/esp32-obd2.git
```
