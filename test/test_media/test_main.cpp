// Task 106: media player widget and popup (config, views, layout, popup model, app state, service calls).
#include <unity.h>

#include <string>
#include <utility>
#include <vector>

#include "app_state.h"
#include "config_model.h"
#include "entity_state.h"
#include "media_intent.h"
#include "media_layout.h"
#include "media_popup.h"
#include "media_view.h"
#include "service_call.h"
#include "tap_intent.h"
#include "template_builder.h"
#include "ui_layout.h"

using namespace uistate;
using config::WidgetType;
namespace L = ui;

static const settings::Orientation P = settings::Orientation::Portrait;
static const settings::Orientation LS = settings::Orientation::Landscape;

void setUp() {}
void tearDown() {}

static EntityState player(const char *state, const char *title = "", const char *artist = "", double vol = -1) {
  EntityState e;
  e.id = "media_player.living";
  e.state = state;
  e.available = true;
  e.friendly_name = "Living room";
  if (*title) e.attrs.emplace_back("media_title", title);
  if (*artist) e.attrs.emplace_back("media_artist", artist);
  if (vol >= 0) e.attrs.emplace_back("volume_level", std::to_string(vol));
  return e;
}

static const char *kYaml =
    "version: \"1\"\ntabs:\n  - title: T\n    widgets:\n      - entity: media_player.living\n        name: Music\n"
    "      - entity: media_player.tv\n        type: media_player\n        size: 2x1\n";

static config::Config parsed() {
  config::ParseResult r = config::parseConfig(std::string(kYaml));
  TEST_ASSERT_TRUE(r.ok());
  return r.config;
}

// ---- config ----

void test_config_media_type() {
  const config::Config c = parsed();
  TEST_ASSERT_EQUAL(2, (int)c.tabs[0].widgets.size());
  TEST_ASSERT_TRUE(c.tabs[0].widgets[0].type == WidgetType::MediaPlayer);
  TEST_ASSERT_TRUE(c.tabs[0].widgets[1].type == WidgetType::MediaPlayer);
  TEST_ASSERT_EQUAL_STRING("media_player", config::widgetTypeName(WidgetType::MediaPlayer));
  TEST_ASSERT_EQUAL_STRING("Music", c.tabs[0].widgets[0].name.c_str());
}

void test_template_requests_media_attributes() {
  std::vector<std::string> names = state_attribute_names("media_player.x", {});
  for (const char *n : {"media_title", "media_artist", "media_album_name", "volume_level", "is_volume_muted", "supported_features"}) {
    bool found = false;
    for (const auto &s : names) found |= s == n;
    TEST_ASSERT_TRUE_MESSAGE(found, n);
  }
}

// ---- service calls ----

void test_service_calls() {
  ServiceCall c;
  TEST_ASSERT_TRUE(build_service_call(Action::MediaPlayPause, {"media_player.a"}, c));
  TEST_ASSERT_EQUAL_STRING("media_player", c.domain.c_str());
  TEST_ASSERT_EQUAL_STRING("media_play_pause", c.service.c_str());
  TEST_ASSERT_EQUAL_STRING("{\"entity_id\":[\"media_player.a\"]}", c.body.c_str());
  TEST_ASSERT_TRUE(build_service_call(Action::MediaPrevious, {"media_player.a"}, c));
  TEST_ASSERT_EQUAL_STRING("media_previous_track", c.service.c_str());
  TEST_ASSERT_TRUE(build_service_call(Action::MediaNext, {"media_player.a"}, c));
  TEST_ASSERT_EQUAL_STRING("media_next_track", c.service.c_str());
  TEST_ASSERT_TRUE(build_service_call(Action::MediaTurnOn, {"media_player.a"}, c));
  TEST_ASSERT_EQUAL_STRING("turn_on", c.service.c_str());

  TEST_ASSERT_TRUE(build_media_volume_set({"media_player.a"}, 45, c));
  TEST_ASSERT_EQUAL_STRING("volume_set", c.service.c_str());
  TEST_ASSERT_EQUAL_STRING("{\"entity_id\":[\"media_player.a\"],\"volume_level\":0.45}", c.body.c_str());
  TEST_ASSERT_TRUE(build_media_volume_set({"media_player.a"}, 150, c));
  TEST_ASSERT_EQUAL_STRING("{\"entity_id\":[\"media_player.a\"],\"volume_level\":1}", c.body.c_str());
  TEST_ASSERT_TRUE(build_media_volume_set({"media_player.a"}, -3, c));
  TEST_ASSERT_EQUAL_STRING("{\"entity_id\":[\"media_player.a\"],\"volume_level\":0}", c.body.c_str());
  TEST_ASSERT_FALSE(build_media_volume_set({}, 10, c));
  TEST_ASSERT_TRUE(build_media_volume_mute({"media_player.a"}, true, c));
  TEST_ASSERT_EQUAL_STRING("volume_mute", c.service.c_str());
  TEST_ASSERT_EQUAL_STRING("{\"entity_id\":[\"media_player.a\"],\"is_volume_muted\":true}", c.body.c_str());
  TEST_ASSERT_FALSE(build_media_volume_mute({}, true, c));
}

void test_tap_intents() {
  using app_logic::intentFromRequest;
  EntityState playing = player("playing"), paused = player("paused"), on = player("on");
  auto ev = intentFromRequest(WidgetType::MediaPlayer, 2, {paused});  // was playing -> paused: play_pause
  TEST_ASSERT_TRUE(ev.has_value());
  TEST_ASSERT_TRUE(ev->action == Action::MediaPlayPause);
  ev = intentFromRequest(WidgetType::MediaPlayer, 2, {playing});
  TEST_ASSERT_TRUE(ev->action == Action::MediaPlayPause);
  ev = intentFromRequest(WidgetType::MediaPlayer, 2, {on});  // off -> on: turn_on
  TEST_ASSERT_TRUE(ev->action == Action::MediaTurnOn);
  ev = intentFromRequest(WidgetType::MediaPlayer, 1, {playing});
  TEST_ASSERT_TRUE(ev->action == Action::MediaPrevious);
  ev = intentFromRequest(WidgetType::MediaPlayer, 3, {playing});
  TEST_ASSERT_TRUE(ev->action == Action::MediaNext);
  TEST_ASSERT_FALSE(intentFromRequest(WidgetType::MediaPlayer, 4, {playing}).has_value());
  TEST_ASSERT_FALSE(intentFromRequest(WidgetType::MediaPlayer, 0, {playing}).has_value());

  auto vol = app_logic::mediaVolumeIntent("media_player.a", 30);
  TEST_ASSERT_TRUE(vol.has_value());
  TEST_ASSERT_TRUE(vol->custom);
  TEST_ASSERT_EQUAL_STRING("volume_set", vol->custom_call.service.c_str());
  auto mute = app_logic::mediaMuteIntent("media_player.a", false);
  TEST_ASSERT_EQUAL_STRING("volume_mute", mute->custom_call.service.c_str());
}

// ---- view model ----

void test_view_states_and_fallbacks() {
  MediaView v = mediaView(nullptr, "Music");  // loading
  TEST_ASSERT_TRUE(v.look == Look::Loading);
  TEST_ASSERT_EQUAL_STRING("Music", v.line1.c_str());
  TEST_ASSERT_EQUAL_STRING("-", v.line2.c_str());
  TEST_ASSERT_FALSE(v.allDotted);

  EntityState una = player("unavailable");
  una.available = false;
  v = mediaView(&una, "Music");
  TEST_ASSERT_TRUE(v.look == Look::Unavailable);
  TEST_ASSERT_EQUAL_STRING("n/a", v.line2.c_str());
  TEST_ASSERT_TRUE(v.allDotted);
  TEST_ASSERT_TRUE(v.skipDotted);

  EntityState pl = player("playing", "Blue in Green", "Miles Davis");
  v = mediaView(&pl, "Music");
  TEST_ASSERT_EQUAL_STRING("Blue in Green", v.line1.c_str());
  TEST_ASSERT_EQUAL_STRING("Miles Davis", v.line2.c_str());
  TEST_ASSERT_TRUE(v.playFilled);
  TEST_ASSERT_TRUE(v.glyph == PlayGlyph::Pause);

  EntityState noArtist = player("paused", "Podcast");
  v = mediaView(&noArtist, "Music");
  TEST_ASSERT_EQUAL_STRING("Podcast", v.line1.c_str());
  TEST_ASSERT_EQUAL_STRING("Paused", v.line2.c_str());
  TEST_ASSERT_FALSE(v.playFilled);
  TEST_ASSERT_TRUE(v.glyph == PlayGlyph::Play);

  EntityState idle = player("idle", "stale title", "stale artist");  // title ignored while idle
  v = mediaView(&idle, "Music");
  TEST_ASSERT_EQUAL_STRING("Music", v.line1.c_str());
  TEST_ASSERT_EQUAL_STRING("Idle", v.line2.c_str());
  TEST_ASSERT_FALSE(v.skipDotted);

  EntityState off = player("off");
  v = mediaView(&off, "Music");
  TEST_ASSERT_EQUAL_STRING("Off", v.line2.c_str());
  TEST_ASSERT_TRUE(v.glyph == PlayGlyph::Power);
  TEST_ASSERT_TRUE(v.skipDotted);
  TEST_ASSERT_FALSE(v.allDotted);

  EntityState buf = player("buffering", "Song");
  TEST_ASSERT_TRUE(mediaView(&buf, "M").playFilled);
  EntityState unk = player("unknown");
  TEST_ASSERT_EQUAL_STRING("-", mediaView(&unk, "M").line2.c_str());
  TEST_ASSERT_EQUAL_STRING("Standby", mediaStateWord("standby").c_str());
  TEST_ASSERT_EQUAL_STRING("On", mediaStateWord("on").c_str());
}

void test_popup_view_volume_features() {
  EntityState e = player("playing", "T", "A", 0.456);
  e.attrs.emplace_back("media_album_name", "Alb");
  e.attrs.emplace_back("is_volume_muted", "true");
  MediaPopupView v = mediaPopupView(&e);  // supported_features unknown: volume present -> supported
  TEST_ASSERT_TRUE(v.hasVolume);
  TEST_ASSERT_TRUE(v.hasMute);
  TEST_ASSERT_EQUAL(46, v.volumePct);
  TEST_ASSERT_TRUE(v.muted);
  TEST_ASSERT_EQUAL_STRING("Alb", v.album.c_str());
  TEST_ASSERT_EQUAL_STRING("Playing", v.stateWord.c_str());

  e.supported_features = 1 | 16 | 32;  // no volume bits
  v = mediaPopupView(&e);
  TEST_ASSERT_FALSE(v.hasVolume);
  TEST_ASSERT_FALSE(v.hasMute);
  e.supported_features = 4;  // volume_set only
  v = mediaPopupView(&e);
  TEST_ASSERT_TRUE(v.hasVolume);
  TEST_ASSERT_FALSE(v.hasMute);

  EntityState novol = player("playing", "T");
  TEST_ASSERT_FALSE(mediaPopupView(&novol).hasVolume);
  TEST_ASSERT_EQUAL(-1, mediaVolumePct(novol));
  EntityState off = player("off", "", "", 0.3);
  v = mediaPopupView(&off);
  TEST_ASSERT_FALSE(v.hasVolume);
  TEST_ASSERT_EQUAL_STRING("Off", v.title.c_str());
  TEST_ASSERT_TRUE(v.glyph == PlayGlyph::Power);
  TEST_ASSERT_EQUAL_STRING("-", mediaPopupView(nullptr).title.c_str());
  TEST_ASSERT_EQUAL(100, mediaVolumePct(player("playing", "", "", 1.7)));
}

// ---- layout (numbers of docs/ui/widgets-new.md 4 / 4.1) ----

void test_widget_sub_regions_match_spec() {
  for (int W : {552, 508, 364}) {
    const L::Rect r{20, 100, W, 104};
    const auto subs = L::subRegions(WidgetType::MediaPlayer, config::WidgetSize{}, r);
    TEST_ASSERT_EQUAL(4, (int)subs.size());
    TEST_ASSERT_TRUE((subs[0].rect == L::Rect{r.x + W - 228, r.y + 20, 64, 64}));
    TEST_ASSERT_TRUE((subs[1].rect == L::Rect{r.x + W - 152, r.y + 20, 64, 64}));
    TEST_ASSERT_TRUE((subs[2].rect == L::Rect{r.x + W - 76, r.y + 20, 64, 64}));
    TEST_ASSERT_TRUE((subs[3].rect == L::Rect{r.x + 12, r.y + 12, W - 264, 80}));
    TEST_ASSERT_EQUAL(1, subs[0].id);
    TEST_ASSERT_EQUAL(4, subs[3].id);
    TEST_ASSERT_FALSE(L::rectsOverlap(subs[3].rect, subs[0].rect));
  }
}

void test_widget_sizes() {
  TEST_ASSERT_EQUAL(3, L::sizeLimits(WidgetType::MediaPlayer, LS, 5).def.w);
  TEST_ASSERT_EQUAL(2, L::sizeLimits(WidgetType::MediaPlayer, LS, 4).def.w);
  TEST_ASSERT_EQUAL(2, L::sizeLimits(WidgetType::MediaPlayer, P, 2).def.w);
  TEST_ASSERT_EQUAL(3, L::sizeLimits(WidgetType::MediaPlayer, P, 3).min.w);
  TEST_ASSERT_EQUAL(4, L::sizeLimits(WidgetType::MediaPlayer, P, 4).max.w);
  // landscape default grid: 552 wide, portrait 2 columns: 508
  config::Widget w;
  w.type = WidgetType::MediaPlayer;
  const L::Flow fl = L::computeFlow(LS, {w});
  TEST_ASSERT_EQUAL(552, fl.placements[0].rect.w);
  const L::Flow fp = L::computeFlow(P, {w});
  TEST_ASSERT_EQUAL(508, fp.placements[0].rect.w);
}

void test_popup_layout_landscape_numbers() {
  const MediaLayout l = mediaLayout(LS);
  TEST_ASSERT_TRUE((l.panel == L::Rect{40, 94, 880, 408}));
  TEST_ASSERT_TRUE((l.dirty == L::Rect{38, 92, 890, 418}));
  TEST_ASSERT_TRUE((l.close == L::Rect{840, 110, 64, 64}));
  TEST_ASSERT_TRUE((l.prev == L::Rect{56, 318, 96, 64}));
  TEST_ASSERT_TRUE((l.play == L::Rect{164, 318, 96, 64}));
  TEST_ASSERT_TRUE((l.next == L::Rect{272, 318, 96, 64}));
  TEST_ASSERT_TRUE((l.volMinus == L::Rect{56, 406, 64, 64}));
  TEST_ASSERT_TRUE((l.volPlus == L::Rect{616, 406, 64, 64}));
  TEST_ASSERT_TRUE((l.barTrack == L::Rect{132, 426, 472, 24}));
  TEST_ASSERT_TRUE((l.mute == L::Rect{840, 406, 64, 64}));
  TEST_ASSERT_EQUAL(142, l.nameCy);
  TEST_ASSERT_EQUAL(206, l.titleCy);
  TEST_ASSERT_EQUAL(254, l.artistCy);
  TEST_ASSERT_EQUAL(288, l.albumCy);
  TEST_ASSERT_EQUAL(392, l.stateX);
  TEST_ASSERT_EQUAL(350, l.stateCy);
  TEST_ASSERT_EQUAL(696, l.valueX);
  TEST_ASSERT_EQUAL(438, l.valueCy);
}

void test_popup_layout_portrait_numbers() {
  const MediaLayout l = mediaLayout(P);  // panel (16, 188, 508, 560); spec numbers are panel-relative
  const int px = 16, py = 188;
  TEST_ASSERT_TRUE((l.panel == L::Rect{16, 188, 508, 560}));
  TEST_ASSERT_TRUE((l.close == L::Rect{px + 428, py + 16, 64, 64}));
  TEST_ASSERT_TRUE((l.prev == L::Rect{px + 16, py + 232, 96, 64}));
  TEST_ASSERT_TRUE((l.play == L::Rect{px + 124, py + 232, 96, 64}));
  TEST_ASSERT_TRUE((l.next == L::Rect{px + 232, py + 232, 96, 64}));
  TEST_ASSERT_TRUE((l.volMinus == L::Rect{px + 16, py + 320, 64, 64}));
  TEST_ASSERT_TRUE((l.barTrack == L::Rect{px + 92, py + 340, 324, 24}));
  TEST_ASSERT_TRUE((l.volPlus == L::Rect{px + 428, py + 320, 64, 64}));
  TEST_ASSERT_TRUE((l.mute == L::Rect{px + 428, py + 400, 64, 64}));
  TEST_ASSERT_EQUAL(py + 120, l.titleCy);
  TEST_ASSERT_EQUAL(py + 168, l.artistCy);
  TEST_ASSERT_EQUAL(py + 202, l.albumCy);
  TEST_ASSERT_EQUAL(px + 344, l.stateX);
  TEST_ASSERT_EQUAL(py + 264, l.stateCy);
  TEST_ASSERT_EQUAL(py + 432, l.valueCy);
  // everything inside the panel
  for (const L::Rect &r : {l.close, l.prev, l.play, l.next, l.volMinus, l.volPlus, l.barTrack, l.mute})
    TEST_ASSERT_TRUE(L::rectInside(r, l.panel));
}

void test_popup_hit_test() {
  for (auto o : {LS, P}) {
    const MediaLayout l = mediaLayout(o);
    auto at = [&](const L::Rect &r) { return mediaHitTest(l, {r.x + r.w / 2, r.y + r.h / 2}); };
    TEST_ASSERT_EQUAL(int(MediaHit::Outside), int(mediaHitTest(l, {1, 1})));
    TEST_ASSERT_EQUAL(int(MediaHit::Close), int(at(l.close)));
    TEST_ASSERT_EQUAL(int(MediaHit::Prev), int(at(l.prev)));
    TEST_ASSERT_EQUAL(int(MediaHit::Play), int(at(l.play)));
    TEST_ASSERT_EQUAL(int(MediaHit::Next), int(at(l.next)));
    TEST_ASSERT_EQUAL(int(MediaHit::VolMinus), int(at(l.volMinus)));
    TEST_ASSERT_EQUAL(int(MediaHit::VolPlus), int(at(l.volPlus)));
    TEST_ASSERT_EQUAL(int(MediaHit::Bar), int(at(l.barTrack)));
    TEST_ASSERT_EQUAL(int(MediaHit::Mute), int(at(l.mute)));
    TEST_ASSERT_EQUAL(int(MediaHit::Panel), int(mediaHitTest(l, {l.panel.x + 2, l.panel.y + 2})));
    TEST_ASSERT_EQUAL(0, int(100 * mediaBarFraction(l, l.barTrack.x - 10)));
    TEST_ASSERT_EQUAL(100, int(100 * mediaBarFraction(l, l.barTrack.x + l.barTrack.w + 10)));
  }
}

// ---- popup model ----

void test_popup_steps_clamp_and_grid() {
  MediaPopup p;
  p.open("media_player.a", 0, 1, 47, false, 0);
  TEST_ASSERT_TRUE(p.step(+1, 10));
  TEST_ASSERT_EQUAL(50, p.volumeOverride());
  TEST_ASSERT_TRUE(p.step(-1, 20));
  TEST_ASSERT_EQUAL(45, p.volumeOverride());
  p.setPct(98, 30);
  p.step(+1, 40);
  TEST_ASSERT_EQUAL(100, p.volumeOverride());
  TEST_ASSERT_FALSE(p.step(+1, 50));
  p.setPct(2, 60);
  p.step(-1, 70);
  TEST_ASSERT_EQUAL(0, p.volumeOverride());
  TEST_ASSERT_FALSE(p.step(-1, 80));
  TEST_ASSERT_TRUE(p.tapBar(0.734, 90));
  TEST_ASSERT_EQUAL(73, p.volumeOverride());
}

void test_popup_debounce_one_second() {
  MediaPopup p;
  std::vector<std::pair<MediaSend, int>> sent;
  p.setSink([&](const std::string &id, MediaSend w, int v) {
    TEST_ASSERT_EQUAL_STRING("media_player.a", id.c_str());
    sent.push_back({w, v});
  });
  p.open("media_player.a", 0, 0, 40, false, 1000);
  TEST_ASSERT_EQUAL(-1, p.volumeOverride());  // untouched: follow the real state
  p.step(+1, 1000);
  p.step(+1, 1400);
  p.tick(2300);  // 900 ms after the last step
  TEST_ASSERT_EQUAL(0, (int)sent.size());
  TEST_ASSERT_TRUE(p.sendPending());
  p.tick(2400);
  TEST_ASSERT_EQUAL(1, (int)sent.size());
  TEST_ASSERT_TRUE(sent[0].first == MediaSend::Volume);
  TEST_ASSERT_EQUAL(50, sent[0].second);
  TEST_ASSERT_EQUAL(50, p.volumeOverride());  // unconfirmed: still the requested value
  p.clearOptimistic();
  TEST_ASSERT_EQUAL(-1, p.volumeOverride());
  p.step(+1, 5000);
  p.done(5100);  // closing flushes
  TEST_ASSERT_EQUAL(2, (int)sent.size());
  TEST_ASSERT_EQUAL(55, sent[1].second);
  TEST_ASSERT_FALSE(p.isOpen());
}

void test_popup_mute_and_idle_close() {
  MediaPopup p;
  std::vector<std::pair<MediaSend, int>> sent;
  p.setSink([&](const std::string &, MediaSend w, int v) { sent.push_back({w, v}); });
  p.open("media_player.a", 0, 0, 40, false, 0);
  TEST_ASSERT_TRUE(p.toggleMute(10));
  TEST_ASSERT_EQUAL(1, (int)sent.size());  // mute is sent at once
  TEST_ASSERT_TRUE(sent[0].first == MediaSend::Mute);
  TEST_ASSERT_EQUAL(1, sent[0].second);
  TEST_ASSERT_TRUE(p.muteOverridden());
  TEST_ASSERT_TRUE(p.mutedOverride());
  TEST_ASSERT_TRUE(p.toggleMute(20));
  TEST_ASSERT_EQUAL(0, sent[1].second);
  p.poke(25000);
  TEST_ASSERT_FALSE(p.tick(kMediaIdleCloseMs + 20));  // poke restarted the idle time
  TEST_ASSERT_TRUE(p.tick(25000 + kMediaIdleCloseMs));
  TEST_ASSERT_FALSE(p.isOpen());
  p.open("media_player.a", 0, 0, 40, false, 0);
  p.step(+1, 5);
  p.cancel();  // cancel sends nothing
  p.tick(100000);
  TEST_ASSERT_EQUAL(2, (int)sent.size());
}

// ---- app state ----

static int mediaIdx(const AppState &a) {
  const auto &ws = a.config().tabs[0].widgets;
  for (size_t i = 0; i < ws.size(); ++i)
    if (ws[i].type == WidgetType::MediaPlayer) return int(i);
  return -1;
}

static L::Rect widgetRect(const AppState &a, int idx) {
  for (const auto &pl : a.layout().widgets)
    if (pl.index == idx) return pl.rect;
  return {};
}

static AppState mediaApp(settings::Orientation o, bool marks) {
  AppState a;
  a.init(parsed(), o, false);
  a.setMarksDriven(marks, 5000);
  EntityState e = player("playing", "Blue in Green", "Miles Davis", 0.40);
  e.supported_features = 4 | 8 | 16 | 32 | 1;
  a.store().update({e});
  return a;
}

static L::Point subCentre(const AppState &a, int idx, int sub) {
  for (const auto &s : L::subRegions(WidgetType::MediaPlayer, {}, widgetRect(a, idx)))
    if (s.id == sub) return {s.rect.x + s.rect.w / 2, s.rect.y + s.rect.h / 2};
  return {0, 0};
}

void test_app_widget_taps_send_actions() {
  AppState a = mediaApp(P, true);
  std::vector<std::pair<int, std::string>> acts;  // sub, requested state
  a.setActionSink([&](int, int, int sub, const std::vector<EntityState> &req) { acts.push_back({sub, req[0].state}); });
  const int idx = mediaIdx(a);
  TEST_ASSERT_TRUE(a.touch(subCentre(a, idx, 2), 0));  // play/pause: playing -> paused
  TEST_ASSERT_EQUAL(1, (int)acts.size());
  TEST_ASSERT_EQUAL(2, acts[0].first);
  TEST_ASSERT_EQUAL_STRING("paused", acts[0].second.c_str());
  TEST_ASSERT_TRUE(a.touch(subCentre(a, idx, 3), 10));  // next: state unchanged but the call is sent
  TEST_ASSERT_EQUAL(2, (int)acts.size());
  TEST_ASSERT_EQUAL(3, acts[1].first);
  TEST_ASSERT_FALSE(a.mediaOpen());
}

void test_app_text_block_opens_popup_and_volume_sends() {
  AppState a = mediaApp(P, true);
  std::vector<std::pair<MediaSend, int>> sent;
  a.setMediaSink([&](const std::string &id, MediaSend w, int v) {
    TEST_ASSERT_EQUAL_STRING("media_player.living", id.c_str());
    sent.push_back({w, v});
  });
  const int idx = mediaIdx(a);
  TEST_ASSERT_TRUE(a.touch(subCentre(a, idx, 4), 0));
  TEST_ASSERT_TRUE(a.mediaOpen());
  TEST_ASSERT_TRUE(a.modalOpen());
  TEST_ASSERT_EQUAL_STRING("Music", a.mediaTitle().c_str());
  MediaPopupView v = a.mediaPopupData();
  TEST_ASSERT_TRUE(v.hasVolume);
  TEST_ASSERT_EQUAL(40, v.volumePct);
  const MediaLayout l = mediaLayout(P);
  TEST_ASSERT_TRUE(a.touch({l.volPlus.x + 5, l.volPlus.y + 5}, 100));
  TEST_ASSERT_TRUE(a.touch({l.volPlus.x + 5, l.volPlus.y + 5}, 200));
  TEST_ASSERT_EQUAL(50, a.mediaPopupData().volumePct);  // optimistic at once
  TEST_ASSERT_TRUE(a.mediaAwaiting());
  a.tick(1100);
  TEST_ASSERT_EQUAL(0, (int)sent.size());
  a.tick(1200);
  TEST_ASSERT_EQUAL(1, (int)sent.size());
  TEST_ASSERT_TRUE(sent[0].first == MediaSend::Volume);
  TEST_ASSERT_EQUAL(50, sent[0].second);
  // mute
  TEST_ASSERT_TRUE(a.touch({l.mute.x + 5, l.mute.y + 5}, 1300));
  TEST_ASSERT_EQUAL(2, (int)sent.size());
  TEST_ASSERT_TRUE(a.mediaPopupData().muted);
  // a tap on the panel changes nothing; outside closes
  TEST_ASSERT_FALSE(a.touch({l.panel.x + 2, l.panel.y + 2}, 1400));
  TEST_ASSERT_TRUE(a.mediaOpen());
  TEST_ASSERT_TRUE(a.touch({1, 1}, 1500));
  TEST_ASSERT_FALSE(a.mediaOpen());
}

void test_app_popup_transport_and_confirmation() {
  AppState a = mediaApp(P, true);
  std::vector<int> subs;
  a.setActionSink([&](int, int, int sub, const std::vector<EntityState> &) { subs.push_back(sub); });
  std::vector<int> vols;
  a.setMediaSink([&](const std::string &, MediaSend w, int v) { if (w == MediaSend::Volume) vols.push_back(v); });
  a.touch(subCentre(a, mediaIdx(a), 4), 0);
  const MediaLayout l = mediaLayout(P);
  a.touch({l.prev.x + 5, l.prev.y + 5}, 10);
  a.touch({l.play.x + 5, l.play.y + 5}, 20);
  TEST_ASSERT_EQUAL(2, (int)subs.size());
  TEST_ASSERT_EQUAL(1, subs[0]);
  TEST_ASSERT_EQUAL(2, subs[1]);
  TEST_ASSERT_TRUE(a.mediaOpen());  // transport does not close it
  // volume: send, then the driver marks Pending, then clears -> optimistic value dropped
  a.touch({l.volMinus.x + 5, l.volMinus.y + 5}, 100);
  a.tick(1200);
  TEST_ASSERT_EQUAL(1, (int)vols.size());
  TEST_ASSERT_EQUAL(35, vols[0]);
  a.setMarks({{"media_player.living", Mark::Pending}});
  TEST_ASSERT_TRUE(a.mediaAwaiting());
  EntityState e = *a.store().get("media_player.living");
  e.attrs.clear();
  e.attrs.emplace_back("volume_level", "0.35");
  a.store().update({e});
  a.setMarks({});
  TEST_ASSERT_FALSE(a.mediaAwaiting());
  TEST_ASSERT_EQUAL(35, a.mediaPopupData().volumePct);
  // failure: alert shown, real state again
  a.touch({l.volMinus.x + 5, l.volMinus.y + 5}, 2000);
  a.tick(3100);
  a.setMarks({{"media_player.living", Mark::Pending}});
  a.setMarks({{"media_player.living", Mark::Failed}});
  TEST_ASSERT_TRUE(a.mediaFailed());
  TEST_ASSERT_EQUAL(35, a.mediaPopupData().volumePct);
  a.closeMedia(4000);
  TEST_ASSERT_FALSE(a.mediaFailed());
}

void test_app_rocker_focus_and_popup() {
  AppState a = mediaApp(P, true);
  std::vector<int> subs;
  a.setActionSink([&](int, int, int sub, const std::vector<EntityState> &) { subs.push_back(sub); });
  std::vector<int> vols;
  a.setMediaSink([&](const std::string &, MediaSend w, int v) { if (w == MediaSend::Volume) vols.push_back(v); });
  const int idx = mediaIdx(a);
  // Tab mode -> Push -> first widget; Push -> INNER with play/pause (entry); Down x2 -> text block
  a.rocker(ui::PhysicalKey::Push, 0);
  TEST_ASSERT_EQUAL(idx, a.focusedWidgetIndex());
  a.rocker(ui::PhysicalKey::Push, 10);
  TEST_ASSERT_TRUE(a.focus().mode == ui::FocusMode::Inner);
  TEST_ASSERT_EQUAL(2, a.focus().inner);
  a.rocker(ui::PhysicalKey::Push, 20);  // play/pause
  TEST_ASSERT_EQUAL(1, (int)subs.size());
  TEST_ASSERT_EQUAL(2, subs[0]);
  a.rocker(ui::PhysicalKey::Down, 30);
  TEST_ASSERT_EQUAL(3, a.focus().inner);
  a.rocker(ui::PhysicalKey::Down, 40);
  TEST_ASSERT_EQUAL(4, a.focus().inner);
  TEST_ASSERT_FALSE(a.mediaOpen());
  a.rocker(ui::PhysicalKey::Push, 50);  // text block opens the popup
  TEST_ASSERT_TRUE(a.mediaOpen());
  a.rocker(ui::PhysicalKey::Up, 60);    // portrait Up = louder
  TEST_ASSERT_EQUAL(45, a.mediaPopupData().volumePct);
  a.rocker(ui::PhysicalKey::Down, 70);
  a.rocker(ui::PhysicalKey::Down, 80);
  TEST_ASSERT_EQUAL(35, a.mediaPopupData().volumePct);
  a.setMarks({{"media_player.living", Mark::Pending}});  // the first play/pause is confirmed
  a.setMarks({});
  a.rocker(ui::PhysicalKey::Push, 90);  // push = play/pause in the popup
  TEST_ASSERT_EQUAL(2, (int)subs.size());
  TEST_ASSERT_EQUAL(2, subs[1]);
  a.rocker(ui::PhysicalKey::LongPush, 100);  // long push closes and flushes
  TEST_ASSERT_FALSE(a.mediaOpen());
  TEST_ASSERT_EQUAL(1, (int)vols.size());
  TEST_ASSERT_EQUAL(35, vols[0]);
}

void test_app_rocker_landscape_down_is_louder() {
  AppState a = mediaApp(LS, true);
  a.touch(subCentre(a, mediaIdx(a), 4), 0);
  TEST_ASSERT_TRUE(a.mediaOpen());
  a.rocker(ui::PhysicalKey::Down, 10);
  TEST_ASSERT_EQUAL(45, a.mediaPopupData().volumePct);
  a.rocker(ui::PhysicalKey::Up, 20);
  TEST_ASSERT_EQUAL(40, a.mediaPopupData().volumePct);
}

void test_app_unavailable_and_loading() {
  AppState a;
  a.init(parsed(), P, false);
  const int idx = mediaIdx(a);
  // loading (nothing cached): the popup opens with "-"
  TEST_ASSERT_TRUE(a.touch(subCentre(a, idx, 4), 0));
  TEST_ASSERT_EQUAL_STRING("-", a.mediaPopupData().title.c_str());
  TEST_ASSERT_FALSE(a.mediaPopupData().hasVolume);
  a.closeMedia(10);
  // unavailable: does not open, buttons do nothing
  EntityState e = player("unavailable");
  e.available = false;
  a.store().update({e});
  TEST_ASSERT_FALSE(a.touch(subCentre(a, idx, 4), 20));
  TEST_ASSERT_FALSE(a.mediaOpen());
  TEST_ASSERT_FALSE(a.touch(subCentre(a, idx, 2), 30));
  // a player that becomes unavailable closes the popup without sending
  e = player("playing", "T", "A", 0.4);
  a.store().update({e});
  std::vector<int> vols;
  a.setMediaSink([&](const std::string &, MediaSend, int v) { vols.push_back(v); });
  a.touch(subCentre(a, idx, 4), 100);
  TEST_ASSERT_TRUE(a.mediaOpen());
  a.rocker(ui::PhysicalKey::Up, 110);
  e.state = "unavailable";
  e.available = false;
  a.store().update({e});
  TEST_ASSERT_TRUE(a.tick(200));
  TEST_ASSERT_FALSE(a.mediaOpen());
  a.tick(5000);
  TEST_ASSERT_EQUAL(0, (int)vols.size());
}

void test_app_demo_mode_applies_volume_to_store() {
  AppState a;
  a.init(parsed(), P, true);  // demo: states from demoStates
  const int idx = mediaIdx(a);
  TEST_ASSERT_TRUE(a.touch(subCentre(a, idx, 4), 0));
  const MediaLayout l = mediaLayout(P);
  const int before = a.mediaPopupData().volumePct;
  a.touch({l.volPlus.x + 5, l.volPlus.y + 5}, 10);
  a.tick(1100);
  TEST_ASSERT_EQUAL(before + 5, mediaVolumePct(*a.store().get("media_player.living")));
  a.touch({l.mute.x + 5, l.mute.y + 5}, 1200);
  TEST_ASSERT_TRUE(a.mediaPopupData().muted);
}

void test_app_close_before_sleep_flushes() {
  AppState a = mediaApp(P, true);
  std::vector<int> vols;
  a.setMediaSink([&](const std::string &, MediaSend w, int v) { if (w == MediaSend::Volume) vols.push_back(v); });
  a.touch(subCentre(a, mediaIdx(a), 4), 0);
  a.rocker(ui::PhysicalKey::Up, 10);
  TEST_ASSERT_TRUE(a.closeMedia(20));
  TEST_ASSERT_EQUAL(1, (int)vols.size());
  TEST_ASSERT_FALSE(a.closeMedia(30));  // already closed
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_config_media_type);
  RUN_TEST(test_template_requests_media_attributes);
  RUN_TEST(test_service_calls);
  RUN_TEST(test_tap_intents);
  RUN_TEST(test_view_states_and_fallbacks);
  RUN_TEST(test_popup_view_volume_features);
  RUN_TEST(test_widget_sub_regions_match_spec);
  RUN_TEST(test_widget_sizes);
  RUN_TEST(test_popup_layout_landscape_numbers);
  RUN_TEST(test_popup_layout_portrait_numbers);
  RUN_TEST(test_popup_hit_test);
  RUN_TEST(test_popup_steps_clamp_and_grid);
  RUN_TEST(test_popup_debounce_one_second);
  RUN_TEST(test_popup_mute_and_idle_close);
  RUN_TEST(test_app_widget_taps_send_actions);
  RUN_TEST(test_app_text_block_opens_popup_and_volume_sends);
  RUN_TEST(test_app_popup_transport_and_confirmation);
  RUN_TEST(test_app_rocker_focus_and_popup);
  RUN_TEST(test_app_rocker_landscape_down_is_louder);
  RUN_TEST(test_app_unavailable_and_loading);
  RUN_TEST(test_app_demo_mode_applies_volume_to_store);
  RUN_TEST(test_app_close_before_sleep_flushes);
  return UNITY_END();
}
