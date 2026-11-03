#pragma once
// The full list of HA persistent notifications for the notifications widget and its popup (task 103).
// Same WebSocket call as the sleep screen's summary (notifications.h); this parses the items, and builds the
// dismiss command. Pure: no network, no Arduino.
#include <cstddef>
#include <string>
#include <vector>

constexpr size_t kNotifMaxItems = 50;     // more are dropped (the count stays the list size up to this cap)
constexpr size_t kNotifTitleBytes = 80;   // titles and messages are cut at a character boundary
constexpr size_t kNotifMessageBytes = 160;

struct NotifItem {
  std::string id;       // notification_id (the map key on older cores)
  std::string title;    // may be empty
  std::string message;  // one line, markup stripped, cut to kNotifMessageBytes
  std::string created;  // created_at as sent (ISO 8601)
};

struct NotifList {
  bool ok = false;
  std::vector<NotifItem> items;  // newest first (created_at descending), at most kNotifMaxItems
};

// `json` is the whole result message of persistent_notification/get. Never throws; ok = false for anything
// that is not such a message.
NotifList parse_notification_list(const std::string& json);

// {"id":<id>,"type":"persistent_notification/dismiss","notification_id":"<notification_id>"} (id JSON-escaped).
std::string build_notification_dismiss_command(int id, const std::string& notificationId);

// One line of plain text from a notification message: markdown links keep their text, emphasis / code marks and
// leading '#' / '>' are dropped, line breaks and runs of blanks become one space.
std::string notif_plain_line(const std::string& message);
