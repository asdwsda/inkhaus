#include "provision.h"

#include <M5Unified.h>
#include <SD.h>
#include <esp_heap_caps.h>

#include <string>

#include "app_config.h"
#include "boot.h"
#include "config_sd.h"
#include "provision_proto.h"

namespace provision {
namespace {

void err(const char *reason) { Serial.printf("PUT-ERR %s\n", reason); }
void err2(const char *reason) { Serial.printf("GET-ERR %s\n", reason); }

// Writes `data` to /<name>.tmp, reads it back to verify size + CRC, then replaces /<name>.
// Returns nullptr on success, else the PUT-ERR reason. An existing target is only removed once the
// verified tmp file exists.
const char *writeAtomic(const std::string &name, const uint8_t *data, size_t len, uint32_t crc) {
  if (!config_sd::mount()) {
    config_sd::unmount();
    return "no-card";
  }
  const std::string path = "/" + name, tmp = path + ".tmp";
  const char *result = nullptr;
  SD.remove(tmp.c_str());
  {
    File f = SD.open(tmp.c_str(), FILE_WRITE);
    if (!f) {
      result = "write-failed";
    } else {
      size_t off = 0;
      while (off < len) {
        const size_t n = len - off > 1024 ? 1024 : len - off;
        if (f.write(data + off, n) != n) break;
        off += n;
      }
      f.close();
      if (off != len) result = "write-failed";
    }
  }
  if (!result) {
    File f = SD.open(tmp.c_str(), FILE_READ);
    uint32_t c = 0;
    size_t total = 0;
    if (f) {
      uint8_t buf[512];
      int n;
      while ((n = f.read(buf, sizeof buf)) > 0) {
        c = crc32Update(c, buf, size_t(n));
        total += size_t(n);
      }
      f.close();
    }
    if (total != len || c != crc) result = "verify-failed";
  }
  if (!result) {
    SD.remove(path.c_str());
    if (!SD.rename(tmp.c_str(), path.c_str())) result = "write-failed";
  }
  if (result) SD.remove(tmp.c_str());
  config_sd::unmount();
  return result;
}

// Quiet the display before a blocking transfer or a restart: finish a refresh that may be in
// flight, then close the controller's read burst that waitDisplay() leaves open.
void settleDisplay() {
  M5.Display.waitDisplay();
  boot::endBurstRead();
}

void receive(const Request &req) {
  settleDisplay();
  uint8_t *buf = static_cast<uint8_t *>(heap_caps_malloc(req.size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM));
  if (!buf) buf = static_cast<uint8_t *>(malloc(req.size));
  if (!buf) {
    err("no-memory");
    return;
  }
  Receiver rc;
  rc.begin(req, millis());
  Serial.print("PUT-READY\n");
  uint8_t chunk[256];
  size_t stored = 0;
  while (!rc.complete()) {
    if (rc.timedOut(millis())) {
      err("timeout");
      free(buf);
      return;
    }
    boot::feed();  // harmless once boot::setupDone() has removed the loop task from the watchdog
    int avail = Serial.available();
    if (avail <= 0) {
      delay(1);
      continue;
    }
    size_t n = Serial.readBytes(chunk, avail > (int)sizeof chunk ? sizeof chunk : (size_t)avail);
    const uint8_t *p = chunk;
    while (n > 0) {
      const Receiver::Result r = rc.feed(p, n, millis());
      if (r.consumed == 0) break;
      memcpy(buf + stored, p, r.consumed);
      stored += r.consumed;
      p += r.consumed;
      n -= r.consumed;
      if (r.ack) Serial.printf("PUT-ACK %u\n", (unsigned)r.ackOffset);
    }
  }
  if (rc.verdict() != Verdict::Ok) {
    err("crc-mismatch");
    free(buf);
    return;
  }
  const char *why = writeAtomic(req.name, buf, req.size, req.crc);
  if (why) {
    err(why);
    free(buf);
    return;
  }
  if (req.name == "config.yaml" || req.name == "secrets.yaml") app_config::invalidateCache();
  Serial.printf("PUT-OK %u %s\n", (unsigned)req.size, hex32(req.crc).c_str());
  const char *text = reinterpret_cast<const char *>(buf);
  if (req.name == "secrets.yaml")
    Serial.printf("%s\n", secretsSummary(config::parseSecrets(text, req.size)).c_str());
  else
    Serial.printf("%s\n", configSummary(config::parseConfig(text, req.size)).c_str());
  free(buf);
}

// `getfile config.yaml` (task 119): sends /config.yaml as GET-BEGIN <size> <crc>, raw bytes, GET-END.
// The name was whitelisted by parseGetfile (config.yaml only); nothing else is ever read here.
void sendConfig() {
  settleDisplay();
  if (!config_sd::mount()) {
    config_sd::unmount();
    Serial.print("GET-ERR no-card\n");
    return;
  }
  uint8_t *buf = nullptr;
  size_t len = 0;
  const char *result = nullptr;
  File f = SD.open("/config.yaml", FILE_READ);
  if (!f) {
    result = "no-file";
  } else {
    len = f.size();
    if (len == 0) result = "no-file";
    else if (len > kMaxFileBytes) result = "too-big";
    else {
      buf = static_cast<uint8_t *>(heap_caps_malloc(len, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM));
      if (!buf) buf = static_cast<uint8_t *>(malloc(len));
      if (!buf) result = "no-memory";
      else {
        size_t off = 0;
        int n;
        while (off < len && (n = f.read(buf + off, len - off)) > 0) off += size_t(n);
        if (off != len) result = "read-failed";
      }
    }
    f.close();
  }
  config_sd::unmount();
  if (result) {
    Serial.printf("GET-ERR %s\n", result);
    free(buf);
    return;
  }
  Serial.printf("%s\n", getBegin(uint32_t(len), crc32(buf, len)).c_str());
  size_t off = 0;
  while (off < len) {
    const size_t n = len - off > 256 ? 256 : len - off;
    Serial.write(buf + off, n);
    off += n;
    boot::feed();
  }
  Serial.print("\nGET-END\n");
  Serial.flush();
  free(buf);
}

}  // namespace

bool handleLine(const std::string &line) {
  if (line == "reboot") {
    Serial.print("CMD: reboot\n");
    Serial.flush();
    settleDisplay();  // ends with endBurstRead(): the last controller operation before the restart
    ESP.restart();
    return true;  // not reached
  }
  std::string getName;
  const GetStatus gs = parseGetfile(line, getName);
  if (gs != GetStatus::NotGetfile) {
    if (gs != GetStatus::Ok) err2(getStatusReason(gs));
    else {
      Serial.print("CMD: getfile config.yaml\n");
      sendConfig();
    }
    return true;
  }
  Request req;
  const ParseStatus ps = parsePutfile(line, req);
  if (ps == ParseStatus::NotPutfile) return false;
  if (ps != ParseStatus::Ok) {
    err(parseStatusReason(ps));
    return true;
  }
  Serial.printf("CMD: putfile %s %u %s\n", req.name.c_str(), (unsigned)req.size, hex32(req.crc).c_str());
  receive(req);
  return true;
}

}  // namespace provision
