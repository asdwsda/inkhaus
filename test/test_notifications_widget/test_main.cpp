// Task 103: notifications widget: list parsing, config, view model, popup model, AppState wiring.
#include <unity.h>

#include "app_state.h"
#include "config_cache.h"
#include "notif_list.h"
#include "notif_popup.h"
#include "notif_widget.h"

using namespace uistate;
using settings::Orientation;

void setUp() {}
void tearDown() {}

namespace {

constexpr int64_t kNow = 1793714700;  // 2026-11-03T14:05:00Z
const char *kList =
    "{\"id\":1,\"type\":\"result\",\"success\":true,\"result\":["
    "{\"notification_id\":\"a\",\"title\":\"Old\",\"message\":\"**bold** and [link](http://x/y)\\nsecond\","
    "\"created_at\":\"2026-11-01T08:00:00.123456+00:00\"},"
    "{\"notification_id\":\"b\",\"title\":\"\",\"message\":\"# Head\\n\\n> quote  text\",\"created_at\":\"2026-11-03T12:58:00+00:00\"},"
    "{\"notification_id\":\"c\",\"title\":\"Mid\",\"message\":\"m\",\"created_at\":\"2026-11-02T08:00:00+00:00\"}]}";

NotifStore storeOf(int n) {
  std::vector<NotifItem> v;
  for (int i = 0; i < n; ++i) {
    char id[16];
    snprintf(id, sizeof id, "n%02d", i);
    v.push_back({id, std::string("T") + id, "message", "2026-11-03T12:58:00+00:00"});
  }
  NotifStore s;
  s.setList(std::move(v));
  return s;
}

const char *kYaml = R"(version: 1
tabs:
  - title: A
    widgets:
      - type: notifications
      - type: notifications
        name: Inbox
      - entity: light.x
)";

AppState makeApp(Orientation o) {
  auto r = config::parseConfig(kYaml);
  TEST_ASSERT_TRUE(r.ok());
  AppState a;
  a.init(r.config, o, false);
  return a;
}

ui::Point centreOf(const ui::Rect &r) { return {r.x + r.w / 2, r.y + r.h / 2}; }

}  // namespace

// ---- list parsing ----

void test_parse_list_array_sorted_and_plain() {
  const NotifList l = parse_notification_list(kList);
  TEST_ASSERT_TRUE(l.ok);
  TEST_ASSERT_EQUAL(3, (int)l.items.size());
  TEST_ASSERT_EQUAL_STRING("b", l.items[0].id.c_str());  // newest first
  TEST_ASSERT_EQUAL_STRING("c", l.items[1].id.c_str());
  TEST_ASSERT_EQUAL_STRING("a", l.items[2].id.c_str());
  TEST_ASSERT_EQUAL_STRING("bold and link second", l.items[2].message.c_str());
  TEST_ASSERT_EQUAL_STRING("Head quote text", l.items[0].message.c_str());
  TEST_ASSERT_EQUAL_STRING("", l.items[0].title.c_str());
}

void test_parse_list_map_shape_and_errors() {
  const NotifList m = parse_notification_list(
      "{\"id\":1,\"type\":\"result\",\"success\":true,\"result\":{\"k1\":{\"title\":\"A\",\"message\":\"x\","
      "\"created_at\":\"2026-11-01T00:00:00+00:00\"}}}");
  TEST_ASSERT_TRUE(m.ok);
  TEST_ASSERT_EQUAL(1, (int)m.items.size());
  TEST_ASSERT_EQUAL_STRING("k1", m.items[0].id.c_str());  // the key is the id on older cores
  TEST_ASSERT_TRUE(parse_notification_list("{\"id\":1,\"type\":\"result\",\"success\":true,\"result\":[]}").ok);
  TEST_ASSERT_FALSE(parse_notification_list("{\"id\":1,\"type\":\"result\",\"success\":false}").ok);
  TEST_ASSERT_FALSE(parse_notification_list("garbage").ok);
  TEST_ASSERT_FALSE(parse_notification_list("{\"result\":null}").ok);
  // an item without id (array shape) cannot be dismissed and is skipped
  const NotifList n = parse_notification_list("{\"success\":true,\"result\":[{\"title\":\"x\"}]}");
  TEST_ASSERT_TRUE(n.ok);
  TEST_ASSERT_EQUAL(0, (int)n.items.size());
}

void test_parse_list_caps() {
  std::string j = "{\"success\":true,\"result\":[";
  for (int i = 0; i < 60; ++i) {
    if (i) j += ",";
    j += "{\"notification_id\":\"i" + std::to_string(i) + "\",\"title\":\"" + std::string(100, 'x') + "\",\"message\":\"" +
         std::string(300, 'y') + "\",\"created_at\":\"2026-11-03T10:" + (i < 10 ? "0" : "") + std::to_string(i % 60) + ":00+00:00\"}";
  }
  j += "]}";
  const NotifList l = parse_notification_list(j);
  TEST_ASSERT_TRUE(l.ok);
  TEST_ASSERT_EQUAL((int)kNotifMaxItems, (int)l.items.size());
  TEST_ASSERT_EQUAL((int)kNotifTitleBytes, (int)l.items[0].title.size());
  TEST_ASSERT_EQUAL((int)kNotifMessageBytes, (int)l.items[0].message.size());
  // UTF-8 is not split: 2-byte characters, odd cut
  std::string u = "{\"success\":true,\"result\":[{\"notification_id\":\"u\",\"title\":\"";
  for (int i = 0; i < 60; ++i) u += "\xC3\xA9";
  u += "\"}]}";
  const NotifList ul = parse_notification_list(u);
  TEST_ASSERT_TRUE(ul.items[0].title.size() % 2 == 0);
  TEST_ASSERT_TRUE(ul.items[0].title.size() <= kNotifTitleBytes);
}

void test_dismiss_command() {
  TEST_ASSERT_EQUAL_STRING("{\"id\":4,\"type\":\"persistent_notification/dismiss\",\"notification_id\":\"abc\"}",
                           build_notification_dismiss_command(4, "abc").c_str());
  TEST_ASSERT_TRUE(build_notification_dismiss_command(1, "a\"b").find("a\\\"b") != std::string::npos);
}

// ---- config ----

void test_config_notifications_widget() {
  auto r = config::parseConfig(kYaml);
  TEST_ASSERT_TRUE(r.ok());
  const auto &ws = r.config.tabs[0].widgets;
  TEST_ASSERT_TRUE(ws[0].type == config::WidgetType::Notifications);
  TEST_ASSERT_EQUAL_STRING("", ws[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("Inbox", ws[1].name.c_str());
  TEST_ASSERT_TRUE(ws[0].entities.empty());
  TEST_ASSERT_EQUAL(1, (int)r.config.entityIds.size());  // only light.x
  TEST_ASSERT_EQUAL_STRING("notifications", config::widgetTypeName(config::WidgetType::Notifications));
  TEST_ASSERT_TRUE(configHasNotificationsWidget(r.config));
  auto none = config::parseConfig("version: 1\ntabs:\n  - title: A\n    widgets:\n      - entity: light.x\n");
  TEST_ASSERT_FALSE(configHasNotificationsWidget(none.config));
  // cache round trip keeps the type
  config::Config back;
  TEST_ASSERT_TRUE(config::deserializeConfig(config::serializeConfig(r.config), back));
  TEST_ASSERT_TRUE(back.tabs[0].widgets[1].type == config::WidgetType::Notifications);
}

void test_config_notifications_errors() {
  auto a = config::parseConfig("version: 1\ntabs:\n  - title: A\n    widgets:\n      - type: notifications\n        entity: sensor.x\n");
  TEST_ASSERT_FALSE(a.ok());
  auto b = config::parseConfig("version: 1\ntabs:\n  - title: A\n    widgets:\n      - type: notifications\n        size: 9x9\n");
  TEST_ASSERT_FALSE(b.ok());
  auto c = config::parseConfig("version: 1\ntabs:\n  - title: A\n    widgets:\n      - type: notifications\n        name: 5\n");
  // a number is accepted as a string by the YAML layer or reported; either way no crash
  (void)c;
}

// ---- view model ----

void test_view_states() {
  NotifStore s;
  NotifView v = notifView(s, "");
  TEST_ASSERT_TRUE(v.look == Look::Loading);
  TEST_ASSERT_EQUAL_STRING("-", v.count.c_str());
  TEST_ASSERT_EQUAL_STRING("Alerts", v.name.c_str());
  TEST_ASSERT_FALSE(v.fill);
  s.setFailed();  // nothing cached
  v = notifView(s, "Inbox");
  TEST_ASSERT_TRUE(v.look == Look::Unavailable);
  TEST_ASSERT_EQUAL_STRING("n/a", v.count.c_str());
  TEST_ASSERT_EQUAL_STRING("Inbox", v.name.c_str());
  s = storeOf(0);
  v = notifView(s, "");
  TEST_ASSERT_TRUE(v.look == Look::Normal);
  TEST_ASSERT_EQUAL_STRING("0", v.count.c_str());
  TEST_ASSERT_FALSE(v.fill);  // 0 -> outline
  s = storeOf(4);
  v = notifView(s, "");
  TEST_ASSERT_EQUAL_STRING("4", v.count.c_str());
  TEST_ASSERT_TRUE(v.fill);  // > 0 -> INK fill
  TEST_ASSERT_EQUAL_STRING("99", notifView(storeOf(99), "").count.c_str());
  TEST_ASSERT_EQUAL_STRING("99+", notifView(storeOf(100), "").count.c_str());
  s.setFailed();  // cached list stays, stale
  v = notifView(s, "");
  TEST_ASSERT_TRUE(v.look == Look::Normal);
  TEST_ASSERT_EQUAL_STRING("4", v.count.c_str());
  TEST_ASSERT_TRUE(s.stale());
}

void test_time_text() {
  TEST_ASSERT_EQUAL_STRING("13:58", notifTimeText("2026-11-03T12:58:00+00:00", kNow, 3600).c_str());  // CET, today
  TEST_ASSERT_EQUAL_STRING("3.11.", notifTimeText("2026-11-03T12:58:00+00:00", 0, 3600).c_str());     // no clock
  TEST_ASSERT_EQUAL_STRING("1.11.", notifTimeText("2026-11-01T08:00:00.5+00:00", kNow, 3600).c_str());
  // local midnight: 23:30Z on the 2nd is 00:30 on the 3rd in CET = today
  TEST_ASSERT_EQUAL_STRING("00:30", notifTimeText("2026-11-02T23:30:00+00:00", kNow, 3600).c_str());
  TEST_ASSERT_EQUAL_STRING("", notifTimeText("junk", kNow, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("", notifTimeText("", kNow, 0).c_str());
}

// ---- layout numbers (spec 1.1, 7) ----

void test_layout_landscape_numbers() {
  const NotifPopupLayout l = notifPopupLayout(Orientation::Landscape);
  TEST_ASSERT_TRUE(l.panel == (ui::Rect{40, 94, 880, 408}));
  TEST_ASSERT_TRUE(l.shadow == (ui::Rect{46, 100, 880, 408}));
  TEST_ASSERT_TRUE(l.dirty == (ui::Rect{38, 92, 890, 418}));
  TEST_ASSERT_TRUE(l.clearAll == (ui::Rect{520, 110, 152, 64}));
  TEST_ASSERT_TRUE(l.prev == (ui::Rect{688, 110, 64, 64}));
  TEST_ASSERT_TRUE(l.next == (ui::Rect{764, 110, 64, 64}));
  TEST_ASSERT_TRUE(l.close == (ui::Rect{840, 110, 64, 64}));
  TEST_ASSERT_EQUAL(3, l.rowsPerPage);
  TEST_ASSERT_TRUE(l.rows[0] == (ui::Rect{56, 190, 848, 76}));
  TEST_ASSERT_TRUE(l.rows[2] == (ui::Rect{56, 366, 848, 76}));
  TEST_ASSERT_TRUE(l.dismiss(1) == (ui::Rect{828, 284, 64, 64}));  // (828, row + 6)
  TEST_ASSERT_TRUE(l.rowsArea == (ui::Rect{56, 190, 848, 252}));
  TEST_ASSERT_EQUAL(142, l.titleCy);
  TEST_ASSERT_EQUAL(470, l.footerCy);
  TEST_ASSERT_EQUAL(812, l.rows[0].x + l.rowTimeRightDx);
  TEST_ASSERT_EQUAL(650, l.rowTitleMaxW);
  TEST_ASSERT_EQUAL(730, l.rowMsgMaxW);
  const ConfirmLayout c = confirmLayout(Orientation::Landscape);
  TEST_ASSERT_TRUE(c.panel == (ui::Rect{200, 188, 560, 220}));
  TEST_ASSERT_TRUE(c.yes == (ui::Rect{228, 316, 160, 64}));
  TEST_ASSERT_TRUE(c.no == (ui::Rect{400, 316, 160, 64}));
  TEST_ASSERT_TRUE(c.skip == (ui::Rect{572, 316, 160, 64}));
  TEST_ASSERT_EQUAL(232, c.titleCy);
  TEST_ASSERT_EQUAL(266, c.subCy);
}

void test_layout_portrait_numbers() {
  const NotifPopupLayout l = notifPopupLayout(Orientation::Portrait);
  TEST_ASSERT_TRUE(l.panel == (ui::Rect{16, 116, 508, 704}));
  TEST_ASSERT_EQUAL(6, l.rowsPerPage);
  // panel-relative values of the spec: prev (276,16), next (352,16), close (428,16), rows y = 96 + 88 i
  TEST_ASSERT_TRUE(l.prev == (ui::Rect{16 + 276, 116 + 16, 64, 64}));
  TEST_ASSERT_TRUE(l.next == (ui::Rect{16 + 352, 116 + 16, 64, 64}));
  TEST_ASSERT_TRUE(l.close == (ui::Rect{16 + 428, 116 + 16, 64, 64}));
  TEST_ASSERT_TRUE(l.clearAll == (ui::Rect{16 + 16, 116 + 628, 152, 64}));
  for (int i = 0; i < 6; ++i) TEST_ASSERT_TRUE(l.rows[i] == (ui::Rect{32, 116 + 96 + 88 * i, 476, 76}));
  TEST_ASSERT_TRUE(l.dismiss(0) == (ui::Rect{16 + 400, 116 + 96 + 6, 64, 64}));
  TEST_ASSERT_EQUAL(16 + 184, l.footerCountX);
  TEST_ASSERT_EQUAL(116 + 660, l.footerCy);
  TEST_ASSERT_EQUAL(16 + 384, l.rows[0].x + l.rowTimeRightDx);
  // everything inside the panel
  TEST_ASSERT_TRUE(ui::rectInside(l.clearAll, l.panel));
  TEST_ASSERT_TRUE(ui::rectInside(l.dismiss(5), l.panel));
  const ConfirmLayout c = confirmLayout(Orientation::Portrait);
  TEST_ASSERT_TRUE(c.panel == (ui::Rect{30, 356, 480, 224}));
  TEST_ASSERT_TRUE(c.yes == (ui::Rect{30 + 24, 356 + 128, 136, 64}));
  TEST_ASSERT_TRUE(c.no == (ui::Rect{30 + 172, 356 + 128, 136, 64}));
  TEST_ASSERT_TRUE(c.skip == (ui::Rect{30 + 320, 356 + 128, 136, 64}));
}

void test_texts() {
  TEST_ASSERT_EQUAL_STRING("Notifications (4)", notifPopupTitle(4).c_str());
  TEST_ASSERT_EQUAL_STRING("1-3 of 4", notifFooterText(0, 3, 4).c_str());
  TEST_ASSERT_EQUAL_STRING("4-4 of 4", notifFooterText(1, 3, 4).c_str());
  TEST_ASSERT_EQUAL_STRING("", notifFooterText(0, 3, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("Dismiss all 4?", notifConfirmTitle(4).c_str());
}

// ---- popup model ----

void test_popup_rocker_focus_and_paging() {
  const Orientation o = Orientation::Landscape;
  NotifStore s = storeOf(4);
  NotifPopup p;
  p.open(s, 0, true);
  TEST_ASSERT_EQUAL(0, p.focus());
  p.rocker(ui::PhysicalKey::Down, o, s, 0);  // DOWN = next
  p.rocker(ui::PhysicalKey::Down, o, s, 0);
  TEST_ASSERT_EQUAL(2, p.focus());
  TEST_ASSERT_EQUAL(0, p.page());
  p.rocker(ui::PhysicalKey::Down, o, s, 0);  // continues on the next page
  TEST_ASSERT_EQUAL(3, p.focus());
  TEST_ASSERT_EQUAL(1, p.page());
  p.rocker(ui::PhysicalKey::Down, o, s, 0);  // Clear all
  TEST_ASSERT_EQUAL(4, p.focus());
  p.rocker(ui::PhysicalKey::Down, o, s, 0);  // stays
  TEST_ASSERT_EQUAL(4, p.focus());
  p.rocker(ui::PhysicalKey::Up, o, s, 0);
  TEST_ASSERT_EQUAL(3, p.focus());
  p.rocker(ui::PhysicalKey::Up, o, s, 0);
  p.rocker(ui::PhysicalKey::Up, o, s, 0);
  TEST_ASSERT_EQUAL(1, p.focus());
  TEST_ASSERT_EQUAL(0, p.page());
  // long push closes
  const NotifResult r = p.rocker(ui::PhysicalKey::LongPush, o, s, 0);
  TEST_ASSERT_TRUE(r.closed);
  TEST_ASSERT_FALSE(p.isOpen());
}

void test_popup_open_by_touch_has_no_ring() {
  NotifStore s = storeOf(2);
  NotifPopup p;
  p.open(s, 0, false);
  TEST_ASSERT_EQUAL(-1, p.focus());
  p.rocker(ui::PhysicalKey::Push, Orientation::Landscape, s, 0);  // nothing focused: nothing happens
  TEST_ASSERT_TRUE(p.isOpen());
  p.rocker(ui::PhysicalKey::Up, Orientation::Landscape, s, 0);  // first UP from "no ring" goes to Clear all
  TEST_ASSERT_EQUAL(2, p.focus());
}

void test_popup_touch_dismiss_and_pages() {
  const Orientation o = Orientation::Landscape;
  const NotifPopupLayout l = notifPopupLayout(o);
  NotifStore s = storeOf(4);
  NotifPopup p;
  p.open(s, 0, false);
  NotifResult r = p.touch(centreOf(l.dismiss(1)), o, s, 0);
  TEST_ASSERT_EQUAL(1, (int)r.dismissIds.size());
  TEST_ASSERT_EQUAL_STRING("n01", r.dismissIds[0].c_str());
  TEST_ASSERT_TRUE(p.rowState("n01") == NotifRowState::Pending);
  r = p.touch(centreOf(l.dismiss(1)), o, s, 0);  // a second tap while pending sends nothing
  TEST_ASSERT_EQUAL(0, (int)r.dismissIds.size());
  p.dismissDone("n01", false);  // failed: row stays with the alert glyph, retry allowed
  TEST_ASSERT_TRUE(p.rowState("n01") == NotifRowState::Failed);
  r = p.touch(centreOf(l.dismiss(1)), o, s, 0);
  TEST_ASSERT_EQUAL(1, (int)r.dismissIds.size());
  s.remove("n01");  // success: removed, the list moves up
  p.dismissDone("n01", true);
  p.sync(s, o);
  TEST_ASSERT_TRUE(p.rowState("n01") == NotifRowState::Normal);
  TEST_ASSERT_EQUAL(3, s.count());
  // pages: prev inert on the first page, next opens page 2 (needs > 3 items)
  s = storeOf(5);
  p.open(s, 0, false);
  TEST_ASSERT_FALSE(p.touch(centreOf(l.prev), o, s, 0).changed);
  TEST_ASSERT_TRUE(p.touch(centreOf(l.next), o, s, 0).changed);
  TEST_ASSERT_EQUAL(1, p.page());
  // page 2 row 0 is item 3
  r = p.touch(centreOf(l.dismiss(0)), o, s, 0);
  TEST_ASSERT_EQUAL_STRING("n03", r.dismissIds[0].c_str());
  TEST_ASSERT_FALSE(p.touch(centreOf(l.next), o, s, 0).changed);  // last page: inert
  // the third row of page 2 has no item
  r = p.touch(centreOf(l.dismiss(2)), o, s, 0);
  TEST_ASSERT_EQUAL(0, (int)r.dismissIds.size());
}

void test_popup_close_outside_and_button() {
  const Orientation o = Orientation::Landscape;
  const NotifPopupLayout l = notifPopupLayout(o);
  NotifStore s = storeOf(1);
  NotifPopup p;
  p.open(s, 0, false);
  TEST_ASSERT_FALSE(p.touch({10, 10}, o, s, 0).changed == false);  // outside the panel closes
  TEST_ASSERT_FALSE(p.isOpen());
  p.open(s, 0, false);
  TEST_ASSERT_TRUE(p.touch(centreOf(l.close), o, s, 0).closed);
  p.open(s, 0, false);
  TEST_ASSERT_FALSE(p.touch({400, 460}, o, s, 0).closed);  // inside the panel, no control
  TEST_ASSERT_TRUE(p.isOpen());
}

void test_popup_clear_all_confirm_flow() {
  const Orientation o = Orientation::Landscape;
  const NotifPopupLayout l = notifPopupLayout(o);
  const ConfirmLayout c = confirmLayout(o);
  NotifStore s = storeOf(4);
  NotifPopup p;
  p.open(s, 0, false);
  TEST_ASSERT_TRUE(p.touch(centreOf(l.clearAll), o, s, 100).changed);
  TEST_ASSERT_TRUE(p.mode() == NotifPopup::Mode::Confirm);
  // skip by touch (outside the dialog)
  p.touch({10, 10}, o, s, 200);
  TEST_ASSERT_TRUE(p.mode() == NotifPopup::Mode::List);
  TEST_ASSERT_TRUE(p.isOpen());
  // No by touch
  p.touch(centreOf(l.clearAll), o, s, 300);
  p.touch(centreOf(c.no), o, s, 300);
  TEST_ASSERT_TRUE(p.mode() == NotifPopup::Mode::List);
  // timeout after 15 s
  p.touch(centreOf(l.clearAll), o, s, 1000);
  TEST_ASSERT_FALSE(p.tick(15999));
  TEST_ASSERT_TRUE(p.tick(16000));
  TEST_ASSERT_TRUE(p.mode() == NotifPopup::Mode::List);
  // PUSH = skip, DOWN = no, UP = yes
  p.touch(centreOf(l.clearAll), o, s, 0);
  p.rocker(ui::PhysicalKey::Push, o, s, 0);
  TEST_ASSERT_TRUE(p.mode() == NotifPopup::Mode::List);
  p.touch(centreOf(l.clearAll), o, s, 0);
  p.rocker(ui::PhysicalKey::Down, o, s, 0);
  TEST_ASSERT_TRUE(p.mode() == NotifPopup::Mode::List);
  p.touch(centreOf(l.clearAll), o, s, 0);
  const NotifResult r = p.rocker(ui::PhysicalKey::Up, o, s, 0);
  TEST_ASSERT_EQUAL(4, (int)r.dismissIds.size());  // one dismiss per id
  TEST_ASSERT_EQUAL_STRING("n00", r.dismissIds[0].c_str());
  TEST_ASSERT_TRUE(p.mode() == NotifPopup::Mode::List);
  TEST_ASSERT_TRUE(p.anyPending());
  // each result removes its row
  for (const std::string &id : r.dismissIds) {
    s.remove(id);
    p.dismissDone(id, true);
  }
  p.sync(s, o);
  TEST_ASSERT_FALSE(p.anyPending());
  // empty list: Clear all and chevrons are inert
  TEST_ASSERT_FALSE(p.touch(centreOf(l.clearAll), o, s, 0).changed);
  TEST_ASSERT_FALSE(p.rocker(ui::PhysicalKey::Down, o, s, 0).changed);
}

void test_popup_confirm_by_touch_yes_and_sync_clamps() {
  const Orientation o = Orientation::Landscape;
  const ConfirmLayout c = confirmLayout(o);
  const NotifPopupLayout l = notifPopupLayout(o);
  NotifStore s = storeOf(5);
  NotifPopup p;
  p.open(s, 0, true);
  for (int i = 0; i < 5; ++i) p.rocker(ui::PhysicalKey::Down, o, s, 0);  // Clear all, page 2
  TEST_ASSERT_EQUAL(5, p.focus());
  TEST_ASSERT_EQUAL(1, p.page());
  // the list shrinks to 2 behind the popup (a fetch): page and focus clamp
  s = storeOf(2);
  p.sync(s, o);
  TEST_ASSERT_EQUAL(0, p.page());
  TEST_ASSERT_EQUAL(2, p.focus());  // Clear all
  p.rocker(ui::PhysicalKey::Push, o, s, 0);  // activates Clear all
  TEST_ASSERT_TRUE(p.mode() == NotifPopup::Mode::Confirm);
  const NotifResult r = p.touch(centreOf(c.yes), o, s, 0);
  TEST_ASSERT_EQUAL(2, (int)r.dismissIds.size());
  (void)l;
}

// ---- AppState wiring ----

void test_widget_tap_opens_popup_and_dismiss_demo_free() {
  AppState a = makeApp(Orientation::Landscape);
  TEST_ASSERT_EQUAL(1, a.layout().widgets[0].cellW);
  TEST_ASSERT_EQUAL(1, a.layout().widgets[0].cellH);
  std::vector<std::string> sent;
  a.setNotifDismissSink([&](const std::string &id) { sent.push_back(id); });
  std::vector<NotifItem> items = {{"x1", "One", "m1", "2026-11-03T12:58:00+00:00"},
                                  {"x2", "Two", "m2", "2026-11-02T12:58:00+00:00"}};
  TEST_ASSERT_TRUE(a.setNotifications(std::move(items)));
  TEST_ASSERT_EQUAL_STRING("2", a.widgetData(0).notif.count.c_str());
  TEST_ASSERT_TRUE(a.widgetData(0).notif.fill);
  TEST_ASSERT_EQUAL_STRING("Alerts", a.widgetData(0).notif.name.c_str());
  TEST_ASSERT_EQUAL_STRING("Inbox", a.widgetData(1).notif.name.c_str());
  TEST_ASSERT_FALSE(a.modalOpen());
  TEST_ASSERT_TRUE(a.touch(centreOf(a.layout().widgets[0].rect), 1000));  // tap opens the popup
  TEST_ASSERT_TRUE(a.notifOpen());
  TEST_ASSERT_TRUE(a.modalOpen());
  // dismiss row 0
  const NotifPopupLayout l = notifPopupLayout(Orientation::Landscape);
  TEST_ASSERT_TRUE(a.touch(centreOf(l.dismiss(0)), 1100));
  TEST_ASSERT_EQUAL(1, (int)sent.size());
  TEST_ASSERT_EQUAL_STRING("x1", sent[0].c_str());
  TEST_ASSERT_TRUE(a.notifPopup().rowState("x1") == NotifRowState::Pending);
  TEST_ASSERT_EQUAL(2, a.notifStore().count());  // removed only when the call succeeded
  a.notifDismissDone("x1", true);
  TEST_ASSERT_EQUAL(1, a.notifStore().count());
  TEST_ASSERT_EQUAL_STRING("1", a.widgetData(0).notif.count.c_str());
  // a new fetch result with the same content changes nothing
  std::vector<NotifItem> same = {{"x2", "Two", "m2", "2026-11-02T12:58:00+00:00"}};
  TEST_ASSERT_FALSE(a.setNotifications(std::move(same)));
  // close by tapping outside
  TEST_ASSERT_TRUE(a.touch({5, 5}, 1200));
  TEST_ASSERT_FALSE(a.notifOpen());
}

void test_rocker_push_on_focused_widget_opens_popup() {
  AppState a = makeApp(Orientation::Portrait);
  // Tab focus -> widget 1 (first stop)
  a.rocker(ui::PhysicalKey::Push, 100);  // from the tab bar into the widgets
  const int idx = a.focusedWidgetIndex();
  TEST_ASSERT_EQUAL(0, idx);
  TEST_ASSERT_TRUE(a.rocker(ui::PhysicalKey::Push, 200));  // PUSH on the notifications widget
  TEST_ASSERT_TRUE(a.notifOpen());
  TEST_ASSERT_EQUAL(-1, a.notifPopup().focus());  // empty/loading list: no row to focus
  TEST_ASSERT_TRUE(a.rocker(ui::PhysicalKey::LongPush, 300));  // closes
  TEST_ASSERT_FALSE(a.notifOpen());
}

void test_loading_unavailable_and_offline_failure() {
  AppState a = makeApp(Orientation::Landscape);
  TEST_ASSERT_TRUE(a.widgetData(0).notif.look == Look::Loading);
  TEST_ASSERT_TRUE(a.notifFetchFailed());
  TEST_ASSERT_TRUE(a.widgetData(0).notif.look == Look::Unavailable);
  TEST_ASSERT_EQUAL_STRING("n/a", a.widgetData(0).notif.count.c_str());
  // the popup also opens at count 0 / unavailable
  TEST_ASSERT_TRUE(a.touch(centreOf(a.layout().widgets[0].rect), 0));
  TEST_ASSERT_TRUE(a.notifOpen());
  // without a sink a dismiss fails at once (offline): the row would show the alert glyph
  AppState b = makeApp(Orientation::Landscape);
  std::vector<NotifItem> items = {{"x1", "One", "m1", "2026-11-03T12:58:00+00:00"}};
  b.setNotifications(std::move(items));
  b.touch(centreOf(b.layout().widgets[0].rect), 0);
  b.touch(centreOf(notifPopupLayout(Orientation::Landscape).dismiss(0)), 10);
  TEST_ASSERT_TRUE(b.notifPopup().rowState("x1") == NotifRowState::Failed);
  TEST_ASSERT_EQUAL(1, b.notifStore().count());
}

void test_demo_mode_dismisses_locally() {
  auto r = config::parseConfig(kYaml);
  AppState a;
  a.init(r.config, Orientation::Landscape, true);
  TEST_ASSERT_EQUAL(4, a.notifStore().count());
  a.touch(centreOf(a.layout().widgets[0].rect), 0);
  a.touch(centreOf(notifPopupLayout(Orientation::Landscape).dismiss(0)), 10);
  TEST_ASSERT_EQUAL(3, a.notifStore().count());
}

void test_layout_1x1_in_every_grid() {
  for (Orientation o : {Orientation::Landscape, Orientation::Portrait})
    for (int cols : {0, 3, 4}) {
      const ui::SizeLimits lim = ui::sizeLimits(config::WidgetType::Notifications, o, cols);
      TEST_ASSERT_EQUAL(1, lim.def.w);
      TEST_ASSERT_EQUAL(1, lim.max.w);
      TEST_ASSERT_EQUAL(1, lim.def.h);
    }
  TEST_ASSERT_TRUE(ui::isFocusable(config::WidgetType::Notifications));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_list_array_sorted_and_plain);
  RUN_TEST(test_parse_list_map_shape_and_errors);
  RUN_TEST(test_parse_list_caps);
  RUN_TEST(test_dismiss_command);
  RUN_TEST(test_config_notifications_widget);
  RUN_TEST(test_config_notifications_errors);
  RUN_TEST(test_view_states);
  RUN_TEST(test_time_text);
  RUN_TEST(test_layout_landscape_numbers);
  RUN_TEST(test_layout_portrait_numbers);
  RUN_TEST(test_texts);
  RUN_TEST(test_popup_rocker_focus_and_paging);
  RUN_TEST(test_popup_open_by_touch_has_no_ring);
  RUN_TEST(test_popup_touch_dismiss_and_pages);
  RUN_TEST(test_popup_close_outside_and_button);
  RUN_TEST(test_popup_clear_all_confirm_flow);
  RUN_TEST(test_popup_confirm_by_touch_yes_and_sync_clamps);
  RUN_TEST(test_widget_tap_opens_popup_and_dismiss_demo_free);
  RUN_TEST(test_rocker_push_on_focused_widget_opens_popup);
  RUN_TEST(test_loading_unavailable_and_offline_failure);
  RUN_TEST(test_demo_mode_dismisses_locally);
  RUN_TEST(test_layout_1x1_in_every_grid);
  return UNITY_END();
}
