#pragma once
// The shared confirm dialog (task 108, docs/ui/widgets-new.md section 7) for lock / garage. Pure model with a
// millisecond clock. Rocker rule STYLE 10b: UP = yes, DOWN = no, PUSH = skip (never push for yes). Skip and No change
// nothing. Skips by itself after 15 s. The geometry is the notifications dialog's (same spec numbers).
#include <cstdint>
#include <string>

#include "notif_popup.h"
#include "settings.h"
#include "ui_layout.h"
#include "ui_rocker.h"

namespace uistate {

constexpr uint32_t kConfirmTimeoutMs = 15000;

using ConfirmLayout = NotifConfirmLayout;
inline ConfirmLayout confirmLayout(settings::Orientation o) { return notifConfirmLayout(o); }

enum class ConfirmAnswer { None, Yes, No, Skip };  // None: the dialog stays open

class ConfirmDialog {
 public:
  // widget / sub: what a Yes runs (the widget of the shown tab and its action sub-region).
  void open(const std::string &title, int widget, int sub, uint32_t now) {
    open_ = true;
    title_ = title;
    widget_ = widget;
    sub_ = sub;
    at_ = now;
  }
  bool isOpen() const { return open_; }
  const std::string &title() const { return title_; }
  int widget() const { return widget_; }
  int sub() const { return sub_; }
  void cancel() { open_ = false; }

  // Closes on any answer except None.
  ConfirmAnswer touch(ui::Point p, settings::Orientation o) {
    if (!open_) return ConfirmAnswer::None;
    const ConfirmLayout l = confirmLayout(o);
    if (l.yes.contains(p)) return finish(ConfirmAnswer::Yes);
    if (l.no.contains(p)) return finish(ConfirmAnswer::No);
    if (l.skip.contains(p) || !l.panel.contains(p)) return finish(ConfirmAnswer::Skip);  // a tap outside = skip
    return ConfirmAnswer::None;
  }
  ConfirmAnswer rocker(ui::PhysicalKey key) {
    if (!open_) return ConfirmAnswer::None;
    if (key == ui::PhysicalKey::Up) return finish(ConfirmAnswer::Yes);
    if (key == ui::PhysicalKey::Down) return finish(ConfirmAnswer::No);
    return finish(ConfirmAnswer::Skip);  // push and long push
  }
  // True when it skipped by itself.
  bool tick(uint32_t now) {
    if (!open_ || int32_t(now - at_) < int32_t(kConfirmTimeoutMs)) return false;
    open_ = false;
    return true;
  }

 private:
  ConfirmAnswer finish(ConfirmAnswer a) {
    open_ = false;
    return a;
  }
  bool open_ = false;
  std::string title_;
  int widget_ = 0, sub_ = 0;
  uint32_t at_ = 0;
};

}  // namespace uistate
