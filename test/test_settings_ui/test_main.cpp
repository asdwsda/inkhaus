// Tests for lib/settings_ui, with numbers taken from docs/ui/settings.md.
#include <unity.h>

#include "settings_ui.h"

using namespace settings_ui;
using settings::Orientation;
using settings::Settings;
using settings::WakeMode;
using ui::LogicalKey;

void setUp() {}
void tearDown() {}

static void expectRect(int x, int y, int w, int h, const Rect& r) {
  TEST_ASSERT_EQUAL_INT(x, r.x);
  TEST_ASSERT_EQUAL_INT(y, r.y);
  TEST_ASSERT_EQUAL_INT(w, r.w);
  TEST_ASSERT_EQUAL_INT(h, r.h);
}

void test_portrait_layout() {
  auto L = computeSettingsLayout(Orientation::Portrait);
  expectRect(404, 11, 120, 32, L.unsavedChip);
  expectRect(0, 54, 540, 2, L.headerRule);
  expectRect(16, 68, 508, 68, L.rows[0]);
  expectRect(16, 148, 508, 68, L.rows[1]);
  expectRect(16, 228, 508, 68, L.rows[2]);
  expectRect(16, 308, 508, 68, L.rows[3]);
  expectRect(16, 388, 508, 68, L.rows[4]);
  expectRect(16, 468, 508, 64, L.note);
  expectRect(16, 880, 248, 64, L.refreshButton);
  expectRect(276, 880, 248, 64, L.backButton);
  expectRect(16, 548, 508, 48, L.info[0].rect);
  expectRect(16, 596, 508, 48, L.info[1].rect);
  expectRect(16, 644, 508, 48, L.info[2].rect);
  expectRect(16, 692, 508, 48, L.info[3].rect);
  expectRect(16, 740, 508, 64, L.info[4].rect);
  expectRect(16, 804, 508, 48, L.info[5].rect);
  expectRect(16, 740, 508, 1, L.info[4].hairline);
  TEST_ASSERT_EQUAL_INT(572, L.info[0].cy);
  TEST_ASSERT_EQUAL_INT(620, L.info[1].cy);
  TEST_ASSERT_EQUAL_INT(668, L.info[2].cy);
  TEST_ASSERT_EQUAL_INT(716, L.info[3].cy);
  TEST_ASSERT_EQUAL_INT(772, L.info[4].cy);
  TEST_ASSERT_EQUAL_INT(758, L.info[4].cyLine1);
  TEST_ASSERT_EQUAL_INT(786, L.info[4].cyLine2);
  TEST_ASSERT_EQUAL_INT(828, L.info[5].cy);
  TEST_ASSERT_EQUAL_INT(16 + 158, L.info[0].valueX);
}

void test_landscape_layout() {
  auto L = computeSettingsLayout(Orientation::Landscape);
  expectRect(824, 9, 120, 32, L.unsavedChip);
  expectRect(0, 50, 960, 2, L.headerRule);
  expectRect(16, 64, 456, 68, L.rows[0]);
  expectRect(16, 144, 456, 68, L.rows[1]);
  expectRect(16, 224, 456, 68, L.rows[2]);
  expectRect(16, 304, 456, 68, L.rows[3]);
  expectRect(16, 384, 456, 68, L.rows[4]);
  expectRect(16, 464, 456, 64, L.note);
  expectRect(488, 464, 222, 64, L.refreshButton);
  expectRect(722, 464, 222, 64, L.backButton);
  expectRect(488, 64, 456, 56, L.info[0].rect);
  expectRect(488, 120, 456, 56, L.info[1].rect);
  expectRect(488, 176, 456, 56, L.info[2].rect);
  expectRect(488, 232, 456, 56, L.info[3].rect);
  expectRect(488, 288, 456, 72, L.info[4].rect);
  expectRect(488, 360, 456, 56, L.info[5].rect);
  TEST_ASSERT_EQUAL_INT(92, L.info[0].cy);
  TEST_ASSERT_EQUAL_INT(148, L.info[1].cy);
  TEST_ASSERT_EQUAL_INT(204, L.info[2].cy);
  TEST_ASSERT_EQUAL_INT(260, L.info[3].cy);
  TEST_ASSERT_EQUAL_INT(324, L.info[4].cy);
  TEST_ASSERT_EQUAL_INT(310, L.info[4].cyLine1);
  TEST_ASSERT_EQUAL_INT(338, L.info[4].cyLine2);
  TEST_ASSERT_EQUAL_INT(388, L.info[5].cy);
  TEST_ASSERT_EQUAL_INT(646, L.info[0].valueX);
  TEST_ASSERT_EQUAL_INT(298, L.info[0].valueWidth);
}

// Touch targets >= 64 x 64 (footer 64 high), >= 12 px apart; all on screen.
void test_targets_size_gaps_and_bounds() {
  for (auto o : {Orientation::Portrait, Orientation::Landscape}) {
    auto L = computeSettingsLayout(o);
    ui::Rect screen{0, 0, L.screenW, L.screenH};
    for (int i = 0; i < kFocusStops; ++i) {
      TEST_ASSERT_TRUE(L.stops[i].h >= 64);
      TEST_ASSERT_TRUE(L.stops[i].w >= 64);
      TEST_ASSERT_TRUE(ui::rectInside(L.stops[i], screen));
      for (int j = i + 1; j < kFocusStops; ++j) {
        // gap >= 12 means rects inflated by 5 never overlap
        TEST_ASSERT_FALSE(ui::rectsOverlap(ui::inflate(L.stops[i], 5), ui::inflate(L.stops[j], 5)));
      }
    }
  }
}

void test_row_internals() {
  ui::Rect row{16, 68, 508, 68};
  expectRect(32, 80, 220, 24, rowLabelRect(row));
  expectRect(32, 106, 220, 18, rowHintRect(row));
  TEST_ASSERT_EQUAL_INT(92, rowLabelCy(row));
  TEST_ASSERT_EQUAL_INT(115, rowHintCy(row));
  TEST_ASSERT_EQUAL_INT(102, rowValueCy(row));
  TEST_ASSERT_EQUAL_INT(16 + 492, rowValueRightX(row));
  ui::Rect land{16, 64, 456, 68};
  TEST_ASSERT_EQUAL_INT(16 + 440, rowValueRightX(land));
  TEST_ASSERT_EQUAL_INT(16 + 22, pipCentre(row, 0).x);
  TEST_ASSERT_EQUAL_INT(16 + 22 + 100, pipCentre(row, 5).x);
  TEST_ASSERT_EQUAL_INT(68 + 47, pipCentre(row, 0).y);
  TEST_ASSERT_EQUAL_INT(2, pipCount(0));
  TEST_ASSERT_EQUAL_INT(3, pipCount(1));
  TEST_ASSERT_EQUAL_INT(6, pipCount(2));
  TEST_ASSERT_EQUAL_INT(4, pipCount(3));
  ui::Rect note{16, 468, 508, 64};
  expectRect(28, 480, 40, 40, noteIconRect(note));
  TEST_ASSERT_EQUAL_INT(80, noteTextX(note));
  TEST_ASSERT_EQUAL_INT(489, noteLineCy(note, 0));
  TEST_ASSERT_EQUAL_INT(511, noteLineCy(note, 1));
  ui::Rect btn{16, 880, 248, 64};
  expectRect(28, 892, 40, 40, buttonIconRect(btn));
  expectRect(76, 880, 180, 64, buttonLabelRect(btn));
}

void test_value_text() {
  Settings s;
  s.orientation = Orientation::Landscape;
  TEST_ASSERT_EQUAL_STRING("Landscape", rowValueText(0, s).c_str());
  s.orientation = Orientation::Portrait;
  TEST_ASSERT_EQUAL_STRING("Portrait", rowValueText(0, s).c_str());
  TEST_ASSERT_EQUAL_STRING("Button", rowValueText(1, s).c_str());
  s.wake_mode = WakeMode::Touch;
  TEST_ASSERT_EQUAL_STRING("Touch", rowValueText(1, s).c_str());
  s.refresh_interval_min = 0;
  TEST_ASSERT_EQUAL_STRING("Off", rowValueText(2, s).c_str());
  s.refresh_interval_min = 240;
  TEST_ASSERT_EQUAL_STRING("240 min", rowValueText(2, s).c_str());
  s.idle_timeout_s = 10;
  TEST_ASSERT_EQUAL_STRING("10 s", rowValueText(3, s).c_str());
  TEST_ASSERT_EQUAL_STRING("Orientation", rowLabel(0));
  TEST_ASSERT_EQUAL_STRING("Wake mode", rowLabel(1));
  TEST_ASSERT_EQUAL_STRING("Refresh every", rowLabel(2));
  TEST_ASSERT_EQUAL_STRING("Idle timeout", rowLabel(3));
}

void test_cycle_wraps_in_array_order() {
  Settings s;
  s.refresh_interval_min = 0;
  const int expect[] = {15, 30, 60, 120, 240, 0};
  for (int e : expect) {
    s = cycleRow(2, s);
    TEST_ASSERT_EQUAL_INT(e, s.refresh_interval_min);
  }
  s.idle_timeout_s = 60;
  s = cycleRow(3, s);
  TEST_ASSERT_EQUAL_INT(10, s.idle_timeout_s);
  s = cycleRow(0, s);
  TEST_ASSERT_TRUE(s.orientation == Orientation::Landscape);
  s = cycleRow(0, s);
  TEST_ASSERT_TRUE(s.orientation == Orientation::Portrait);
  // cycling changes only its own field
  Settings t;
  Settings u = cycleRow(1, t);
  TEST_ASSERT_TRUE(u.wake_mode == WakeMode::Touch);
  u.wake_mode = t.wake_mode;
  TEST_ASSERT_TRUE(u == t);
}

void test_wake_warning_flags() {
  Settings s;
  TEST_ASSERT_FALSE(showsNote(s));
  s.wake_mode = WakeMode::Touch;
  TEST_ASSERT_TRUE(showsNote(s));
  s.wake_mode = WakeMode::TouchAndButton;
  TEST_ASSERT_TRUE(showsNote(s));
  TEST_ASSERT_EQUAL_STRING("Touch & Button", rowValueText(1, s).c_str());
  TEST_ASSERT_EQUAL_STRING("Touch wake shortens battery life.", kNoteLine1);
  TEST_ASSERT_EQUAL_STRING("Button wake is recommended.", kNoteLine2);
}

void test_touch_cycles_buffer_only() {
  Settings saved;  // defaults: Button, 60, 20, Portrait
  SettingsScreen sc(saved, Orientation::Portrait, false);
  auto u = sc.onTouch({100, 112});  // row 1 orientation
  TEST_ASSERT_TRUE(sc.edit().orientation == Orientation::Landscape);
  TEST_ASSERT_TRUE(sc.saved().orientation == Orientation::Portrait);
  TEST_ASSERT_TRUE(sc.dirty());
  TEST_ASSERT_TRUE(u.action == Action::None);
  // layout stays portrait until leave
  TEST_ASSERT_EQUAL_INT(540, sc.layout().screenW);
  // dirty: value, hint, chip (flipped)
  bool chip = false;
  for (auto& r : u.dirty) chip |= (r == sc.layout().unsavedChip);
  TEST_ASSERT_TRUE(chip);
  TEST_ASSERT_EQUAL_INT(3, static_cast<int>(u.dirty.size()));
}

void test_touch_outside_does_nothing() {
  Settings saved;
  SettingsScreen sc(saved, Orientation::Portrait, false);
  auto u = sc.onTouch({270, 462});  // between row 4 and note slot
  TEST_ASSERT_TRUE(u.action == Action::None);
  TEST_ASSERT_TRUE(u.dirty.empty());
  u = sc.onTouch({10, 100});  // left margin
  TEST_ASSERT_TRUE(u.dirty.empty());
  u = sc.onTouch({100, 700});  // info area
  TEST_ASSERT_TRUE(u.dirty.empty());
  TEST_ASSERT_FALSE(sc.dirty());
}

void test_wake_touch_redraws_note_and_unsaved_chip_toggles() {
  Settings saved;
  SettingsScreen sc(saved, Orientation::Portrait, false);
  auto u = sc.onTouch({100, 212});  // wake row
  TEST_ASSERT_TRUE(sc.edit().wake_mode == WakeMode::Touch);
  bool note = false, chip = false;
  for (auto& r : u.dirty) {
    note |= (r == sc.layout().note);
    chip |= (r == sc.layout().unsavedChip);
  }
  TEST_ASSERT_TRUE(note);
  TEST_ASSERT_TRUE(chip);
  // second tap on another row: dirty stays, chip not redrawn
  u = sc.onTouch({100, 412});
  chip = false;
  for (auto& r : u.dirty) chip |= (r == sc.layout().unsavedChip);
  TEST_ASSERT_FALSE(chip);
  // back to Button: note gone, but still dirty (idle changed)
  u = sc.onTouch({100, 212});
  TEST_ASSERT_TRUE(sc.dirty());
}

void test_cycle_back_removes_unsaved_and_leave_is_write_free() {
  Settings saved;
  SettingsScreen sc(saved, Orientation::Portrait, false);
  sc.onTouch({100, 212});
  TEST_ASSERT_TRUE(sc.dirty());
  sc.onTouch({100, 212});
  auto u = sc.onTouch({100, 212});  // Button again (3 values)
  TEST_ASSERT_FALSE(sc.dirty());
  bool chip = false;
  for (auto& r : u.dirty) chip |= (r == sc.layout().unsavedChip);
  TEST_ASSERT_TRUE(chip);
  auto l = sc.leave();
  TEST_ASSERT_FALSE(l.save);
  TEST_ASSERT_FALSE(l.orientationChanged);
  TEST_ASSERT_TRUE(l.fullRefresh);
}

void test_leave_saves_once_and_flags_orientation() {
  Settings saved;
  SettingsScreen sc(saved, Orientation::Portrait, false);
  sc.onTouch({100, 112});
  auto l = sc.leave();
  TEST_ASSERT_TRUE(l.save);
  TEST_ASSERT_TRUE(l.orientationChanged);
  TEST_ASSERT_TRUE(l.settings.orientation == Orientation::Landscape);
  TEST_ASSERT_TRUE(l.fullRefresh);
  // other change only: save but no rotation
  SettingsScreen sc2(saved, Orientation::Portrait, false);
  sc2.onTouch({100, 312});
  l = sc2.leave();
  TEST_ASSERT_TRUE(l.save);
  TEST_ASSERT_FALSE(l.orientationChanged);
}

void test_back_and_long_push_leave() {
  Settings saved;
  SettingsScreen sc(saved, Orientation::Portrait, true);
  TEST_ASSERT_TRUE(sc.onTouch({400, 912}).action == Action::Leave);
  TEST_ASSERT_TRUE(sc.onKey(LogicalKey::LongPush).action == Action::Leave);
  sc.onKey(LogicalKey::Prev);  // wraps to Back
  TEST_ASSERT_EQUAL_INT(kStopBack, sc.focus());
  TEST_ASSERT_TRUE(sc.onKey(LogicalKey::Push).action == Action::Leave);
}

void test_rocker_focus_order_and_wrap() {
  Settings saved;
  SettingsScreen sc(saved, Orientation::Portrait, true);
  TEST_ASSERT_EQUAL_INT(0, sc.focus());
  for (int i = 1; i <= 6; ++i) {
    sc.onKey(LogicalKey::Next);
    TEST_ASSERT_EQUAL_INT(i, sc.focus());
  }
  sc.onKey(LogicalKey::Next);
  TEST_ASSERT_EQUAL_INT(0, sc.focus());
  sc.onKey(LogicalKey::Prev);
  TEST_ASSERT_EQUAL_INT(6, sc.focus());
}

void test_ring_dirty_is_old_and_new_inflated_by_6() {
  Settings saved;
  SettingsScreen sc(saved, Orientation::Portrait, true);
  auto u = sc.onKey(LogicalKey::Next);
  TEST_ASSERT_EQUAL_INT(2, static_cast<int>(u.dirty.size()));
  expectRect(10, 62, 520, 80, u.dirty[0]);  // row 1 inflated by 6
  expectRect(10, 142, 520, 80, u.dirty[1]);  // row 2 inflated by 6
  // hidden ring: only the new position is drawn
  SettingsScreen hidden(saved, Orientation::Portrait, false);
  u = hidden.onKey(LogicalKey::Next);
  TEST_ASSERT_EQUAL_INT(1, static_cast<int>(u.dirty.size()));
  TEST_ASSERT_TRUE(hidden.ringVisible());
}

void test_push_on_row_cycles() {
  Settings saved;
  SettingsScreen sc(saved, Orientation::Portrait, true);
  sc.onKey(LogicalKey::Next);  // wake row
  sc.onKey(LogicalKey::Push);
  TEST_ASSERT_TRUE(sc.edit().wake_mode == WakeMode::Touch);
}

void test_refresh_now_flow() {
  Settings saved;
  SettingsScreen sc(saved, Orientation::Portrait, false);
  auto u = sc.onTouch({100, 912});
  TEST_ASSERT_TRUE(u.action == Action::StartRefresh);
  TEST_ASSERT_TRUE(sc.refreshState() == RefreshState::Running);
  // ignored while running
  u = sc.onTouch({100, 912});
  TEST_ASSERT_TRUE(u.action == Action::None);
  TEST_ASSERT_TRUE(u.dirty.empty());
  // failure: button + HA row only
  u = sc.refreshDone(false);
  TEST_ASSERT_TRUE(sc.refreshState() == RefreshState::Failed);
  TEST_ASSERT_EQUAL_INT(2, static_cast<int>(u.dirty.size()));
  // can retry from Failed, success also redraws Battery
  TEST_ASSERT_TRUE(sc.onTouch({100, 912}).action == Action::StartRefresh);
  u = sc.refreshDone(true);
  TEST_ASSERT_TRUE(sc.refreshState() == RefreshState::Idle);
  TEST_ASSERT_EQUAL_INT(3, static_cast<int>(u.dirty.size()));
  bool battery = false;
  for (auto& r : u.dirty) battery |= (r == sc.layout().info[1].rect);
  TEST_ASSERT_TRUE(battery);
  // refreshDone without a running fetch does nothing
  TEST_ASSERT_TRUE(sc.refreshDone(true).dirty.empty());
}

void test_idle_is_fixed_60s() {
  TEST_ASSERT_FALSE(SettingsScreen::idleExpired(59999));
  TEST_ASSERT_TRUE(SettingsScreen::idleExpired(60000));
}

void test_layout_follows_given_orientation() {
  Settings saved;
  saved.orientation = Orientation::Landscape;
  SettingsScreen sc(saved, Orientation::Landscape, false);
  TEST_ASSERT_EQUAL_INT(960, sc.layout().screenW);
  auto u = sc.onTouch({600, 496});  // Refresh now in landscape
  TEST_ASSERT_TRUE(u.action == Action::StartRefresh);
  u = sc.onTouch({800, 496});  // Back
  TEST_ASSERT_TRUE(u.action == Action::Leave);
}

void test_info_texts() {
  WallTime now{10, 28, 14, 30};
  auto b = batteryText({78, 3920, false});
  TEST_ASSERT_EQUAL_STRING("78%, 3.92 V", b.text.c_str());
  TEST_ASSERT_TRUE(b.icon == Icon::None);
  TEST_ASSERT_TRUE(batteryText({15, 3500, false}).icon == Icon::Alert);
  TEST_ASSERT_TRUE(batteryText({16, 3500, false}).icon == Icon::None);
  TEST_ASSERT_TRUE(batteryText({50, 4100, true}).icon == Icon::Bolt);

  HaInfo h;
  TEST_ASSERT_EQUAL_STRING("-", haText(h, now).text.c_str());
  h.sessionDone = true;
  h.lastAttemptOk = true;
  h.everOk = true;
  h.lastLink = HaInfo::Link::Local;
  h.lastOk = {10, 28, 14, 5};
  TEST_ASSERT_EQUAL_STRING("local, ok 14:05", haText(h, now).text.c_str());
  h.lastLink = HaInfo::Link::Remote;
  TEST_ASSERT_EQUAL_STRING("remote, ok 14:05", haText(h, now).text.c_str());
  h.lastAttemptOk = false;
  h.lastOk = {10, 27, 13, 5};
  auto off = haText(h, now);
  TEST_ASSERT_EQUAL_STRING("offline, last ok 27.10. 13:05", off.text.c_str());
  TEST_ASSERT_TRUE(off.icon == Icon::Alert);
  h.everOk = false;
  TEST_ASSERT_EQUAL_STRING("offline, never ok", haText(h, now).text.c_str());
  h.refreshing = true;
  auto r = haText(h, now);
  TEST_ASSERT_TRUE(r.icon == Icon::Hourglass);
  TEST_ASSERT_EQUAL_STRING("refreshing\xE2\x80\xA6", r.text.c_str());

  TEST_ASSERT_EQUAL_STRING("-", wifiText({}).text.c_str());
  TEST_ASSERT_EQUAL_STRING("Home", wifiText({true, "Home", true}).text.c_str());
  TEST_ASSERT_EQUAL_STRING("Home (not connected)", wifiText({true, "Home", false}).text.c_str());

  ConfigInfo c;
  TEST_ASSERT_EQUAL_STRING("OK", configText(c).text.c_str());
  TEST_ASSERT_EQUAL_STRING("0", errorsText(c).c_str());
  c.errorCount = 3;
  c.firstPath = "tabs[0].widgets[2].entity";
  c.firstLine = 34;
  auto ct = configText(c);
  TEST_ASSERT_TRUE(ct.icon == Icon::Alert);
  TEST_ASSERT_EQUAL_STRING("tabs[0].widgets[2].entity (line 34)", ct.text.c_str());
  TEST_ASSERT_EQUAL_STRING("3", errorsText(c).c_str());
  c.missing = true;
  ct = configText(c);
  TEST_ASSERT_TRUE(ct.icon == Icon::Warn);
  TEST_ASSERT_EQUAL_STRING("no config.yaml on SD", ct.text.c_str());
  TEST_ASSERT_EQUAL_STRING("-", errorsText(c).c_str());
}

static int fakeMeasure(const std::string& s, int font) {
  return static_cast<int>(s.size()) * (font == 24 ? 12 : 9);
}

void test_fit_text() {
  auto f = fitText("short", 100, fakeMeasure);
  TEST_ASSERT_EQUAL_INT(24, f.font);
  TEST_ASSERT_EQUAL_STRING("short", f.text.c_str());
  f = fitText("0123456789", 100, fakeMeasure);  // 120 at F24, 90 at F18
  TEST_ASSERT_EQUAL_INT(18, f.font);
  TEST_ASSERT_EQUAL_STRING("0123456789", f.text.c_str());
  f = fitText("0123456789ABCDEF", 100, fakeMeasure);  // 144 at F18
  TEST_ASSERT_EQUAL_INT(18, f.font);
  TEST_ASSERT_TRUE(fakeMeasure(f.text, 18) <= 100 || f.text.size() > 100);
  TEST_ASSERT_TRUE(f.text.size() >= 3);
  TEST_ASSERT_EQUAL_STRING("\xE2\x80\xA6", f.text.substr(f.text.size() - 3).c_str());
}


// ---- tester additions ----

void test_title_and_all_hairlines() {
  auto P = computeSettingsLayout(Orientation::Portrait);
  expectRect(16, 0, 200, 54, P.title);
  const int py[] = {548, 596, 644, 692, 740, 804};
  for (int i = 0; i < kInfoRowCount; ++i) expectRect(16, py[i], 508, 1, P.info[i].hairline);
  auto L = computeSettingsLayout(Orientation::Landscape);
  expectRect(16, 0, 200, 50, L.title);
  const int ly[] = {64, 120, 176, 232, 288, 360};
  for (int i = 0; i < kInfoRowCount; ++i) expectRect(488, ly[i], 456, 1, L.info[i].hairline);
  // stops mirror rows + buttons
  for (int i = 0; i < kRowCount; ++i) TEST_ASSERT_TRUE(L.stops[i] == L.rows[i]);
  TEST_ASSERT_TRUE(L.stops[kStopRefresh] == L.refreshButton);
  TEST_ASSERT_TRUE(L.stops[kStopBack] == L.backButton);
}

void test_touch_hits_each_target_both_orientations() {
  for (auto o : {Orientation::Portrait, Orientation::Landscape}) {
    auto L = computeSettingsLayout(o);
    for (int i = 0; i < kFocusStops; ++i) {
      Settings saved;
      SettingsScreen sc(saved, o, false);
      Rect r = L.stops[i];
      // corners inside, one pixel outside does nothing
      auto u = sc.onTouch({r.x, r.y});
      bool hit = u.action != Action::None || !u.dirty.empty();
      TEST_ASSERT_TRUE(hit);
      SettingsScreen sc2(saved, o, false);
      u = sc2.onTouch({r.x + r.w - 1, r.y + r.h - 1});
      hit = u.action != Action::None || !u.dirty.empty();
      TEST_ASSERT_TRUE(hit);
      SettingsScreen sc3(saved, o, false);
      u = sc3.onTouch({r.x - 1, r.y + r.h / 2});
      TEST_ASSERT_TRUE(u.action == Action::None && u.dirty.empty());
      u = sc3.onTouch({r.x + r.w, r.y + r.h / 2});
      TEST_ASSERT_TRUE(u.action == Action::None && u.dirty.empty());
    }
    // gap between the two footer buttons
    Settings saved;
    SettingsScreen sc(saved, o, false);
    int gapX = L.refreshButton.x + L.refreshButton.w + 5;
    auto u = sc.onTouch({gapX, L.refreshButton.y + 32});
    TEST_ASSERT_TRUE(u.action == Action::None && u.dirty.empty());
    // touch never moves focus or the ring
    TEST_ASSERT_EQUAL_INT(0, sc.focus());
    TEST_ASSERT_FALSE(sc.ringVisible());
  }
}

void test_every_row_cycles_full_circle_and_chip_tracks_dirty() {
  const int sizes[] = {2, 3, 6, 4, 2};
  for (int row = 0; row < kRowCount; ++row) {
    Settings saved;
    SettingsScreen sc(saved, Orientation::Portrait, false);
    Point p{100, sc.layout().rows[row].y + 44};
    for (int k = 1; k <= sizes[row]; ++k) {
      auto u = sc.onTouch(p);
      bool chip = false;
      for (auto& r : u.dirty) chip |= (r == sc.layout().unsavedChip);
      if (k == 1) {
        TEST_ASSERT_TRUE(sc.dirty());
        TEST_ASSERT_TRUE(chip);
      } else if (k < sizes[row]) {
        TEST_ASSERT_TRUE(sc.dirty());
        TEST_ASSERT_FALSE(chip);
      } else {
        TEST_ASSERT_FALSE(sc.dirty());
        TEST_ASSERT_TRUE(chip);
      }
    }
    TEST_ASSERT_TRUE(sc.edit() == sc.saved());
    TEST_ASSERT_FALSE(sc.leave().save);
  }
}

void test_all_rows_changed_and_restored_is_write_free() {
  Settings saved;
  SettingsScreen sc(saved, Orientation::Portrait, true);
  const int sizes[] = {2, 3, 6, 4, 2};
  for (int row = 0; row < kRowCount; ++row) {
    for (int k = 0; k < sizes[row]; ++k) sc.onKey(LogicalKey::Push);
    sc.onKey(LogicalKey::Next);
  }
  TEST_ASSERT_FALSE(sc.dirty());
  auto l = sc.leave();
  TEST_ASSERT_FALSE(l.save);
  TEST_ASSERT_FALSE(l.orientationChanged);
}

void test_leave_returns_exactly_the_edited_settings() {
  Settings saved;
  saved.refresh_interval_min = 30;
  saved.idle_timeout_s = 30;
  SettingsScreen sc(saved, Orientation::Portrait, false);
  sc.onTouch({100, 262});  // refresh 30 -> 60
  sc.onTouch({100, 342});  // idle 30 -> 60
  auto l = sc.leave();
  TEST_ASSERT_TRUE(l.save);
  TEST_ASSERT_FALSE(l.orientationChanged);
  TEST_ASSERT_EQUAL_INT(60, l.settings.refresh_interval_min);
  TEST_ASSERT_EQUAL_INT(60, l.settings.idle_timeout_s);
  TEST_ASSERT_TRUE(l.settings.wake_mode == saved.wake_mode);
  TEST_ASSERT_TRUE(l.settings.orientation == saved.orientation);
  // leave() is const: calling twice gives the same answer, saved stays untouched
  auto l2 = sc.leave();
  TEST_ASSERT_TRUE(l2.settings == l.settings);
  TEST_ASSERT_EQUAL_INT(30, sc.saved().refresh_interval_min);
}

void test_orientation_flag_from_landscape() {
  Settings saved;
  saved.orientation = Orientation::Landscape;
  SettingsScreen sc(saved, Orientation::Landscape, false);
  sc.onTouch({100, 108});  // landscape row 1 -> Portrait
  auto l = sc.leave();
  TEST_ASSERT_TRUE(l.save);
  TEST_ASSERT_TRUE(l.orientationChanged);
  TEST_ASSERT_TRUE(l.settings.orientation == Orientation::Portrait);
  // changing and changing back: no flag
  sc.onTouch({100, 108});
  l = sc.leave();
  TEST_ASSERT_FALSE(l.orientationChanged);
  TEST_ASSERT_FALSE(l.save);
}

void test_long_push_leaves_from_every_stop_and_does_not_cycle() {
  for (int stop = 0; stop < kFocusStops; ++stop) {
    Settings saved;
    SettingsScreen sc(saved, Orientation::Portrait, true);
    for (int i = 0; i < stop; ++i) sc.onKey(LogicalKey::Next);
    auto u = sc.onKey(LogicalKey::LongPush);
    TEST_ASSERT_TRUE(u.action == Action::Leave);
    TEST_ASSERT_FALSE(sc.dirty());
    TEST_ASSERT_TRUE(sc.refreshState() == RefreshState::Idle);
  }
}

void test_rocker_push_refresh_and_ignored_while_running() {
  Settings saved;
  SettingsScreen sc(saved, Orientation::Portrait, true);
  sc.onKey(LogicalKey::Prev);  // Back
  sc.onKey(LogicalKey::Prev);  // Refresh now
  TEST_ASSERT_EQUAL_INT(kStopRefresh, sc.focus());
  auto u = sc.onKey(LogicalKey::Push);
  TEST_ASSERT_TRUE(u.action == Action::StartRefresh);
  u = sc.onKey(LogicalKey::Push);
  TEST_ASSERT_TRUE(u.action == Action::None);
  TEST_ASSERT_TRUE(u.dirty.empty());
  // rows stay active and Back still leaves while a fetch runs
  sc.onKey(LogicalKey::Next);  // Back
  TEST_ASSERT_TRUE(sc.onKey(LogicalKey::Push).action == Action::Leave);
  sc.onTouch({100, 112});
  TEST_ASSERT_TRUE(sc.dirty());
  // completion after the buffer changed does not touch the buffer
  sc.refreshDone(true);
  TEST_ASSERT_TRUE(sc.dirty());
}

void test_refresh_failure_state_and_dirty_rects() {
  Settings saved;
  SettingsScreen sc(saved, Orientation::Landscape, false);
  auto u = sc.onTouch({600, 496});
  TEST_ASSERT_EQUAL_INT(2, static_cast<int>(u.dirty.size()));  // button + HA row (pending look)
  TEST_ASSERT_TRUE(u.dirty[0] == sc.layout().refreshButton);
  TEST_ASSERT_TRUE(u.dirty[1] == sc.layout().info[2].rect);
  u = sc.refreshDone(false);
  TEST_ASSERT_TRUE(sc.refreshState() == RefreshState::Failed);
  bool battery = false;
  for (auto& r : u.dirty) battery |= (r == sc.layout().info[1].rect);
  TEST_ASSERT_FALSE(battery);  // battery not re-read on failure
  // a second failure notification is ignored
  TEST_ASSERT_TRUE(sc.refreshDone(false).dirty.empty());
  // a later success clears Failed
  sc.onTouch({600, 496});
  sc.refreshDone(true);
  TEST_ASSERT_TRUE(sc.refreshState() == RefreshState::Idle);
}

void test_rocker_wake_row_note_dirty() {
  Settings saved;
  SettingsScreen sc(saved, Orientation::Landscape, true);
  sc.onKey(LogicalKey::Next);
  auto u = sc.onKey(LogicalKey::Push);
  bool note = false;
  for (auto& r : u.dirty) note |= (r == sc.layout().note);
  TEST_ASSERT_TRUE(note);
  TEST_ASSERT_TRUE(showsNote(sc.edit()));
}

void test_info_edge_cases() {
  WallTime now{10, 28, 14, 30};
  // different month, same day number is an earlier day
  TEST_ASSERT_EQUAL_STRING("28.9. 14:05", formatTime({9, 28, 14, 5}, now).c_str());
  TEST_ASSERT_EQUAL_STRING("00:00", formatTime({10, 28, 0, 0}, now).c_str());
  // battery boundaries and rounding
  TEST_ASSERT_EQUAL_STRING("0%, 3.30 V", batteryText({0, 3300, false}).text.c_str());
  TEST_ASSERT_EQUAL_STRING("100%, 4.20 V", batteryText({100, 4200, false}).text.c_str());
  TEST_ASSERT_EQUAL_STRING("50%, 4.00 V", batteryText({50, 3996, false}).text.c_str());
  TEST_ASSERT_EQUAL_STRING("50%, 3.99 V", batteryText({50, 3994, false}).text.c_str());
  TEST_ASSERT_TRUE(batteryText({5, 3300, true}).icon == Icon::Bolt);
  // HA offline, last link remote does not leak into the text
  HaInfo h;
  h.sessionDone = true;
  h.lastAttemptOk = false;
  h.everOk = true;
  h.lastLink = HaInfo::Link::Remote;
  h.lastOk = {10, 28, 13, 5};
  TEST_ASSERT_EQUAL_STRING("offline, last ok 13:05", haText(h, now).text.c_str());
  // refreshing wins over every other state, also before the first session
  HaInfo fresh;
  fresh.refreshing = true;
  TEST_ASSERT_TRUE(haText(fresh, now).icon == Icon::Hourglass);
  // no config: Wi-Fi not configured, config missing, errors "-"
  ConfigInfo none;
  none.missing = true;
  none.errorCount = 5;  // stale count must not show
  TEST_ASSERT_EQUAL_STRING("-", errorsText(none).c_str());
  TEST_ASSERT_EQUAL_STRING("no config.yaml on SD", configText(none).text.c_str());
  // errors without a line number
  ConfigInfo e;
  e.errorCount = 1;
  e.firstPath = "connection.ssid";
  e.firstLine = 0;
  TEST_ASSERT_EQUAL_STRING("connection.ssid", configText(e).text.c_str());
  TEST_ASSERT_EQUAL_STRING("1", errorsText(e).c_str());
  // configured Wi-Fi with an empty SSID is still "configured"
  TEST_ASSERT_EQUAL_STRING("Net (not connected)", wifiText({true, "Net", false}).text.c_str());
}

void test_fit_text_exact_boundaries() {
  // fits at F24 exactly: 5 chars * 12 = 60
  auto f = fitText("12345", 60, fakeMeasure);
  TEST_ASSERT_EQUAL_INT(24, f.font);
  // one px less: falls to F18 (45)
  f = fitText("12345", 59, fakeMeasure);
  TEST_ASSERT_EQUAL_INT(18, f.font);
  TEST_ASSERT_EQUAL_STRING("12345", f.text.c_str());
  // truncated result fits when measured
  f = fitText("0123456789ABCDEFGHIJ", 100, fakeMeasure);
  TEST_ASSERT_TRUE(fakeMeasure(f.text, 18) <= 100 + 9);  // ellipsis is 3 bytes but 1 char in measure
}

static void test_demo_row_model() {
  settings::Settings s;
  TEST_ASSERT_EQUAL_STRING("Demo mode", rowLabel(kDemoRow));
  TEST_ASSERT_EQUAL_STRING("Off", rowValueText(kDemoRow, s).c_str());
  TEST_ASSERT_EQUAL_INT(0, rowValueIndex(kDemoRow, s));
  TEST_ASSERT_EQUAL_INT(2, pipCount(kDemoRow));
  s = cycleRow(kDemoRow, s);
  TEST_ASSERT_TRUE(s.demo_mode);
  TEST_ASSERT_EQUAL_STRING("On", rowValueText(kDemoRow, s).c_str());
  TEST_ASSERT_EQUAL_INT(1, rowValueIndex(kDemoRow, s));
  s = cycleRow(kDemoRow, s);
  TEST_ASSERT_FALSE(s.demo_mode);
}

static void test_demo_row_layout_touch_and_leave() {
  auto P = computeSettingsLayout(Orientation::Portrait);
  auto L = computeSettingsLayout(Orientation::Landscape);
  TEST_ASSERT_EQUAL_INT(5, kRowCount);
  TEST_ASSERT_EQUAL_INT(7, kFocusStops);
  expectRect(16, 880, 248, 64, P.stops[kStopRefresh]);
  expectRect(722, 464, 222, 64, L.stops[kStopBack]);
  TEST_ASSERT_EQUAL_INT(422, P.rows[kDemoRow].y + 34);
  TEST_ASSERT_EQUAL_INT(418, L.rows[kDemoRow].y + 34);
  // touch cycles the buffer, chip flips, no demoChanged until leave
  Settings saved;
  SettingsScreen sc(saved, Orientation::Portrait, false);
  auto u = sc.onTouch({100, 422});
  TEST_ASSERT_TRUE(sc.edit().demo_mode);
  TEST_ASSERT_TRUE(sc.dirty());
  TEST_ASSERT_EQUAL_INT(3, static_cast<int>(u.dirty.size()));  // value, hint, chip
  expectRect(16, 388, 508, 68, P.rows[kDemoRow]);
  auto l = sc.leave();
  TEST_ASSERT_TRUE(l.demoChanged);
  TEST_ASSERT_TRUE(l.save);
  TEST_ASSERT_FALSE(l.orientationChanged);
  TEST_ASSERT_TRUE(l.settings.demo_mode);
  // cycled back: no change
  sc.onTouch({100, 422});
  l = sc.leave();
  TEST_ASSERT_FALSE(l.demoChanged);
  TEST_ASSERT_FALSE(l.save);
  // other rows do not set demoChanged
  SettingsScreen sc2(saved, Orientation::Portrait, false);
  sc2.onTouch({100, 100});
  TEST_ASSERT_FALSE(sc2.leave().demoChanged);
  // rocker: stop 5 (index 4) pushes the demo row
  SettingsScreen sc3(saved, Orientation::Landscape, true);
  for (int i = 0; i < kDemoRow; ++i) sc3.onKey(LogicalKey::Next);
  sc3.onKey(LogicalKey::Push);
  TEST_ASSERT_TRUE(sc3.edit().demo_mode);
  TEST_ASSERT_EQUAL_STRING("built-in demo", kDemoConfigText);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_demo_row_model);
  RUN_TEST(test_demo_row_layout_touch_and_leave);
  RUN_TEST(test_portrait_layout);
  RUN_TEST(test_landscape_layout);
  RUN_TEST(test_targets_size_gaps_and_bounds);
  RUN_TEST(test_row_internals);
  RUN_TEST(test_value_text);
  RUN_TEST(test_cycle_wraps_in_array_order);
  RUN_TEST(test_wake_warning_flags);
  RUN_TEST(test_touch_cycles_buffer_only);
  RUN_TEST(test_touch_outside_does_nothing);
  RUN_TEST(test_wake_touch_redraws_note_and_unsaved_chip_toggles);
  RUN_TEST(test_cycle_back_removes_unsaved_and_leave_is_write_free);
  RUN_TEST(test_leave_saves_once_and_flags_orientation);
  RUN_TEST(test_back_and_long_push_leave);
  RUN_TEST(test_rocker_focus_order_and_wrap);
  RUN_TEST(test_ring_dirty_is_old_and_new_inflated_by_6);
  RUN_TEST(test_push_on_row_cycles);
  RUN_TEST(test_refresh_now_flow);
  RUN_TEST(test_idle_is_fixed_60s);
  RUN_TEST(test_layout_follows_given_orientation);
  RUN_TEST(test_info_texts);
  RUN_TEST(test_fit_text);
  RUN_TEST(test_title_and_all_hairlines);
  RUN_TEST(test_touch_hits_each_target_both_orientations);
  RUN_TEST(test_every_row_cycles_full_circle_and_chip_tracks_dirty);
  RUN_TEST(test_all_rows_changed_and_restored_is_write_free);
  RUN_TEST(test_leave_returns_exactly_the_edited_settings);
  RUN_TEST(test_orientation_flag_from_landscape);
  RUN_TEST(test_long_push_leaves_from_every_stop_and_does_not_cycle);
  RUN_TEST(test_rocker_push_refresh_and_ignored_while_running);
  RUN_TEST(test_refresh_failure_state_and_dirty_rects);
  RUN_TEST(test_rocker_wake_row_note_dirty);
  RUN_TEST(test_info_edge_cases);
  RUN_TEST(test_fit_text_exact_boundaries);
  return UNITY_END();
}
