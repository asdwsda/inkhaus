#include "sht30_i2c.h"

#include <Arduino.h>
#include <M5Unified.h>

namespace sht30_i2c {

namespace {
constexpr uint32_t kFreq = 100000;  // the sensor is happy up to 1 MHz; 100 kHz is the safe default on a short trace
}

// UNVERIFIED (task 100, device-checks): that the SHT30 sits on M5Unified's Ex_I2C pins (G25 SDA / G32 SCL on the
// M5Paper; the RTC and touch are on In_I2C G21/G22), that begin() on a port nobody installed works after our
// manual display bring-up, and that release() leaves the internal bus alone.
bool read(sht30::Reading &out) {
  out = sht30::Reading();
  auto &i2c = M5.Ex_I2C;
  if (!i2c.begin()) return false;
  const uint8_t cmd = sht30::kCmdLsb;
  bool ok = i2c.writeRegister(sht30::kAddress, sht30::kCmdMsb, &cmd, 1, kFreq);  // sends 0x24 0x00
  uint8_t raw[6] = {0};
  if (ok) {
    delay(sht30::kMeasureMs + 4);
    ok = i2c.start(sht30::kAddress, true, kFreq) && i2c.read(raw, sizeof raw, true) && i2c.stop();
  }
  i2c.release();
  if (!ok) return false;
  out = sht30::convert(raw);
  return out.ok;
}

}  // namespace sht30_i2c
