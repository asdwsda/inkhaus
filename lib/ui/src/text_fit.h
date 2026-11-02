#pragma once
// Font fallback for narrow 1x1 cells (layout.md 2b, task 094). Pure logic: the caller supplies the width measure.
#include <functional>
#include <string>

namespace ui {

struct TextFit {
  bool small = false;  // true: F18, false: F24
  std::string text;
};
// Width in px of `text` in F24 (small = false) or F18 (small = true).
using TextMeasure = std::function<int(bool small, const std::string &text)>;

// Name line: F24 if it fits, else F18, else F18 truncated with the ellipsis (never below F18, never wraps).
TextFit fitName(const TextMeasure &m, const std::string &text, int maxW, const std::string &ellipsis);

// State line: F24 (if allowLarge) if it fits, else F18, else the short form at F18, else that truncated.
TextFit fitState(const TextMeasure &m, const std::string &text, int maxW, const std::string &ellipsis,
                 bool allowLarge);

// Short forms of layout.md 2b: Unavailable -> n/a, Unknown -> ?, Opening -> Open<ellipsis>,
// Closing -> Close<ellipsis>. Other text is returned unchanged.
std::string shortState(const std::string &text, const std::string &ellipsis);

}  // namespace ui
