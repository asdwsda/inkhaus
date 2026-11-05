// Internal header of the app glue (task 124e): the state and helpers shared by main.cpp and the
// app_*.cpp files that main.cpp was split into. Not a public API; nothing outside src/ includes it.
#pragma once

#include <M5Unified.h>
#include <time.h>
#include <map>
#include <memory>
#include <vector>
#include "app_config.h"
#include "build_info.h"
#include "demo_mode.h"
#include "app_state.h"
#include "ui_layout.h"
#include "config_sd.h"
#include "boot.h"
#include "battery_learn.h"
#include "battery_limit.h"
#include "brightness_intent.h"
#include "media_intent.h"
#include "secure_intent.h"  // task 108
#include "history_cache_sd.h"
#include "deferred_tap.h"
#include "forecast_glue.h"
#include "calendar_glue.h"  // task 105
#include "art_glue.h"       // task 107
#include "demo_history.h"
#include "input.h"
#include "net.h"
#include "net_task.h"
#include "ota.h"
#include "present.h"
#include "render/screen.h"
#include "render/settings_screen.h"
#include "render/sleep_screen_draw.h"
#include "render/splash_screen.h"
#include "report_glue.h"
#include "session_driver.h"
#include "session_glue.h"
#include "settings.h"
#include "settings_flow.h"
#include "settings_nvs.h"
#include "sleep.h"
#include "state_cache.h"
#include "sleep_logic.h"
#include "sleep_screen.h"
#include "shot.h"
#include "tap_intent.h"
#include "version.h"
#include "wakelog.h"
#include "wall_time.h"
#include "ready_state.h"  // task 120
#include <esp_heap_caps.h>

// Seconds a network change waits for a user action still in flight before the session restarts.
inline constexpr uint32_t kNetRestartWaitMs = 10000;  // above the HTTP timeouts (4 s connect, 8 s total)

void logMem(const char *tag);
bool timedShow(present::Mode mode, uint32_t now);
extern uistate::AppState g_app;
extern render::StatusInfo g_status;
extern settings::Settings g_settings;
extern app_config::Loaded g_cfg;
void dropCfgTabs();
extern render::SettingsInfo g_info;
extern bool g_real;
extern bool g_demoMode;
extern NetTask g_net;
extern std::unique_ptr<session_glue::ArduinoWifi> g_wifi;
extern std::unique_ptr<app_logic::SessionDriver> g_drv;
extern power::IdleTimer g_localIdle;
extern bool g_localIdleOn;
extern uint32_t g_lastInputMs;
extern power::StayAwakeGate g_stayGate;
extern uint32_t g_usbSampleMs;
extern bool g_usbSampled;
extern bool g_sessionOver;
extern bool g_settingsRefreshing;
extern uint32_t g_refreshSeq;
extern bool g_stackLogged;
extern bool g_firstSession;
extern int64_t g_restoredEpoch;
extern power::WakeReason g_wake;
extern bool g_scheduled;
extern app_logic::WakeInputGuard g_wakeGuard;
void applyTimezone(const std::string &tz);
int32_t utcOffsetAt(int64_t epoch);
void appLog(const std::string &line);
int batteryPercent();
int32_t batteryMv();
int32_t batteryLimitMv();
bool landscape();
extern bool g_keepStatus;
void updateStatus();
void composeFrame();
void drawSettingsFrame(present::Mode mode, uint32_t now);
void logFetchApplied();
void showFrame(uint32_t now);
extern std::shared_ptr<volatile bool> g_histCancel;
extern bool g_histWaitNet;
extern bool g_histChanged;
void onHistoryDone(app_logic::NetEvent &&ev);
void historyOpened();
void historyPump();
void historyShutdown();
extern uint32_t g_graphBatchMs;
void graphsInit();
extern bool g_notifAsked, g_notifPending, g_notifValid, g_notifWanted;
extern int g_notifCount;
extern bool g_notifChanged;
extern std::string g_notifTitle;
void notificationsPump();
void notifDismissSink(const std::string &id);
void notificationsDone(app_logic::NetEvent &&ev);
void graphsOnFetch(bool ok);
bool graphsBusy();
void graphsPump();
extern power::WakeReason g_sessionWake;
extern bool g_netRestartDue;
extern uint32_t g_netRestartSince;
std::vector<std::string> profileNames();
void startSession(uint32_t now, bool restart = false);
extern uint32_t g_tapNow;
void onAppAction(int tab, int widget, int sub, const std::vector<EntityState> &requested);
void onBrightnessSend(const std::string &id, int pct);
void onMediaSend(const std::string &id, uistate::MediaSend what, int value);
void onAlarmSend(const std::string &id, AlarmCmd cmd, const std::string &code);
extern uistate::DeferredTap g_defer;
bool syncMarks();
void snapshotInfo();
void otaQuiesce();
bool actionPending();
void openSettings(bool ring, uint32_t now);
void handleSettingsResult(const settings_flow::Result &r, uint32_t now);
void goToSleep(power::SleepPlan plan, power::Session session, bool localIdle = false);
void manualSleep();
bool batteryTooLowForWifi();
void lowBatterySleep();
