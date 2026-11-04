#pragma once
// Per-player album art state (task 107, docs/ui/widgets-new-media-art.md 1 and 3). Pure.
// Rule: a picture is fetched once per URL; the art is redrawn only when the URL changes (or the player moves
// to/from a no-picture state); a failed fetch is retried only on a new URL or on the next wake.
#include <string>
#include <vector>

#include "art_image.h"

namespace art {

enum class ArtState { None, Loading, Ready, Failed };

// A download the device code should run: fetch `picture` for `entity`, decode, scale to 200 x 200 (Gray8) and
// call ArtStore::deliver. The same download serves the widget (80) and the popup (200).
struct ArtRequest {
  std::string entity;
  std::string picture;
};

constexpr size_t kMaxArtEntries = 8;

class ArtStore {
 public:
  // The picture URL a visible widget shows now ("" = no picture: off, idle without cover, no attribute).
  // Returns true if what the widget draws changed (URL differs: previous picture dropped, Loading or None).
  bool want(const std::string &entity, const std::string &picture);

  ArtState state(const std::string &entity) const;
  const Gray4 *widgetArt(const std::string &entity) const;  // 80 x 80, nullptr unless Ready

  // Popup of `entity` is open ("" = closed, drops the 200 px buffer). Returns true if the popup art changed.
  bool setPopup(const std::string &entity);
  ArtState popupState() const;
  const Gray4 *popupArt() const;  // 200 x 200, nullptr unless Ready for the popup

  // Downloads still to run, each reported once (marked in flight). Includes the popup's 200 px when the widget
  // already holds its 80 px version but the big buffer is missing.
  std::vector<ArtRequest> takeRequests();

  // Result of a download for `picture`; ignored (false) if the entity's URL has moved on meanwhile.
  bool deliver(const std::string &entity, const std::string &picture, const Gray8 &square);
  bool fail(const std::string &entity, const std::string &picture);

  // Next wake: failed entries are tried again (state Loading). True if any changed.
  bool retryFailed();

  // Demo mode: serves every pending "demo:" picture from the built-in sample. True if anything changed.
  bool serveDemo();

  void clear();

 private:
  struct Entry {
    std::string entity, picture;
    ArtState state = ArtState::None;
    bool inFlight = false;
    Gray4 small, big;
  };
  bool needsDownload(const Entry &e) const;
  Entry *find(const std::string &entity);
  const Entry *find(const std::string &entity) const;

  std::vector<Entry> entries_;
  std::string popup_;
};

}  // namespace art
