#pragma once

#include <Arduino.h>

namespace Config {

constexpr uint16_t CURRENT_VERSION = 2;

constexpr uint16_t DOUBLE_CLICK_WINDOW_MS_MIN = 100;
constexpr uint16_t DOUBLE_CLICK_WINDOW_MS_MAX = 1000;
constexpr uint16_t DOUBLE_CLICK_WINDOW_MS_DEFAULT = 300;

constexpr int8_t TX_POWER_DBM_MIN = 2;
constexpr int8_t TX_POWER_DBM_MAX = 20;
constexpr int8_t TX_POWER_DBM_DEFAULT = 14;

// Packed because this struct is written to NVS verbatim: its size and layout
// are an on-disk format, not an implementation detail.
struct __attribute__((packed)) Values {
  uint16_t cfgVersion;
  uint16_t doubleClickWindowMs;
  int8_t txPowerDbm;
};

static_assert(sizeof(Values) == 5, "Config blob layout must stay stable");

// Loads config from NVS, migrating older cfg_version layouts forward and
// persisting the migrated result. Falls back to defaults if NVS is empty.
void load();

// Returns the current in-memory config.
const Values &get();

// Validates the given field against its allowed range and, if valid,
// updates the in-memory config and persists it to NVS. Returns false
// (without writing) if the value is out of range.
bool setDoubleClickWindowMs(uint16_t value);
bool setTxPowerDbm(int8_t value);

// Erases NVS and reloads defaults.
void reset();

// Test helper, not part of the released firmware: writes a v1-layout config
// (no tx_power field) into NVS so a reboot exercises the v1 -> v2 migration.
void simulateV1();

} // namespace Config
