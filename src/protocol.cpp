#include "protocol.h"

namespace Protocol {

namespace {
uint16_t nextSeq = 0;
}

uint16_t crc16(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

Frame buildButtonEvent(uint8_t payload, uint8_t postMask) {
  Frame frame;
  frame.magic = MAGIC;
  frame.protoVer = VERSION;
  frame.msgType = MSG_BUTTON_EVENT;
  frame.seq = nextSeq++;
  frame.uptimeMs = millis();
  frame.postMask = postMask;
  frame.payload = payload;
  frame.crc16 = crc16(reinterpret_cast<const uint8_t*>(&frame),
                      FRAME_SIZE - sizeof(uint16_t));
  return frame;
}

String toHex(const Frame& frame) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&frame);
  String out;
  out.reserve(FRAME_SIZE * 3);
  for (size_t i = 0; i < FRAME_SIZE; i++) {
    if (i > 0) {
      out += ' ';
    }
    if (bytes[i] < 0x10) {
      out += '0';
    }
    out += String(bytes[i], HEX);
  }
  out.toUpperCase();
  return out;
}

}  // namespace Protocol
