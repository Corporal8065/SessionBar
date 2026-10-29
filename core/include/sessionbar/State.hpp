#pragma once
#include <map>
#include <string>
#include <vector>
namespace sessionbar {
// Every persisted user setting, with v0.1 defaults. The JUCE app maps
// this 1:1 to a JSON settings file (app/Source/SettingsStore.h); the
// core tests pin the defaults.
enum class BackgroundStyle { CleanDark, MidnightKids, Flat, Pastel }; // Pastel (3) added v0.3.0 — the family ordinal MeterBar already published
enum class DockEdge { Top, Bottom, Left, Right }; // Left/Right added v0.3.0 (vertical orientation); ordinals match the link protocol

struct SessionBarState {
    // Which session cells the bar shows (all three by default).
    bool showNewYork = true;
    bool showLondon = true;
    bool showTokyo = true;
    // Cell clocks: false = 12-hour ("9:41 AM", the US default),
    // true = 24-hour ("09:41").
    bool use24h = false;
    // Appearance — MeterBar family: Midnight Kids theme by default,
    // styled background, opacity control (bar defaults fully solid: a
    // read-at-a-glance instrument, not a meter overlay).
    std::string themeName = "Midnight Kids";
    BackgroundStyle backgroundStyle = BackgroundStyle::MidnightKids;
    float backgroundOpacity = 1.0f; // 0.15..1
    // Window
    bool pinned = true;              // always-on-top: a session bar lives on screen
    bool reserveScreenSpace = false; // family AppBar reservation — strictly opt-in, default OFF
    bool linkEnabled = false;        // v0.2.0: family linking (snap/drag-follow/shared background) — opt-in, default OFF
    DockEdge dockEdge = DockEdge::Top;
    int windowX = 120, windowY = 24, windowW = 1060, windowH = 108; // v0.3.0: +24 for the top toolbar strip (pre-0.3.0 files migrate once, see uiVer)
    // v0.3.0 parity-round additions
    int uiVer = 3;                  // settings schema marker; files from older builds read 0 and migrate once
    bool clickSounds = false;       // UI click sounds on bar buttons (MeterBar parity) — default OFF
    int stateColorScheme = 0;       // SessionColors scheme ordinal (0 = Family)
    int bgEffect = 0;               // background effect: 0 styled, 1 visualizer particles, 2 reactive glow
    float vizHue = 0.83f, vizSat = 0.85f, vizSpread = 0.75f, vizCycle = 24.f; // visualizer/glow palette
    std::vector<std::string> cellOrder = { "NY", "LDN", "TYO" }; // v0.3.0: drag-reorderable cell order (ids, not display names)
    std::map<std::string, int> cellWidths; // per-session explicit cell width px, keyed by cell id (absent = auto-share)
};
} // namespace sessionbar
