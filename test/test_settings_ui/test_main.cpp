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
  s.wifi_profile = "Hotspot";
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
  TEST_ASSERT_EQUAL_STRING("Home", s.wifi_profile.c_str());
  TEST_ASSERT_EQUAL_STRING("Home", rowValueText(RowId::Wifi, s, n).c_str());
  s = cycleRow(RowId::Wifi, s, n);
  TEST_ASSERT_EQUAL_STRING("Hotspot", rowValueText(RowId::Wifi, s, n).c_str());
  s = cycleRow(RowId::Wifi, s, n);
  TEST_ASSERT_TRUE(s.wifi_profile.empty());
  TEST_ASSERT_EQUAL_STRING("Auto", rowValueText(RowId::Wifi, s, n).c_str());
  s.wifi_profile = "Gone";  // profile no longer in secrets.yaml: counts as Auto; cycling goes to the first
  TEST_ASSERT_EQUAL_STRING("Auto", rowValueText(RowId::Wifi, s, n).c_str());
  TEST_ASSERT_EQUAL_STRING("Home", cycleRow(RowId::Wifi, s, n).wifi_profile.c_str());
  NetworkInfo none;  // no profiles: stays Auto
  TEST_ASSERT_TRUE(cycleRow(RowId::Wifi, Settings(), none).wifi_profile.empty());

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
  const NetworkInfo n = twoProfiles();
  TEST_ASSERT_FALSE(pageShowsNote(Page::Sleep, s, n));
  s.wake_mode = WakeMode::Touch;
  TEST_ASSERT_TRUE(pageShowsNote(Page::Sleep, s, n));
  TEST_ASSERT_FALSE(pageShowsNote(Page::Network, s, n));
  s.wifi_profile = "Home";
  TEST_ASSERT_TRUE(pageShowsNote(Page::Network, s, n));
  s.wifi_profile = "Gone";  // not in the list: counts as Auto, no note
  TEST_ASSERT_FALSE(pageShowsNote(Page::Network, s, n));
  TEST_ASSERT_FALSE(pageShowsNote(Page::Tools, s, n));
  s.debug_log = true;
  TEST_ASSERT_TRUE(pageShowsNote(Page::Tools, s, n));
  TEST_ASSERT_FALSE(pageShowsNote(Page::Display, s, n));
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
  TEST_ASSERT_EQUAL_STRING("Home", s.edit().wifi_profile.c_str());
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
  TEST_ASSERT_EQUAL_STRING("Home", r.settings.wifi_profile.c_str());
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

// ---- restored coverage (review of task 047 UI) ----

void test_info_rows_rects_and_hairlines() {
  const int py[] = {504, 552, 600, 648, 696, 760};
  const int ph[] = {48, 48, 48, 48, 64, 48};
  SettingsLayout P = computeSettingsLayout(Orientation::Portrait);
  expectRect(16, 0, 200, 54, P.title);
  for (int i = 0; i < kInfoRowCount; ++i) {
    expectRect(16, py[i], 508, ph[i], P.info[i].rect);
    expectRect(16, py[i], 508, 1, P.info[i].hairline);
    TEST_ASSERT_EQUAL_INT(16, P.info[i].labelX);
    TEST_ASSERT_EQUAL_INT(16 + 158, P.info[i].valueX);
    TEST_ASSERT_EQUAL_INT(py[i] + ph[i] / 2, P.info[i].cy);
    TEST_ASSERT_EQUAL_INT(508 - 158, P.info[i].valueWidth);
  }
  TEST_ASSERT_EQUAL_INT(P.info[4].cy - 14, P.info[4].cyLine1);
  TEST_ASSERT_EQUAL_INT(P.info[4].cy + 14, P.info[4].cyLine2);
  const int ly[] = {64, 120, 176, 232, 288, 360};
  const int lh[] = {56, 56, 56, 56, 72, 56};
  SettingsLayout L = computeSettingsLayout(Orientation::Landscape);
  expectRect(16, 0, 200, 50, L.title);
  for (int i = 0; i < kInfoRowCount; ++i) {
    expectRect(488, ly[i], 456, lh[i], L.info[i].rect);
    expectRect(488, ly[i], 456, 1, L.info[i].hairline);
    TEST_ASSERT_EQUAL_INT(488 + 158, L.info[i].valueX);
  }
  // overview stops mirror categories + the two buttons
  for (int i = 0; i < kCategoryCount; ++i) TEST_ASSERT_TRUE(L.stops[i] == L.categories[i]);
  TEST_ASSERT_TRUE(L.stops[kStopRefresh] == L.refreshButton);
  TEST_ASSERT_TRUE(L.stops[kStopBack] == L.backButton);
}

void test_row_internals_pips_notes_buttons() {
  Rect row{16, 68, 508, 72};
  expectRect(32, 80, 220, 28, rowLabelRect(row));  // y = 68 + 36 - 24
  expectRect(32, 108, 220, 18, rowHintRect(row));
  TEST_ASSERT_EQUAL_INT(68 + 26, rowLabelCy(row));
  TEST_ASSERT_EQUAL_INT(68 + 49, rowHintCy(row));
  TEST_ASSERT_EQUAL_INT(68 + 36, rowValueCy(row));
  TEST_ASSERT_EQUAL_INT(16 + 492, rowValueRightX(row));
  Rect land{16, 64, 456, 68};
  TEST_ASSERT_EQUAL_INT(16 + 440, rowValueRightX(land));
  TEST_ASSERT_EQUAL_INT(64 + 24, rowLabelCy(land));
  TEST_ASSERT_EQUAL_INT(64 + 47, rowHintCy(land));
  TEST_ASSERT_EQUAL_INT(64 + 34, rowValueCy(land));
  TEST_ASSERT_EQUAL_INT(16 + 22, pipCentre(row, 0).x);
  TEST_ASSERT_EQUAL_INT(16 + 22 + 100, pipCentre(row, 5).x);
  TEST_ASSERT_EQUAL_INT(68 + 49, pipCentre(row, 0).y);
  TEST_ASSERT_EQUAL_INT(2, pipCount(RowId::Orientation));
  TEST_ASSERT_EQUAL_INT(3, pipCount(RowId::Wake));
  TEST_ASSERT_EQUAL_INT(6, pipCount(RowId::Refresh));
  TEST_ASSERT_EQUAL_INT(4, pipCount(RowId::Idle));
  TEST_ASSERT_EQUAL_INT(0, pipCount(RowId::Wifi));
  Rect note{16, 320, 508, 64};
  expectRect(28, 332, 40, 40, noteIconRect(note));
  TEST_ASSERT_EQUAL_INT(80, noteTextX(note));
  TEST_ASSERT_EQUAL_INT(341, noteLineCy(note, 0));
  TEST_ASSERT_EQUAL_INT(363, noteLineCy(note, 1));
  Rect btn{16, 880, 248, 64};
  expectRect(28, 892, 40, 40, buttonIconRect(btn));
  expectRect(76, 880, 180, 64, buttonLabelRect(btn));
  // hint rows (88 high)
  Rect hr{16, 68, 508, 88};
  TEST_ASSERT_EQUAL_INT(68 + 32, hintRowLabelCy(hr));
  TEST_ASSERT_EQUAL_INT(68 + 62, hintRowHintCy(hr));
  TEST_ASSERT_EQUAL_INT(68 + 44, hintRowValueCy(hr));
  expectRect(16 + 508 - 268, 68, 268, 88, hintRowValueRect(hr));
}

void test_touch_outside_does_nothing() {
  for (auto o : {Orientation::Portrait, Orientation::Landscape}) {
    SettingsScreen sc(Settings(), o, false);
    Update u = sc.onTouch({0, 0});
    TEST_ASSERT_TRUE(u.action == Action::None && u.dirty.empty());
    const Rect& cat = sc.layout().categories[3];
    u = sc.onTouch({cat.x - 1, cat.y + 10});  // left margin
    TEST_ASSERT_TRUE(u.action == Action::None && u.dirty.empty());
    const Rect& info = sc.layout().info[2].rect;  // read-only info area
    u = sc.onTouch({info.x + 20, info.y + 10});
    TEST_ASSERT_TRUE(u.action == Action::None && u.dirty.empty());
    TEST_ASSERT_FALSE(sc.dirty());
    // on a page: outside the rows and Back
    touchCentre(sc, sc.layout().categories[1]);
    const PageLayout& P = sc.pageLayout();
    u = sc.onTouch({P.backButton.x - 1, P.backButton.y + 10});
    TEST_ASSERT_TRUE(u.action == Action::None && u.dirty.empty());
    u = sc.onTouch({1, P.screenH - 1});
    TEST_ASSERT_TRUE(u.action == Action::None && u.dirty.empty());
    TEST_ASSERT_FALSE(sc.dirty());
    TEST_ASSERT_TRUE(sc.page() == Page::Sleep);
  }
}

void test_touch_hits_every_target_both_orientations() {
  for (auto o : {Orientation::Portrait, Orientation::Landscape}) {
    // overview: corners hit, one pixel outside does not
    SettingsLayout L = computeSettingsLayout(o);
    for (int i = 0; i < kOverviewStops; ++i) {
      const Rect r = L.stops[i];
      Settings st;
      SettingsScreen a(st, o, false);
      Update u = a.onTouch({r.x, r.y});
      TEST_ASSERT_TRUE(u.action != Action::None || !u.dirty.empty());
      SettingsScreen b(st, o, false);
      u = b.onTouch({r.x + r.w - 1, r.y + r.h - 1});
      TEST_ASSERT_TRUE(u.action != Action::None || !u.dirty.empty());
      SettingsScreen c(st, o, false);
      u = c.onTouch({r.x - 1, r.y + r.h / 2});
      TEST_ASSERT_TRUE(u.action == Action::None && u.dirty.empty());
      u = c.onTouch({r.x + r.w, r.y + r.h / 2});
      TEST_ASSERT_TRUE(u.action == Action::None && u.dirty.empty());
      TEST_ASSERT_EQUAL_INT(0, c.focus());  // touch never moves focus or shows the ring
      TEST_ASSERT_FALSE(c.ringVisible());
    }
    // gap between the footer buttons
    SettingsScreen g(Settings(), o, false);
    Update u = g.onTouch({L.refreshButton.x + L.refreshButton.w + 5, L.refreshButton.y + 32});
    TEST_ASSERT_TRUE(u.action == Action::None && u.dirty.empty());
    // every category page: each stop hits at both corners
    for (int cat = 0; cat < kCategoryCount; ++cat) {
      SettingsScreen probe(Settings(), o, false);
      probe.setNetwork(twoProfiles());
      touchCentre(probe, probe.layout().categories[cat]);
      const PageLayout P = probe.pageLayout();
      for (int i = 0; i < P.stopCount; ++i) {
        const Rect r = P.stops[i];
        for (const Point pt : {Point{r.x, r.y}, Point{r.x + r.w - 1, r.y + r.h - 1}}) {
          SettingsScreen s(Settings(), o, false);
          s.setNetwork(twoProfiles());
          touchCentre(s, s.layout().categories[cat]);
          Update h = s.onTouch(pt);
          TEST_ASSERT_TRUE(h.action != Action::None || !h.dirty.empty());
        }
        SettingsScreen s2(Settings(), o, false);
        s2.setNetwork(twoProfiles());
        touchCentre(s2, s2.layout().categories[cat]);
        Update h = s2.onTouch({r.x - 1, r.y + r.h / 2});
        TEST_ASSERT_TRUE(h.action == Action::None && h.dirty.empty());
      }
    }
  }
}

void test_long_push_from_every_stop_and_does_not_cycle() {
  for (int stop = 0; stop < kOverviewStops; ++stop) {  // overview: leave
    SettingsScreen sc(Settings(), Orientation::Portrait, true);
    for (int i = 0; i < stop; ++i) sc.onKey(LogicalKey::Next);
    Update u = sc.onKey(LogicalKey::LongPush);
    TEST_ASSERT_TRUE(u.action == Action::Leave);
    TEST_ASSERT_FALSE(sc.dirty());
    TEST_ASSERT_TRUE(sc.refreshState() == RefreshState::Idle);
  }
  for (int cat = 0; cat < kCategoryCount; ++cat) {  // page: one level up, from every stop, nothing cycles
    SettingsScreen probe(Settings(), Orientation::Portrait, true);
    probe.setNetwork(twoProfiles());
    touchCentre(probe, probe.layout().categories[cat]);
    const int stops = probe.stopCount();
    for (int stop = 0; stop < stops; ++stop) {
      SettingsScreen sc(Settings(), Orientation::Portrait, true);
      sc.setNetwork(twoProfiles());
      touchCentre(sc, sc.layout().categories[cat]);
      for (int i = 0; i < stop; ++i) sc.onKey(LogicalKey::Next);
      Update u = sc.onKey(LogicalKey::LongPush);
      TEST_ASSERT_TRUE(u.action == Action::PageChange);
      TEST_ASSERT_TRUE(sc.page() == Page::Main);
      TEST_ASSERT_EQUAL_INT(cat, sc.focus());
      TEST_ASSERT_FALSE(sc.dirty());
    }
  }
}

void test_refresh_done_and_failed_states_and_dirty_rects() {
  for (auto o : {Orientation::Portrait, Orientation::Landscape}) {
    SettingsScreen sc(Settings(), o, false);
    const SettingsLayout& L = sc.layout();
    Update u = touchCentre(sc, L.refreshButton);
    TEST_ASSERT_TRUE(u.action == Action::StartRefresh);
    TEST_ASSERT_TRUE(sc.refreshState() == RefreshState::Running);
    TEST_ASSERT_EQUAL_INT(2, (int)u.dirty.size());  // button + HA row (pending look)
    TEST_ASSERT_TRUE(u.dirty[0] == L.refreshButton);
    TEST_ASSERT_TRUE(u.dirty[1] == L.info[2].rect);
    // a second tap while running is ignored
    u = touchCentre(sc, L.refreshButton);
    TEST_ASSERT_TRUE(u.action == Action::None && u.dirty.empty());
    // failure: button + HA row, battery is not re-read
    u = sc.refreshDone(false);
    TEST_ASSERT_TRUE(sc.refreshState() == RefreshState::Failed);
    TEST_ASSERT_EQUAL_INT(2, (int)u.dirty.size());
    for (const Rect& r : u.dirty) TEST_ASSERT_FALSE(r == L.info[1].rect);
    TEST_ASSERT_TRUE(sc.refreshDone(false).dirty.empty());  // a second notification is ignored
    // retry from Failed; success redraws the battery too
    TEST_ASSERT_TRUE(touchCentre(sc, L.refreshButton).action == Action::StartRefresh);
    u = sc.refreshDone(true);
    TEST_ASSERT_TRUE(sc.refreshState() == RefreshState::Idle);
    TEST_ASSERT_EQUAL_INT(3, (int)u.dirty.size());
    TEST_ASSERT_TRUE(u.dirty[2] == L.info[1].rect);
    TEST_ASSERT_TRUE(sc.refreshDone(true).dirty.empty());  // nothing running
  }
}

void test_refresh_completion_does_not_touch_the_edit_buffer() {
  SettingsScreen sc(Settings(), Orientation::Portrait, true);
  sc.onKey(LogicalKey::Prev);  // Back
  sc.onKey(LogicalKey::Prev);  // Refresh now
  TEST_ASSERT_EQUAL_INT(kStopRefresh, sc.focus());
  TEST_ASSERT_TRUE(sc.onKey(LogicalKey::Push).action == Action::StartRefresh);
  TEST_ASSERT_TRUE(sc.onKey(LogicalKey::Push).action == Action::None);  // ignored while running
  touchCentre(sc, sc.layout().categories[0]);  // a page opens while the fetch runs
  touchCentre(sc, sc.pageLayout().rows[0]);
  TEST_ASSERT_TRUE(sc.dirty());
  sc.refreshDone(true);
  TEST_ASSERT_TRUE(sc.dirty());
  TEST_ASSERT_TRUE(sc.edit().orientation == Orientation::Landscape);
}

void test_orientation_changed_flag_both_directions() {
  Settings saved;
  saved.orientation = Orientation::Landscape;
  SettingsScreen sc(saved, Orientation::Landscape, false);
  touchCentre(sc, sc.layout().categories[0]);
  touchCentre(sc, sc.pageLayout().rows[0]);  // -> Portrait
  LeaveResult l = sc.leave();
  TEST_ASSERT_TRUE(l.save && l.orientationChanged);
  TEST_ASSERT_TRUE(l.settings.orientation == Orientation::Portrait);
  TEST_ASSERT_FALSE(l.networkChanged);
  touchCentre(sc, sc.pageLayout().rows[0]);  // changed back: no flag, no write
  l = sc.leave();
  TEST_ASSERT_FALSE(l.orientationChanged);
  TEST_ASSERT_FALSE(l.save);
  // portrait -> landscape
  SettingsScreen p(Settings(), Orientation::Portrait, false);
  touchCentre(p, p.layout().categories[0]);
  touchCentre(p, p.pageLayout().rows[0]);
  l = p.leave();
  TEST_ASSERT_TRUE(l.orientationChanged && l.save);
  TEST_ASSERT_TRUE(l.settings.orientation == Orientation::Landscape);
  TEST_ASSERT_FALSE(l.demoChanged);
  TEST_ASSERT_FALSE(l.debugLogChanged);
}

void test_network_rows_dirty_includes_hint_row_value_only() {
  SettingsScreen s(Settings(), Orientation::Landscape, false);
  s.setNetwork(twoProfiles());
  touchCentre(s, s.layout().categories[2]);
  const PageLayout& P = s.pageLayout();
  Update u = touchCentre(s, P.rows[1]);  // HA connection (landscape: right column)
  TEST_ASSERT_TRUE(u.dirty[0] == hintRowValueRect(P.rows[1]));
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
  RUN_TEST(test_info_rows_rects_and_hairlines);
  RUN_TEST(test_row_internals_pips_notes_buttons);
  RUN_TEST(test_touch_outside_does_nothing);
  RUN_TEST(test_touch_hits_every_target_both_orientations);
  RUN_TEST(test_long_push_from_every_stop_and_does_not_cycle);
  RUN_TEST(test_refresh_done_and_failed_states_and_dirty_rects);
  RUN_TEST(test_refresh_completion_does_not_touch_the_edit_buffer);
  RUN_TEST(test_orientation_changed_flag_both_directions);
  RUN_TEST(test_network_rows_dirty_includes_hint_row_value_only);
  return UNITY_END();
}
