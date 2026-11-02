#include "text_fit.h"

namespace ui {

namespace {

// Longest prefix (never inside a UTF-8 sequence) that fits with the ellipsis, in F18.
std::string truncate(const TextMeasure &m, const std::string &s, int maxW, const std::string &ellipsis) {
  std::string t = s;
  while (!t.empty()) {
    size_t cut = t.size() - 1;
    while (cut > 0 && (static_cast<unsigned char>(t[cut]) & 0xC0) == 0x80) --cut;
    t.erase(cut);
    while (!t.empty() && t.back() == ' ') t.pop_back();
    const std::string cand = t + ellipsis;
    if (m(true, cand) <= maxW) return cand;
  }
  return ellipsis;
}

}  // namespace

TextFit fitName(const TextMeasure &m, const std::string &text, int maxW, const std::string &ellipsis) {
  if (m(false, text) <= maxW) return {false, text};
  if (m(true, text) <= maxW) return {true, text};
  return {true, truncate(m, text, maxW, ellipsis)};
}

TextFit fitState(const TextMeasure &m, const std::string &text, int maxW, const std::string &ellipsis,
                 bool allowLarge) {
  if (allowLarge && m(false, text) <= maxW) return {false, text};
  if (m(true, text) <= maxW) return {true, text};
  const std::string s = shortState(text, ellipsis);
  if (m(true, s) <= maxW) return {true, s};
  return {true, truncate(m, s, maxW, ellipsis)};
}

std::string shortState(const std::string &text, const std::string &ellipsis) {
  if (text == "Unavailable") return "n/a";
  if (text == "Unknown") return "?";
  if (text == "Opening") return "Open" + ellipsis;
  if (text == "Closing") return "Close" + ellipsis;
  return text;
}

}  // namespace ui
