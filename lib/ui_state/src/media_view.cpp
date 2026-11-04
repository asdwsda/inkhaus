#include "media_view.h"

#include <cmath>
#include <cstdlib>

namespace uistate {

namespace {

Look lookOf(const EntityState *st) {
  if (!st) return Look::Loading;
  return st->available ? Look::Normal : Look::Unavailable;
}

const std::string *nonEmptyAttr(const EntityState &st, const char *name) {
  const std::string *v = st.attr(name);
  return (v && !v->empty()) ? v : nullptr;
}

bool isPlaying(const std::string &s) { return s == "playing" || s == "buffering"; }

}  // namespace

std::string mediaStateWord(const std::string &state) {
  if (state == "playing") return "Playing";
  if (state == "paused") return "Paused";
  if (state == "idle") return "Idle";
  if (state == "on") return "On";
  if (state == "standby") return "Standby";
  if (state == "buffering") return "Buffering";
  if (state == "off") return "Off";
  return humanise(state);  // unknown -> "-", unavailable -> "n/a", anything else capitalised
}

bool mediaHasTitle(const EntityState &st) {
  const bool active = st.state == "playing" || st.state == "paused" || st.state == "buffering" || st.state == "on";
  return active && nonEmptyAttr(st, "media_title");
}

int mediaVolumePct(const EntityState &st) {
  const std::string *v = st.attr("volume_level");
  if (!v) return -1;
  char *end = nullptr;
  const double d = std::strtod(v->c_str(), &end);
  if (end == v->c_str() || !std::isfinite(d)) return -1;
  const double pct = std::round(d * 100.0);
  return pct < 0 ? 0 : pct > 100 ? 100 : int(pct);
}

bool mediaSupportsVolume(const EntityState &st) {
  if (st.supported_features) return (*st.supported_features & kMediaFeatVolumeSet) != 0 && mediaVolumePct(st) >= 0;
  return mediaVolumePct(st) >= 0;
}

bool mediaSupportsMute(const EntityState &st) {
  if (st.supported_features) return (*st.supported_features & kMediaFeatVolumeMute) != 0;
  return st.attr("is_volume_muted") != nullptr;
}

std::string mediaPicture(const EntityState *st) {
  if (!st || !st->available || st->state == "off" || st->state == "unknown") return "";
  const std::string *p = nonEmptyAttr(*st, "entity_picture");
  return p ? *p : "";
}

MediaView mediaView(const EntityState *st, const std::string &name) {
  MediaView v;
  v.look = lookOf(st);
  v.line1 = name;
  if (v.look == Look::Loading) return v;
  if (v.look == Look::Unavailable) {
    v.line2 = "n/a";
    v.skipDotted = v.allDotted = true;
    return v;
  }
  v.line2 = mediaStateWord(st->state);
  if (mediaHasTitle(*st)) {
    v.line1 = *nonEmptyAttr(*st, "media_title");
    if (const std::string *a = nonEmptyAttr(*st, "media_artist")) v.line2 = *a;
  }
  if (isPlaying(st->state)) {
    v.glyph = PlayGlyph::Pause;
    v.playFilled = true;
  } else if (st->state == "off") {
    v.glyph = PlayGlyph::Power;
    v.skipDotted = true;
  }
  return v;
}

MediaPopupView mediaPopupView(const EntityState *st) {
  MediaPopupView v;
  v.look = lookOf(st);
  if (v.look == Look::Loading) return v;
  if (v.look == Look::Unavailable) {
    v.stateWord = "n/a";
    v.skipDotted = true;
    return v;
  }
  v.stateWord = mediaStateWord(st->state);
  v.title = mediaHasTitle(*st) ? *nonEmptyAttr(*st, "media_title") : v.stateWord;
  if (const std::string *a = nonEmptyAttr(*st, "media_artist")) v.artist = *a;
  if (const std::string *a = nonEmptyAttr(*st, "media_album_name")) v.album = *a;
  if (isPlaying(st->state)) {
    v.glyph = PlayGlyph::Pause;
    v.playFilled = true;
  } else if (st->state == "off") {
    v.glyph = PlayGlyph::Power;
    v.skipDotted = true;
  }
  const bool off = st->state == "off";
  v.hasVolume = !off && mediaSupportsVolume(*st);
  v.hasMute = !off && mediaSupportsMute(*st);
  if (v.hasVolume) v.volumePct = mediaVolumePct(*st);
  if (const std::string *m = st->attr("is_volume_muted")) v.muted = *m == "true";
  return v;
}

}  // namespace uistate
