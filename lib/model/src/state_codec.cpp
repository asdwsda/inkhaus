#include "state_codec.h"

#include <cstring>

namespace statecodec {

namespace {

uint32_t crc32(const uint8_t* p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

struct W {
  std::string s;
  void u8(uint8_t v) { s.push_back(char(v)); }
  void u16(uint16_t v) { u8(v & 0xFF); u8(v >> 8); }
  void u32(uint32_t v) { u16(v & 0xFFFF); u16(v >> 16); }
  void i64(int64_t v) { for (int i = 0; i < 8; ++i) u8(uint8_t(uint64_t(v) >> (8 * i))); }
  void str(const std::string& v) {
    const size_t n = v.size() > 0xFFFF ? 0xFFFF : v.size();
    u16(uint16_t(n));
    s.append(v, 0, n);
  }
  void f64(double v) { char b[8]; std::memcpy(b, &v, 8); s.append(b, 8); }
};

struct R {
  const uint8_t* p;
  size_t n, i = 0;
  bool ok = true;
  bool need(size_t k) { if (!ok || n - i < k) ok = false; return ok; }
  uint8_t u8() { return need(1) ? p[i++] : 0; }
  uint16_t u16() { uint16_t a = u8(); return uint16_t(a | (uint16_t(u8()) << 8)); }
  uint32_t u32() { uint32_t a = u16(); return a | (uint32_t(u16()) << 16); }
  int64_t i64() { uint64_t v = 0; for (int k = 0; k < 8; ++k) v |= uint64_t(u8()) << (8 * k); return int64_t(v); }
  std::string str() {
    const size_t l = u16();
    if (!need(l)) return {};
    std::string r(reinterpret_cast<const char*>(p + i), l);
    i += l;
    return r;
  }
  double f64() { double v = 0; if (need(8)) { std::memcpy(&v, p + i, 8); i += 8; } return v; }
};

// Optional ints: bits 0-1; optional doubles: bits 2-9.
void putOpt(W& w, const EntityState& e) {
  uint16_t m = 0;
  const bool oi[] = {e.current_position.has_value(), e.brightness.has_value(), e.battery_level.has_value()};
  const bool od[] = {e.current_temperature.has_value(), e.temperature.has_value(), e.current_humidity.has_value(),
                     e.humidity.has_value(), e.min_temp.has_value(), e.max_temp.has_value(),
                     e.target_temp_step.has_value()};
  for (int i = 0; i < 3; ++i) if (oi[i]) m |= uint16_t(1u << i);
  for (int i = 0; i < 7; ++i) if (od[i]) m |= uint16_t(1u << (3 + i));
  w.u16(m);
  if (e.current_position) w.u32(uint32_t(*e.current_position));
  if (e.brightness) w.u32(uint32_t(*e.brightness));
  if (e.battery_level) w.u32(uint32_t(*e.battery_level));
  if (e.current_temperature) w.f64(*e.current_temperature);
  if (e.temperature) w.f64(*e.temperature);
  if (e.current_humidity) w.f64(*e.current_humidity);
  if (e.humidity) w.f64(*e.humidity);
  if (e.min_temp) w.f64(*e.min_temp);
  if (e.max_temp) w.f64(*e.max_temp);
  if (e.target_temp_step) w.f64(*e.target_temp_step);
}

void getOpt(R& r, EntityState& e) {
  const uint16_t m = r.u16();
  auto gi = [&](int b, std::optional<int>& o) { if (m & (1u << b)) o = int(int32_t(r.u32())); };
  auto gd = [&](int b, std::optional<double>& o) { if (m & (1u << b)) o = r.f64(); };
  gi(0, e.current_position);
  gi(1, e.brightness);
  gi(2, e.battery_level);
  gd(3, e.current_temperature);
  gd(4, e.temperature);
  gd(5, e.current_humidity);
  gd(6, e.humidity);
  gd(7, e.min_temp);
  gd(8, e.max_temp);
  gd(9, e.target_temp_step);
}

}  // namespace

std::string pack(const std::vector<EntityState>& states, int64_t epoch) {
  W w;
  w.s = "MPST";
  w.u8(kFormat);
  w.i64(epoch);
  w.u16(uint16_t(states.size() > 0xFFFF ? 0xFFFF : states.size()));
  size_t cnt = 0;
  for (const EntityState& e : states) {
    if (cnt++ >= 0xFFFF) break;
    w.str(e.id);
    w.str(e.state);
    w.u8(e.available ? 1 : 0);
    w.i64(e.last_changed);
    w.str(e.friendly_name);
    putOpt(w, e);
    w.str(e.hvac_mode);
    w.str(e.hvac_action);
    w.str(e.unit_of_measurement);
    w.str(e.device_class);
    w.str(e.temperature_unit);
    w.str(e.status);
    const size_t na = e.attrs.size() > 0xFF ? 0xFF : e.attrs.size();
    w.u8(uint8_t(na));
    for (size_t i = 0; i < na; ++i) {
      w.str(e.attrs[i].first);
      w.str(e.attrs[i].second);
    }
  }
  w.u32(crc32(reinterpret_cast<const uint8_t*>(w.s.data()), w.s.size()));
  return w.s;
}

bool unpack(const std::string& data, int64_t& epoch, std::vector<EntityState>& states) {
  if (data.size() < 4 + 1 + 8 + 2 + 4 || data.compare(0, 4, "MPST") != 0) return false;
  const size_t body = data.size() - 4;
  R tail{reinterpret_cast<const uint8_t*>(data.data()) + body, 4};
  if (tail.u32() != crc32(reinterpret_cast<const uint8_t*>(data.data()), body)) return false;
  R r{reinterpret_cast<const uint8_t*>(data.data()), body};
  r.i = 4;
  if (r.u8() != kFormat) return false;
  const int64_t ep = r.i64();
  const uint16_t cnt = r.u16();
  std::vector<EntityState> out;
  for (uint16_t k = 0; k < cnt && r.ok; ++k) {
    EntityState e;
    e.id = r.str();
    e.state = r.str();
    e.available = r.u8() != 0;
    e.last_changed = r.i64();
    e.friendly_name = r.str();
    getOpt(r, e);
    e.hvac_mode = r.str();
    e.hvac_action = r.str();
    e.unit_of_measurement = r.str();
    e.device_class = r.str();
    e.temperature_unit = r.str();
    e.status = r.str();
    const uint8_t na = r.u8();
    for (uint8_t i = 0; i < na && r.ok; ++i) {
      std::string a = r.str();
      std::string b = r.str();
      e.attrs.emplace_back(std::move(a), std::move(b));
    }
    if (r.ok) out.push_back(std::move(e));
  }
  if (!r.ok || r.i != body) return false;
  epoch = ep;
  states = std::move(out);
  return true;
}

std::vector<EntityState> filterIds(const std::vector<EntityState>& states, const std::vector<std::string>& ids) {
  std::vector<EntityState> out;
  for (const EntityState& e : states)
    for (const std::string& id : ids)
      if (id == e.id) {
        out.push_back(e);
        break;
      }
  return out;
}

Source restore(const std::string& rtc, const std::string& flash, const std::vector<std::string>& ids,
               int64_t& epoch, std::vector<EntityState>& states) {
  const std::string* src[2] = {&rtc, &flash};
  const Source kind[2] = {Source::Rtc, Source::Flash};
  for (int i = 0; i < 2; ++i) {
    int64_t ep = 0;
    std::vector<EntityState> v;
    if (src[i]->empty() || !unpack(*src[i], ep, v)) continue;
    v = filterIds(v, ids);
    if (v.empty()) continue;
    epoch = ep;
    states = std::move(v);
    return kind[i];
  }
  return Source::None;
}

}  // namespace statecodec
