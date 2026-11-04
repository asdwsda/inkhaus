#include "art_store.h"

#include <algorithm>

#include "art_fetch.h"

namespace art {

ArtStore::Entry *ArtStore::find(const std::string &entity) {
  for (Entry &e : entries_)
    if (e.entity == entity) return &e;
  return nullptr;
}

const ArtStore::Entry *ArtStore::find(const std::string &entity) const {
  for (const Entry &e : entries_)
    if (e.entity == entity) return &e;
  return nullptr;
}

bool ArtStore::want(const std::string &entity, const std::string &picture) {
  Entry *e = find(entity);
  if (!e) {
    if (picture.empty()) return false;  // nothing known, nothing shown: no entry needed
    if (entries_.size() >= kMaxArtEntries) entries_.erase(entries_.begin());
    entries_.push_back(Entry{entity, "", ArtState::None, false, {}, {}});
    e = &entries_.back();
  }
  if (e->picture == picture) return false;
  const bool hadArt = e->state != ArtState::None;
  e->picture = picture;
  e->inFlight = false;
  e->small = Gray4();  // the previous cover is never kept: a wrong cover is worse than none
  e->big = Gray4();
  e->state = picture.empty() ? ArtState::None : ArtState::Loading;
  return hadArt || !picture.empty();
}

ArtState ArtStore::state(const std::string &entity) const {
  const Entry *e = find(entity);
  return e ? e->state : ArtState::None;
}

const Gray4 *ArtStore::widgetArt(const std::string &entity) const {
  const Entry *e = find(entity);
  return e && e->state == ArtState::Ready && e->small.valid() ? &e->small : nullptr;
}

bool ArtStore::setPopup(const std::string &entity) {
  if (popup_ == entity) return false;
  popup_ = entity;
  for (Entry &e : entries_)
    if (e.entity != entity) e.big = Gray4();  // only the open popup keeps the 20 KB buffer
  return true;
}

ArtState ArtStore::popupState() const {
  const Entry *e = find(popup_);
  if (!e) return ArtState::None;
  if (e->state == ArtState::Ready && !e->big.valid()) return ArtState::Loading;  // 200 px still to come
  return e->state;
}

const Gray4 *ArtStore::popupArt() const {
  const Entry *e = find(popup_);
  return e && e->state == ArtState::Ready && e->big.valid() ? &e->big : nullptr;
}

bool ArtStore::needsDownload(const Entry &e) const {
  if (e.inFlight || e.picture.empty()) return false;
  const bool needSmall = e.state == ArtState::Loading;
  const bool needBig = e.state == ArtState::Ready && e.entity == popup_ && !e.big.valid();
  return needSmall || needBig;
}

std::vector<ArtRequest> ArtStore::takeRequests() {
  std::vector<ArtRequest> out;
  for (Entry &e : entries_) {
    if (!needsDownload(e)) continue;
    e.inFlight = true;
    out.push_back({e.entity, e.picture});
  }
  return out;
}

bool ArtStore::deliver(const std::string &entity, const std::string &picture, const Gray8 &square) {
  Entry *e = find(entity);
  if (!e || e->picture != picture) return false;  // the URL moved on: stale answer
  e->inFlight = false;
  if (!square.valid()) return fail(entity, picture);
  e->small = dither16(squareScale(square, kWidgetArtPx));
  if (e->entity == popup_) e->big = dither16(squareScale(square, kPopupArtPx));
  e->state = ArtState::Ready;
  return true;
}

bool ArtStore::fail(const std::string &entity, const std::string &picture) {
  Entry *e = find(entity);
  if (!e || e->picture != picture) return false;
  e->inFlight = false;
  e->small = Gray4();
  e->big = Gray4();
  e->state = ArtState::Failed;
  return true;
}

bool ArtStore::retryFailed() {
  bool any = false;
  for (Entry &e : entries_)
    if (e.state == ArtState::Failed) {
      e.state = ArtState::Loading;
      e.inFlight = false;
      any = true;
    }
  return any;
}

bool ArtStore::serveDemo() {
  bool any = false;
  std::vector<ArtRequest> demo;
  for (const Entry &e : entries_)
    if (needsDownload(e) && isDemoPicture(e.picture)) demo.push_back({e.entity, e.picture});
  for (const ArtRequest &r : demo) any |= deliver(r.entity, r.picture, sampleImage(kPopupArtPx, 0));
  return any;
}

void ArtStore::clear() {
  entries_.clear();
  popup_.clear();
}

}  // namespace art
