#include "post.h"

#include <Preferences.h>

#include "log.h"

namespace Post {

namespace {
uint8_t lastMask = 0;

bool checkNvs() {
  Preferences testPrefs;
  if (!testPrefs.begin("post", false)) {
    return false;
  }
  // Write-then-read a known value: proves the partition is mounted and
  // writable, not merely that the namespace could be opened.
  testPrefs.putUShort("probe", 0xA5A5);
  uint16_t readBack = testPrefs.getUShort("probe", 0);
  testPrefs.end();
  return readBack == 0xA5A5;
}

bool checkAdc(uint8_t batteryAdcPin) {
  int raw = analogRead(batteryAdcPin);
  return batteryAdcPin != 0 && raw > 0 && raw < 4095;
}

bool checkButton(uint8_t buttonPin) {
  // With INPUT_PULLUP and the button released, the pin must read HIGH.
  // A stuck-LOW pin means a shorted button or missing pull-up.
  return digitalRead(buttonPin) == HIGH;
}
}  // namespace

uint8_t run(bool radioInitOk, uint8_t buttonPin, uint8_t batteryAdcPin) {
  uint8_t mask = 0;

  if (!checkNvs()) {
    mask |= (1 << BIT_NVS);
  }
  if (!radioInitOk) {
    mask |= (1 << BIT_RADIO);
  }
  if (!checkAdc(batteryAdcPin)) {
    mask |= (1 << BIT_ADC);
  }
  if (!checkButton(buttonPin)) {
    mask |= (1 << BIT_BUTTON);
  }

  lastMask = mask;

  if (mask == 0) {
    LOGI("POST passed, mask=0x%02X", mask);
  } else {
    LOGE("POST failed, mask=0x%02X", mask);
  }

  return mask;
}

uint8_t mask() { return lastMask; }

void printReport() {
  Serial.printf("POST mask: 0x%02X\n", lastMask);
  Serial.printf("  NVS:    %s\n",
                (lastMask & (1 << BIT_NVS)) ? "FAIL" : "PASS");
  Serial.printf("  Radio:  %s\n",
                (lastMask & (1 << BIT_RADIO)) ? "FAIL" : "PASS");
  Serial.printf("  ADC:    %s\n",
                (lastMask & (1 << BIT_ADC)) ? "FAIL" : "PASS");
  Serial.printf("  Button: %s\n",
                (lastMask & (1 << BIT_BUTTON)) ? "FAIL" : "PASS");
}

}  // namespace Post
