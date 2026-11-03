#include "sht30_i2c.h"

#include <Arduino.h>
#include <M5Unified.h>

namespace sht30_i2c {

namespace {
constexpr uint32_t kFreq = 100000;  // the sensor is happy up to 1 MHz; 100 kHz is the safe default on a short trace
}

// UNVERIFIED (task 100, device-checks): that the SHT30 sits on the internal bus (M5.In_I2C, G21 SDA / G22 SCL, shared
// with GT911 and BM8563; M5Unified's M5Paper pin table puts Ex_I2C on Port A G25/G32). M5.begin() already set the
// internal bus up and the touch/RTC code own it, so we neither begin() nor release() it. Single-threaded use only
// (called early in setup, before the touch task could run a transfer in parallel).
bool read(sht30::Reading &out) {
  out = sht30::Reading();
  auto &i2c = M5.In_I2C;
  const uint8_t cmd = sht30::kCmdLsb;
  bool ok = i2c.writeRegister(sht30::kAddress, sht30::kCmdMsb, &cmd, 1, kFreq);  // sends 0x24 0x00
  uint8_t raw[6] = {0};
  if (ok) {
    delay(sht30::kMeasureMs + 4);
    ok = i2c.start(sht30::kAddress, true, kFreq) && i2c.read(raw, sizeof raw, true) && i2c.stop();
  }
  if (!ok) return false;
  out = sht30::convert(raw);
  return out.ok;
}

}  // namespace sht30_i2c
