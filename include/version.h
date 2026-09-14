#pragma once

#include <Arduino.h>

#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 0

namespace Version {

inline void print() {
  Serial.printf("Firmware: v%d.%d.%d\n", FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
  Serial.printf("Build hash: %s\n", GIT_HASH);
  Serial.printf("Build date: %s %s\n", __DATE__, __TIME__);
}

} // namespace Version
