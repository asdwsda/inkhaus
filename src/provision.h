#pragma once
// Serial `putfile` receiver (task 020). Protocol (all lines end in '\n'):
//   host:   putfile <name> <size> <crc32 hex8>      name: config.yaml | test-*.yaml, size <= 64 KB
//   device: PUT-READY                               (or PUT-ERR <reason>)
//   host:   <size> raw bytes, in pieces; after every 1 KB it waits for
//   device: PUT-ACK <offset>                        (offset = 1024, 2048, ...; none at the end)
//   device: PUT-OK <size> <crc32>   then   PUT-CONFIG ok|error ...      (or PUT-ERR <reason>)
//   host:   reboot                                  device restarts (endBurstRead first)
// Reasons: bad-command bad-name bad-size bad-crc no-memory timeout crc-mismatch no-card write-failed
// verify-failed. The file content is never logged or echoed.
#include <string>

namespace provision {

// Handles `putfile ...` and `reboot`. Returns true if the line was one of them (the transfer runs to
// completion inside this call and blocks the caller for up to a few seconds; it reads Serial itself).
// Other lines: returns false untouched, so the caller can try its other commands.
bool handleLine(const std::string &line);

}  // namespace provision
