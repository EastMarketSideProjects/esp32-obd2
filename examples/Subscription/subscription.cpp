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