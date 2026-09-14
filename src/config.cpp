#include "config.h"
#include <Preferences.h>
#include "log.h"

namespace Config {

namespace {
constexpr const char *BLOB_KEY = "blob";

Preferences prefs;
Values current;

// The v1 on-disk layout, kept only so migration can parse what older
// firmware wrote. It had no tx_power field.
struct __attribute__((packed)) ValuesV1 {
  uint16_t cfgVersion;
  uint16_t doubleClickWindowMs;
};

void applyDefaults() {
  current.cfgVersion = CURRENT_VERSION;
  current.doubleClickWindowMs = DOUBLE_CLICK_WINDOW_MS_DEFAULT;
  current.txPowerDbm = TX_POWER_DBM_DEFAULT;
}

// The whole config is written under a single NVS key, so a power loss
// mid-write can never leave some fields updated and others stale: NVS
// commits one key atomically, keeping the previous blob intact on failure.
void persist() {
  prefs.putBytes(BLOB_KEY, &current, sizeof(current));
}

// v1 layout had no tx_power field; v2 adds it with a default value.
void migrateV1ToV2(const ValuesV1 &old) {
  current.cfgVersion = 2;
  current.doubleClickWindowMs = old.doubleClickWindowMs;
  current.txPowerDbm = TX_POWER_DBM_DEFAULT;
  LOGW("Config migrated v1 -> v2, txPowerDbm defaulted to %d", current.txPowerDbm);
  persist();
}
} // namespace

void load() {
  prefs.begin("cfg", false);

  size_t storedSize = prefs.getBytesLength(BLOB_KEY);

  if (storedSize == 0) {
    LOGI("No config in NVS, applying defaults");
    applyDefaults();
    persist();
    return;
  }

  if (storedSize == sizeof(ValuesV1)) {
    ValuesV1 old;
    prefs.getBytes(BLOB_KEY, &old, sizeof(old));
    if (old.cfgVersion == 1) {
      migrateV1ToV2(old);
    } else {
      LOGE("Config blob size %u with unexpected version %u, applying defaults",
           (unsigned)storedSize, old.cfgVersion);
      applyDefaults();
      persist();
    }
  } else if (storedSize == sizeof(Values)) {
    prefs.getBytes(BLOB_KEY, &current, sizeof(current));
    if (current.cfgVersion != CURRENT_VERSION) {
      LOGE("Unknown cfgVersion %u, applying defaults", current.cfgVersion);
      applyDefaults();
      persist();
    }
  } else {
    LOGE("Config blob has unexpected size %u, applying defaults", (unsigned)storedSize);
    applyDefaults();
    persist();
  }

  LOGI("Config loaded: cfgVersion=%u dblClickMs=%u txPowerDbm=%d",
       current.cfgVersion, current.doubleClickWindowMs, current.txPowerDbm);
}

const Values &get() { return current; }

bool setDoubleClickWindowMs(uint16_t value) {
  if (value < DOUBLE_CLICK_WINDOW_MS_MIN || value > DOUBLE_CLICK_WINDOW_MS_MAX) {
    LOGW("Rejected doubleClickWindowMs=%u (out of [%u, %u])",
         value, DOUBLE_CLICK_WINDOW_MS_MIN, DOUBLE_CLICK_WINDOW_MS_MAX);
    return false;
  }
  current.doubleClickWindowMs = value;
  persist();
  return true;
}

bool setTxPowerDbm(int8_t value) {
  if (value < TX_POWER_DBM_MIN || value > TX_POWER_DBM_MAX) {
    LOGW("Rejected txPowerDbm=%d (out of [%d, %d])", value, TX_POWER_DBM_MIN, TX_POWER_DBM_MAX);
    return false;
  }
  current.txPowerDbm = value;
  persist();
  return true;
}

void reset() {
  prefs.clear();
  applyDefaults();
  persist();
  LOGI("Config reset to defaults");
}

} // namespace Config
