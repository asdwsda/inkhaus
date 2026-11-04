#pragma once
// Fetch plan and limits for album art (task 107, docs/ui/widgets-new-media-art.md). Pure: decides the URL,
// whether the HA token may be sent, and whether a download is within the caps. The HTTP call and the
// JPEG/PNG decode are device code (UNVERIFIED, not part of the PC step).
#include <cstddef>
#include <cstdint>
#include <string>

namespace art {

constexpr size_t kMaxArtBytes = 150 * 1024;   // download cap (body); larger -> Failed
constexpr uint32_t kMaxArtFetchMs = 6000;     // time cap for fetch + decode, inside the session caps (UNVERIFIED value)
constexpr int kMaxSourceDim = 4096;           // decoder refuses larger pictures
constexpr size_t kMaxPictureUrl = 512;        // longer entity_picture values are ignored

struct FetchPlan {
  bool ok = false;
  std::string url;
  bool sendToken = false;  // Authorization: Bearer only to the HA host itself
};

// picture: the entity_picture attribute. baseUrl: the HA URL in use ("http://host:8123", no path needed).
// Relative ("/api/...") -> baseUrl + picture with token; absolute http(s) on the same host:port -> as is with
// token; absolute elsewhere -> as is, no token; anything else (empty, "demo:", other schemes, too long) -> not ok.
FetchPlan planFetch(const std::string &baseUrl, const std::string &picture);

// True for the built-in demo pictures ("demo:sample"); they never touch the network.
bool isDemoPicture(const std::string &picture);

// A body of this declared/received size may be downloaded (contentLength < 0: unknown, checked while reading).
bool bodyWithinCap(long contentLength);

}  // namespace art
