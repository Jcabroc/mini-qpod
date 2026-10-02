#pragma once
#include <stdint.h>

// Byte-for-byte layout emitted by codigo_control_lite_v1.ino on AVR.
enum PS2J_Button : uint16_t {
  PS2J_SELECT = (1u << 0), PS2J_START = (1u << 1),
  PS2J_SQUARE = (1u << 2), PS2J_CROSS = (1u << 3),
  PS2J_CIRCLE = (1u << 4), PS2J_TRIANGLE = (1u << 5),
  PS2J_R3 = (1u << 6)
};
struct __attribute__((packed)) PS2J_Packet {
  int8_t lx, ly, rx, ry;
  uint16_t btn;
  uint8_t mode3, seq, flags;
};
static_assert(sizeof(PS2J_Packet) == 9, "Control Lite PS2J v1 must be 9 bytes");
static const uint8_t PS2J_FLAG_LITE = 1;
