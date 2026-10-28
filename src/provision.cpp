#include "provision.h"

#include <M5Unified.h>
#include <SD.h>
#include <esp_heap_caps.h>

#include <string>

#include "boot.h"
#include "config_sd.h"
#include "provision_proto.h"

namespace provision {
namespace {

void err(const char *reason) { Serial.printf("PUT-ERR %s\n", reason); }

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
  Serial.printf("PUT-OK %u %s\n", (unsigned)req.size, hex32(req.crc).c_str());
  Serial.printf("%s\n", configSummary(config::parseConfig(reinterpret_cast<const char *>(buf), req.size)).c_str());
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
