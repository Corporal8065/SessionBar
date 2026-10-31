#pragma once
#include <algorithm>
namespace sessionbar {
// ARRANGE MATH — ported from MeterBar v0.18 (meterbar::arrangeFit), the
// fitting rule behind the "Arrange windows" sweep in
// app/Source/ScreenSpace.cpp (Windows-only: EnumWindows +
// SetWindowPos). The rule itself is a PURE function with no OS calls,
// so the Linux test suite grades it exactly, even though the sweep that
// consumes it can only run on a Windows PC. Same division of labour as
// MeterBar: math here, platform plumbing in the app.

// Integer rectangle in pixels: x/y = top-left corner, w/h = size.
// On Windows these are PHYSICAL pixels (what GetWindowRect /
// GetMonitorInfo report); the tests use plain numbers.
struct IntRect {
    int x = 0, y = 0, w = 0, h = 0;
    int right() const { return x + w; }
    int bottom() const { return y + h; }
    bool operator==(const IntRect& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
    bool operator!=(const IntRect& o) const { return !(*this == o); }
};

// Fit `window` inside `monitor` BELOW a reserved strip of stripPx pixels
// at the monitor's top edge (the docked bar's height). Rules, in order:
//  1. stripPx is clamped to [0, monitor.h]. A monitor with no free area
//     left gets the window back UNCHANGED — never make things worse.
//  2. A window already fully inside the free area is returned UNCHANGED.
//  3. Otherwise the window keeps as much size as fits and slides the
//     minimum distance to sit fully inside the free area.
inline IntRect arrangeFit(IntRect window, IntRect monitor, int stripPx) {
    if (window.w <= 0 || window.h <= 0 || monitor.w <= 0 || monitor.h <= 0) return window;
    const int strip = std::clamp(stripPx, 0, monitor.h);
    const IntRect freeArea{ monitor.x, monitor.y + strip, monitor.w, monitor.h - strip };
    if (freeArea.w <= 0 || freeArea.h <= 0) return window;
    IntRect out;
    out.w = std::min(window.w, freeArea.w);
    out.h = std::min(window.h, freeArea.h);
    out.x = std::clamp(window.x, freeArea.x, freeArea.right() - out.w);
    out.y = std::clamp(window.y, freeArea.y, freeArea.bottom() - out.h);
    return out;
}

// Bottom-edge variant (SessionBar can dock at the bottom): fit `window`
// inside `monitor` ABOVE a reserved strip of stripPx at the bottom.
inline IntRect arrangeFitAbove(IntRect window, IntRect monitor, int stripPx) {
    if (window.w <= 0 || window.h <= 0 || monitor.w <= 0 || monitor.h <= 0) return window;
    const int strip = std::clamp(stripPx, 0, monitor.h);
    const IntRect freeArea{ monitor.x, monitor.y, monitor.w, monitor.h - strip };
    if (freeArea.w <= 0 || freeArea.h <= 0) return window;
    IntRect out;
    out.w = std::min(window.w, freeArea.w);
    out.h = std::min(window.h, freeArea.h);
    out.x = std::clamp(window.x, freeArea.x, freeArea.right() - out.w);
    out.y = std::clamp(window.y, freeArea.y, freeArea.bottom() - out.h);
    return out;
}

// ------------------------------------------------------------------
// MAXIMIZED WINDOWS (v0.4.6) — Jackson: "the arrange does not work
// when the application is already meximized". The old sweep SKIPPED
// zoomed windows outright (IsZoomed in the skip condition), so a
// maximized app was never arranged. A maximized window's live rect
// is the whole monitor — meaningless to fit; its NORMAL (restored)
// rect (GetWindowPlacement's rcNormalPosition) is the one
// arrangeFit can place. The plan: a zoomed window participates via
// its normal rect, is snapshotted BEFORE being touched (normal rect
// + wasMaximized), restored, then fitted. Restore reverses the
// state, not just the rect: a window that was maximized goes back
// to maximized after its snapshot rect is applied.
struct ArrangeWindowPlan {
    bool participate = false;  // false: skip this window entirely
    bool restoreFirst = false; // true when zoomed: un-maximize before fitting
    IntRect fitInput{};        // the rect to run through arrangeFit
};
inline ArrangeWindowPlan planArrangeWindow(bool visible, bool iconic, bool zoomed,
                                           IntRect windowRect, IntRect normalRect) {
    if (!visible || iconic) return {};             // the historical skips stay skips
    if (zoomed) {
        if (normalRect.w <= 0 || normalRect.h <= 0) return {};
        return { true, true, normalRect };         // fit the NORMAL rect, never the zoomed one
    }
    if (windowRect.w <= 0 || windowRect.h <= 0) return {};
    return { true, false, windowRect };
}
struct RestoreWindowPlan {
    bool applyRect = true;   // SetWindowPos the snapshot rect first
    bool remaximize = false; // then SW_MAXIMIZE — a maximized home is a maximized window
};
inline RestoreWindowPlan planRestoreWindow(bool wasMaximized) {
    return { true, wasMaximized };
}
} // namespace sessionbar
