#include "art_fetch.h"

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

}  // namespace art
