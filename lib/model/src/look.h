#pragma once

// Shared by ui_state (widget views) and calendar (its view); kept in model so those two libs need not include each other.
namespace uistate {
enum class Look { Normal, Loading, Unavailable };
}
