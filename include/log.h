#pragma once

#include <Arduino.h>

enum class LogLevel : uint8_t { LOG_ERROR = 0, LOG_WARN = 1, LOG_INFO = 2, LOG_DEBUG = 3 };

namespace Log {

constexpr size_t RING_CAPACITY = 32;
constexpr size_t MESSAGE_MAX_LEN = 64;

struct Entry {
  uint32_t timestampMs;
  LogLevel level;
  char message[MESSAGE_MAX_LEN];
};

void init(LogLevel minLevel);
void write(LogLevel level, const char *format, ...);
size_t count();
// Returns entries oldest-first, up to `maxCount`. Returns how many were written into `out`.
size_t dump(Entry *out, size_t maxCount);
const char *levelName(LogLevel level);

} // namespace Log

#define LOGE(...) Log::write(LogLevel::LOG_ERROR, __VA_ARGS__)
#define LOGW(...) Log::write(LogLevel::LOG_WARN, __VA_ARGS__)
#define LOGI(...) Log::write(LogLevel::LOG_INFO, __VA_ARGS__)
#define LOGD(...) Log::write(LogLevel::LOG_DEBUG, __VA_ARGS__)
