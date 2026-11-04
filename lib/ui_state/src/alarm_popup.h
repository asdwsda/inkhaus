#pragma once
// Alarm keypad popup (task 108, docs/ui/widgets-new.md 5.1): geometry, hit testing and the model (code buffer, focus,
// messages). The code is digits only (max 8), lives in RAM only and is cleared on send, close, a wrong-code reply and
// when the popup idles shut (UNVERIFIED: the spec has no idle time; keeps the device able to sleep, like media). Pure
// logic with a millisecond clock; the owner (AppState) sends the services.
#include <cstdint>
#include <string>
#include <vector>

#include "secure_view.h"
#include "service_call.h"
#include "settings.h"
#include "ui_layout.h"
#include "ui_rocker.h"

namespace uistate {

constexpr int kAlarmMaxCode = 8;
constexpr uint32_t kAlarmIdleCloseMs = 30000;

struct AlarmLayout {
  bool landscape = true;
  ui::Rect panel, shadow, dirty;
  ui::Rect close, codeBox;
  ui::Rect keys[12];   // 1 2 3 / 4 5 6 / 7 8 9 / backspace 0 C
  ui::Rect modes[4];   // Disarm, Home, Away, Night
  ui::Rect keysArea;   // union of the keys (the code box and keys redraw rect for a key press: codeBox only)
  int nameX = 0, nameCy = 0;
  int stateX = 0, stateCy = 0, stateBaseline = 0;
  int codeTextX = 0, codeCy = 0;
  int msgX = 0, msgCy = 0;
  int hintX = 0, hint1Cy = 0, hint2Cy = 0;
};
AlarmLayout alarmLayout(settings::Orientation o);

// Key index -> label. 0..8 = "1".."9", 9 = backspace, 10 = "0", 11 = "C".
constexpr int kKeyBackspace = 9, kKeyZero = 10, kKeyClear = 11;
constexpr int kStopKeys = 12, kStopModes = 4;  // rocker walk: keys 0..11, modes 12..15, close 16
constexpr int kStopClose = 16;
constexpr int kEntryFocus = 4;  // key "5"

enum class AlarmMsg { EnterCode, CodeEntered, WrongCode, Failed, Sent, TextUnsupported };
const char *alarmMsgText(AlarmMsg m);

enum class AlarmHitKind { Outside, Panel, Key, Mode, Close, CodeBox };
struct AlarmHit {
  AlarmHitKind kind = AlarmHitKind::Outside;
  int index = 0;  // key / mode index
};
AlarmHit alarmHitTest(const AlarmLayout &l, ui::Point p);

struct AlarmResult {
  bool changed = false;
  bool closed = false;
  bool send = false;  // the owner sends `cmd` with `code` (the model has already cleared its buffer)
  AlarmCmd cmd = AlarmCmd::Disarm;
  std::string code;   // never log
  int mode = 0;       // index of the pressed mode button
};

class AlarmPopup {
 public:
  // viaRocker: the focus ring shows, starting on key "5"; by touch no ring.
  void open(const std::string &entityId, int tab, int widget, bool viaRocker, uint32_t now);
  bool isOpen() const { return open_; }
  const std::string &entityId() const { return id_; }
  int tab() const { return tab_; }
  int widget() const { return widget_; }
  // Clears the code and closes (idle, sleep, panel unavailable).
  void close();

  AlarmResult touch(ui::Point p, settings::Orientation o, const AlarmPanelInfo &info, uint32_t now);
  AlarmResult rocker(ui::PhysicalKey key, settings::Orientation o, const AlarmPanelInfo &info, uint32_t now);
  bool tick(uint32_t now);  // idle close; true if it closed

  // The owner's feedback for the last send.
  void sendFailed(bool codeWasSent);  // service error or unchanged state: "Wrong code" / "Failed", buttons usable again
  void sendConfirmed();               // the state changed: message "Sent", awaiting ends
  int awaitingMode() const { return awaiting_; }  // pressed mode button (dashed border + hourglass), -1 none

  int codeLen() const { return int(code_.size()); }  // the dots; the code itself stays private
  bool ringVisible() const { return ring_; }
  int focus() const { return focus_; }  // stop index (see kStop*), valid while ringVisible()
  AlarmMsg message(const AlarmPanelInfo &info) const;
  bool keysActive(const AlarmPanelInfo &info) const { return info.codeFormat != CodeFormat::Text; }
  bool modeActive(int mode, const AlarmPanelInfo &info) const;
  // Focus stops in walk order (inert controls are skipped).
  std::vector<int> stops(const AlarmPanelInfo &info) const;

 private:
  AlarmResult press(int mode, const AlarmPanelInfo &info);
  bool keyPress(int key, const AlarmPanelInfo &info);
  void clearCode() { code_.clear(); }

  bool open_ = false, ring_ = false;
  std::string id_, code_;
  int tab_ = 0, widget_ = 0, focus_ = kEntryFocus, awaiting_ = -1;
  bool hasOverride_ = false;
  AlarmMsg override_ = AlarmMsg::EnterCode;
  uint32_t lastInput_ = 0;
};

}  // namespace uistate
