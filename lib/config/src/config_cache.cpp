#include "config_cache.h"

#include <cstring>

namespace config {
namespace {

constexpr char kMagic[4] = {'M', 'P', 'C', 'C'};
constexpr uint32_t kMaxString = kMaxFileBytes;
constexpr uint32_t kMaxCount = 4096;

uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

struct Writer {
  std::string s;
  void u8(uint32_t v) { s.push_back(char(v & 0xFF)); }
  void u32(uint32_t v) {
    for (int i = 0; i < 4; ++i) u8(v >> (8 * i));
  }
  void str(const std::string &v) {
    u32(uint32_t(v.size()));
    s += v;
  }
  void strs(const std::vector<std::string> &v) {
    u32(uint32_t(v.size()));
    for (const auto &x : v) str(x);
  }
};

struct Reader {
  const uint8_t *p;
  size_t n;
  bool ok = true;
  bool u8(uint32_t &v) {
    if (!ok || n < 1) return ok = false;
    v = *p++;
    --n;
    return true;
  }
  bool u32(uint32_t &v) {
    if (!ok || n < 4) return ok = false;
    v = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    p += 4;
    n -= 4;
    return true;
  }
  bool boolean(bool &b) {
    uint32_t v;
    if (!u8(v) || v > 1) return ok = false;
    b = v == 1;
    return true;
  }
  bool str(std::string &v) {
    uint32_t len;
    if (!u32(len) || len > kMaxString || len > n) return ok = false;
    v.assign(reinterpret_cast<const char *>(p), len);
    p += len;
    n -= len;
    return true;
  }
  bool count(uint32_t &c, uint32_t max) {
    // Every element needs at least one byte, so a count above the rest is bogus.
    if (!u32(c) || c > max || c > n) return ok = false;
    return true;
  }
  bool strs(std::vector<std::string> &v, uint32_t max) {
    uint32_t c;
    if (!count(c, max)) return false;
    v.assign(c, std::string());
    for (auto &x : v)
      if (!str(x)) return false;
    return true;
  }
};

}  // namespace

std::string serializeConfig(const Config &cfg) {
  Writer w;
  w.s.assign(kMagic, 4);
  w.u8(kCacheFormat);
  w.u32(uint32_t(cfg.version));
  w.str(cfg.timezone);
  w.u8(cfg.hasConnection);
  const ConnectionConfig &c = cfg.connection;
  w.str(c.wifi.ssid);
  w.str(c.wifi.password);
  w.u8(c.wifi.hasStaticIp);
  w.str(c.wifi.staticIp.ip);
  w.str(c.wifi.staticIp.gateway);
  w.str(c.wifi.staticIp.subnet);
  w.str(c.wifi.staticIp.dns);
  w.str(c.ha.internalUrl);
  w.str(c.ha.externalUrl);
  w.str(c.ha.token);
  w.u32(uint32_t(cfg.groups.size()));
  for (const Group &g : cfg.groups) {
    w.str(g.name);
    w.strs(g.members);
  }
  w.u32(uint32_t(cfg.tabs.size()));
  for (const Tab &t : cfg.tabs) {
    w.str(t.title);
    w.u32(uint32_t(t.widgets.size()));
    for (const Widget &wd : t.widgets) {
      w.u8(uint32_t(wd.type));
      w.str(wd.entity);
      w.str(wd.group);
      w.str(wd.name);
      w.u8(wd.size.has_value());
      w.u8(wd.size ? uint32_t(wd.size->w) : 0);
      w.u8(wd.size ? uint32_t(wd.size->h) : 0);
      w.strs(wd.entities);
    }
  }
  w.strs(cfg.entityIds);
  w.u32(crc32(reinterpret_cast<const uint8_t *>(w.s.data()), w.s.size()));
  return w.s;
}

bool deserializeConfig(const uint8_t *data, size_t len, Config &out) {
  if (!data || len < sizeof(kMagic) + 1 + 4) return false;
  uint32_t stored = uint32_t(data[len - 4]) | uint32_t(data[len - 3]) << 8 |
                    uint32_t(data[len - 2]) << 16 | uint32_t(data[len - 1]) << 24;
  if (crc32(data, len - 4) != stored) return false;
  if (std::memcmp(data, kMagic, 4) != 0 || data[4] != kCacheFormat) return false;
  Reader r{data + 5, len - 5 - 4};
  Config c;
  uint32_t v = 0;
  if (!r.u32(v)) return false;
  c.version = int(v);
  r.str(c.timezone);
  r.boolean(c.hasConnection);
  ConnectionConfig &cn = c.connection;
  r.str(cn.wifi.ssid);
  r.str(cn.wifi.password);
  r.boolean(cn.wifi.hasStaticIp);
  r.str(cn.wifi.staticIp.ip);
  r.str(cn.wifi.staticIp.gateway);
  r.str(cn.wifi.staticIp.subnet);
  r.str(cn.wifi.staticIp.dns);
  r.str(cn.ha.internalUrl);
  r.str(cn.ha.externalUrl);
  r.str(cn.ha.token);
  uint32_t ng = 0;
  if (!r.count(ng, kMaxCount)) return false;
  c.groups.resize(ng);
  for (Group &g : c.groups) {
    r.str(g.name);
    r.strs(g.members, kMaxCount);
    if (!r.ok) return false;
  }
  uint32_t nt = 0;
  if (!r.count(nt, kMaxTabs)) return false;
  c.tabs.resize(nt);
  for (Tab &t : c.tabs) {
    r.str(t.title);
    uint32_t nw = 0;
    if (!r.count(nw, kMaxWidgetsPerTab)) return false;
    t.widgets.resize(nw);
    for (Widget &wd : t.widgets) {
      uint32_t type = 0, hasSize = 0, sw = 0, sh = 0;
      r.u8(type);
      r.str(wd.entity);
      r.str(wd.group);
      r.str(wd.name);
      r.u8(hasSize);
      r.u8(sw);
      r.u8(sh);
      r.strs(wd.entities, kMaxCount);
      if (!r.ok || type > uint32_t(WidgetType::Vacuum) || hasSize > 1) return false;
      wd.type = WidgetType(type);
      if (hasSize) {
        if (sw < 1 || sw > uint32_t(kMaxWidgetSize) || sh < 1 || sh > uint32_t(kMaxWidgetSize))
          return false;
        wd.size = WidgetSize{int(sw), int(sh)};
      }
    }
    if (!r.ok) return false;
  }
  r.strs(c.entityIds, kMaxCount);
  if (!r.ok || r.n != 0) return false;
  out = std::move(c);
  return true;
}

std::string packCache(const Fingerprint &fp, const Config &cfg) {
  Writer w;
  w.u32(fp.size);
  w.u32(fp.mtime);
  return w.s + serializeConfig(cfg);
}

bool unpackCache(const std::string &file, Fingerprint &fp, Config &out) {
  if (file.size() < 8) return false;
  Reader r{reinterpret_cast<const uint8_t *>(file.data()), file.size()};
  Fingerprint f;
  r.u32(f.size);
  r.u32(f.mtime);
  if (!deserializeConfig(r.p, r.n, out)) return false;
  fp = f;
  return true;
}

}  // namespace config
