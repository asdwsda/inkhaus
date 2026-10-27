#include "ui_rocker.h"

#include <algorithm>

namespace ui {

using config::WidgetType;

LogicalKey mapRocker(PhysicalKey key, settings::Orientation, bool invert) {
  switch (key) {
    case PhysicalKey::Up: return invert ? LogicalKey::Next : LogicalKey::Prev;
    case PhysicalKey::Down: return invert ? LogicalKey::Prev : LogicalKey::Next;
    case PhysicalKey::Push: return LogicalKey::Push;
    case PhysicalKey::LongPush: return LogicalKey::LongPush;
  }
  return LogicalKey::Push;
}

bool isFocusable(WidgetType t) {
  switch (t) {
    case WidgetType::Sensor:
    case WidgetType::BinarySensor:
    case WidgetType::Weather: return false;
    default: return true;
  }
}
bool isSingleAction(WidgetType t) {
  return t == WidgetType::Light || t == WidgetType::Switch || t == WidgetType::Automation;
}

std::vector<int> focusStops(const FocusEnv &env, int subPage) {
  std::vector<const Placement *> v;
  if (env.flow) {
    for (const Placement &p : env.flow->placements) {
      if (p.page != subPage || !isFocusable(p.type)) continue;
      if (p.index < (int)env.unavailable.size() && env.unavailable[p.index]) continue;
      v.push_back(&p);
    }
  }
  std::stable_sort(v.begin(), v.end(), [](const Placement *a, const Placement *b) {
    return a->cellY != b->cellY ? a->cellY < b->cellY : a->cellX < b->cellX;
  });
  std::vector<int> out;
  for (auto *p : v) out.push_back(p->index);
  return out;
}

static const Placement *findPlacement(const FocusEnv &env, int widgetIndex) {
  if (!env.flow) return nullptr;
  for (const Placement &p : env.flow->placements)
    if (p.index == widgetIndex) return &p;
  return nullptr;
}

std::vector<int> innerStops(const FocusEnv &env, int widgetIndex) {
  const Placement *p = findPlacement(env, widgetIndex);
  if (!p) return {};
  switch (p->type) {
    case WidgetType::Cover:
    case WidgetType::Vacuum: return {1, 2, 3};
    case WidgetType::Climate:
      return env.climateModeEnabled ? std::vector<int>{1, 2, 3} : std::vector<int>{1, 2};
    default: return {};
  }
}

int focusedWidget(const FocusState &st, const FocusEnv &env) {
  if (st.mode == FocusMode::Tab || st.stop < 1) return -1;
  auto stops = focusStops(env, st.subPage);
  if (st.stop > (int)stops.size()) return -1;
  return stops[st.stop - 1];
}

namespace {

void changeTab(FocusState &st, const FocusEnv &env, int tab) {
  st.tab = tab;
  st.subPage = 0;
  st.group = groupForTab(env.orientation, env.tabCount, tab);
  st.stop = 0;
}

int entryInner(const FocusEnv &env, int widgetIndex) {
  const Placement *p = findPlacement(env, widgetIndex);
  if (p && p->type == WidgetType::Cover) return 2;  // stop
  return 1;
}

// Nearest sub-page in direction dir (+1/-1) from `from` with at least one focus stop; -1 if none.
int findPage(const FocusEnv &env, int from, int dir) {
  int count = env.flow ? env.flow->pageCount : 1;
  for (int p = from + dir; p >= 0 && p < count; p += dir)
    if (!focusStops(env, p).empty()) return p;
  return -1;
}

}  // namespace

FocusResult focusHandle(FocusState &st, const FocusEnv &env, LogicalKey key) {
  FocusResult res;
  st.ringVisible = true;
  switch (st.mode) {
    case FocusMode::Tab: {
      if (key == LogicalKey::Prev || key == LogicalKey::Next) {
        if (env.tabCount <= 0) return res;
        int d = key == LogicalKey::Next ? 1 : -1;
        int t = (st.tab + d + env.tabCount) % env.tabCount;
        if (t == st.tab) { res.effect = Effect::FocusMoved; return res; }
        changeTab(st, env, t);
        res.effect = Effect::TabChanged;
      } else if (key == LogicalKey::Push) {
        auto stops = focusStops(env, st.subPage);
        if (stops.empty()) return res;
        st.mode = FocusMode::Widget;
        st.stop = 1;
        res.effect = Effect::FocusMoved;
      } else {
        res.effect = Effect::RefreshNow;
      }
      return res;
    }
    case FocusMode::Widget: {
      auto stops = focusStops(env, st.subPage);
      int n = (int)stops.size();
      if (key == LogicalKey::Next) {
        if (st.stop < n) {
          ++st.stop;
          res.effect = Effect::FocusMoved;
        } else {
          int p = findPage(env, st.subPage, +1);
          if (p >= 0) {
            st.subPage = p;
            st.stop = 1;
            res.effect = Effect::SubPageChanged;
          } else {
            st.mode = FocusMode::Tab;
            st.stop = 0;
            res.effect = Effect::FocusMoved;
          }
        }
      } else if (key == LogicalKey::Prev) {
        if (st.stop > 1) {
          --st.stop;
          res.effect = Effect::FocusMoved;
        } else {
          int p = findPage(env, st.subPage, -1);
          if (p >= 0) {
            st.subPage = p;
            st.stop = (int)focusStops(env, p).size();
            res.effect = Effect::SubPageChanged;
          } else {
            st.mode = FocusMode::Tab;
            st.stop = 0;
            res.effect = Effect::FocusMoved;
          }
        }
      } else if (key == LogicalKey::Push) {
        int w = focusedWidget(st, env);
        if (w < 0) return res;
        const Placement *p = findPlacement(env, w);
        if (p && isSingleAction(p->type)) {
          res.effect = Effect::WidgetAction;
          res.widgetIndex = w;
          res.sub = 0;
        } else {
          st.mode = FocusMode::Inner;
          st.inner = entryInner(env, w);
          res.effect = Effect::FocusMoved;
          res.widgetIndex = w;
        }
      } else {
        st.mode = FocusMode::Tab;
        st.stop = 0;
        res.effect = Effect::FocusMoved;
      }
      return res;
    }
    case FocusMode::Inner: {
      int w = focusedWidget(st, env);
      if (w < 0) { st.mode = FocusMode::Widget; return res; }
      auto stops = innerStops(env, w);
      auto it = std::find(stops.begin(), stops.end(), st.inner);
      int i = it == stops.end() ? 0 : (int)(it - stops.begin());
      int n = (int)stops.size();
      res.widgetIndex = w;
      if (key == LogicalKey::Prev || key == LogicalKey::Next) {
        if (n == 0) return res;
        i = (i + (key == LogicalKey::Next ? 1 : n - 1)) % n;
        st.inner = stops[i];
        res.effect = Effect::FocusMoved;
      } else if (key == LogicalKey::Push) {
        res.effect = Effect::WidgetAction;
        res.sub = st.inner;
      } else {
        st.mode = FocusMode::Widget;
        st.inner = 0;
        res.effect = Effect::FocusMoved;
      }
      return res;
    }
  }
  return res;
}

void focusTouchTab(FocusState &st, settings::Orientation o, int tabCount, int tab) {
  st.mode = FocusMode::Tab;
  st.tab = tab;
  st.subPage = 0;
  st.stop = 0;
  st.inner = 0;
  st.group = groupForTab(o, tabCount, tab);
}

void focusOverflow(FocusState &st, settings::Orientation o, int tabCount) {
  st.group = nextGroup(o, tabCount, st.group);
}

bool focusPagerStep(FocusState &st, int delta, int pageCount) {
  int np = st.subPage + delta;
  if (np < 0 || np >= pageCount) return false;
  st.subPage = np;
  st.mode = FocusMode::Tab;
  st.stop = 0;
  st.inner = 0;
  return true;
}

}  // namespace ui
