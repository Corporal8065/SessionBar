#pragma once
#include <string>
namespace sessionbar {
// Every persisted user setting, with v0.1 defaults. The JUCE app maps
// this 1:1 to a JSON settings file (app/Source/SettingsStore.h); the
// core tests pin the defaults.
enum class BackgroundStyle { CleanDark, MidnightKids, Flat };
enum class DockEdge { Top, Bottom };

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
    int windowX = 120, windowY = 24, windowW = 880, windowH = 84;
};
} // namespace sessionbar
