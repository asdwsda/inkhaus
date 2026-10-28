#include "net_task.h"

#include <Arduino.h>

#include "ha_transport_arduino.h"

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
  for (;;) {
    Cmd c;
    if (!cmds_.pop(c)) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (c.epoch != epoch_.load()) continue;  // belongs to a finished session
    Evt e;
    e.epoch = c.epoch;
    e.ev = worker.run(c.cmd);
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
