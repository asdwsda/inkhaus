#include "settings_flow.h"

#include <optional>

namespace settings_flow {

namespace {
std::optional<settings_ui::SettingsScreen> g_screen;
uint32_t g_lastInput = 0;

Result fromUpdate(const settings_ui::Update &u) {
  Result r;
  r.redraw = !u.dirty.empty();
  if (u.action == settings_ui::Action::StartRefresh) {
    r.refresh = true;
    r.redraw = true;
  } else if (u.action == settings_ui::Action::PageChange) {
    r.pageChanged = true;
    r.redraw = true;
    r.toolsOpened = g_screen->page() == settings_ui::Page::Tools;
  } else if (u.action == settings_ui::Action::Leave) {
    r.left = true;
    r.leave = g_screen->leave();
    g_screen.reset();
  }
  return r;
}
}  // namespace

bool active() { return g_screen.has_value(); }
const settings_ui::SettingsScreen &screen() { return *g_screen; }

void open(const settings::Settings &saved, settings::Orientation current, bool ringVisible, uint32_t now) {
  g_screen.emplace(saved, current, ringVisible);
  g_lastInput = now;
}

void setSdPresent(bool present) {
  if (g_screen) g_screen->setSdPresent(present);
}

Result onTouch(ui::Point p, uint32_t now) {
  if (!g_screen) return {};
  g_lastInput = now;
  return fromUpdate(g_screen->onTouch(p));
}

Result onKey(ui::LogicalKey key, uint32_t now) {
  if (!g_screen) return {};
  g_lastInput = now;
  return fromUpdate(g_screen->onKey(key));
}

Result tick(uint32_t now) {
  if (!g_screen) return {};
  if (!settings_ui::SettingsScreen::idleExpired(now - g_lastInput)) return {};
  settings_ui::Update u;
  u.action = settings_ui::Action::Leave;
  return fromUpdate(u);
}

Result refreshDone(bool ok) {
  if (!g_screen) return {};
  return fromUpdate(g_screen->refreshDone(ok));
}

}  // namespace settings_flow
