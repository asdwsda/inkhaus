#include "net_task.h"

#include <Arduino.h>
#include <Preferences.h>

#include <cstring>

#include "ha_transport_arduino.h"
#include "net.h"
#include "url_plan.h"

namespace {
// What the device learned per Wi-Fi network (task 047): 1 = internal URL answered, 2 = external.
// Stored as a side, never as a URL; key = "b" + SSID hash (hex), namespace "netbase".
void sideKey(uint32_t ssidHash, char *out) { snprintf(out, 12, "b%08x", (unsigned)ssidHash); }

app_logic::Side loadSide(uint32_t ssidHash) {
  Preferences p;
  if (!p.begin("netbase", true)) return app_logic::Side::None;
  char k[12];
  sideKey(ssidHash, k);
  const int v = p.getUChar(k, 0);
  p.end();
  return v == 1 ? app_logic::Side::Internal : v == 2 ? app_logic::Side::External : app_logic::Side::None;
}

void saveSide(uint32_t ssidHash, app_logic::Side s) {
  Preferences p;
  if (!p.begin("netbase", false)) return;
  char k[12];
  sideKey(ssidHash, k);
  p.putUChar(k, (uint8_t)s);
  p.end();
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
  app_logic::HaWorker worker(transport, internal_, external_, token_);
  // The plan depends on the connected network and the Settings: recomputed when either changes.
  // UNVERIFIED: net::linkInfo() read from this core while the UI core writes it is tear-tolerant
  // (three words; a stale mix only delays the plan to the next command).
  net::LinkInfo planned;
  bool havePlan = false;
  app_logic::UrlPlan plan;
  app_logic::Side stored = app_logic::Side::None;
  for (;;) {
    Cmd c;
    if (!cmds_.pop(c)) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (c.epoch != epoch_.load()) continue;  // belongs to a finished session
    const net::LinkInfo li = net::linkInfo();
    if (c.cmd.type != app_logic::NetCommand::Type::Close &&
        (!havePlan || li.ssidHash != planned.ssidHash || li.haUrl != planned.haUrl || li.haLink != planned.haLink)) {
      stored = li.ssidHash ? loadSide(li.ssidHash) : app_logic::Side::None;
      plan = app_logic::planUrls(toMode(li.haLink), toMode(li.haUrl), stored, !external_.empty());
      worker.applyPlan(plan);
      planned = li;
      havePlan = true;
      Serial.printf("APP: url_plan first=%s exclusive=%d learn=%d\n",
                    plan.first == app_logic::Side::External ? "external" : "internal", plan.exclusive ? 1 : 0,
                    plan.learn ? 1 : 0);
    }
    Evt e;
    e.epoch = c.epoch;
    e.ev = worker.run(c.cmd);
    if (e.ev.type != app_logic::NetEvent::Type::Closed && e.ev.http_status != 0)
      Serial.printf("HTTP: ms=%u reused=%d link=%s\n", (unsigned)e.ev.ms, e.ev.reused ? 1 : 0,
                    e.ev.external ? "remote" : "local");
    if (e.ev.type != app_logic::NetEvent::Type::Closed && e.ev.http_status != 0 && planned.ssidHash) {
      const app_logic::Side answered = e.ev.external ? app_logic::Side::External : app_logic::Side::Internal;
      const app_logic::Side w = app_logic::sideToStore(plan, answered, stored);
      if (w != app_logic::Side::None) {
        saveSide(planned.ssidHash, w);
        stored = w;
      }
    }
    evts_.push(std::move(e));
  }
}

void NetTask::post(const app_logic::NetCommand &cmd) {
  Cmd c;
  c.epoch = epoch_.load();
  c.cmd = cmd;
  cmds_.push(std::move(c));
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
