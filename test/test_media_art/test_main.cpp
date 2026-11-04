// Task 107: album art (pixels, fetch plan, store, config option, layouts, app state, demo).
#include <unity.h>

#include "ha_worker.h"

#include <string>

#include "app_state.h"
#include "art_fetch.h"
#include "art_image.h"
#include "art_store.h"
#include "config_model.h"
#include "config_cache.h"
#include "entity_state.h"
#include "media_layout.h"
#include "media_view.h"
#include "template_builder.h"
#include "ui_layout.h"

using namespace art;
using config::WidgetType;
namespace L = ui;

static const settings::Orientation P = settings::Orientation::Portrait;
static const settings::Orientation LS = settings::Orientation::Landscape;

void setUp() {}
void tearDown() {}

// ---- pixels ----

static Gray8 flat(int w, int h, uint8_t v) {
  Gray8 g;
  g.w = w;
  g.h = h;
  g.px.assign(size_t(w) * size_t(h), v);
  return g;
}

void test_gray4_pack() {
  Gray4 g = Gray4::make(3, 3);  // 9 px -> 5 bytes
  TEST_ASSERT_TRUE(g.valid());
  TEST_ASSERT_EQUAL(5, (int)g.px.size());
  for (int i = 0; i < 9; ++i) g.set(i % 3, i / 3, uint8_t(i + 3));
  for (int i = 0; i < 9; ++i) TEST_ASSERT_EQUAL(i + 3, g.at(i % 3, i / 3));
  TEST_ASSERT_EQUAL(0, g.at(5, 5));  // outside
  Gray4 big = Gray4::make(200, 200);
  TEST_ASSERT_EQUAL(20000, (int)big.px.size());  // 20 KB, as the spec says
  TEST_ASSERT_EQUAL(3200, (int)Gray4::make(80, 80).px.size());
}

void test_luma() {
  TEST_ASSERT_EQUAL(0, luma8(0, 0, 0));
  TEST_ASSERT_EQUAL(255, luma8(255, 255, 255));
  TEST_ASSERT_EQUAL(255, luma565(0xFFFF));
  TEST_ASSERT_EQUAL(0, luma565(0));
  TEST_ASSERT_TRUE(luma8(0, 255, 0) > luma8(255, 0, 0));
  TEST_ASSERT_TRUE(luma8(255, 0, 0) > luma8(0, 0, 255));
}

void test_square_scale_crops_centre() {
  // 6 x 2: left third 0, middle third 100, right third 250; the centre square is the middle 2 x 2.
  Gray8 g;
  g.w = 6;
  g.h = 2;
  g.px = {0, 0, 100, 100, 250, 250, 0, 0, 100, 100, 250, 250};
  const Gray8 s = squareScale(g, 2);
  TEST_ASSERT_TRUE(s.valid());
  TEST_ASSERT_EQUAL(2, s.w);
  for (uint8_t v : s.px) TEST_ASSERT_EQUAL(100, v);
  // tall: 2 x 6 cropped vertically
  Gray8 t;
  t.w = 2;
  t.h = 6;
  t.px = {0, 0, 0, 0, 100, 100, 100, 100, 250, 250, 250, 250};
  const Gray8 st = squareScale(t, 2);
  for (uint8_t v : st.px) TEST_ASSERT_EQUAL(100, v);
}

void test_square_scale_averages_and_upsamples() {
  Gray8 g;
  g.w = g.h = 4;
  g.px = {0, 100, 0, 100, 100, 0, 100, 0, 0, 100, 0, 100, 100, 0, 100, 0};
  const Gray8 s = squareScale(g, 1);
  TEST_ASSERT_EQUAL(50, s.px[0]);
  const Gray8 up = squareScale(flat(2, 2, 77), 5);  // upscale of a flat picture stays flat
  TEST_ASSERT_EQUAL(25, (int)up.px.size());
  for (uint8_t v : up.px) TEST_ASSERT_EQUAL(77, v);
  TEST_ASSERT_FALSE(squareScale(Gray8(), 8).valid());
  TEST_ASSERT_FALSE(squareScale(flat(4, 4, 0), 0).valid());
}

void test_dither_levels_and_mean() {
  const Gray4 white = dither16(flat(8, 8, 255));
  const Gray4 black = dither16(flat(8, 8, 0));
  for (int i = 0; i < 64; ++i) {
    TEST_ASSERT_EQUAL(15, white.at(i % 8, i / 8));
    TEST_ASSERT_EQUAL(0, black.at(i % 8, i / 8));
  }
  // exact grey levels are kept (no error): 17 * 7 = 119
  const Gray4 exact = dither16(flat(8, 8, 119));
  for (int i = 0; i < 64; ++i) TEST_ASSERT_EQUAL(7, exact.at(i % 8, i / 8));
  // a grey between two levels: the mean of the 16-grey result stays close (error diffusion)
  const Gray4 mid = dither16(flat(40, 40, 128));  // 128 / 17 = 7.53
  long sum = 0;
  for (int i = 0; i < 1600; ++i) {
    const int v = mid.at(i % 40, i / 40);
    TEST_ASSERT_TRUE(v == 7 || v == 8);
    sum += v;
  }
  const double mean = double(sum) / 1600.0;
  TEST_ASSERT_TRUE(mean > 7.4 && mean < 7.65);
  TEST_ASSERT_FALSE(dither16(Gray8()).valid());
}

void test_sample_image_deterministic_and_varied() {
  const Gray8 a = sampleImage(200, 0), b = sampleImage(200, 0);
  TEST_ASSERT_TRUE(a.valid());
  TEST_ASSERT_TRUE(a.px == b.px);
  uint8_t lo = 255, hi = 0;
  for (uint8_t v : a.px) {
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  TEST_ASSERT_TRUE(hi - lo > 150);  // real contrast, not a flat grey
  const Gray4 small = dither16(squareScale(a, kWidgetArtPx));
  TEST_ASSERT_EQUAL(80, small.w);
}

// ---- fetch plan ----

void test_plan_fetch() {
  FetchPlan p = planFetch("http://ha.local:8123", "/api/media_player_proxy/media_player.x?token=abc");
  TEST_ASSERT_TRUE(p.ok);
  TEST_ASSERT_EQUAL_STRING("http://ha.local:8123/api/media_player_proxy/media_player.x?token=abc", p.url.c_str());
  TEST_ASSERT_TRUE(p.sendToken);
  p = planFetch("https://ha.example.org/", "/local/x.jpg");
  TEST_ASSERT_EQUAL_STRING("https://ha.example.org/local/x.jpg", p.url.c_str());
  p = planFetch("http://ha.local:8123/some/path", "/x");
  TEST_ASSERT_EQUAL_STRING("http://ha.local:8123/x", p.url.c_str());
  // absolute, same host: token; other host: no token (never leak it)
  p = planFetch("http://ha.local:8123", "http://HA.local:8123/a.png");
  TEST_ASSERT_TRUE(p.ok && p.sendToken);
  p = planFetch("http://ha.local:8123", "https://i.scdn.co/image/abc");
  TEST_ASSERT_TRUE(p.ok);
  TEST_ASSERT_FALSE(p.sendToken);
  // rejected
  TEST_ASSERT_FALSE(planFetch("http://ha", "").ok);
  TEST_ASSERT_FALSE(planFetch("http://ha", "//evil.example/x").ok);
  TEST_ASSERT_FALSE(planFetch("http://ha", "ftp://x/y").ok);
  TEST_ASSERT_FALSE(planFetch("http://ha", "demo:sample").ok);
  TEST_ASSERT_FALSE(planFetch("", "/x").ok);
  TEST_ASSERT_FALSE(planFetch("http://ha", "/" + std::string(600, 'a')).ok);
  TEST_ASSERT_TRUE(isDemoPicture("demo:sample"));
  TEST_ASSERT_FALSE(isDemoPicture("/api/x"));
}

void test_body_cap() {
  TEST_ASSERT_TRUE(bodyWithinCap(-1));
  TEST_ASSERT_TRUE(bodyWithinCap(150 * 1024));
  TEST_ASSERT_FALSE(bodyWithinCap(150 * 1024 + 1));
  TEST_ASSERT_EQUAL(150 * 1024, (int)kMaxArtBytes);
}

// ---- store ----

void test_store_lifecycle_and_redraw_rule() {
  ArtStore s;
  TEST_ASSERT_FALSE(s.want("media_player.a", ""));  // nothing shown, nothing known
  TEST_ASSERT_TRUE(s.state("media_player.a") == ArtState::None);
  TEST_ASSERT_TRUE(s.want("media_player.a", "/api/p1"));
  TEST_ASSERT_TRUE(s.state("media_player.a") == ArtState::Loading);
  TEST_ASSERT_FALSE(s.want("media_player.a", "/api/p1"));  // same URL: no change, no redraw
  auto rq = s.takeRequests();
  TEST_ASSERT_EQUAL(1, (int)rq.size());
  TEST_ASSERT_EQUAL_STRING("/api/p1", rq[0].picture.c_str());
  TEST_ASSERT_EQUAL(0, (int)s.takeRequests().size());  // reported once
  TEST_ASSERT_TRUE(s.deliver("media_player.a", "/api/p1", sampleImage(200)));
  TEST_ASSERT_TRUE(s.state("media_player.a") == ArtState::Ready);
  TEST_ASSERT_NOT_NULL(s.widgetArt("media_player.a"));
  TEST_ASSERT_EQUAL(80, s.widgetArt("media_player.a")->w);
  TEST_ASSERT_FALSE(s.want("media_player.a", "/api/p1"));  // title/artist/volume changes never touch the art
  TEST_ASSERT_EQUAL(0, (int)s.takeRequests().size());
  // new track: previous cover dropped at once, fetch again
  TEST_ASSERT_TRUE(s.want("media_player.a", "/api/p2"));
  TEST_ASSERT_NULL(s.widgetArt("media_player.a"));
  TEST_ASSERT_TRUE(s.state("media_player.a") == ArtState::Loading);
  // the late answer for the old URL is ignored
  TEST_ASSERT_FALSE(s.deliver("media_player.a", "/api/p1", sampleImage(200)));
  TEST_ASSERT_TRUE(s.state("media_player.a") == ArtState::Loading);
  // to a no-picture state (off) and back
  TEST_ASSERT_TRUE(s.want("media_player.a", ""));
  TEST_ASSERT_TRUE(s.state("media_player.a") == ArtState::None);
  TEST_ASSERT_TRUE(s.want("media_player.a", "/api/p2"));
  TEST_ASSERT_TRUE(s.state("media_player.a") == ArtState::Loading);
}

void test_store_failure_and_retry() {
  ArtStore s;
  s.want("media_player.a", "/api/p1");
  s.takeRequests();
  TEST_ASSERT_TRUE(s.fail("media_player.a", "/api/p1"));
  TEST_ASSERT_TRUE(s.state("media_player.a") == ArtState::Failed);
  TEST_ASSERT_FALSE(s.want("media_player.a", "/api/p1"));  // not retried for the same URL
  TEST_ASSERT_EQUAL(0, (int)s.takeRequests().size());
  TEST_ASSERT_FALSE(s.fail("media_player.a", "/other"));  // stale
  TEST_ASSERT_TRUE(s.retryFailed());                       // next wake
  TEST_ASSERT_TRUE(s.state("media_player.a") == ArtState::Loading);
  TEST_ASSERT_EQUAL(1, (int)s.takeRequests().size());
  // a failed answer (bad picture) counts as failure
  TEST_ASSERT_TRUE(s.deliver("media_player.a", "/api/p1", Gray8()));
  TEST_ASSERT_TRUE(s.state("media_player.a") == ArtState::Failed);
  // a new URL retries too
  TEST_ASSERT_TRUE(s.want("media_player.a", "/api/p2"));
  TEST_ASSERT_TRUE(s.state("media_player.a") == ArtState::Loading);
}

void test_store_popup_big_buffer() {
  ArtStore s;
  s.want("media_player.a", "/api/p1");
  s.takeRequests();
  s.deliver("media_player.a", "/api/p1", sampleImage(200));
  TEST_ASSERT_NULL(s.popupArt());  // no popup open: no 20 KB kept
  TEST_ASSERT_TRUE(s.setPopup("media_player.a"));
  TEST_ASSERT_TRUE(s.popupState() == ArtState::Loading);  // 200 px still to come
  auto rq = s.takeRequests();
  TEST_ASSERT_EQUAL(1, (int)rq.size());
  s.deliver("media_player.a", "/api/p1", sampleImage(200));
  TEST_ASSERT_TRUE(s.popupState() == ArtState::Ready);
  TEST_ASSERT_EQUAL(200, s.popupArt()->w);
  TEST_ASSERT_NOT_NULL(s.widgetArt("media_player.a"));
  s.setPopup("");
  TEST_ASSERT_NULL(s.popupArt());
  TEST_ASSERT_TRUE(s.state("media_player.a") == ArtState::Ready);  // widget art stays
}

void test_store_demo_and_entry_limit() {
  ArtStore s;
  s.want("media_player.a", "demo:sample");
  s.want("media_player.b", "/api/real");
  TEST_ASSERT_TRUE(s.serveDemo());
  TEST_ASSERT_TRUE(s.state("media_player.a") == ArtState::Ready);
  TEST_ASSERT_TRUE(s.state("media_player.b") == ArtState::Loading);  // real downloads are left alone
  TEST_ASSERT_EQUAL(1, (int)s.takeRequests().size());
  TEST_ASSERT_FALSE(s.serveDemo());
  for (int i = 0; i < 12; ++i) s.want("media_player.x" + std::to_string(i), "/p");
  int loading = 0;
  for (int i = 0; i < 12; ++i) loading += s.state("media_player.x" + std::to_string(i)) == ArtState::Loading;
  TEST_ASSERT_EQUAL((int)kMaxArtEntries, loading);  // oldest entries evicted
}

// ---- config ----

static const char *kYaml =
    "version: \"1\"\ntabs:\n  - title: T\n    widgets:\n      - entity: media_player.a\n"
    "      - entity: media_player.b\n        art: false\n      - entity: media_player.c\n        art: true\n";

void test_config_art_option() {
  config::ParseResult r = config::parseConfig(std::string(kYaml));
  TEST_ASSERT_TRUE(r.ok());
  const auto &w = r.config.tabs[0].widgets;
  TEST_ASSERT_TRUE(w[0].art);   // default on
  TEST_ASSERT_FALSE(w[1].art);
  TEST_ASSERT_TRUE(w[2].art);
  config::Config out;
  TEST_ASSERT_TRUE(config::deserializeConfig(config::serializeConfig(r.config), out));
  TEST_ASSERT_TRUE(out.tabs[0].widgets[0].art);
  TEST_ASSERT_FALSE(out.tabs[0].widgets[1].art);
  TEST_ASSERT_TRUE(out.tabs[0].widgets[2].art);
}

void test_config_art_errors() {
  config::ParseResult r = config::parseConfig(std::string(
      "version: \"1\"\ntabs:\n  - title: T\n    widgets:\n      - entity: media_player.a\n        art: maybe\n"
      "      - entity: light.x\n        art: false\n"));
  TEST_ASSERT_FALSE(r.ok());
  TEST_ASSERT_EQUAL(2, (int)r.errors.size());
  TEST_ASSERT_EQUAL_STRING("tabs[0].widgets[0].art", r.errors[0].path.c_str());
  TEST_ASSERT_EQUAL_STRING("tabs[0].widgets[1].art", r.errors[1].path.c_str());
  for (const auto &e : r.errors) TEST_ASSERT_TRUE(e.message.find("maybe") == std::string::npos);
}

void test_template_requests_entity_picture() {
  std::vector<std::string> names = state_attribute_names("media_player.x", {});
  bool found = false;
  for (const auto &n : names) found |= n == "entity_picture";
  TEST_ASSERT_TRUE(found);
}

// ---- views ----

static EntityState player(const char *state, const char *pic) {
  EntityState e;
  e.id = "media_player.a";
  e.state = state;
  e.available = true;
  if (*pic) e.attrs.emplace_back("entity_picture", pic);
  return e;
}

static EntityState keep;
static EntityState &pl(const char *s, const char *p) { keep = player(s, p); return keep; }

void test_media_picture_rule() {
  using uistate::mediaPicture;
  TEST_ASSERT_EQUAL_STRING("/p", mediaPicture(&pl("playing", "/p")).c_str());
  TEST_ASSERT_EQUAL_STRING("/p", mediaPicture(&pl("paused", "/p")).c_str());
  TEST_ASSERT_EQUAL_STRING("/p", mediaPicture(&pl("idle", "/p")).c_str());
  TEST_ASSERT_EQUAL_STRING("", mediaPicture(&pl("off", "/p")).c_str());
  TEST_ASSERT_EQUAL_STRING("", mediaPicture(&pl("playing", "")).c_str());
  EntityState u = player("unavailable", "/p");
  u.available = false;
  TEST_ASSERT_EQUAL_STRING("", mediaPicture(&u).c_str());
  TEST_ASSERT_EQUAL_STRING("", mediaPicture(nullptr).c_str());
}

// ---- layout numbers (docs/ui/widgets-new-media-art.md) ----

void test_widget_layout_numbers() {
  for (int W : {552, 508, 740}) {
    const auto l = uistate::mediaWidgetLayout(W, true);
    TEST_ASSERT_TRUE((l.artRect == L::Rect{12, 12, 80, 80}));
    TEST_ASSERT_TRUE((l.edge == L::Rect{11, 11, 82, 82}));
    TEST_ASSERT_TRUE((l.tile == L::Rect{13, 13, 78, 78}));
    TEST_ASSERT_EQUAL(104, l.textX);
    TEST_ASSERT_EQUAL(W - 344, l.line1MaxW);
    TEST_ASSERT_EQUAL(W - 376, l.line2MaxW);
    TEST_ASSERT_EQUAL(36, l.line1Cy);
    TEST_ASSERT_EQUAL(69, l.line2Cy);
    const auto old = uistate::mediaWidgetLayout(W, false);  // art: false keeps the old layout
    TEST_ASSERT_EQUAL(12, old.textX);
    TEST_ASSERT_EQUAL(W - 252, old.line1MaxW);
    TEST_ASSERT_EQUAL(W - 284, old.line2MaxW);
  }
  TEST_ASSERT_EQUAL(208, uistate::mediaWidgetLayout(552, true).line1MaxW);  // "text room 208 px"
  TEST_ASSERT_EQUAL(164, uistate::mediaWidgetLayout(508, true).line1MaxW);
}

void test_sub_regions_with_art() {
  const L::Rect r{20, 100, 552, 104};
  const auto subs = L::subRegions(WidgetType::MediaPlayer, {}, r, true);
  TEST_ASSERT_EQUAL(5, (int)subs.size());
  TEST_ASSERT_TRUE((subs[3].rect == L::Rect{r.x + 104, r.y + 12, 552 - 344, 80}));
  TEST_ASSERT_EQUAL(5, subs[4].id);
  TEST_ASSERT_TRUE((subs[4].rect == L::Rect{r.x + 12, r.y + 12, 80, 80}));
  TEST_ASSERT_FALSE(subs[3].rect.contains({subs[4].rect.x + 5, subs[4].rect.y + 5}));
  TEST_ASSERT_EQUAL(4, (int)L::subRegions(WidgetType::MediaPlayer, {}, r, false).size());
}

void test_popup_layout_landscape_art() {
  const auto l = uistate::mediaLayout(LS, true);
  TEST_ASSERT_TRUE((l.panel == L::Rect{40, 94, 880, 408}));
  TEST_ASSERT_TRUE((l.artRect == L::Rect{56, 190, 200, 200}));
  TEST_ASSERT_TRUE((l.edge == L::Rect{55, 189, 202, 202}));
  TEST_ASSERT_TRUE((l.tile == L::Rect{57, 191, 198, 198}));
  TEST_ASSERT_EQUAL(272, l.titleX);
  TEST_ASSERT_EQUAL(552, l.titleMaxW);
  TEST_ASSERT_EQUAL(206, l.titleCy);
  TEST_ASSERT_EQUAL(254, l.artistCy);
  TEST_ASSERT_EQUAL(288, l.albumCy);
  TEST_ASSERT_TRUE((l.prev == L::Rect{272, 318, 96, 64}));
  TEST_ASSERT_TRUE((l.play == L::Rect{380, 318, 96, 64}));
  TEST_ASSERT_TRUE((l.next == L::Rect{488, 318, 96, 64}));
  TEST_ASSERT_EQUAL(604, l.stateX);
  TEST_ASSERT_EQUAL(350, l.stateCy);
  TEST_ASSERT_EQUAL(904, l.hintRight);
  TEST_ASSERT_EQUAL_STRING("UP/DOWN volume", l.hint);
  TEST_ASSERT_TRUE((l.volMinus == L::Rect{56, 406, 64, 64}));
  TEST_ASSERT_TRUE((l.barTrack == L::Rect{132, 426, 472, 24}));
  TEST_ASSERT_TRUE((l.mute == L::Rect{840, 406, 64, 64}));
  // clearance art to the volume row 16 (art to close: close ends at y 174, art starts at 190)
  TEST_ASSERT_EQUAL(16, l.artRect.y - (l.close.y + l.close.h));
  TEST_ASSERT_EQUAL(16, l.volMinus.y - (l.artRect.y + l.artRect.h));
  TEST_ASSERT_TRUE(L::rectInside(l.edge, l.panel));
  TEST_ASSERT_FALSE(l.prev.contains({l.artRect.x + 100, l.artRect.y + 100}));
  TEST_ASSERT_EQUAL(int(uistate::MediaHit::Panel), int(uistate::mediaHitTest(l, {l.artRect.x + 100, l.artRect.y + 100})));
}

void test_popup_layout_portrait_art() {
  const auto l = uistate::mediaLayout(P, true);  // panel (16, 120, 508, 700); spec numbers are panel-relative
  const int px = 16, py = 120;
  TEST_ASSERT_TRUE((l.panel == L::Rect{16, 120, 508, 700}));
  TEST_ASSERT_TRUE((l.close == L::Rect{px + 428, py + 16, 64, 64}));
  TEST_ASSERT_TRUE((l.artRect == L::Rect{px + 154, py + 88, 200, 200}));
  TEST_ASSERT_TRUE((l.edge == L::Rect{px + 153, py + 87, 202, 202}));
  TEST_ASSERT_EQUAL(py + 48, l.nameCy);
  TEST_ASSERT_EQUAL(px + 16, l.titleX);
  TEST_ASSERT_EQUAL(476, l.titleMaxW);
  TEST_ASSERT_EQUAL(py + 324, l.titleCy);
  TEST_ASSERT_EQUAL(py + 372, l.artistCy);
  TEST_ASSERT_EQUAL(py + 406, l.albumCy);
  TEST_ASSERT_TRUE((l.prev == L::Rect{px + 16, py + 432, 96, 64}));
  TEST_ASSERT_TRUE((l.play == L::Rect{px + 124, py + 432, 96, 64}));
  TEST_ASSERT_TRUE((l.next == L::Rect{px + 232, py + 432, 96, 64}));
  TEST_ASSERT_EQUAL(px + 344, l.stateX);
  TEST_ASSERT_EQUAL(py + 464, l.stateCy);
  TEST_ASSERT_TRUE((l.volMinus == L::Rect{px + 16, py + 520, 64, 64}));
  TEST_ASSERT_TRUE((l.barTrack == L::Rect{px + 92, py + 540, 324, 24}));
  TEST_ASSERT_TRUE((l.volPlus == L::Rect{px + 428, py + 520, 64, 64}));
  TEST_ASSERT_EQUAL(px + 16, l.valueX);
  TEST_ASSERT_EQUAL(py + 632, l.valueCy);
  TEST_ASSERT_EQUAL(px + 412, l.hintRight);
  TEST_ASSERT_EQUAL(py + 632, l.hintCy);
  TEST_ASSERT_FALSE(l.hintLeft);
  TEST_ASSERT_TRUE((l.mute == L::Rect{px + 428, py + 600, 64, 64}));
  for (const L::Rect &r : {l.close, l.prev, l.play, l.next, l.volMinus, l.volPlus, l.barTrack, l.mute, l.edge})
    TEST_ASSERT_TRUE(L::rectInside(r, l.panel));
  TEST_ASSERT_TRUE(L::rectInside(l.dirty, L::Rect{0, 0, 540, 960}));
}

void test_popup_layout_without_art_is_old() {
  const auto l = uistate::mediaLayout(LS, false);
  TEST_ASSERT_FALSE(l.art);
  TEST_ASSERT_TRUE((l.prev == L::Rect{56, 318, 96, 64}));
  TEST_ASSERT_TRUE((uistate::mediaLayout(P, false).panel == L::Rect{16, 188, 508, 560}));
}

// ---- app state ----

static config::Config parsed() {
  config::ParseResult r = config::parseConfig(std::string(kYaml));
  TEST_ASSERT_TRUE(r.ok());
  return r.config;
}

static L::Rect widgetRect(const uistate::AppState &a, int idx) {
  for (const auto &pl : a.layout().widgets)
    if (pl.index == idx) return pl.rect;
  return {};
}

static EntityState withPic(const char *id, const char *state, const char *pic) {
  EntityState e = player(state, pic);
  e.id = id;
  e.friendly_name = "Player";
  e.attrs.emplace_back("volume_level", "0.4");
  return e;
}

void test_app_art_follows_picture_and_redraws_once() {
  uistate::AppState a;
  a.init(parsed(), P, false);
  a.store().update({withPic("media_player.a", "playing", "/api/p1"), withPic("media_player.b", "playing", "/api/pb")});
  TEST_ASSERT_TRUE(a.artSync());
  TEST_ASSERT_TRUE(a.widgetData(0).artState == ArtState::Loading);
  TEST_ASSERT_TRUE(a.widgetData(0).art);
  TEST_ASSERT_FALSE(a.widgetData(1).art);  // art: false -> old look, no art state
  auto rq = a.takeArtRequests();
  TEST_ASSERT_EQUAL(1, (int)rq.size());  // only the widget with art
  TEST_ASSERT_EQUAL_STRING("media_player.a", rq[0].entity.c_str());
  a.art().deliver("media_player.a", "/api/p1", sampleImage(200));
  TEST_ASSERT_TRUE(a.widgetData(0).artState == ArtState::Ready);
  TEST_ASSERT_NOT_NULL(a.widgetData(0).artPx);
  // other attribute changes (title, volume): the art does not change, no art redraw signal
  EntityState e = withPic("media_player.a", "playing", "/api/p1");
  e.attrs.emplace_back("media_title", "Next song");
  a.store().update({e});
  TEST_ASSERT_FALSE(a.artSync());
  TEST_ASSERT_TRUE(a.widgetData(0).artState == ArtState::Ready);
  // new track
  a.store().update({withPic("media_player.a", "playing", "/api/p2")});
  TEST_ASSERT_TRUE(a.artSync());
  TEST_ASSERT_NULL(a.widgetData(0).artPx);
  // off: no picture
  a.store().update({withPic("media_player.a", "off", "/api/p2")});
  TEST_ASSERT_TRUE(a.artSync());
  TEST_ASSERT_TRUE(a.widgetData(0).artState == ArtState::None);
}

void test_app_art_tap_opens_popup_and_loads_big() {
  uistate::AppState a;
  a.init(parsed(), P, false);
  a.setMarksDriven(true, 5000);
  a.store().update({withPic("media_player.a", "playing", "/api/p1")});
  a.artSync();
  a.takeArtRequests();
  a.art().deliver("media_player.a", "/api/p1", sampleImage(200));
  const L::Rect r = widgetRect(a, 0);
  TEST_ASSERT_TRUE(a.touch({r.x + 50, r.y + 50}, 0));  // the art itself opens the popup
  TEST_ASSERT_TRUE(a.mediaOpen());
  TEST_ASSERT_TRUE(a.mediaArt());
  TEST_ASSERT_TRUE(a.mediaArtState() == ArtState::Loading);  // 200 px version still to come
  auto rq = a.takeArtRequests();
  TEST_ASSERT_EQUAL(1, (int)rq.size());
  a.art().deliver(rq[0].entity, rq[0].picture, sampleImage(200));
  TEST_ASSERT_TRUE(a.mediaArtState() == ArtState::Ready);
  TEST_ASSERT_NOT_NULL(a.mediaArtPixels());
  // the art is not a touch target in the popup
  const auto l = uistate::mediaLayout(P, true);
  TEST_ASSERT_FALSE(a.touch({l.artRect.x + 100, l.artRect.y + 100}, 10));
  TEST_ASSERT_TRUE(a.mediaOpen());
  a.closeMedia(20);
  TEST_ASSERT_NULL(a.mediaArtPixels());
}

void test_app_art_off_widget_uses_old_popup() {
  uistate::AppState a;
  a.init(parsed(), P, false);
  a.store().update({withPic("media_player.b", "playing", "/api/pb")});
  const L::Rect r = widgetRect(a, 1);
  TEST_ASSERT_TRUE(a.touch({r.x + 100, r.y + 50}, 0));  // text block at x 12 without art
  TEST_ASSERT_TRUE(a.mediaOpen());
  TEST_ASSERT_FALSE(a.mediaArt());
  TEST_ASSERT_EQUAL(0, (int)a.takeArtRequests().size());
}

// ---- demo ----

void test_demo_new_tab_media_with_sample_art() {
  config::Config cfg;
  {
    // The shipped demo.yaml is parsed in test_ui_state; here the two media entities of the New tab.
    config::ParseResult r = config::parseConfig(std::string(
        "version: \"1\"\ntabs:\n  - title: T\n    widgets:\n      - entity: media_player.living_room\n"
        "      - entity: media_player.kitchen\n"));
    TEST_ASSERT_TRUE(r.ok());
    cfg = r.config;
  }
  uistate::AppState a;
  a.init(cfg, P, true);
  const auto d0 = a.widgetData(0), d1 = a.widgetData(1);
  TEST_ASSERT_TRUE(d0.artState == ArtState::Ready);  // sample cover is built in, ready at the first frame
  TEST_ASSERT_NOT_NULL(d0.artPx);
  TEST_ASSERT_EQUAL(80, d0.artPx->w);
  TEST_ASSERT_TRUE(d1.artState == ArtState::None);  // paused kitchen: the no-picture tile
  TEST_ASSERT_EQUAL(0, (int)a.takeArtRequests().size());
  // the popup shows the 200 px sample without any network
  const L::Rect r = widgetRect(a, 0);
  TEST_ASSERT_TRUE(a.touch({r.x + 50, r.y + 50}, 0));
  TEST_ASSERT_TRUE(a.mediaArtState() == ArtState::Ready);
  TEST_ASSERT_EQUAL(200, a.mediaArtPixels()->w);
}

static void test_image_info_and_decode_plan() {
  const uint8_t png[24] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 2, 0, 0, 0, 1, 0x80};
  art::ImageInfo i = art::imageInfo(png, sizeof png);
  TEST_ASSERT_TRUE(i.kind == art::ImageKind::Png);
  TEST_ASSERT_EQUAL(512, i.w);
  TEST_ASSERT_EQUAL(384, i.h);
  const uint8_t jpg[] = {0xFF, 0xD8, 0xFF, 0xE0, 0, 4, 0, 0, 0xFF, 0xC0, 0, 11, 8, 0x01, 0x90, 0x02, 0x58, 3, 0, 0, 0};
  i = art::imageInfo(jpg, sizeof jpg);
  TEST_ASSERT_TRUE(i.kind == art::ImageKind::Jpeg);
  TEST_ASSERT_EQUAL(600, i.w);
  TEST_ASSERT_EQUAL(400, i.h);
  TEST_ASSERT_TRUE(art::imageInfo(jpg, 12).w == 0);  // truncated before the SOF
  const uint8_t junk[] = {1, 2, 3, 4, 5, 6, 7, 8};
  TEST_ASSERT_TRUE(art::imageInfo(junk, sizeof junk).kind == art::ImageKind::Unknown);
  TEST_ASSERT_TRUE(art::imageInfo(junk, 0).kind == art::ImageKind::Unknown);

  art::DecodePlan p = art::planDecode(300, 300);  // 150 < 200: no reduction
  TEST_ASSERT_TRUE(p.ok);
  TEST_ASSERT_EQUAL(0, p.shift);
  p = art::planDecode(1000, 1000);  // 500 >= 200, 250 >= 200, 125 < 200
  TEST_ASSERT_EQUAL(2, p.shift);
  TEST_ASSERT_EQUAL(250, p.outW);
  p = art::planDecode(4096, 4096);
  TEST_ASSERT_EQUAL(3, p.shift);
  TEST_ASSERT_TRUE(p.ok);
  TEST_ASSERT_FALSE(art::planDecode(5000, 100).ok);
  TEST_ASSERT_FALSE(art::planDecode(0, 100).ok);
  TEST_ASSERT_FALSE(art::planDecode(4096, 399).ok);  // thin and long: decoded area over the cap
}

static void test_store_requeue() {
  art::ArtStore s;
  s.want("media_player.a", "/api/x");
  TEST_ASSERT_EQUAL(1, (int)s.takeRequests().size());
  TEST_ASSERT_EQUAL(0, (int)s.takeRequests().size());
  TEST_ASSERT_FALSE(s.requeue("media_player.a", "/api/other"));
  TEST_ASSERT_TRUE(s.requeue("media_player.a", "/api/x"));
  TEST_ASSERT_EQUAL(1, (int)s.takeRequests().size());
}

namespace {
struct ArtTransport : HttpTransport {
  std::vector<std::string> urls;
  std::vector<bool> tokenSent;
  HttpResponse reply;
  HttpResponse request(const std::string &, const std::string &u, const HttpHeaders &h, const std::string &) override {
    urls.push_back(u);
    bool t = false;
    for (const auto &kv : h)
      if (kv.first == "Authorization") t = true;
    tokenSent.push_back(t);
    return reply;
  }
};
app_logic::NetCommand artCmd(const char *pic) {
  app_logic::NetCommand c;
  c.type = app_logic::NetCommand::Type::Art;
  c.artEntity = "media_player.a";
  c.artPicture = pic;
  return c;
}
}  // namespace

static void test_worker_art_relative_and_absolute() {
  ArtTransport t;
  t.reply.status = 200;
  t.reply.body = std::string("\xFF\xD8 jpeg bytes");
  app_logic::HaWorker w(t, "http://ha.local:8123", "", "TOKEN");
  app_logic::NetEvent e = w.run(artCmd("/api/media_player_proxy/media_player.a?token=x"));
  TEST_ASSERT_TRUE(e.type == app_logic::NetEvent::Type::ArtDone);
  TEST_ASSERT_TRUE(e.error == HaError::None);
  TEST_ASSERT_EQUAL_UINT(t.reply.body.size(), e.artBody.size());
  TEST_ASSERT_EQUAL_STRING("media_player.a", e.artEntity.c_str());
  TEST_ASSERT_EQUAL_STRING("http://ha.local:8123/api/media_player_proxy/media_player.a?token=x", t.urls[0].c_str());
  TEST_ASSERT_TRUE(t.tokenSent[0]);
  e = w.run(artCmd("https://cdn.example.com/c.jpg"));  // foreign host: as is, no token
  TEST_ASSERT_TRUE(e.error == HaError::None);
  TEST_ASSERT_EQUAL_STRING("https://cdn.example.com/c.jpg", t.urls[1].c_str());
  TEST_ASSERT_FALSE(t.tokenSent[1]);
  e = w.run(artCmd("http://ha.local:8123/local/c.png"));  // HA itself: token
  TEST_ASSERT_TRUE(t.tokenSent[2]);
}

static void test_worker_art_failures() {
  ArtTransport t;
  app_logic::HaWorker w(t, "http://ha.local:8123", "", "TOKEN");
  t.reply.status = 404;
  TEST_ASSERT_TRUE(w.run(artCmd("/api/x")).error == HaError::Http);
  t.reply.status = 200;
  t.reply.body.assign(art::kMaxArtBytes + 1, 'a');  // over the cap: nothing kept
  app_logic::NetEvent e = w.run(artCmd("/api/x"));
  TEST_ASSERT_TRUE(e.error == HaError::TooLarge);
  TEST_ASSERT_TRUE(e.artBody.empty());
  const size_t n = t.urls.size();
  TEST_ASSERT_TRUE(w.run(artCmd("demo:sample")).error == HaError::Parse);  // never touches the network
  TEST_ASSERT_TRUE(w.run(artCmd("")).error == HaError::Parse);
  TEST_ASSERT_EQUAL_UINT(n, t.urls.size());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_gray4_pack);
  RUN_TEST(test_luma);
  RUN_TEST(test_square_scale_crops_centre);
  RUN_TEST(test_square_scale_averages_and_upsamples);
  RUN_TEST(test_dither_levels_and_mean);
  RUN_TEST(test_sample_image_deterministic_and_varied);
  RUN_TEST(test_plan_fetch);
  RUN_TEST(test_body_cap);
  RUN_TEST(test_image_info_and_decode_plan);
  RUN_TEST(test_store_requeue);
  RUN_TEST(test_worker_art_relative_and_absolute);
  RUN_TEST(test_worker_art_failures);
  RUN_TEST(test_store_lifecycle_and_redraw_rule);
  RUN_TEST(test_store_failure_and_retry);
  RUN_TEST(test_store_popup_big_buffer);
  RUN_TEST(test_store_demo_and_entry_limit);
  RUN_TEST(test_config_art_option);
  RUN_TEST(test_config_art_errors);
  RUN_TEST(test_template_requests_entity_picture);
  RUN_TEST(test_media_picture_rule);
  RUN_TEST(test_widget_layout_numbers);
  RUN_TEST(test_sub_regions_with_art);
  RUN_TEST(test_popup_layout_landscape_art);
  RUN_TEST(test_popup_layout_portrait_art);
  RUN_TEST(test_popup_layout_without_art_is_old);
  RUN_TEST(test_app_art_follows_picture_and_redraws_once);
  RUN_TEST(test_app_art_tap_opens_popup_and_loads_big);
  RUN_TEST(test_app_art_off_widget_uses_old_popup);
  RUN_TEST(test_demo_new_tab_media_with_sample_art);
  return UNITY_END();
}
