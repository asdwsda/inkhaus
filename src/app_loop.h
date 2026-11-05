// The phases of loop() (task 124e). main.cpp calls them in this order; each takes what the earlier phases
// produced (the input event, whether a popup counts as activity, whether the screen changed).
#pragma once

#include <stdint.h>

#include "input.h"

namespace loopphase {

// Input: poll the rocker / touch, filter the wake press. False: a manual sleep ran, end this loop() pass.
bool pollInput(uint32_t now, input::Event &e);
// Input: keep-awake battery sampling and idle-timer feed. False: low-battery sleep ran, end this pass.
bool keepAwake(uint32_t now);
// A popup that counts as activity (brightness, media, alarm, confirm, a fetching history popup).
bool popupActive();
// Input: the session driver / local idle timer see the activity; a finished session restarts.
void sessionInput(uint32_t now, const input::Event &e, bool popupOpen);
// Sleep: the idle sleep of a demo / config-error run (no session machine).
void localIdleSleep(uint32_t now, const input::Event &e, bool popupOpen);
// Input: route the event to Settings, the tabs, popups or the rocker handler.
void dispatchInput(uint32_t now, const input::Event &e, bool &changed);
// Net events: pending network restart, session driver tick, fetch results, sleep requests.
void netEvents(uint32_t now, bool &changed);
// Pumps: history / graph / notification / forecast / calendar / art glue.
void pumps(bool &changed);
// Frame: status label check, shot hooks, deferred tap, show the frame, burst clean, idle full refresh.
void frame(uint32_t now, const input::Event &e, bool popupOpen, bool changed);

}  // namespace loopphase
