#pragma once
// The network task (ARCHITECTURE.md: all HA network I/O off the UI loop). A FreeRTOS task takes
// commands from a queue, runs them with the HA client over the Arduino transport and posts the
// results to an event queue. post() and poll() never block; a stalled request only stalls this
// task. Results of an older session (before reset()) are dropped.
#include <atomic>
#include <string>

#include "ha_worker.h"
#include "mailbox.h"

class NetTask : public app_logic::INetPort {
 public:
  // Starts the task (once). The URLs and the token are copied; they are never logged.
  void begin(std::string internalUrl, std::string externalUrl, std::string token);
  void post(const app_logic::NetCommand &cmd) override;
  bool poll(app_logic::NetEvent &out) override;
  // Starts a new session: queued commands are dropped and late results of the old one ignored.
  void reset();
  bool running() const { return started_; }
  // Smallest free stack of the task so far, in bytes (0 if not running).
  uint32_t stackFreeBytes() const;

 private:
  struct Cmd { uint32_t epoch = 0; app_logic::NetCommand cmd; };
  struct Evt { uint32_t epoch = 0; app_logic::NetEvent ev; };
  static void entry(void *self);
  void loop();

  app_logic::Mailbox<Cmd> cmds_;
  app_logic::Mailbox<Evt> evts_;
  std::atomic<uint32_t> epoch_{0};
  std::string internal_, external_, token_;
  bool started_ = false;
  void *handle_ = nullptr;
};
