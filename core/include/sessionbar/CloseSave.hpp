#pragma once
#include <sessionbar/State.hpp>
// v0.4.20: the close-time save prompt's dirty check (Jackson's
// ruling, 2026-11-03): on close, the app asks to save ONLY if the
// state changed since the app was opened. The baseline is the state
// as captured just after startup (post-load, post window placement);
// the candidate is the live state with the window rect stamped in.
// Pure field-by-field equality over the whole persisted state —
// settings, geometry and the active preset's identity all count:
// any one of them differing means "changed since open". Floats
// compare exactly on purpose: every float in the state is either
// loaded from disk, set by a slider, or adopted verbatim from a
// link packet — never recomputed — so an unchanged value compares
// bit-equal and a changed one does not.
namespace sessionbar {
inline bool sameState(const SessionBarState& a, const SessionBarState& b) {
    return a.showNewYork == b.showNewYork
        && a.showLondon == b.showLondon
        && a.showTokyo == b.showTokyo
        && a.use24h == b.use24h
        && a.themeName == b.themeName
        && a.backgroundStyle == b.backgroundStyle
        && a.backgroundOpacity == b.backgroundOpacity
        && a.pinned == b.pinned
        && a.reserveScreenSpace == b.reserveScreenSpace
        && a.linkEnabled == b.linkEnabled
        && a.linkBackground == b.linkBackground
        && a.dockEdge == b.dockEdge
        && a.windowX == b.windowX
        && a.windowY == b.windowY
        && a.windowW == b.windowW
        && a.windowH == b.windowH
        && a.uiVer == b.uiVer
        && a.clickSounds == b.clickSounds
        && a.stateColorScheme == b.stateColorScheme
        && a.bgEffect == b.bgEffect
        && a.vizStyle == b.vizStyle
        && a.vizHue == b.vizHue
        && a.vizSat == b.vizSat
        && a.vizSpread == b.vizSpread
        && a.vizCycle == b.vizCycle
        && a.cellOrder == b.cellOrder
        && a.cellWidths == b.cellWidths
        && a.activePresetName == b.activePresetName;
}
inline bool changedSinceOpen(const SessionBarState& openSnapshot, const SessionBarState& current) {
    return !sameState(openSnapshot, current);
}
} // namespace sessionbar
