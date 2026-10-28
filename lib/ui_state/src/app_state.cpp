#include "app_state.h"

#include <algorithm>

#include "demo_states.h"
#include "fake_actions.h"
#include "widget_view.h"

namespace uistate {

using config::WidgetType;

void AppState::init(const config::Config &cfg, settings::Orientation o, bool demo) {
  cfg_ = cfg;
  orientation_ = o;
  demo_ = demo;
  screen_ = Screen::Tabs;
  store_ = StateStore();
  if (demo) store_.update(demoStates(cfg_));
  focus_ = ui::FocusState();
  focus_.group = ui::groupForTab(o, int(cfg_.tabs.size()), 0);
  shownTab_ = 0;
  shownSubPage_ = 0;
  pending_.clear();
  marks_.clear();
  switchDue_ = false;
  full_ = false;
  relayout();
  refreshFocusFlow();
}

void AppState::initError(const std::vector<config::ConfigError> &errors, settings::Orientation o) {
  screen_ = Screen::ConfigError;
  errors_ = errors;
  orientation_ = o;
}

void AppState::relayout() {
  layout_ = ui::computeLayout(orientation_, cfg_.tabs, shownTab_, shownSubPage_, focus_.group);
  shownSubPage_ = layout_.pager.subPage;
}

void AppState::refreshFocusFlow() {
  const int t = std::clamp(focus_.tab, 0, std::max(0, int(cfg_.tabs.size()) - 1));
  focusFlow_ = cfg_.tabs.empty() ? ui::Flow() : ui::computeFlow(orientation_, cfg_.tabs[size_t(t)].widgets);
}

const config::Widget *AppState::widgetAt(int tab, int idx) const {
  if (tab < 0 || size_t(tab) >= cfg_.tabs.size()) return nullptr;
  const auto &ws = cfg_.tabs[size_t(tab)].widgets;
  if (idx < 0 || size_t(idx) >= ws.size()) return nullptr;
  return &ws[size_t(idx)];
}

std::vector<const EntityState *> AppState::membersOf(const config::Widget &w) const {
  std::vector<const EntityState *> m;
  for (const std::string &id : w.entities) m.push_back(store_.get(id));
  return m;
}

ui::FocusEnv AppState::focusEnv() const {
  ui::FocusEnv env;
  env.orientation = orientation_;
  env.tabCount = int(cfg_.tabs.size());
  env.flow = &focusFlow_;
  const int t = std::clamp(focus_.tab, 0, std::max(0, env.tabCount - 1));
  if (env.tabCount > 0) {
    const auto &ws = cfg_.tabs[size_t(t)].widgets;
    env.unavailable.assign(ws.size(), false);
    for (size_t i = 0; i < ws.size(); ++i) {
      bool anyAvail = false;
      for (auto *m : membersOf(ws[i])) anyAvail |= (m && m->available);
      // Loading widgets (nothing cached) stay focusable; only known-unavailable ones are skipped.
      bool anyPresent = false;
      for (auto *m : membersOf(ws[i])) anyPresent |= (m != nullptr);
      env.unavailable[i] = anyPresent && !anyAvail;
    }
  }
  return env;
}

std::string AppState::widgetTitle(int idx) const {
  const config::Widget *w = widgetAt(shownTab_, idx);
  if (!w) return "";
  const EntityState *first = nullptr;
  for (auto *m : membersOf(*w)) if (m) { first = m; break; }
  return widgetName(*w, first);
}

WidgetData AppState::widgetData(int idx) const {
  WidgetData d;
  const config::Widget *w = widgetAt(shownTab_, idx);
  if (!w) return d;
  d.type = w->type;
  d.group = !w->group.empty();
  d.members = membersOf(*w);
  for (const std::string &id : w->entities) {
    auto it = marks_.find(id);
    if (it != marks_.end() && it->second == Mark::Failed) d.failed = true;
  }
  for (const Pending &p : pending_) {
    if (p.tab != shownTab_ || p.widget != idx) continue;
    d.pending = true;
    d.pendingSub = p.sub;
    // Toggles and climate show the requested result at once; cover and vacuum keep the real state.
    if (w->type != WidgetType::Cover && w->type != WidgetType::Vacuum) {
      for (size_t i = 0; i < d.members.size(); ++i) {
        for (const EntityState &s : p.states) {
          if (d.members[i] && s.id == d.members[i]->id) d.members[i] = &s;
        }
      }
    }
  }
  return d;
}

int AppState::focusedWidgetIndex() const {
  if (focus_.tab != shownTab_ || screen_ != Screen::Tabs) return -1;
  return ui::focusedWidget(focus_, focusEnv());
}

bool AppState::takeFull() {
  const bool f = full_;
  full_ = false;
  return f;
}

bool AppState::applyTabSwitch() {
  switchDue_ = false;
  if (shownTab_ == focus_.tab && shownSubPage_ == focus_.subPage) return false;
  shownTab_ = focus_.tab;
  shownSubPage_ = focus_.subPage;
  relayout();
  full_ = true;
  return true;
}

bool AppState::runAction(int idx, int sub, uint32_t now) {
  const config::Widget *w = widgetAt(shownTab_, idx);
  if (!w) return false;
  // Climate taps accumulate in the pending target; other controls ignore a repeat of the same
  // control while pending.
  Pending *existing = nullptr;
  for (Pending &p : pending_) if (p.tab == shownTab_ && p.widget == idx) existing = &p;
  std::vector<const EntityState *> base = membersOf(*w);
  if (existing) {
    if (w->type == WidgetType::Climate) {
      for (size_t i = 0; i < base.size(); ++i)
        for (const EntityState &s : existing->states)
          if (base[i] && s.id == base[i]->id) base[i] = &s;
    } else if (existing->sub == sub) {
      return false;
    }
  }
  ActionResult r = fakeAction(w->type, sub, base);
  if (!r.changed) return false;
  if (existing) {
    existing->sub = sub;
    existing->commitAt = now + pendingMs_;
    existing->states = std::move(r.states);
    existing->seenMark = false;
  } else {
    Pending p;
    p.tab = shownTab_;
    p.widget = idx;
    p.sub = sub;
    p.commitAt = now + pendingMs_;
    p.states = std::move(r.states);
    pending_.push_back(std::move(p));
  }
  if (marksDriven_) {
    for (Pending &p : pending_)
      if (p.tab == shownTab_ && p.widget == idx) p.commitAt = now + safetyMs_;
  }
  if (sink_) {
    for (const Pending &p : pending_)
      if (p.tab == shownTab_ && p.widget == idx) { sink_(p.tab, p.widget, p.sub, p.states); break; }
  }
  return true;
}

bool AppState::setMarks(const std::map<std::string, Mark> &marks) {
  bool changed = marks != marks_;
  marks_ = marks;
  for (size_t i = 0; i < pending_.size();) {
    Pending &p = pending_[i];
    bool anyPending = false, anyFailed = false;
    for (const EntityState &s : p.states) {
      auto it = marks_.find(s.id);
      if (it == marks_.end()) continue;
      anyPending |= it->second == Mark::Pending;
      anyFailed |= it->second == Mark::Failed;
    }
    if (anyPending && !anyFailed) p.seenMark = true;
    if (anyFailed || (!anyPending && p.seenMark)) {
      pending_.erase(pending_.begin() + long(i));
      changed = true;
    } else {
      ++i;
    }
  }
  return changed;
}

bool AppState::tick(uint32_t now) {
  bool changed = false;
  if (switchDue_ && int32_t(now - switchAt_) >= 0) changed |= applyTabSwitch();
  for (size_t i = 0; i < pending_.size();) {
    if (int32_t(now - pending_[i].commitAt) >= 0) {
      if (!marksDriven_) store_.update(pending_[i].states);
      pending_.erase(pending_.begin() + long(i));
      changed = true;
    } else {
      ++i;
    }
  }
  return changed;
}

bool AppState::touch(ui::Point p, uint32_t now) {
  if (screen_ != Screen::Tabs) return false;
  const ui::Hit hit = ui::hitTest(layout_, p);
  const int tabCount = int(cfg_.tabs.size());
  switch (hit.kind) {
    case ui::HitKind::Tab: {
      ui::focusTouchTab(focus_, orientation_, tabCount, hit.index);
      switchDue_ = false;
      refreshFocusFlow();
      const bool same = shownTab_ == focus_.tab && shownSubPage_ == 0;
      shownTab_ = focus_.tab;
      shownSubPage_ = 0;
      relayout();
      if (!same) full_ = true;
      return true;  // at least the tab highlight moved
    }
    case ui::HitKind::TabOverflow: {
      ui::focusOverflow(focus_, orientation_, tabCount);
      relayout();
      return true;
    }
    case ui::HitKind::PagerPrev:
    case ui::HitKind::PagerNext: {
      const int delta = hit.kind == ui::HitKind::PagerNext ? 1 : -1;
      if (!ui::focusPagerStep(focus_, delta, layout_.pager.subPageCount)) return false;
      shownSubPage_ = focus_.subPage;
      relayout();
      full_ = true;
      return true;
    }
    case ui::HitKind::Widget: {
      const config::Widget *w = widgetAt(shownTab_, hit.index);
      if (!w) return false;
      if (!ui::isFocusable(w->type)) return false;
      const bool single = ui::isSingleAction(w->type);
      if (single ? hit.sub != 0 : hit.sub == 0) return false;
      return runAction(hit.index, hit.sub, now);
    }
    default:
      return false;  // refresh / settings: not in the demo
  }
}

bool AppState::rocker(ui::PhysicalKey key, uint32_t now) {
  if (screen_ != Screen::Tabs) return false;
  const ui::LogicalKey lk = ui::mapRocker(key, orientation_, false);
  // Anything but tab stepping acts on the shown content: let a pending tab switch happen first.
  bool changed = false;
  if (switchDue_ && lk != ui::LogicalKey::Prev && lk != ui::LogicalKey::Next) changed |= applyTabSwitch();
  const bool wasRing = focus_.ringVisible;
  const int oldTab = focus_.tab;
  ui::FocusResult r = ui::focusHandle(focus_, focusEnv(), lk);
  switch (r.effect) {
    case ui::Effect::None:
      return changed || (focus_.ringVisible != wasRing);
    case ui::Effect::FocusMoved:
      return true;
    case ui::Effect::TabChanged:
      if (focus_.tab != oldTab) refreshFocusFlow();
      relayout();  // the visible tab group may have moved
      switchDue_ = true;
      switchAt_ = now + kTabDebounceMs;
      return true;
    case ui::Effect::SubPageChanged:
      applyTabSwitch();
      return true;
    case ui::Effect::OpenSettings:
      return changed || (focus_.ringVisible != wasRing);  // no Settings screen yet
    case ui::Effect::WidgetAction:
      return runAction(r.widgetIndex, r.sub, now) || true;
  }
  return changed;
}

}  // namespace uistate
