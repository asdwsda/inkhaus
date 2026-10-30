#pragma once
// Executes one network command with the HA client (blocking; runs on the network task, or inline
// in tests). Never logs the token, bodies or error messages.
#include <memory>
#include <string>
#include <vector>

#include "url_plan.h"

#include "ha_client.h"
#include "ws_client.h"
#include "net_msgs.h"

namespace app_logic {

// Opens the byte socket of a WebSocket connection (device: WiFiClient / WiFiClientSecure with the CA bundle).
class WsConnector {
 public:
  virtual ~WsConnector() = default;
  // nullptr when the connection could not be made (TCP / TLS).
  virtual std::unique_ptr<ws::Socket> open(const std::string &host, uint16_t port, bool tls) = 0;
  virtual size_t freeInternalHeap() = 0;
};

// Below this much free internal heap the kept-alive HTTP socket is closed before the TLS handshake of a
// WebSocket (two TLS sessions at once do not fit; ~40 KB each).
constexpr size_t kWsHeapMargin = 70 * 1024;

class HaWorker {
 public:
  HaWorker(HttpTransport &transport, std::string internalUrl, std::string externalUrl,
           std::string token)
      : transport_(transport),
        internal_(internalUrl),
        external_(externalUrl),
        token_(token),
        client_(new HaClient(transport, std::move(internalUrl), std::move(externalUrl), std::move(token))) {}

  NetEvent run(const NetCommand &cmd);
  const std::string &remembered() const { return client_->remembered_base(); }
  void setRemembered(const std::string &b) { client_->set_remembered(b); }
  // Applies a URL plan (task 047): `first` is tried first; exclusive = no fallback to the other URL.
  void applyPlan(const UrlPlan &plan);
  // Without a connector the statistics source answers Network (tests of the history path).
  void setWsConnector(WsConnector *c) { ws_ = c; }
  // Which URL answered last (None before the first success).
  Side answeredSide() const;

 private:
  bool isExternal(const std::string &base) const;
  void runHistory(const NetCommand &cmd, NetEvent &e);
  void runStatistics(const NetCommand &cmd, NetEvent &e);
  std::vector<std::string> wsBases() const;
  WsConnector *ws_ = nullptr;
  bool planExclusive_ = false, planExternalFirst_ = false;
  HttpTransport &transport_;
  std::string internal_, external_, token_;
  std::unique_ptr<HaClient> client_;
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
