// Test-only TLS check (task 008). Compiled only with -DTLS_CHECK -DTLS_CHECK_URL=\"https://...\".
// Not wired into main.cpp. Wiring (one line, in setup() after boot, only under the flag):
//   #ifdef TLS_CHECK
//   tls_check::run();   // declared via: namespace tls_check { void run(); }
//   #endif
// Joins Wi-Fi from the SD config, does a GET and logs TLSCHK: status/error/ms. A verified
// handshake against ha.example.org gives status=401; an invalid certificate gives status=0 and an
// error text (the transport never falls back to an insecure connection).
#ifdef TLS_CHECK

#include <Arduino.h>

#include "config_sd.h"
#include "ha_transport_arduino.h"
#include "net.h"

#ifndef TLS_CHECK_URL
#error "TLS_CHECK needs -DTLS_CHECK_URL=\"https://host/path\""
#endif

namespace tls_check {

void run() {
  auto loaded = config_sd::load();
  Serial.printf("TLSCHK: sd=%s\n", config_sd::statusText(loaded.status));
  if (loaded.status != config_sd::Status::Ok || !loaded.parse.ok()) {
    Serial.println("TLSCHK: result=no_config");
    return;
  }
  net::begin(loaded.parse.config.connection.wifi);
  const uint32_t t0 = millis();
  while (net::poll() == net::State::Connecting && millis() - t0 < 20000) delay(50);
  if (net::poll() != net::State::Connected) {
    Serial.println("TLSCHK: result=no_wifi");
    return;
  }
  Serial.printf("TLSCHK: wifi_ms=%u rssi=%d\n", (unsigned)net::connectMs(), net::rssi());

  ArduinoHttpTransport transport(8000, 10000);
  const uint32_t t1 = millis();
  HttpResponse r = transport.request("GET", TLS_CHECK_URL, {}, "");
  Serial.printf("TLSCHK: url=%s status=%d ms=%u error=%s body_bytes=%u\n", TLS_CHECK_URL, r.status,
                (unsigned)(millis() - t1), r.error.c_str(), (unsigned)r.body.size());
  Serial.printf("TLSCHK: verified=%s\n", r.status > 0 ? "yes" : "no");
}

}  // namespace tls_check

#endif  // TLS_CHECK
