#pragma once
// Executes one network command with the HA client (blocking; runs on the network task, or inline
// in tests). Never logs the token, bodies or error messages.
#include <string>

#include "ha_client.h"
#include "net_msgs.h"

namespace app_logic {

class HaWorker {
 public:
  HaWorker(HttpTransport &transport, std::string internalUrl, std::string externalUrl,
           std::string token)
      : internal_(internalUrl), client_(transport, std::move(internalUrl), std::move(externalUrl), std::move(token)) {}

  NetEvent run(const NetCommand &cmd);
  const std::string &remembered() const { return client_.remembered_base(); }
  void setRemembered(const std::string &b) { client_.set_remembered(b); }

 private:
  bool isExternal(const std::string &base) const;
  std::string internal_;
  HaClient client_;
};

// Port between the driver and the network side.
class INetPort {
 public:
  virtual ~INetPort() = default;
  virtual void post(const NetCommand &cmd) = 0;  // never blocks
  virtual bool poll(NetEvent &out) = 0;          // never blocks
};

// Runs commands inline on post() (tests, host tools).
class SyncNetPort : public INetPort {
 public:
  explicit SyncNetPort(HaWorker &w) : worker_(w) {}
  void post(const NetCommand &cmd) override { events_.push_back(worker_.run(cmd)); }
  bool poll(NetEvent &out) override {
    if (events_.empty()) return false;
    out = std::move(events_.front());
    events_.erase(events_.begin());
    return true;
  }

 private:
  HaWorker &worker_;
  std::vector<NetEvent> events_;
};

}  // namespace app_logic
