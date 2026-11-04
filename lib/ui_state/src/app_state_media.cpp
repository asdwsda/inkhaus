// AppState: the media popup (task 106, docs/ui/widgets-new.md 4.1). Kept apart from app_state.cpp.
#include <algorithm>
#include <cmath>

#include "app_state.h"
#include "fake_actions.h"

namespace uistate {

using config::WidgetType;

namespace {

void setAttr(EntityState &s, const std::string &name, const std::string &value) {
  for (auto &kv : s.attrs)
    if (kv.first == name) {
      kv.second = value;
      return;
    }
  s.attrs.emplace_back(name, value);
}

}  // namespace

bool AppState::openMedia(int idx, uint32_t now) {
  if (screen_ != Screen::Tabs || modalOpen()) return false;
  const config::Widget *w = widgetAt(shownTab_, idx);
  if (!w || w->type != WidgetType::MediaPlayer || w->entities.empty()) return false;
  const EntityState *st = store_.get(w->entities[0]);
  if (st && !st->available) return false;  // unavailable: the popup does not open
  maAwaiting_ = maFailed_ = maSawPending_ = false;
  const int vol = st ? mediaVolumePct(*st) : -1;
  const std::string *m = st ? st->attr("is_volume_muted") : nullptr;
  media_.open(w->entities[0], shownTab_, idx, vol, m && *m == "true", now);
  if (w->art) art_.setPopup(w->entities[0]);  // task 107: the 200 px version, from the same download if possible
  artSync();
  return true;
}

bool AppState::mediaArt() const {
  const config::Widget *w = media_.isOpen() ? widgetAt(media_.tab(), media_.widget()) : nullptr;
  return w && w->art;
}

bool AppState::artSync() {
  if (screen_ != Screen::Tabs || shownTab_ < 0 || size_t(shownTab_) >= cfg_.tabs.size()) return false;
  bool changed = false;
  for (const config::Widget &w : cfg_.tabs[size_t(shownTab_)].widgets) {
    if (w.type != WidgetType::MediaPlayer || !w.art || w.entities.empty()) continue;
    changed |= art_.want(w.entities[0], mediaPicture(store_.get(w.entities[0])));
  }
  if (media_.isOpen()) {  // the open popup follows its player's picture too (a hidden tab's widget)
    const config::Widget *w = widgetAt(media_.tab(), media_.widget());
    if (w && w->art && !w->entities.empty()) changed |= art_.want(w->entities[0], mediaPicture(store_.get(w->entities[0])));
  }
  changed |= art_.serveDemo();
  return changed;
}

bool AppState::closeMedia(uint32_t now) {
  const bool was = media_.isOpen();
  art_.setPopup("");  // drops the 200 px buffer
  media_.done(now);
  if (maFailed_) media_.clearOptimistic();  // the real state shows again
  maFailed_ = false;
  return was;
}

MediaPopupView AppState::mediaPopupData() const {
  MediaPopupView v = mediaPopupView(store_.get(media_.entityId()));
  const int vo = media_.volumeOverride();
  if (vo >= 0 && v.hasVolume) v.volumePct = vo;
  if (media_.muteOverridden() && v.hasMute) v.muted = media_.mutedOverride();
  return v;
}

bool AppState::mediaTouch(ui::Point p, uint32_t now) {
  const MediaLayout l = mediaLayout(orientation_, mediaArt());
  const MediaPopupView v = mediaPopupData();
  bool changed = false;
  switch (mediaHitTest(l, p)) {
    case MediaHit::Outside:
    case MediaHit::Close: return closeMedia(now);
    case MediaHit::Panel: media_.poke(now); return false;
    case MediaHit::Prev: media_.poke(now); return !v.skipDotted && runAction(media_.widget(), kMediaSubPrev, now);
    case MediaHit::Play: media_.poke(now); return v.look != Look::Unavailable && runAction(media_.widget(), kMediaSubPlay, now);
    case MediaHit::Next: media_.poke(now); return !v.skipDotted && runAction(media_.widget(), kMediaSubNext, now);
    case MediaHit::VolMinus: if (v.hasVolume) changed = media_.step(-1, now); else media_.poke(now); break;
    case MediaHit::VolPlus: if (v.hasVolume) changed = media_.step(+1, now); else media_.poke(now); break;
    case MediaHit::Bar:
      if (v.hasVolume) changed = media_.tapBar(mediaBarFraction(l, p.x), now);
      else media_.poke(now);
      break;
    case MediaHit::Mute: if (v.hasMute) changed = media_.toggleMute(now); else media_.poke(now); break;
  }
  if (changed) maFailed_ = false;
  return changed;
}

bool AppState::mediaRocker(ui::PhysicalKey key, uint32_t now) {
  switch (key) {
    case ui::PhysicalKey::Up:
    case ui::PhysicalKey::Down: {
      // Same direction rule as the brightness popup: portrait Up = more; landscape BtnA points left,
      // so the key pointing right (Down) = more.
      const bool prev = ui::mapRocker(key, orientation_, false) == ui::LogicalKey::Prev;
      const bool more = orientation_ == settings::Orientation::Landscape ? !prev : prev;
      if (!mediaPopupData().hasVolume) {
        media_.poke(now);
        return false;
      }
      const bool changed = media_.step(more ? +1 : -1, now);
      if (changed) maFailed_ = false;
      return changed;
    }
    case ui::PhysicalKey::Push: {
      media_.poke(now);
      const MediaPopupView v = mediaPopupData();
      return v.look != Look::Unavailable && runAction(media_.widget(), kMediaSubPlay, now);
    }
    case ui::PhysicalKey::LongPush: return closeMedia(now);
  }
  return false;
}

bool AppState::mediaTick(uint32_t now) {
  bool changed = false;
  if (media_.isOpen()) {
    const EntityState *s = store_.get(media_.entityId());
    if (s && !s->available) {  // the player became unavailable: close without sending
      media_.cancel();
      maFailed_ = false;
      changed = true;
    } else if (media_.tick(now)) {
      closeMedia(now);  // already closed: clears the flags
      changed = true;
    }
  }
  if (marksDriven_ && maAwaiting_ && int32_t(now - maSentAt_) >= int32_t(safetyMs_)) {
    maAwaiting_ = false;  // the driver never confirmed: show the real state again
    media_.clearOptimistic();
    changed = true;
  }
  return changed;
}

void AppState::onMediaSend(const std::string &id, MediaSend what, int value) {
  maAwaiting_ = true;
  maSawPending_ = false;
  maFailed_ = false;
  maSentAt_ = lastNow_;
  if (marksDriven_) {
    if (msink_) msink_(id, what, value);
    return;
  }
  // Demo: apply to the local store at once.
  if (const EntityState *cur = store_.get(id)) {
    EntityState s = *cur;
    if (what == MediaSend::Volume) setAttr(s, "volume_level", fmtNumber(value / 100.0, 2));
    else setAttr(s, "is_volume_muted", value ? "true" : "false");
    store_.update({s});
  }
  maAwaiting_ = false;
  media_.clearOptimistic();
}

// UNVERIFIED (device): like the brightness popup, the session marks the player Pending right after the send
// and clears (or Fails) it when the confirmation arrives. Failed shows the alert until the next change / close.
void AppState::mediaMarks(bool &changed) {
  if (!maAwaiting_ || media_.sendPending()) return;
  auto it = marks_.find(media_.entityId());
  const Mark m = it == marks_.end() ? Mark::None : it->second;
  if (m == Mark::Pending) {
    maSawPending_ = true;
  } else if (m == Mark::Failed) {
    maAwaiting_ = false;
    maFailed_ = media_.isOpen();
    media_.clearOptimistic();
    changed = true;
  } else if (maSawPending_) {
    maAwaiting_ = false;
    maSawPending_ = false;
    media_.clearOptimistic();
    changed = true;
  }
}

}  // namespace uistate
