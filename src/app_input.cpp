// loop() input phases. Split from main.cpp (task 124e).
#include "app_internal.h"
#include "app_loop.h"

namespace loopphase {

bool pollInput(uint32_t now, input::Event &e) {
  e = g_scheduled ? input::Event() : input::poll();
  if (!g_scheduled) {
    g_wakeGuard.observe(now, M5.BtnB.isPressed(), M5.Touch.getCount() > 0);
    if (((e.kind == input::Event::Key || e.kind == input::Event::SleepNow) && g_wakeGuard.blocksKey()) ||
        (e.kind == input::Event::Touch && g_wakeGuard.blocksTouch()))
      e = input::Event();  // the press that woke the device
  }
  if (e.kind == input::Event::SleepNow) {  // rocker held 2.5 s: any screen, any popup (task 081)
    manualSleep();
    return false;
  }
  return true;
}

bool keepAwake(uint32_t now) {
  if (g_scheduled) return true;
  // keep awake (tasks 081, 085): sample the battery, keep feeding the idle timers
  if (!g_usbSampled || now - g_usbSampleMs >= 5000) {
    g_usbSampled = true;
    g_usbSampleMs = now;
    power::set_usb_probe(sleepwake::probeUsbRx());  // task 085: re-probe to catch an unplug
    const int32_t mv = batteryMv();
    batterylimit::sample(mv);
    if (power::stay_awake(g_settings, power::usbPresent()) && power::below_low_battery(mv, batteryLimitMv())) {
      lowBatterySleep();  // mid-session guard: stop staying awake (task 081)
      return false;
    }
  }
  const bool usb = power::usbPresent();  // probe (wake + every 5 s), fallback: power-off attempt found the board running
  if (g_stayGate.feed_due(g_settings, usb, now / 1000)) {
    if (g_real && g_drv) {
      if (!g_sessionOver) g_drv->input(now);
    } else {
      g_localIdle.on_input(now / 1000);
    }
  }
  return true;
}

bool popupActive() {
  // activity: brightness closes itself after 10 s; an open history popup only counts while it fetches, then the
  // idle timer closes it (goToSleep)
  return g_app.brightnessOpen() || g_app.mediaOpen() || g_app.alarmOpen() || g_app.confirmOpen() || g_app.historyPopup().fetching();
}

void sessionInput(uint32_t now, const input::Event &e, bool popupOpen) {
  if ((e.kind != input::Event::None || popupOpen) && g_real && g_drv) {
    if (g_sessionOver) startSession(now);  // a new interactive session (real sleep comes later)
    else g_drv->input(now);
  }
}

void dispatchInput(uint32_t now, const input::Event &e, bool &changed) {
  if (settings_flow::active()) {
    if (e.kind == input::Event::Touch) handleSettingsResult(settings_flow::onTouch(e.point, now), now);
    else if (e.kind == input::Event::Key)
      handleSettingsResult(settings_flow::onKey(ui::mapRocker(e.key, g_app.orientation(), false), now), now);
    handleSettingsResult(settings_flow::tick(millis()), now);
  } else if (e.kind == input::Event::LongTouch) {
    const auto d = g_defer.longPress();
    if (d.kind == uistate::DeferredTap::Result::LongPress) changed |= g_app.longPress(d.point, now);
    else if (g_app.longPress(e.point, now)) {  // a sensor / binary widget: the history popup
      historyOpened();
      changed = true;
    }
  } else if (e.kind == input::Event::TouchRelease) {
    const auto d = g_defer.release();  // a short tap on a dimmable light: on/off as usual
    if (d.kind == uistate::DeferredTap::Result::Tap) {
      g_tapNow = now;
      changed |= g_app.touch(d.point, now);
    }
  } else if (e.kind == input::Event::Touch && g_app.modalOpen()) {
    g_tapNow = now;
    changed |= g_app.touch(e.point, now);  // modal: outside closes, controls act
  } else if (e.kind == input::Event::Touch && g_app.screen() == uistate::Screen::Tabs &&
             g_app.dimmableLightAt(e.point)) {
    g_defer.press(e.point, now);
  } else if (e.kind == input::Event::Touch) {
    const ui::Hit hit = g_app.screen() == uistate::Screen::Tabs ? ui::hitTest(g_app.layout(), e.point) : ui::Hit();
    if (hit.kind == ui::HitKind::Settings) {
      openSettings(false, now);
    } else if (hit.kind == ui::HitKind::Refresh) {
      if (g_real && g_drv && !g_drv->fetching() && !g_sessionOver) {
        g_drv->refresh(now);
        changed = true;  // the icon switches to the dot
      }
    } else {
      g_tapNow = now;
      changed |= g_app.touch(e.point, now);
      if (g_app.historyOpen()) historyOpened();  // task 118: a tap on a sensor opened the history
    }
  } else if (e.kind == input::Event::Key) {
    if (e.key == ui::PhysicalKey::LongPush && !g_app.modalOpen() && g_app.screen() == uistate::Screen::Tabs &&
        g_app.focus().mode == ui::FocusMode::Tab) {
      openSettings(true, now);
    } else if (e.key == ui::PhysicalKey::LongPush && !g_app.modalOpen() && g_app.openBrightnessFocused(now)) {
      g_defer.cancel();
      changed = true;
    } else if ((e.key == ui::PhysicalKey::LongPush || e.key == ui::PhysicalKey::Push) && !g_app.modalOpen() &&
               g_app.screen() == uistate::Screen::Tabs && g_app.openHistoryFocused(now)) {
      historyOpened();
      changed = true;
    } else {
      g_tapNow = now;
      changed |= g_app.rocker(e.key, now);
    }
  }
}

}  // namespace loopphase
