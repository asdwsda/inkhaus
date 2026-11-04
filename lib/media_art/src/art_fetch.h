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

enum class ImageKind { Unknown, Jpeg, Png };
struct ImageInfo {
  ImageKind kind = ImageKind::Unknown;
  int w = 0, h = 0;
};
// Sniffs JPEG/PNG and reads the pixel size from the header (JPEG: first SOF marker). Unknown/truncated -> kind
// Unknown or w/h 0.
ImageInfo imageInfo(const uint8_t *data, size_t len);

constexpr long kMaxDecodePixels = 512L * 1024;  // decoded RGB565 sprite cap (1 MB of PSRAM)
// How to decode a w x h picture: halve it `shift` times (0..3) while the short side stays >= 200 (the popup size);
// refuse (ok=false) pictures over kMaxSourceDim or whose decoded area is over kMaxDecodePixels.
struct DecodePlan {
  bool ok = false;
  int shift = 0;
  int outW = 0, outH = 0;
};
DecodePlan planDecode(int w, int h);

}  // namespace art
