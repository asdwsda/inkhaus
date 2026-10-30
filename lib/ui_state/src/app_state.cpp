#include "app_state.h"

#include <algorithm>
#include <cmath>

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
  hist_.close();
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
  popup_ = BrightnessPopup();
  popup_.setSink([this](const std::string &id, int pct) { onBrightnessSend(id, pct); });
  bpUnknown_ = bpAwaiting_ = bpFailed_ = bpSawPending_ = false;
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
  focusFlow_ = cfg_.tabs.empty() ? ui::Flow() : ui::computeFlow(orientation_, cfg_.tabs[size_t(t)].widgets, cfg_.tabs[size_t(t)].landscapeColumns);
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

static bool endsWithFails(const std::string &id) {
  static const std::string kSuffix = "_fails";
  return id.size() > kSuffix.size() && id.compare(id.size() - kSuffix.size(), kSuffix.size(), kSuffix) == 0;
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
  // Popup value (task 045): the light shows the optimistic brightness while the popup is open or a
  // send is unconfirmed ("On 60%" on the widget).
  const int opt = popup_.optimisticPct();
  if (opt >= 0 && popup_.tab() == shownTab_ && popup_.widget() == idx && !popup_.entityId().empty()) {
    for (size_t i = 0; i < d.members.size(); ++i) {
      if (!d.members[i] || d.members[i]->id != popup_.entityId()) continue;
      optState_ = *d.members[i];
      if (!bpUnknown_) {
        optState_.state = opt > 0 ? "on" : "off";
        if (opt > 0) optState_.brightness = int(std::lround(opt * 255.0 / 100.0));
      }
      d.members[i] = &optState_;
    }
  }
  for (const Pending &p : pending_) {
    if (p.tab != shownTab_ || p.widget != idx) continue;
    // Optimistic (task 036): the requested result shows at once for every widget type, with no
    // hourglass / dashed ring (`pending` stays false). The entry ends with the driver's mark: the
    // store then holds the confirmed state, or a Failed mark reverts to the last real one.
    d.optimistic = true;
    d.pendingSub = p.sub;
    for (size_t i = 0; i < d.members.size(); ++i) {
      for (const EntityState &s : p.states) {
        if (d.members[i] && s.id == d.members[i]->id) d.members[i] = &s;
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
  if (!marksDriven_)
    for (const std::string &id : w->entities)
      if (endsWithFails(id)) marks_.erase(id);  // a new tap clears the demo Failed badge
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
  brightnessMarks(changed);
  return changed;
}

bool AppState::tick(uint32_t now) {
  bool changed = false;
  lastNow_ = now;
  if (hist_.tick(now)) changed = true;  // the Loading frame appears after its delay
  if (popup_.isOpen()) {
    const EntityState *s = store_.get(popup_.entityId());
    bool closed = false;
    if (s && !s->available) {  // the light became unavailable: close without sending
      popup_.cancel();
      closed = true;
    } else {
      closed = popup_.tick(now);
    }
    if (closed) {
      closeBrightness(now);  // already closed: clears the flags
      changed = true;
    }
  }
  if (marksDriven_ && bpAwaiting_ && int32_t(now - bpSentAt_) >= int32_t(safetyMs_)) {
    bpAwaiting_ = false;  // the driver never confirmed: show the real state again
    popup_.clearOptimistic();
    changed = true;
  }
  if (switchDue_ && int32_t(now - switchAt_) >= 0) changed |= applyTabSwitch();
  for (size_t i = 0; i < pending_.size();) {
    if (int32_t(now - pending_[i].commitAt) >= 0) {
      if (!marksDriven_) {
        // Demo: an entity id ending in "_fails" ends its action in the Failed state.
        bool fails = false;
        for (const EntityState &st : pending_[i].states)
          if (endsWithFails(st.id)) { marks_[st.id] = Mark::Failed; fails = true; }
        if (!fails) store_.update(pending_[i].states);
      }
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
  lastNow_ = now;
  if (hist_.isOpen()) return historyTouch(p);
  if (popup_.isOpen()) return popupTouch(p, now);
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
  lastNow_ = now;
  if (hist_.isOpen()) {  // UP / DOWN do nothing; PUSH and long push close
    if (key == ui::PhysicalKey::Push || key == ui::PhysicalKey::LongPush) return closeHistory();
    return false;
  }
  if (popup_.isOpen()) return popupRocker(key, now);
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

// ---- brightness popup (task 045) ----

const EntityState *AppState::dimmableMember(const config::Widget &w) const {
  if (w.type != WidgetType::Light) return nullptr;
  for (auto *m : membersOf(w))
    if (m && m->available && m->dimmable()) return m;
  return nullptr;
}

bool AppState::dimmableLightAt(ui::Point p) const {
  if (screen_ != Screen::Tabs || popup_.isOpen() || hist_.isOpen()) return false;  // one modal at a time
  const ui::Hit hit = ui::hitTest(layout_, p);
  if (hit.kind != ui::HitKind::Widget) return false;
  const config::Widget *w = widgetAt(shownTab_, hit.index);
  return w && dimmableMember(*w);
}

bool AppState::openBrightness(int idx, uint32_t now) {
  if (screen_ != Screen::Tabs || popup_.isOpen() || hist_.isOpen()) return false;
  const config::Widget *w = widgetAt(shownTab_, idx);
  const EntityState *m = w ? dimmableMember(*w) : nullptr;
  if (!m) return false;
  const bool on = m->state == "on";
  bpUnknown_ = on && !m->brightness;
  bpAwaiting_ = bpFailed_ = bpSawPending_ = false;
  popup_.open(m->id, shownTab_, idx, on && m->brightness ? brightnessToPct(*m->brightness) : 0, now);
  return true;
}

bool AppState::longPress(ui::Point p, uint32_t now) {
  if (dimmableLightAt(p)) return openBrightness(ui::hitTest(layout_, p).index, now);
  if (historyWidgetAt(p)) return openHistory(ui::hitTest(layout_, p).index, now);
  return false;
}

// ---- history popup (task 053) ----

bool AppState::historyWidgetAt(ui::Point p) const {
  if (screen_ != Screen::Tabs || popup_.isOpen() || hist_.isOpen()) return false;
  const ui::Hit hit = ui::hitTest(layout_, p);
  if (hit.kind != ui::HitKind::Widget) return false;
  const config::Widget *w = widgetAt(shownTab_, hit.index);
  return w && historyEnabledFor(*w);
}

bool AppState::openHistory(int idx, uint32_t now) {
  if (screen_ != Screen::Tabs || popup_.isOpen() || hist_.isOpen()) return false;
  const config::Widget *w = widgetAt(shownTab_, idx);
  if (!w || !historyEnabledFor(*w)) return false;
  hist_.open(makeHistorySpec(*w, orientation_ == settings::Orientation::Landscape), shownTab_, idx, now);
  return true;
}

bool AppState::openHistoryFocused(uint32_t now) {
  if (focus_.mode == ui::FocusMode::Tab || !focus_.ringVisible) return false;
  const int idx = focusedWidgetIndex();
  return idx >= 0 && openHistory(idx, now);
}

bool AppState::closeHistory() { return hist_.close(); }

bool AppState::historyTouch(ui::Point p) {
  switch (historyHitTest(historyLayout(orientation_), p)) {
    case HistoryHit::Outside:
    case HistoryHit::Close: return closeHistory();
    case HistoryHit::Panel: return false;
  }
  return false;
}

bool AppState::openBrightnessFocused(uint32_t now) {
  if (focus_.mode == ui::FocusMode::Tab || !focus_.ringVisible) return false;
  const int idx = focusedWidgetIndex();
  return idx >= 0 && openBrightness(idx, now);
}

bool AppState::closeBrightness(uint32_t now) {
  const bool was = popup_.isOpen();
  popup_.done(now);
  if (bpFailed_) popup_.clearOptimistic();  // the widget shows the real state (with the failed badge)
  bpFailed_ = false;
  bpUnknown_ = false;
  return was;
}

bool AppState::popupTouch(ui::Point p, uint32_t now) {
  const BrightnessLayout l = brightnessLayout(orientation_);
  bool changed = false;
  switch (brightnessHitTest(l, p)) {
    case BrightnessHit::Outside:
    case BrightnessHit::Close:
    case BrightnessHit::Done:
      return closeBrightness(now);
    case BrightnessHit::Panel: popup_.poke(now); return false;
    case BrightnessHit::Bar: {
      const int pct = std::max(1, int(std::lround(brightnessBarFraction(l, p.x) * 100.0)));
      changed = popup_.setPct(pct, now);
      break;
    }
    case BrightnessHit::Minus: changed = popup_.step(-1, now); break;
    case BrightnessHit::Plus: changed = popup_.step(+1, now); break;
    case BrightnessHit::Preset0: changed = popup_.preset(0, now); break;
    case BrightnessHit::Preset1: changed = popup_.preset(1, now); break;
    case BrightnessHit::Preset2: changed = popup_.preset(2, now); break;
    case BrightnessHit::Preset3: changed = popup_.preset(3, now); break;
    case BrightnessHit::Off: changed = popup_.off(now); break;
  }
  if (changed) { bpUnknown_ = false; bpFailed_ = false; }
  return changed;
}

bool AppState::popupRocker(ui::PhysicalKey key, uint32_t now) {
  switch (key) {
    case ui::PhysicalKey::Up:
    case ui::PhysicalKey::Down: {
      // Portrait: Up = brighter. Landscape: BtnA points left, so the key pointing right (Down) = brighter.
      const bool prev = ui::mapRocker(key, orientation_, false) == ui::LogicalKey::Prev;
      const bool brighter = orientation_ == settings::Orientation::Landscape ? !prev : prev;
      const bool changed = popup_.step(brighter ? +1 : -1, now);
      if (changed) { bpUnknown_ = false; bpFailed_ = false; }
      return changed;
    }
    case ui::PhysicalKey::Push:
    case ui::PhysicalKey::LongPush: return closeBrightness(now);
  }
  return false;
}

void AppState::onBrightnessSend(const std::string &id, int pct) {
  bpAwaiting_ = true;
  bpSawPending_ = false;
  bpFailed_ = false;
  bpSentAt_ = lastNow_;
  if (marksDriven_) {
    if (bsink_) bsink_(id, pct);
    return;
  }
  // Demo: apply to the local store at once.
  if (const EntityState *cur = store_.get(id)) {
    EntityState s = *cur;
    s.state = pct > 0 ? "on" : "off";
    if (pct > 0) s.brightness = int(std::lround(pct * 255.0 / 100.0));
    store_.update({s});
  }
  bpAwaiting_ = false;
  popup_.clearOptimistic();
}

// UNVERIFIED: the session marks the light Pending right after userAction and clears (or Fails) it when the
// 036 confirmation arrives; a stale Failed mark from an earlier action would show alert at once.
// Driver marks for the popup's light: Pending then absent = confirmed, Failed = alert until close.
void AppState::brightnessMarks(bool &changed) {
  if (!bpAwaiting_ || popup_.sendPending()) return;
  auto it = marks_.find(popup_.entityId());
  const Mark m = it == marks_.end() ? Mark::None : it->second;
  if (m == Mark::Pending) {
    bpSawPending_ = true;
  } else if (m == Mark::Failed) {
    bpAwaiting_ = false;
    bpFailed_ = popup_.isOpen();
    popup_.clearOptimistic();
    changed = true;
  } else if (bpSawPending_) {
    bpAwaiting_ = false;
    bpSawPending_ = false;
    popup_.clearOptimistic();
    changed = true;
  }
}

}  // namespace uistate
