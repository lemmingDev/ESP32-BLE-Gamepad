#include <BleGamepad.h>

// Example Feature Report (Report ID 5 for illustration)
uint8_t featureReport[3] = { 0x10, 0x20, 0x30 };

BleGamepad bleGamepad("ESP32 Gamepad", "LeeNX", 75);

void onFeatureReportRead() {
  // This gets called when the host reads the feature report
  // Nothing special here; just ensure the latest data is returned
}

void onFeatureReportWrite(uint8_t* data, size_t length) {
  // Copy the new data into our buffer
  for (size_t i = 0; i < length && i < sizeof(featureReport); i++) {
    featureReport[i] = data[i];
  }

  Serial.print("Feature Report Updated: ");
  for (int i = 0; i < sizeof(featureReport); i++) {
    Serial.printf("0x%02X ", featureReport[i]);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);

  // Optional: register callbacks the way your PR implements them
  bleGamepad.setFeatureReport(featureReport, sizeof(featureReport));
  bleGamepad.setFeatureReportReadCallback(onFeatureReportRead);
  bleGamepad.setFeatureReportWriteCallback(onFeatureReportWrite);

  bleGamepad.begin();
}

void loop() {
  // Nothing needed here for the feature report example
}
