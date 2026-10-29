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
  TEST_ASSERT_EQUAL_STRING("internal, ok 14:05", haText(h, now).text.c_str());
  h.lastLink = HaInfo::Link::Remote;
  TEST_ASSERT_EQUAL_STRING("external, ok 14:05", haText(h, now).text.c_str());
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

static void test_value_fit_level() {
  TEST_ASSERT_EQUAL(0, settings_ui::valueFitLevel(100, 60, 100));
  TEST_ASSERT_EQUAL(1, settings_ui::valueFitLevel(101, 60, 100));
  TEST_ASSERT_EQUAL(1, settings_ui::valueFitLevel(300, 100, 100));
  TEST_ASSERT_EQUAL(2, settings_ui::valueFitLevel(300, 101, 100));
}

// ---- revision 7: overview, category pages, shared buffer ----

static NetworkInfo twoProfiles() {
  NetworkInfo n;
  n.profiles = {"Home", "Hotspot"};
  return n;
}

static Update touchCentre(SettingsScreen& s, const Rect& r) { return s.onTouch({r.x + r.w / 2, r.y + r.h / 2}); }

void test_overview_layout_both_orientations() {
  SettingsLayout p = computeSettingsLayout(Orientation::Portrait);
  expectRect(16, 68, 508, 96, p.categories[0]);
  expectRect(16, 176, 508, 96, p.categories[1]);
  expectRect(16, 284, 508, 96, p.categories[2]);
  expectRect(16, 392, 508, 96, p.categories[3]);
  expectRect(16, 504, 508, 48, p.info[0].rect);
  expectRect(16, 696, 508, 64, p.info[4].rect);
  TEST_ASSERT_EQUAL_INT(808, p.info[5].rect.y + p.info[5].rect.h);
  expectRect(16, 880, 248, 64, p.refreshButton);
  expectRect(276, 880, 248, 64, p.backButton);
  SettingsLayout l = computeSettingsLayout(Orientation::Landscape);
  expectRect(16, 64, 456, 96, l.categories[0]);
  expectRect(16, 388, 456, 96, l.categories[3]);
  expectRect(488, 64, 456, 56, l.info[0].rect);
  expectRect(722, 464, 222, 64, l.backButton);
  expectRect(16 + 456 - 52, 64 + 28, 40, 40, categoryChevronRect(l.categories[0]));
  TEST_ASSERT_EQUAL_INT(64 + 34, categoryLabelCy(l.categories[0]));
  TEST_ASSERT_EQUAL_INT(64 + 66, categorySummaryCy(l.categories[0]));
}

void test_page_layouts() {
  PageLayout d = computePageLayout(Page::Display, Orientation::Portrait);
  TEST_ASSERT_EQUAL_INT(1, d.rowCount);
  expectRect(16, 68, 508, 72, d.rows[0]);
  expectRect(16, 152, 508, 32, d.caption);
  TEST_ASSERT_EQUAL_INT(0, d.note.w);
  TEST_ASSERT_EQUAL_INT(2, d.stopCount);
  PageLayout dl = computePageLayout(Page::Display, Orientation::Landscape);
  expectRect(16, 64, 456, 68, dl.rows[0]);
  TEST_ASSERT_EQUAL_INT(132, dl.caption.y);

  PageLayout s = computePageLayout(Page::Sleep, Orientation::Portrait);
  expectRect(16, 68, 508, 72, s.rows[0]);
  expectRect(16, 152, 508, 72, s.rows[1]);
  expectRect(16, 236, 508, 72, s.rows[2]);
  expectRect(16, 320, 508, 64, s.note);
  PageLayout sl = computePageLayout(Page::Sleep, Orientation::Landscape);
  expectRect(16, 144, 456, 68, sl.rows[1]);
  expectRect(16, 304, 456, 64, sl.note);

  PageLayout n = computePageLayout(Page::Network, Orientation::Portrait);
  expectRect(16, 68, 508, 88, n.rows[0]);
  expectRect(16, 168, 508, 88, n.rows[1]);
  expectRect(16, 268, 508, 64, n.note);
  PageLayout nl = computePageLayout(Page::Network, Orientation::Landscape);
  expectRect(488, 64, 456, 88, nl.rows[1]);
  expectRect(16, 164, 456, 64, nl.note);

  PageLayout t = computePageLayout(Page::Tools, Orientation::Landscape);
  expectRect(488, 164, 456, 64, t.note);
  TEST_ASSERT_TRUE(t.rowIds[0] == RowId::Demo && t.rowIds[1] == RowId::Debug);
}

void test_page_targets_size_gaps_and_screen_bounds() {
  for (Orientation o : {Orientation::Portrait, Orientation::Landscape}) {
    for (int pg = 1; pg <= 4; ++pg) {
      PageLayout L = computePageLayout(static_cast<Page>(pg), o);
      for (int i = 0; i < L.stopCount; ++i) {
        const Rect& a = L.stops[i];
        TEST_ASSERT_TRUE(a.h >= 64 || a.h >= 68);
        TEST_ASSERT_TRUE(a.x >= 0 && a.y >= 0 && a.x + a.w <= L.screenW && a.y + a.h <= L.screenH);
        for (int j = i + 1; j < L.stopCount; ++j) {
          const Rect& b = L.stops[j];
          const bool apartX = a.x + a.w + 12 <= b.x || b.x + b.w + 12 <= a.x;
          const bool apartY = a.y + a.h + 12 <= b.y || b.y + b.h + 12 <= a.y;
          TEST_ASSERT_TRUE(apartX || apartY);
        }
      }
      if (L.note.w > 0) TEST_ASSERT_TRUE(L.note.y + L.note.h <= L.screenH);
    }
    SettingsLayout L = computeSettingsLayout(o);
    for (int i = 0; i < kOverviewStops; ++i)
      for (int j = i + 1; j < kOverviewStops; ++j) {
        const Rect &a = L.stops[i], &b = L.stops[j];
        const bool apartX = a.x + a.w + 12 <= b.x || b.x + b.w + 12 <= a.x;
        const bool apartY = a.y + a.h + 12 <= b.y || b.y + b.h + 12 <= a.y;
        TEST_ASSERT_TRUE(apartX || apartY);
      }
  }
}

void test_summaries() {
  Settings s;
  NetworkInfo n = twoProfiles();
  TEST_ASSERT_EQUAL_STRING("Portrait", categorySummary(0, s, n).c_str());
  s.orientation = Orientation::Landscape;
  TEST_ASSERT_EQUAL_STRING("Landscape", categorySummary(0, s, n).c_str());
  s.wake_mode = WakeMode::Touch;
  s.refresh_interval_min = 60;
  s.idle_timeout_s = 20;
  TEST_ASSERT_EQUAL_STRING("Touch, 60 min, 20 s", categorySummary(1, s, n).c_str());
  s.wake_mode = WakeMode::TouchAndButton;
  TEST_ASSERT_EQUAL_STRING("Touch & Button, 60 min, 20 s", categorySummary(1, s, n).c_str());
  TEST_ASSERT_EQUAL_STRING("Both, 60 min, 20 s", categorySummary(1, s, n, true).c_str());
  TEST_ASSERT_EQUAL_STRING("Wi-Fi Auto, HA Auto", categorySummary(2, s, n).c_str());
  s.wifi_profile = 2;
  s.ha_link = settings::HaLink::Internal;
  TEST_ASSERT_EQUAL_STRING("Wi-Fi Hotspot, HA Internal", categorySummary(2, s, n).c_str());
  TEST_ASSERT_EQUAL_STRING("Demo Off, Log Off", categorySummary(3, s, n).c_str());
  s.debug_log = true;
  TEST_ASSERT_EQUAL_STRING("Demo Off, Log On", categorySummary(3, s, n).c_str());
}

void test_wifi_and_ha_cycling() {
  NetworkInfo n = twoProfiles();
  Settings s;
  s = cycleRow(RowId::Wifi, s, n);
  TEST_ASSERT_EQUAL_INT(1, s.wifi_profile);
  TEST_ASSERT_EQUAL_STRING("Home", rowValueText(RowId::Wifi, s, n).c_str());
  s = cycleRow(RowId::Wifi, s, n);
  TEST_ASSERT_EQUAL_STRING("Hotspot", rowValueText(RowId::Wifi, s, n).c_str());
  s = cycleRow(RowId::Wifi, s, n);
  TEST_ASSERT_EQUAL_INT(0, s.wifi_profile);
  TEST_ASSERT_EQUAL_STRING("Auto", rowValueText(RowId::Wifi, s, n).c_str());
  s.wifi_profile = 4;  // profile no longer in secrets.yaml: back to Auto, shown as such a profile meanwhile
  TEST_ASSERT_EQUAL_STRING("Profile 4", rowValueText(RowId::Wifi, s, n).c_str());
  TEST_ASSERT_EQUAL_INT(0, cycleRow(RowId::Wifi, s, n).wifi_profile);
  NetworkInfo none;  // no profiles: stays Auto
  TEST_ASSERT_EQUAL_INT(0, cycleRow(RowId::Wifi, Settings(), none).wifi_profile);

  s = Settings();
  const char* want[] = {"Internal", "External", "Auto", "Internal"};
  for (const char* w : want) {
    s = cycleRow(RowId::HaLink, s, n);
    TEST_ASSERT_EQUAL_STRING(w, rowValueText(RowId::HaLink, s, n).c_str());
  }
}

void test_network_hints() {
  Settings s;
  NetworkInfo n = twoProfiles();
  TEST_ASSERT_EQUAL_STRING("Not connected", wifiHint(n).c_str());
  TEST_ASSERT_EQUAL_STRING("Link: -", haLinkHint(s, n).c_str());
  n.connected = "Hotspot";
  n.link = LinkState::External;
  n.profileDecides = true;
  TEST_ASSERT_EQUAL_STRING("Connected: Hotspot", wifiHint(n).c_str());
  TEST_ASSERT_EQUAL_STRING("Link: external (profile)", haLinkHint(s, n).c_str());
  s.ha_link = settings::HaLink::External;  // forced: the profile does not decide
  TEST_ASSERT_EQUAL_STRING("Link: external", haLinkHint(s, n).c_str());
  n.link = LinkState::Internal;
  n.profileDecides = false;
  s.ha_link = settings::HaLink::Auto;
  TEST_ASSERT_EQUAL_STRING("Link: internal", haLinkHint(s, n).c_str());
  n.link = LinkState::None;
  TEST_ASSERT_EQUAL_STRING("Link: none (offline)", haLinkHint(s, n).c_str());
  n.demo = true;
  TEST_ASSERT_EQUAL_STRING("Not used (demo mode)", wifiHint(n).c_str());
  TEST_ASSERT_EQUAL_STRING("Not used (demo mode)", haLinkHint(s, n).c_str());
}

void test_notes_per_page() {
  Settings s;
  TEST_ASSERT_FALSE(pageShowsNote(Page::Sleep, s));
  s.wake_mode = WakeMode::Touch;
  TEST_ASSERT_TRUE(pageShowsNote(Page::Sleep, s));
  TEST_ASSERT_FALSE(pageShowsNote(Page::Network, s));
  s.wifi_profile = 1;
  TEST_ASSERT_TRUE(pageShowsNote(Page::Network, s));
  TEST_ASSERT_FALSE(pageShowsNote(Page::Tools, s));
  s.debug_log = true;
  TEST_ASSERT_TRUE(pageShowsNote(Page::Tools, s));
  TEST_ASSERT_FALSE(pageShowsNote(Page::Display, s));
  TEST_ASSERT_EQUAL_STRING(kToolsNoCardLine1, pageNote(Page::Tools, false).line1);
  TEST_ASSERT_EQUAL_STRING(kNetNoteLine1, pageNote(Page::Network, true).line1);
}

void test_category_touch_opens_page_without_saving_and_back_returns() {
  SettingsScreen s(Settings(), Orientation::Portrait, false);
  s.setNetwork(twoProfiles());
  Update u = touchCentre(s, s.layout().categories[2]);
  TEST_ASSERT_TRUE(u.action == Action::PageChange);
  TEST_ASSERT_TRUE(s.page() == Page::Network);
  TEST_ASSERT_FALSE(s.dirty());
  TEST_ASSERT_EQUAL_INT(0, s.focus());
  TEST_ASSERT_EQUAL_INT(3, s.stopCount());
  // Back: up, overview focus on the category just left
  u = touchCentre(s, s.pageLayout().backButton);
  TEST_ASSERT_TRUE(u.action == Action::PageChange);
  TEST_ASSERT_TRUE(s.page() == Page::Main);
  TEST_ASSERT_EQUAL_INT(2, s.focus());
}

void test_one_shared_buffer_across_pages_and_chip() {
  SettingsScreen s(Settings(), Orientation::Portrait, true);
  s.setNetwork(twoProfiles());
  touchCentre(s, s.layout().categories[0]);  // Display
  Update u = touchCentre(s, s.pageLayout().rows[0]);  // Orientation -> Landscape
  TEST_ASSERT_TRUE(s.edit().orientation == Orientation::Landscape);
  bool chip = false;
  for (const Rect& r : u.dirty) chip |= r.x == s.pageLayout().unsavedChip.x && r.y == s.pageLayout().unsavedChip.y;
  TEST_ASSERT_TRUE(chip);
  touchCentre(s, s.pageLayout().backButton);
  touchCentre(s, s.layout().categories[2]);  // Network
  u = touchCentre(s, s.pageLayout().rows[1]);  // HA connection
  TEST_ASSERT_TRUE(s.edit().ha_link == settings::HaLink::Internal);
  TEST_ASSERT_EQUAL_INT(1, (int)u.dirty.size());  // value only: chip already shown, no note
  u = touchCentre(s, s.pageLayout().rows[0]);  // Wi-Fi -> Home: forced profile note appears
  TEST_ASSERT_EQUAL_INT(1, s.edit().wifi_profile);
  bool note = false;
  for (const Rect& r : u.dirty) note |= r.y == s.pageLayout().note.y && r.w == s.pageLayout().note.w;
  TEST_ASSERT_TRUE(note);
  // nothing saved by moving between levels; leave reports everything
  touchCentre(s, s.pageLayout().backButton);
  u = touchCentre(s, s.layout().backButton);
  TEST_ASSERT_TRUE(u.action == Action::Leave);
  LeaveResult r = s.leave();
  TEST_ASSERT_TRUE(r.save && r.orientationChanged && r.networkChanged);
  TEST_ASSERT_FALSE(r.demoChanged);
  TEST_ASSERT_EQUAL_INT(1, r.settings.wifi_profile);
}

void test_cycling_back_makes_leave_write_free() {
  SettingsScreen s(Settings(), Orientation::Portrait, true);
  s.setNetwork(twoProfiles());
  s.onKey(LogicalKey::Push);  // Display
  s.onKey(LogicalKey::Push);  // Orientation twice
  s.onKey(LogicalKey::Push);
  TEST_ASSERT_FALSE(s.dirty());
  s.onKey(LogicalKey::LongPush);
  LeaveResult r = s.leave();
  TEST_ASSERT_FALSE(r.save);
  TEST_ASSERT_FALSE(r.networkChanged);
}

void test_network_changed_flag_only_for_network_fields() {
  SettingsScreen s(Settings(), Orientation::Portrait, true);
  s.setNetwork(twoProfiles());
  touchCentre(s, s.layout().categories[3]);  // Tools
  touchCentre(s, s.pageLayout().rows[1]);    // Debug log
  LeaveResult r = s.leave();
  TEST_ASSERT_TRUE(r.save && r.debugLogChanged);
  TEST_ASSERT_FALSE(r.networkChanged);
}

void test_rocker_focus_orders_and_wrap() {
  SettingsScreen s(Settings(), Orientation::Portrait, true);
  s.setNetwork(twoProfiles());
  TEST_ASSERT_EQUAL_INT(0, s.focus());
  for (int i = 1; i <= 6; ++i) {  // 6 stops: Display, Sleep, Network, Tools, Refresh, Back
    s.onKey(LogicalKey::Next);
    TEST_ASSERT_EQUAL_INT(i % 6, s.focus());
  }
  s.onKey(LogicalKey::Prev);
  TEST_ASSERT_EQUAL_INT(kStopBack, s.focus());
  // enter Sleep & wake: 3 rows + Back
  s.onKey(LogicalKey::Next);  // 0
  s.onKey(LogicalKey::Next);  // 1 Sleep
  Update u = s.onKey(LogicalKey::Push);
  TEST_ASSERT_TRUE(u.action == Action::PageChange);
  TEST_ASSERT_TRUE(s.page() == Page::Sleep);
  TEST_ASSERT_EQUAL_INT(4, s.stopCount());
  TEST_ASSERT_EQUAL_INT(0, s.focus());
  s.onKey(LogicalKey::Prev);
  TEST_ASSERT_EQUAL_INT(3, s.focus());  // Back
  // Push on a row cycles the buffer
  s.onKey(LogicalKey::Next);
  s.onKey(LogicalKey::Push);  // Wake mode Button -> Touch
  TEST_ASSERT_TRUE(s.edit().wake_mode == WakeMode::Touch);
  // Push on Back goes up, focus on the Sleep row
  s.onKey(LogicalKey::Prev);
  u = s.onKey(LogicalKey::Push);
  TEST_ASSERT_TRUE(s.page() == Page::Main);
  TEST_ASSERT_EQUAL_INT(1, s.focus());
  TEST_ASSERT_TRUE(s.dirty());  // going up saves/applies nothing: still buffered
}

void test_long_push_is_one_level_back() {
  SettingsScreen s(Settings(), Orientation::Portrait, true);
  s.setNetwork(twoProfiles());
  for (int cat = 0; cat < 4; ++cat) {
    while (s.focus() != cat) s.onKey(LogicalKey::Next);
    s.onKey(LogicalKey::Push);
    TEST_ASSERT_TRUE(s.page() == categoryPage(cat));
    Update u = s.onKey(LogicalKey::LongPush);
    TEST_ASSERT_TRUE(u.action == Action::PageChange);
    TEST_ASSERT_TRUE(s.page() == Page::Main);
    TEST_ASSERT_EQUAL_INT(cat, s.focus());
  }
  Update u = s.onKey(LogicalKey::LongPush);  // overview: leave
  TEST_ASSERT_TRUE(u.action == Action::Leave);
}

void test_ring_dirty_on_pages() {
  SettingsScreen s(Settings(), Orientation::Landscape, true);
  s.setNetwork(twoProfiles());
  touchCentre(s, s.layout().categories[2]);
  Update u = s.onKey(LogicalKey::Next);  // Wi-Fi -> HA connection (landscape: right column)
  TEST_ASSERT_EQUAL_INT(2, (int)u.dirty.size());
  const Rect infl = ui::inflate(s.pageLayout().rows[1], kRingInflate);
  expectRect(infl.x, infl.y, infl.w, infl.h, u.dirty[1]);
  TEST_ASSERT_EQUAL_INT(488, s.focusRect().x);
}

void test_refresh_still_works_on_overview_only() {
  SettingsScreen s(Settings(), Orientation::Portrait, true);
  s.setNetwork(twoProfiles());
  Update u = touchCentre(s, s.layout().refreshButton);
  TEST_ASSERT_TRUE(u.action == Action::StartRefresh);
  u = touchCentre(s, s.layout().refreshButton);
  TEST_ASSERT_TRUE(u.action == Action::None);
  u = s.refreshDone(true);
  TEST_ASSERT_EQUAL_INT(3, (int)u.dirty.size());
  TEST_ASSERT_TRUE(s.refreshState() == RefreshState::Idle);
  touchCentre(s, s.layout().categories[0]);
  // touches on the overview's rects do nothing on a category page
  u = touchCentre(s, s.layout().refreshButton);
  TEST_ASSERT_TRUE(u.action == Action::None && u.dirty.empty());
}

void test_idle_is_fixed_60s_from_any_page() {
  TEST_ASSERT_FALSE(SettingsScreen::idleExpired(59999));
  TEST_ASSERT_TRUE(SettingsScreen::idleExpired(60000));
}

void test_tools_toggles_and_values() {
  NetworkInfo n;
  Settings s;
  s = cycleRow(RowId::Demo, s, n);
  s = cycleRow(RowId::Debug, s, n);
  TEST_ASSERT_TRUE(s.demo_mode && s.debug_log);
  TEST_ASSERT_EQUAL_STRING("On", rowValueText(RowId::Demo, s, n).c_str());
  s = cycleRow(RowId::Demo, s, n);
  TEST_ASSERT_EQUAL_STRING("Off", rowValueText(RowId::Demo, s, n).c_str());
  TEST_ASSERT_EQUAL_STRING("Sample dashboard, offline", hintRowStaticHint(RowId::Demo));
  TEST_ASSERT_EQUAL_STRING("Log lines to SD card", hintRowStaticHint(RowId::Debug));
}

void test_pip_rows_cycle_full_circle() {
  NetworkInfo n;
  const RowId pips[] = {RowId::Orientation, RowId::Wake, RowId::Refresh, RowId::Idle};
  for (RowId r : pips) {
    Settings s;
    TEST_ASSERT_TRUE(isPipRow(r));
    Settings cur = s;
    for (int i = 0; i < pipCount(r); ++i) cur = cycleRow(r, cur, n);
    TEST_ASSERT_TRUE(cur == s);
  }
  TEST_ASSERT_FALSE(isPipRow(RowId::Wifi));
  TEST_ASSERT_EQUAL_INT(0, pipCount(RowId::Wifi));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_info_texts);
  RUN_TEST(test_fit_text);
  RUN_TEST(test_info_edge_cases);
  RUN_TEST(test_fit_text_exact_boundaries);
  RUN_TEST(test_value_fit_level);
  RUN_TEST(test_overview_layout_both_orientations);
  RUN_TEST(test_page_layouts);
  RUN_TEST(test_page_targets_size_gaps_and_screen_bounds);
  RUN_TEST(test_summaries);
  RUN_TEST(test_wifi_and_ha_cycling);
  RUN_TEST(test_network_hints);
  RUN_TEST(test_notes_per_page);
  RUN_TEST(test_category_touch_opens_page_without_saving_and_back_returns);
  RUN_TEST(test_one_shared_buffer_across_pages_and_chip);
  RUN_TEST(test_cycling_back_makes_leave_write_free);
  RUN_TEST(test_network_changed_flag_only_for_network_fields);
  RUN_TEST(test_rocker_focus_orders_and_wrap);
  RUN_TEST(test_long_push_is_one_level_back);
  RUN_TEST(test_ring_dirty_on_pages);
  RUN_TEST(test_refresh_still_works_on_overview_only);
  RUN_TEST(test_idle_is_fixed_60s_from_any_page);
  RUN_TEST(test_tools_toggles_and_values);
  RUN_TEST(test_pip_rows_cycle_full_circle);
  return UNITY_END();
}
