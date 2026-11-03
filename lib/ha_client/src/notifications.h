#pragma once
// HA persistent notifications for the sleep screen (task 101). They are no longer entities, so they come from one
// WebSocket call, `persistent_notification/get`, whose result is the list (older cores: a map) of notifications
// {notification_id, title, message, created_at}. Pure: the command and the summary of the result message.
#include <cstddef>
#include <string>

// {"id":<id>,"type":"persistent_notification/get"}
std::string build_notifications_command(int id);

constexpr size_t kNotificationsMaxBytes = 48 * 1024;  // result message cap (a few KB in practice)

struct NotificationSummary {
  bool ok = false;      // the result message parsed (count is valid)
  int count = 0;        // notifications in the list
  std::string newest;   // title of the newest by created_at; its first message line when it has no title
};

// `json` is the whole result message ({"id":1,"type":"result","success":true,"result":[...]}).
// Never throws; ok = false for anything that is not such a message. `newest` is cut to 120 bytes at a
// character boundary.
NotificationSummary parse_notifications(const std::string& json);
