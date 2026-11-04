#include "art_glue.h"

#include <M5Unified.h>

#include <deque>

#include "art_fetch.h"
#include "art_image.h"
#include "wakelog.h"

namespace art_glue {

namespace {
bool g_pending = false, g_changed = false;
std::deque<art::ArtRequest> g_queue;  // taken from the store (marked in flight), not yet posted
art::ArtRequest g_asking;

// UNVERIFIED: decode through M5GFX (JPEG: reduces by 1/2/4/8 while decoding; PNG: scaled draw) into a 16-bit
// PSRAM sprite, then luma. The sprite is RGB565 byte-swapped (big endian).
bool decode(const std::string &body, art::Gray8 &out) {
  const uint8_t *data = reinterpret_cast<const uint8_t *>(body.data());
  const art::ImageInfo info = art::imageInfo(data, body.size());
  if (info.kind == art::ImageKind::Unknown) return false;
  const art::DecodePlan plan = art::planDecode(info.w, info.h);
  if (!plan.ok) return false;
  M5Canvas sp(&M5.Display);
  sp.setPsram(true);
  sp.setColorDepth(16);
  if (!sp.createSprite(plan.outW, plan.outH)) return false;
  sp.fillSprite(0xFFFFFFu);  // transparent PNG pixels end up paper
  const float scale = 1.0f / float(1 << plan.shift);
  const bool ok = info.kind == art::ImageKind::Jpeg
                      ? sp.drawJpg(data, uint32_t(body.size()), 0, 0, plan.outW, plan.outH, 0, 0, scale, scale)
                      : sp.drawPng(data, uint32_t(body.size()), 0, 0, plan.outW, plan.outH, 0, 0, scale, scale);
  if (!ok) return false;
  const uint8_t *px = static_cast<const uint8_t *>(sp.getBuffer());
  if (!px) return false;
  art::Gray8 g;
  g.w = plan.outW;
  g.h = plan.outH;
  g.px.resize(size_t(g.w) * size_t(g.h));
  for (size_t i = 0; i < g.px.size(); ++i) g.px[i] = art::luma565(uint16_t((px[2 * i] << 8) | px[2 * i + 1]));
  out = art::squareScale(g, art::kPopupArtPx);  // the store scales this to 80 (and keeps the 200 for the popup)
  return out.valid();
}
}  // namespace

bool pending() { return g_pending; }

bool takeChanged() {
  const bool c = g_changed;
  g_changed = false;
  return c;
}

void newSession(uistate::AppState &app) {
  g_pending = false;
  g_queue.clear();
  if (app.art().retryFailed()) g_changed = true;
}

void pump(app_logic::SessionDriver *drv, uistate::AppState &app, bool graphsBusy) {
  if (!drv || g_pending || graphsBusy) return;
  if (drv->status() != session::Status::Online || !drv->wifiUp() || !drv->sessionDone()) return;
  for (art::ArtRequest &r : app.takeArtRequests()) g_queue.push_back(std::move(r));
  if (g_queue.empty()) return;
  g_asking = g_queue.front();
  g_queue.pop_front();
  g_pending = true;
  app_logic::NetCommand c;
  c.type = app_logic::NetCommand::Type::Art;
  c.artEntity = g_asking.entity;
  c.artPicture = g_asking.picture;
  drv->postArt(c);
}

void onDone(app_logic::NetEvent &&ev, uistate::AppState &app) {
  g_pending = false;
  if (ev.error == HaError::Cancelled) {  // yielded to a user command: ask again later
    if (app.art().fail(g_asking.entity, g_asking.picture)) g_changed = true;  // like Failed: no backoff-free loop, retried at the next session
    wakelog::logf("APP: art dropped (yield), retry at next session\n");
    return;
  }
  const uint32_t t0 = millis();
  bool ok = ev.error == HaError::None;
  art::Gray8 g;
  if (ok) ok = decode(ev.artBody, g);
  const size_t bytes = ev.artBody.size();
  ev.artBody = std::string();  // free the file before the store allocates
  const bool changed = ok ? app.art().deliver(ev.artEntity, ev.artPicture, g) : app.art().fail(ev.artEntity, ev.artPicture);
  if (changed) g_changed = true;
  wakelog::logf("APP: art err=%d ok=%d bytes=%u net_ms=%u decode_ms=%u\n", (int)ev.error, ok ? 1 : 0, (unsigned)bytes,
                (unsigned)ev.ms, (unsigned)(millis() - t0));
}

}  // namespace art_glue
