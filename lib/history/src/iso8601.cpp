#include "iso8601.h"

namespace history {

int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

namespace {
bool digits(const char* s, size_t len, size_t pos, size_t n, int& out) {
  if (pos + n > len) return false;
  int v = 0;
  for (size_t i = 0; i < n; ++i) {
    char c = s[pos + i];
    if (c < '0' || c > '9') return false;
    v = v * 10 + (c - '0');
  }
  out = v;
  return true;
}
}  // namespace

bool parseIso8601(const char* s, size_t len, int64_t& epoch) {
  if (!s) return false;
  int Y, M, D, h, m, sec;
  if (!digits(s, len, 0, 4, Y) || len < 19 || s[4] != '-' || !digits(s, len, 5, 2, M) || s[7] != '-' ||
      !digits(s, len, 8, 2, D) || (s[10] != 'T' && s[10] != ' ') || !digits(s, len, 11, 2, h) ||
      s[13] != ':' || !digits(s, len, 14, 2, m) || s[16] != ':' || !digits(s, len, 17, 2, sec))
    return false;
  if (M < 1 || M > 12 || D < 1 || D > 31 || h > 23 || m > 59 || sec > 60) return false;
  size_t p = 19;
  if (p < len && s[p] == '.') {
    ++p;
    size_t f = p;
    while (p < len && s[p] >= '0' && s[p] <= '9') ++p;
    if (p == f) return false;
  }
  int64_t off = 0;
  if (p < len) {
    char c = s[p];
    if (c == 'Z' || c == 'z') {
      ++p;
    } else if (c == '+' || c == '-') {
      int oh, om = 0;
      if (!digits(s, len, p + 1, 2, oh)) return false;
      p += 3;
      if (p < len && s[p] == ':') ++p;
      if (p < len && !digits(s, len, p, 2, om)) return false;
      if (p < len) p += 2;
      off = (oh * 3600 + om * 60) * (c == '-' ? -1 : 1);
    } else {
      return false;
    }
  }
  if (p != len) return false;
  epoch = daysFromCivil(Y, static_cast<unsigned>(M), static_cast<unsigned>(D)) * 86400 + h * 3600 + m * 60 +
          sec - off;
  return true;
}

}  // namespace history
