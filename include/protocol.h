#pragma once

#include <Arduino.h>

namespace Protocol {

constexpr uint16_t MAGIC = 0x4C42; // "LB" - LoRa Button
constexpr uint8_t VERSION = 1;

constexpr uint8_t MSG_BUTTON_EVENT = 0x01;

constexpr uint8_t PAYLOAD_SINGLE = 0x01;
constexpr uint8_t PAYLOAD_DOUBLE = 0x02;

// Wire format is little-endian throughout, matching the ESP32's native byte
// order so no swapping is needed on this side of the link.
struct __attribute__((packed)) Frame {
  uint16_t magic;
  uint8_t protoVer;
  uint8_t msgType;
  uint16_t seq;
  uint32_t uptimeMs;
  uint8_t postMask;
  uint8_t payload;
  uint16_t crc16;
};

constexpr size_t FRAME_SIZE = 14;
static_assert(sizeof(Frame) == FRAME_SIZE, "Frame must stay 14 bytes on the wire");

// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final xor.
uint16_t crc16(const uint8_t *data, size_t length);

// Fills every field except crc16, then computes and stores the CRC over
// bytes 0..11. `seq` is incremented internally on each call.
Frame buildButtonEvent(uint8_t payload, uint8_t postMask);

// Renders a frame as a spaced hex string, e.g. "42 4C 01 01 ...".
String toHex(const Frame &frame);

} // namespace Protocol
