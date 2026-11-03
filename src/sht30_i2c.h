#pragma once
// SHT30 on the M5Paper (I2C 0x44): the transfer. The protocol (CRC, conversion) is lib/device_report/sht30.h.
#include "sht30.h"

namespace sht30_i2c {

// One single-shot high-repeatability measurement (~20 ms). Returns false on a bus error or a CRC mismatch
// (out.ok is false then). UNVERIFIED (task 100): uses M5.Ex_I2C = port A pins G25 SDA / G32 SCL.
bool read(sht30::Reading &out);

}  // namespace sht30_i2c
