#pragma once
// Serial screenshot and test commands (task 017). Lines on Serial:
//   shot            dump the current canvas: "SHOT-BEGIN <w> <h>\n" + PGM (P5) bytes + "\nSHOT-END\n"
//   shotz           same, run-length coded: "SHOT-BEGIN <w> <h> rle <n>\n" + n bytes + "\nSHOT-END\n"
//   tap <x> <y>     inject a touch tap        key up|down|push|long   inject a rocker key
//   pend <ms>       change the fake pending time (default 600)
//   shotp           dump the presenter's previous frame (what we believe is on the panel), as shotz
//   mem x y w h     dump the IT8951 image memory of a rect (screen coordinates): "MEM-BEGIN x y w h rle n"
//                   + n RLE bytes (the controller's 4-bit level as the R channel of readRectRGB) + "\nMEM-END\n"
//   cleanup 0|1     disable/enable the idle clean-up pass (debugging)
#include <M5Unified.h>

#include <functional>

#include "input.h"

namespace shot {

struct Hooks {
  const M5Canvas *frame = nullptr;                  // the canvas to dump
  const M5Canvas *previous = nullptr;               // the presenter's previous frame
  std::function<void(bool)> setCleanup;
  std::function<void(uint32_t)> setPendingMs;
};

// Reads available serial bytes; returns true if a command was handled. Injected events go to input.
bool poll(const Hooks &hooks);

}  // namespace shot
