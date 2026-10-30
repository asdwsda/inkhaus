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
  void f32(float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    u32(b);
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
  bool f32(float &f) {
    uint32_t b;
    if (!u32(b)) return false;
    std::memcpy(&f, &b, 4);
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
  w.u32(uint32_t(c.wifiProfiles.size()));
  for (const WifiConfig &p : c.wifiProfiles) {
    w.str(p.ssid);
    w.str(p.password);
    w.u8(p.hasStaticIp);
    w.str(p.staticIp.ip);
    w.str(p.staticIp.gateway);
    w.str(p.staticIp.subnet);
    w.str(p.staticIp.dns);
    w.str(p.name);
    w.u8(uint32_t(p.haUrl));
  }
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
    w.u8(uint32_t(t.landscapeColumns));
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
      w.str(wd.attribute);
      w.strs(wd.details);
      w.u8(wd.unit.has_value());
      w.str(wd.unit ? *wd.unit : std::string());
      w.u8(wd.decimals.has_value());
      w.u8(wd.decimals ? uint32_t(*wd.decimals) : 0);
      const HistoryConfig &h = wd.history;
      w.u8(h.enabled);
      w.u32(uint32_t(h.hours));
      w.u8(uint32_t(h.type));
      w.u8(h.statistic.has_value());
      w.u8(h.statistic ? uint32_t(*h.statistic) : 0);
      w.u8(uint32_t(h.period));
      w.u8(uint32_t(h.source));
      w.u8(h.min.has_value());
      w.f32(h.min ? *h.min : 0.f);
      w.u8(h.max.has_value());
      w.f32(h.max ? *h.max : 0.f);
      w.u8(uint32_t(wd.display));
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
  uint32_t np = 0;
  if (!r.count(np, kMaxWifiProfiles)) return false;
  cn.wifiProfiles.resize(np);
  for (WifiConfig &p : cn.wifiProfiles) {
    r.str(p.ssid);
    r.str(p.password);
    r.boolean(p.hasStaticIp);
    r.str(p.staticIp.ip);
    r.str(p.staticIp.gateway);
    r.str(p.staticIp.subnet);
    r.str(p.staticIp.dns);
    r.str(p.name);
    uint32_t m = 0;
    r.u8(m);
    if (!r.ok || m > 2) return false;
    p.haUrl = HaUrlMode(m);
  }
  if (!cn.wifiProfiles.empty()) cn.wifi = cn.wifiProfiles[0];
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
    uint32_t lcols = 0;
    r.u8(lcols);
    if (!r.ok || (lcols != 4 && lcols != 5)) return false;
    t.landscapeColumns = int(lcols);
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
      uint32_t hasUnit = 0, hasDec = 0, dec = 0;
      std::string unit;
      r.str(wd.attribute);
      r.strs(wd.details, kMaxDetails);
      r.u8(hasUnit);
      r.str(unit);
      r.u8(hasDec);
      r.u8(dec);
      if (!r.ok || hasUnit > 1 || hasDec > 1 || dec > uint32_t(kMaxDecimals)) return false;
      if (hasUnit) wd.unit = unit;
      if (hasDec) wd.decimals = int(dec);
      HistoryConfig &h = wd.history;
      uint32_t hHours = 0, hType = 0, hHasStat = 0, hStat = 0, hPeriod = 0, hSource = 0;
      uint32_t hHasMin = 0, hHasMax = 0;
      float hMin = 0, hMax = 0;
      r.boolean(h.enabled);
      r.u32(hHours);
      r.u8(hType);
      r.u8(hHasStat);
      r.u8(hStat);
      r.u8(hPeriod);
      r.u8(hSource);
      r.u8(hHasMin);
      r.f32(hMin);
      r.u8(hHasMax);
      r.f32(hMax);
      uint32_t disp = 0;
      r.u8(disp);
      if (!r.ok || disp > uint32_t(WidgetDisplay::Graph) || hHours < uint32_t(kMinHistoryHours) || hHours > uint32_t(kMaxHistoryHours) ||
          hType > uint32_t(HistoryType::Step) || hHasStat > 1 ||
          hStat > uint32_t(HistoryStatistic::Change) || hPeriod > uint32_t(HistoryPeriod::Day) ||
          hSource > uint32_t(HistorySource::Statistics) || hHasMin > 1 || hHasMax > 1)
        return false;
      h.hours = int(hHours);
      h.type = HistoryType(hType);
      if (hHasStat) h.statistic = HistoryStatistic(hStat);
      h.period = HistoryPeriod(hPeriod);
      h.source = HistorySource(hSource);
      if (hHasMin) h.min = hMin;
      if (hHasMax) h.max = hMax;
      if (!r.ok || type > uint32_t(WidgetType::Vacuum) || hasSize > 1) return false;
      wd.type = WidgetType(type);
      wd.display = WidgetDisplay(disp);
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
  w.u32(fp.secretsSize);
  w.u32(fp.secretsMtime);
  return w.s + serializeConfig(cfg);
}

bool unpackCache(const std::string &file, Fingerprint &fp, Config &out) {
  if (file.size() < 16) return false;
  Reader r{reinterpret_cast<const uint8_t *>(file.data()), file.size()};
  Fingerprint f;
  r.u32(f.size);
  r.u32(f.mtime);
  r.u32(f.secretsSize);
  r.u32(f.secretsMtime);
  if (!deserializeConfig(r.p, r.n, out)) return false;
  fp = f;
  return true;
}

}  // namespace config
