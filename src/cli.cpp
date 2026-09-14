#include "cli.h"
#include <Arduino.h>
#include "version.h"
#include "log.h"
#include "config.h"
#include "post.h"

namespace Cli {

namespace {
String lineBuffer;

void printLog() {
  Log::Entry entries[Log::RING_CAPACITY];
  size_t n = Log::dump(entries, Log::RING_CAPACITY);
  Serial.printf("--- log (%u entries) ---\n", (unsigned)n);
  for (size_t i = 0; i < n; i++) {
    Serial.printf("[%lu][%s] %s\n", (unsigned long)entries[i].timestampMs,
                  Log::levelName(entries[i].level), entries[i].message);
  }
}

void printConfig() {
  const Config::Values &cfg = Config::get();
  Serial.printf("cfg_version: %u\n", cfg.cfgVersion);
  Serial.printf("double_click_window_ms: %u\n", cfg.doubleClickWindowMs);
  Serial.printf("tx_power_dbm: %d\n", cfg.txPowerDbm);
}

void handleConfigSet(const String &key, const String &valueStr) {
  long value = valueStr.toInt();

  if (key == "double_click_window_ms") {
    if (Config::setDoubleClickWindowMs((uint16_t)value)) {
      Serial.printf("OK: double_click_window_ms=%u\n", Config::get().doubleClickWindowMs);
    } else {
      Serial.printf("ERROR: double_click_window_ms must be in [%u, %u]\n",
                    Config::DOUBLE_CLICK_WINDOW_MS_MIN, Config::DOUBLE_CLICK_WINDOW_MS_MAX);
    }
  } else if (key == "tx_power_dbm") {
    if (Config::setTxPowerDbm((int8_t)value)) {
      Serial.printf("OK: tx_power_dbm=%d\n", Config::get().txPowerDbm);
    } else {
      Serial.printf("ERROR: tx_power_dbm must be in [%d, %d]\n",
                    Config::TX_POWER_DBM_MIN, Config::TX_POWER_DBM_MAX);
    }
  } else {
    Serial.printf("ERROR: unknown config key \"%s\"\n", key.c_str());
  }
}

void handleConfig(const String &args) {
  int firstSpace = args.indexOf(' ');
  String subcommand = (firstSpace == -1) ? args : args.substring(0, firstSpace);

  if (subcommand == "get") {
    printConfig();
  } else if (subcommand == "reset") {
    Config::reset();
  } else if (subcommand == "simulate_v1") {
    Config::simulateV1();
  } else if (subcommand == "set") {
    String rest = args.substring(firstSpace + 1);
    int secondSpace = rest.indexOf(' ');
    if (secondSpace == -1) {
      Serial.println("ERROR: usage: config set <key> <value>");
      return;
    }
    String key = rest.substring(0, secondSpace);
    String value = rest.substring(secondSpace + 1);
    handleConfigSet(key, value);
  } else {
    Serial.println("ERROR: usage: config <get|set|reset>");
  }
}

void dispatch(const String &line) {
  if (line == "version") {
    Version::print();
  } else if (line == "log") {
    printLog();
  } else if (line == "post") {
    Post::printReport();
  } else if (line.startsWith("config")) {
    String args = line.length() > 6 ? line.substring(7) : "";
    handleConfig(args);
  } else if (line.length() > 0) {
    Serial.printf("Unknown command: \"%s\"\n", line.c_str());
  }
}
} // namespace

void poll() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineBuffer.length() > 0) {
        dispatch(lineBuffer);
        lineBuffer = "";
      }
    } else {
      lineBuffer += c;
    }
  }
}

} // namespace Cli
