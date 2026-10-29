#pragma once
#include <string>
#include <vector>
#include <sessionbar/Arrange.hpp>
// Screen-space management for the docked bar — PORTED from MeterBar
// v0.18.1 (same service, same semantics), extended with a bottom edge
// because SessionBar docks top OR bottom (the extension TickerBar made
// when it ported the same service):
//  1) RESERVE — register the bar's window as a Windows AppBar at its
//     docked edge, so Windows shrinks that monitor's work area and
//     maximised windows stop at the bar instead of sliding under it.
//     The mechanism the taskbar itself uses. The CALLER drives this
//     only on state transitions (see sessionbar::ReserveMachine —
//     dock/undock hysteresis, event-fed); nothing is re-asserted from
//     a timer (the MeterBar v0.18 flicker lesson).
//  2) ARRANGE — a one-shot sweep that slides every other top-level
//     window on the bar's monitor into the free area (fitting rule =
//     sessionbar::arrangeFit / arrangeFitAbove, unit-tested), with
//     Restore putting them back. Runs ONLY when the user presses
//     Arrange; never from a timer or a sync path.
// Windows: the real implementation lives in ScreenSpace.cpp.
// Other platforms: every call is a no-op and statusInfo() says so —
// the stub idiom, so the UI never branches on platform and the feature
// reads honestly as unavailable there.
class ScreenSpace {
public:
    ScreenSpace();
    ~ScreenSpace();
    // Register hwnd as an AppBar at `edge` (0 = top, 1 = bottom)
    // occupying the given rect in PHYSICAL pixels (GetWindowRect —
    // AppBar rects are physical; per-monitor DPI makes a logical rect
    // land at the wrong size). Idempotent for an unchanged REQUESTED
    // rect; a different rect/edge re-registers cleanly. False = refused.
    bool reserve(void* hwnd, int edge, int physX, int physY, int physW, int physH);
    void release(); // ABM_REMOVE; safe to call at any time
    bool isReserved() const;
    struct SavedWindow { void* hwnd; sessionbar::IntRect rect; };
    // Fit every other top-level window on hwnd's monitor into the area
    // beyond a strip of stripPhysPx pixels at the given edge. Skips:
    // the bar itself, the shell, minimised, maximised (the AppBar owns
    // those), cloaked and tool windows, and any window currently in an
    // interactive move/size loop. Originals are remembered WRITE-ONCE
    // for restore(). Returns how many windows were actually moved.
    int arrange(void* hwnd, int stripPhysPx, int edge);
    int restore(); // put the remembered windows back; returns the count
    std::string statusInfo() const; // one status line for diagnostics
private:
    struct Impl; Impl* impl_ = nullptr;
};
