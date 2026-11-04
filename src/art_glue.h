#pragma once
// Task 107: glue of the media widget's album art between the session driver and the UI (UNVERIFIED on the device).
// Posts one background download at a time for what AppState::takeArtRequests reports, decodes the JPEG/PNG
// (M5GFX) into PSRAM, scales to 200 px grey and hands it to the ArtStore (which dithers to 80/200 px).
#include "app_state.h"
#include "session_driver.h"

namespace art_glue {

// A new net session started: failed pictures get one more try, the old answer is dropped.
void newSession(uistate::AppState &app);
// Every loop: posts the next due download when online and nothing else is fetching.
void pump(app_logic::SessionDriver *drv, uistate::AppState &app, bool graphsBusy);
// Handler of the ArtDone events (decodes on the UI task, bounded by art::kMaxArtFetchMs).
void onDone(app_logic::NetEvent &&ev, uistate::AppState &app);
// A download is posted and not answered yet: the sleep hold waits for it.
bool pending();
// Set by onDone when the art state changed, cleared by the call.
bool takeChanged();

}  // namespace art_glue
