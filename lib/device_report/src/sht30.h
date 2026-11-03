#pragma once
// SHT30 (M5Paper on-board temperature/humidity sensor) protocol: CRC and conversion. Pure; the I2C transfer
// lives in src/sht30_i2c.cpp. Datasheet: Sensirion SHT3x-DIS, single shot, clock stretching disabled.
#include <cstddef>
#include <cstdint>

namespace sht30 {

constexpr uint8_t kAddress = 0x44;
// Single-shot measurement, high repeatability, clock stretching disabled (command 0x2400).
constexpr uint8_t kCmdMsb = 0x24, kCmdLsb = 0x00;
constexpr uint32_t kMeasureMs = 16;  // max measurement time at high repeatability (datasheet: 15.5 ms)

// CRC-8, polynomial 0x31, init 0xFF, no reflection, no final xor. crc8({0xBE, 0xEF}) == 0x92.
uint8_t crc8(const uint8_t *data, size_t len);

struct Reading {
  bool ok = false;       // both CRCs matched
  int32_t temp_x10 = 0;  // degrees C * 10, rounded
  int32_t hum_x10 = 0;   // percent RH * 10, rounded, clamped to 0..1000
};
// raw: the 6 bytes read after the measurement: T msb, T lsb, T crc, RH msb, RH lsb, RH crc.
Reading convert(const uint8_t raw[6]);

}  // namespace sht30
