# ESP32-OBD2

A library to handle asynchronus OBD2/CAN bus communication on ESP32

Currently only supports reading from the CAN bus not writing

# Usage


This is simple example how to subscribe to an OBD PID (Parameter ID) with a callback function

```cpp
#include <OBD.h>

#define CAN_RX GPIO_NUM_4
#define CAN_TX GPIO_NUM_5
#define CAN_SPEED 500  // kbps

#define REQUEST_INTERVAL 250

OBD obd;

void on_speed(const OBDResult& result) {
  Serial.printf("Speed: %0.0f %s\n", result.value, result.unit);
}

void setup() {
  Serial.begin(115200);

  if (!obd.begin(CAN_TX, CAN_RX, CAN_SPEED)) {
    Serial.println("CAN failed to start!");
    return;
  }

  // named handler function
  obd.subscribe(PID::SPEED, REQUEST_INTERVAL, on_speed);

  // inline lambda handler
  obd.subscribe(PID::RPM, REQUEST_INTERVAL, [](const OBDResult& result) {
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


# Hardware Used

- [ESP32-S3 Super Mini](https://www.amazon.com/dp/B0D47HBFDY?linkCode=ssc&tag=onamzmayaodom-20&creativeASIN=B0D47HBFDY&asc_item-id=amzn1.ideas.91GBZA4ZURP2&ref_=aipsflist_asin)

- [TJA1050 CAN Transceiver](https://www.amazon.com/dp/B0FJ47QVZZ?linkCode=ssc&creativeASIN=B0FJ47QVZZ&asc_item-id=amzn1.ideas.91GBZA4ZURP2&ref_=aipsflist_qv_asin&tag=onamzmayaodom-20)

- [OBD2 Connector & Housing](https://www.amazon.com/dp/B083FDYGKS?linkCode=ssc&tag=onamzmayaodom-20&creativeASIN=B083FDYGKS&asc_item-id=amzn1.ideas.91GBZA4ZURP2&ref_=aipsflist_asin)
## Wiring 

```
ESP32-S3                TJA1050                   OBD-II Port (J1962)
┌─────────────┐         ┌─────────────┐            ┌─────────────────┐
│             │         │             │            │                 │
│    GPIO 5   ├────────►│ TXD         │            │                 │
│    GPIO 4   │◄────────┤ RXD         │            │   Pin 6  (CANH) │
│             │         │         CANH├───────────►│                 │
│          5V ├────────►│ VCC         │            │   Pin 14 (CANL) │
│             │         │         CANL├───────────►│                 │
│         GND ├────┐    │         GND ├────┐       │   Pin 4/5 (GND) │
│             │    │    │             │    │       └────────┬────────┘
└─────────────┘    │    └─────────────┘    │                │
                   │                       │                │
                   └───────────────────────┴────────────────┘
                                     GND (common)
```