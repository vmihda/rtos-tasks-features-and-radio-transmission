#include "log.h"
#include <stdarg.h>

namespace Log {

namespace {
Entry ring[RING_CAPACITY];
size_t writeIndex = 0;
size_t storedCount = 0;
LogLevel currentMinLevel = LogLevel::LOG_INFO;
} // namespace

void init(LogLevel minLevel) {
  currentMinLevel = minLevel;
  writeIndex = 0;
  storedCount = 0;
}

const char *levelName(LogLevel level) {
  switch (level) {
    case LogLevel::LOG_ERROR: return "ERROR";
    case LogLevel::LOG_WARN:  return "WARN";
    case LogLevel::LOG_INFO:  return "INFO";
    case LogLevel::LOG_DEBUG: return "DEBUG";
  }
  return "?";
}

void write(LogLevel level, const char *format, ...) {
  if (static_cast<uint8_t>(level) > static_cast<uint8_t>(currentMinLevel)) {
    return;
  }

  Entry &entry = ring[writeIndex];
  entry.timestampMs = millis();
  entry.level = level;

  va_list args;
  va_start(args, format);
  vsnprintf(entry.message, MESSAGE_MAX_LEN, format, args);
  va_end(args);

  Serial.printf("[%lu][%s] %s\n", (unsigned long)entry.timestampMs, levelName(level), entry.message);

  writeIndex = (writeIndex + 1) % RING_CAPACITY;
  if (storedCount < RING_CAPACITY) {
    storedCount++;
  }
}

size_t count() { return storedCount; }

size_t dump(Entry *out, size_t maxCount) {
  size_t n = storedCount < maxCount ? storedCount : maxCount;
  // Oldest entry is at writeIndex when the ring is full; otherwise entries start at 0.
  size_t start = (storedCount < RING_CAPACITY) ? 0 : writeIndex;

  for (size_t i = 0; i < n; i++) {
    out[i] = ring[(start + i) % RING_CAPACITY];
  }
  return n;
}

} // namespace Log
