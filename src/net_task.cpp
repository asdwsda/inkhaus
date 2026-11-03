#include "net_task.h"

#include <Arduino.h>
#include <Preferences.h>

#include <esp_heap_caps.h>

#include <cstring>

#include "ha_transport_arduino.h"
#include "net.h"
#include "session_glue.h"
#include "url_plan.h"
#include "wakelog.h"

namespace {
// What the device learned per Wi-Fi network (task 047): which side answered, when (wall clock and session
// counter, for the re-probe of a learned external). Stored as a side, never as a URL; key = "l" + SSID hash
// (hex), namespace "netbase". The session counter is "sess".
struct LearnedRec {
  uint8_t side;
  int64_t epoch;
  uint32_t session;
};
void sideKey(uint32_t ssidHash, char *out) { snprintf(out, 12, "l%08x", (unsigned)ssidHash); }

app_logic::Learned loadLearned(uint32_t ssidHash) {
  app_logic::Learned l;
  Preferences p;
  if (!p.begin("netbase", true)) return l;
  char k[12];
  sideKey(ssidHash, k);
  LearnedRec r;
  if (p.getBytes(k, &r, sizeof r) == sizeof r)
    l = {r.side == 1 ? app_logic::Side::Internal : r.side == 2 ? app_logic::Side::External : app_logic::Side::None,
         r.epoch, r.session};
  p.end();
  return l;
}

void saveLearned(uint32_t ssidHash, const app_logic::Learned &l) {
  Preferences p;
  if (!p.begin("netbase", false)) return;
  char k[12];
  sideKey(ssidHash, k);
  const LearnedRec r{(uint8_t)l.side, l.epoch, l.session};
  p.putBytes(k, &r, sizeof r);
  p.end();
}

// The counter only matters without a clock (it ages a learned External then), so it is read always but
// bumped and written only while the clock is not plausible: no NVS write per session otherwise.
uint32_t sessionCounter(bool clockValid) {
  Preferences p;
  if (!p.begin("netbase", clockValid)) return 0;
  uint32_t n = p.getUInt("sess", 0);
  if (!clockValid) p.putUInt("sess", ++n);
  p.end();
  return n;
}

app_logic::LinkMode toMode(int v) {  // HaLink and HaUrlMode both: 0 Auto, 1 Internal, 2 External
  return v == 1 ? app_logic::LinkMode::Internal : v == 2 ? app_logic::LinkMode::External : app_logic::LinkMode::Auto;
}
}  // namespace

void NetTask::begin(std::string internalUrl, std::string externalUrl, std::string token) {
  if (started_) return;
  internal_ = std::move(internalUrl);
  external_ = std::move(externalUrl);
  token_ = std::move(token);
  // TLS handshakes need a big stack. Core 0 shares with the Wi-Fi stack; the UI loop runs on core 1.
  TaskHandle_t h = nullptr;
  started_ = xTaskCreatePinnedToCore(&NetTask::entry, "net", 20480, this, 1, &h, 0) == pdPASS;
  handle_ = h;
  Serial.printf("APP: net_task started=%d\n", started_ ? 1 : 0);
}

uint32_t NetTask::stackFreeBytes() const {
  return handle_ ? (uint32_t)uxTaskGetStackHighWaterMark((TaskHandle_t)handle_) * sizeof(StackType_t) : 0;
}

void NetTask::entry(void *self) { static_cast<NetTask *>(self)->loop(); }

void NetTask::loop() {
  ArduinoHttpTransport transport(4000, 8000);
  ArduinoWsConnector wsConnector;
  app_logic::HaWorker worker(transport, internal_, external_, token_);
  worker.setWsConnector(&wsConnector);
  worker.setYieldFlag(&yield_);
  // The plan depends on the connected network and the Settings: recomputed when either changes.
  // net::linkInfo() is a consistent snapshot (published under a spinlock in net.cpp).
  net::LinkInfo planned;
  bool havePlan = false;
  app_logic::UrlPlan plan;
  app_logic::Learned stored;
  bool stale = false;
  uint32_t plannedEpoch = 0;
  uint32_t sessionNo = 0;
  for (;;) {
    Cmd c;
    if (!cmds_.pop(c)) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (c.epoch != epoch_.load()) continue;  // belongs to a finished session
    const net::LinkInfo li = net::linkInfo();
    const bool newSession = c.epoch != plannedEpoch;
    if (c.cmd.type != app_logic::NetCommand::Type::Close &&
        (!havePlan || newSession || li.ssidHash != planned.ssidHash || li.haUrl != planned.haUrl || li.haLink != planned.haLink)) {
      const int64_t nowEpoch = session_glue::wallClock();
      if (newSession) {  // UNVERIFIED: one NVS write per session, only while the clock is not plausible
        sessionNo = sessionCounter(nowEpoch != 0);
        plannedEpoch = c.epoch;
      }
      stored = li.ssidHash ? loadLearned(li.ssidHash) : app_logic::Learned();
      stale = app_logic::learnedStale(stored, nowEpoch, sessionNo);
      plan = app_logic::planUrls(toMode(li.haLink), toMode(li.haUrl),
                                 app_logic::effectiveRemembered(stored, nowEpoch, sessionNo), !external_.empty());
      worker.applyPlan(plan);
      planned = li;
      havePlan = true;
      Serial.printf("APP: url_plan first=%s exclusive=%d learn=%d\n",
                    plan.first == app_logic::Side::External ? "external" : "internal", plan.exclusive ? 1 : 0,
                    plan.learn ? 1 : 0);
    }
    const bool background = app_logic::laneOf(c.cmd) == app_logic::Lane::Background;
    const uint32_t startMs = millis();
    // UNVERIFIED (task 116): per-command queue wait; user commands are the ones the tap budget cares about.
    if (!background || c.requeues == 0)
      Serial.printf("APP: timing queue_wait_ms=%u lane=%s type=%d\n", (unsigned)(startMs - c.enqueuedMs),
                    background ? "bg" : "user", (int)c.cmd.type);
    yield_ = false;
    if (background && cmds_.userWaiting()) yield_ = true;  // post() sets it too when a user command arrives later
    Evt e;
    e.epoch = c.epoch;
    e.ev = worker.run(c.cmd);
    if (background && e.ev.error == HaError::Cancelled && yield_ &&
        !(c.cmd.histCancel && *c.cmd.histCancel)) {  // aborted for a user command, not by the UI
      const app_logic::YieldPolicy pol = app_logic::yieldPolicyOf(c.cmd.type);
      Serial.printf("APP: net_yield type=%d policy=%s requeues=%u\n", (int)c.cmd.type,
                    pol == app_logic::YieldPolicy::Requeue && c.requeues < 3 ? "requeue" : "drop", (unsigned)c.requeues);
      if (pol == app_logic::YieldPolicy::Requeue && c.requeues < 3) {
        ++c.requeues;
        cmds_.requeueFront(std::move(c));  // the user lane goes first at the next pop
        continue;
      }
      // dropped: the Cancelled event below tells the UI the job is over
    }
    if (e.ev.type == app_logic::NetEvent::Type::HistoryDone) {
      e.ev.heapMin = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
      Serial.printf("HIST: src=%s fetch_ms=%u parse_ms=%u bytes=%u points=%u heap_min=%u err=%d%s\n",
                    e.ev.stats ? "stats" : "history", (unsigned)e.ev.fetchMs, (unsigned)e.ev.parseMs,
                    (unsigned)e.ev.bytes, (unsigned)e.ev.points, (unsigned)e.ev.heapMin, (int)e.ev.error,
                    e.ev.noStatistics ? " none=1" : "");
      wakelog::historyFetched(e.ev.fetchMs, e.ev.parseMs, e.ev.bytes);
    }
    if (e.ev.type != app_logic::NetEvent::Type::Closed && e.ev.http_status != 0)
      Serial.printf("HTTP: ms=%u reused=%d link=%s\n", (unsigned)e.ev.ms, e.ev.reused ? 1 : 0,
                    e.ev.external ? "remote" : "local");
    if (e.ev.type != app_logic::NetEvent::Type::Closed && e.ev.http_status != 0 && planned.ssidHash) {
      const app_logic::Side answered = e.ev.external ? app_logic::Side::External : app_logic::Side::Internal;
      if (app_logic::shouldStoreLearned(plan, answered, stored, stale)) {
        stored = {answered, session_glue::wallClock(), sessionNo};
        stale = false;
        saveLearned(planned.ssidHash, stored);
      }
    }
    evts_.push(std::move(e));
  }
}

void NetTask::post(const app_logic::NetCommand &cmd) {
  Cmd c;
  c.epoch = epoch_.load();
  c.enqueuedMs = millis();
  c.cmd = cmd;
  const app_logic::Lane lane = app_logic::laneOf(cmd);
  cmds_.push(lane, std::move(c));
  if (lane == app_logic::Lane::User) yield_ = true;
}

bool NetTask::poll(app_logic::NetEvent &out) {
  Evt e;
  while (evts_.pop(e)) {
    if (e.epoch != epoch_.load()) continue;
    out = std::move(e.ev);
    return true;
  }
  return false;
}

void NetTask::reset() { epoch_.fetch_add(1); }
