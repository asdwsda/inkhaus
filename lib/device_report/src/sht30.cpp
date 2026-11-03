#include "sht30.h"

namespace sht30 {

uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x31) : uint8_t(crc << 1);
  }
  return crc;
}

Reading convert(const uint8_t raw[6]) {
  Reading r;
  if (crc8(raw, 2) != raw[2] || crc8(raw + 3, 2) != raw[5]) return r;
  const int32_t t = (raw[0] << 8) | raw[1];
  const int32_t h = (raw[3] << 8) | raw[4];
  // T = -45 + 175 * t / 65535 ; RH = 100 * h / 65535. Integer maths in tenths, rounded to nearest.
  r.temp_x10 = -450 + (1750 * t + 32767) / 65535;
  r.hum_x10 = (1000 * h + 32767) / 65535;
  if (r.hum_x10 > 1000) r.hum_x10 = 1000;
  r.ok = true;
  return r;
}

}  // namespace sht30
