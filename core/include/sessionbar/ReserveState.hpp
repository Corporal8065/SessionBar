#pragma once
#include "Arrange.hpp" // IntRect
namespace sessionbar {
// RESERVE STATE MACHINE — ported from MeterBar v0.18.1
// (meterbar::ReserveMachine) and generalised from its top-only form to
// the bar's docked edge (top or bottom), the way the family's edge
// generalisation does it: the caller reports the gap to the DOCKED edge
// and which edge that is; an edge change while reserved is a geometry
// transition like any other.
//
// Why this exists (the MeterBar v0.18 flicker, Jackson's field report
// 2026-10-28): v0.18 re-ran a sync from the UI timer ~4x/sec, tested
// "flush with the edge" against a single 2 px threshold, and compared
// the requested AppBar rect against the rect Windows had GRANTED — a
// comparison that could never come back equal once the system trimmed
// the granted rect, so every poll tore the AppBar down and rebuilt it
// and every maximised window bounced with it.
//
// The rules here make that class of flap impossible by construction:
//  - ACTIONS ARE TRANSITIONS, NOT STATES. update() returns Register /
//    Update / Release only when the desired registration actually
//    changes; identical repeat observations return None forever.
//  - HYSTERESIS on the dock test. Docked at gap <= kDockGapPx (2 px);
//    once reserved, stays reserved until gap > kUndockGapPx (8 px).
//    The 3..8 px band is a dead zone in both directions.
//  - GEOMETRY IS A TRANSITION TOO. While reserved, a changed bar rect,
//    monitor, or docked edge emits exactly one Update.
//  - TOGGLE OFF always wins: one Release, then None.
// The UI feeds this machine EVENTS only (toggle, window moved/resized,
// settings/preference changes) — never a timer.
enum class ReserveAction { None, Register, Update, Release };

class ReserveMachine {
public:
    static constexpr int kDockGapPx = 2;   // gap at/below which a non-reserved bar docks
    static constexpr int kUndockGapPx = 8; // gap beyond which a reserved bar releases

    // edge: 0 = top, 1 = bottom. gapPx: absolute distance in physical
    // pixels between the bar's docked-side edge and its monitor's same
    // edge. barRect: the bar's full screen rect (physical pixels).
    // monitorId: any value that changes when the bar changes monitor.
    ReserveAction update(bool toggleOn, int edge, int gapPx, IntRect barRect, long long monitorId) {
        if (!toggleOn) {
            if (reserved_) { reserved_ = false; return ReserveAction::Release; }
            return ReserveAction::None;
        }
        if (!reserved_) {
            if (gapPx <= kDockGapPx && barRect.w > 0 && barRect.h > 0) {
                reserved_ = true; edge_ = edge; rect_ = barRect; monitor_ = monitorId;
                return ReserveAction::Register;
            }
            return ReserveAction::None; // includes the 3..8 px dead zone: no docking from it
        }
        if (gapPx > kUndockGapPx) { reserved_ = false; return ReserveAction::Release; }
        if (edge != edge_ || barRect != rect_ || monitorId != monitor_) {
            edge_ = edge; rect_ = barRect; monitor_ = monitorId;
            return ReserveAction::Update;
        }
        return ReserveAction::None;
    }

    bool reserved() const { return reserved_; }

private:
    bool reserved_ = false;
    int edge_ = 0;
    IntRect rect_{};
    long long monitor_ = 0;
};
} // namespace sessionbar
