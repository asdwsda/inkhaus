#pragma once
// Media player widget and popup view models (task 106, docs/ui/widgets-new.md section 4).
// Pure text/flag logic, no drawing and no Arduino. Strings are UTF-8.
#include <string>

#include "entity_state.h"
#include "widget_view.h"

namespace uistate {

// media_player supported_features bits (HA MediaPlayerEntityFeature).
constexpr int kMediaFeatVolumeSet = 4;
constexpr int kMediaFeatVolumeMute = 8;

enum class PlayGlyph { Play, Pause, Power };

struct MediaView {
  Look look = Look::Loading;
  std::string line1;        // media_title, else the widget name
  std::string line2 = "-";  // media_artist, else the state word
  PlayGlyph glyph = PlayGlyph::Play;
  bool playFilled = false;  // playing / buffering: INK-filled pause button
  bool skipDotted = false;  // prev / next inert (off, unavailable)
  bool allDotted = false;   // unavailable: all three buttons dotted
};
// name: the widget title (config name or friendly_name). st: nullptr = loading.
MediaView mediaView(const EntityState *st, const std::string &name);

// State word of the player: "Playing", "Paused", "Idle", "On", "Standby", "Buffering", "Off", "-", "n/a".
std::string mediaStateWord(const std::string &state);
// The media_title is only used while the player is active (playing, paused, buffering, on).
bool mediaHasTitle(const EntityState &st);

struct MediaPopupView {
  Look look = Look::Loading;
  std::string title = "-";  // F40; "-" while loading
  std::string artist, album;
  std::string stateWord = "-";
  PlayGlyph glyph = PlayGlyph::Play;
  bool playFilled = false;
  bool skipDotted = false;
  bool hasVolume = false, hasMute = false;  // controls usable (dotted when false)
  int volumePct = 0;                        // 0..100 (valid when hasVolume)
  bool muted = false;
};
MediaPopupView mediaPopupView(const EntityState *st);

// Volume of the player in percent (0..100), -1 if it reports none.
int mediaVolumePct(const EntityState &st);
bool mediaSupportsVolume(const EntityState &st);
bool mediaSupportsMute(const EntityState &st);

}  // namespace uistate
