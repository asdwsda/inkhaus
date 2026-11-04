#include "art_fetch.h"

#include "art_image.h"

namespace art {


namespace {

bool startsWith(const std::string &s, const char *p) { return s.rfind(p, 0) == 0; }

// "scheme://host[:port]" prefix of a URL, lower-case scheme/host; empty if not http(s).
std::string origin(const std::string &u) {
  size_t schemeEnd = std::string::npos;
  if (startsWith(u, "http://")) schemeEnd = 7;
  else if (startsWith(u, "https://")) schemeEnd = 8;
  if (schemeEnd == std::string::npos) return "";
  size_t end = u.find_first_of("/?#", schemeEnd);
  std::string o = u.substr(0, end == std::string::npos ? u.size() : end);
  for (char &c : o)
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return o;
}

}  // namespace

bool isDemoPicture(const std::string &picture) { return startsWith(picture, "demo:"); }

FetchPlan planFetch(const std::string &baseUrl, const std::string &picture) {
  FetchPlan p;
  if (picture.empty() || picture.size() > kMaxPictureUrl) return p;
  if (picture[0] == '/') {
    if (picture.size() > 1 && picture[1] == '/') return p;  // protocol-relative: not supported
    const std::string base = origin(baseUrl);
    if (base.empty()) return p;
    p.ok = true;
    p.url = base + picture;
    p.sendToken = true;
    return p;
  }
  const std::string o = origin(picture);
  if (o.empty()) return p;
  p.ok = true;
  p.url = picture;
  p.sendToken = o == origin(baseUrl);
  return p;
}

bool bodyWithinCap(long contentLength) { return contentLength < 0 || size_t(contentLength) <= kMaxArtBytes; }

ImageInfo imageInfo(const uint8_t *d, size_t n) {
  ImageInfo o;
  if (n >= 24 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') {
    o.kind = ImageKind::Png;  // IHDR: width, height big-endian at 16 and 20
    o.w = int((uint32_t(d[16]) << 24) | (uint32_t(d[17]) << 16) | (uint32_t(d[18]) << 8) | d[19]);
    o.h = int((uint32_t(d[20]) << 24) | (uint32_t(d[21]) << 16) | (uint32_t(d[22]) << 8) | d[23]);
    if (o.w < 0 || o.h < 0) o.w = o.h = 0;
    return o;
  }
  if (n >= 4 && d[0] == 0xFF && d[1] == 0xD8) {
    o.kind = ImageKind::Jpeg;
    size_t i = 2;
    while (i + 4 <= n) {
      if (d[i] != 0xFF) { ++i; continue; }
      const uint8_t m = d[i + 1];
      if (m == 0xFF) { ++i; continue; }
      if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) { i += 2; continue; }
      const size_t seg = (size_t(d[i + 2]) << 8) | d[i + 3];
      const bool sof = m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC;
      if (sof) {
        if (i + 9 > n) return o;
        o.h = (d[i + 5] << 8) | d[i + 6];
        o.w = (d[i + 7] << 8) | d[i + 8];
        return o;
      }
      if (m == 0xDA) return o;  // scan data before any SOF
      i += 2 + seg;
    }
  }
  return o;
}

DecodePlan planDecode(int w, int h) {
  DecodePlan p;
  if (w < 1 || h < 1 || w > kMaxSourceDim || h > kMaxSourceDim) return p;
  int shift = 0;
  while (shift < 3 && (w < h ? w : h) >> (shift + 1) >= kPopupArtPx) ++shift;
  p.shift = shift;
  p.outW = (w + (1 << shift) - 1) >> shift;
  p.outH = (h + (1 << shift) - 1) >> shift;
  p.ok = long(p.outW) * long(p.outH) <= kMaxDecodePixels;
  return p;
}

}  // namespace art
