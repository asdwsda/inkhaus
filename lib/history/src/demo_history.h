#pragma once
// Synthetic history for demo mode (task 068): deterministic per entity id, anchored to the window the
// popup asks for, so charts look live without a network. Pure logic.
#include <cstdint>
#include <string>

#include "collector.h"

namespace history {

struct DemoParams {
  std::string entity;          // seeds the curve; keywords (temperature, humidity, power, ...) pick the profile
  bool binary = false;         // on/off steps (values 0/1)
  bool bar = false;            // buckets (cumulative statistic) instead of columns
  int64_t start = 0, end = 0;  // epoch seconds
  int columns = 0;             // line / step
  int64_t bucketSeconds = 0;   // bar
  int bucketCount = 0;         // bar
};

// Plausible value range of the entity's profile; every generated value lies inside it.
void demoRange(const std::string& entity, float& lo, float& hi);

Series demoSeries(const DemoParams& p);

}  // namespace history
