#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <sessionbar/State.hpp>
// Settings persistence: ONE JSON file in the per-user app data folder
// (<AppData>/SessionBar/settings.json on Windows), via JUCE's built-in
// JSON — the family idiom (MeterBar presets / TickerBar store, minus
// the multi-preset machinery: a session bar has one config). Missing
// keys fall back to SessionBarState defaults, so a settings file from
// an older build can never lose a field or invent a value.
namespace settingsstore {
inline juce::File settingsFile() {
    auto f = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile("SessionBar").getChildFile("settings.json");
    f.getParentDirectory().createDirectory();
    return f;
}
inline juce::var toVar(const sessionbar::SessionBarState& s) {
    auto* o = new juce::DynamicObject();
    auto set = [&](const char* k, juce::var v){ o->setProperty(k, v); };
    set("showNY", s.showNewYork);
    set("showLDN", s.showLondon);
    set("showTYO", s.showTokyo);
    set("use24h", s.use24h);
    set("theme", juce::String(s.themeName));
    set("bgStyle", (int)s.backgroundStyle);
    set("bgOpacity", s.backgroundOpacity);
    set("pinned", s.pinned);
    set("reserveSpace", s.reserveScreenSpace);
    set("dockEdge", (int)s.dockEdge);
    set("winX", s.windowX); set("winY", s.windowY); set("winW", s.windowW); set("winH", s.windowH);
    return juce::var(o);
}
inline sessionbar::SessionBarState fromVar(const juce::var& v) {
    sessionbar::SessionBarState s; // defaults first; stored values override
    if (!v.isObject()) return s;
    auto g = [&](const char* k){ return v.getProperty(k, {}); };
    auto gb = [&](const char* k, bool d){ auto x = g(k); return x.isVoid() ? d : (bool)x; };
    auto gi = [&](const char* k, int d){ auto x = g(k); return x.isVoid() ? d : (int)x; };
    auto gf = [&](const char* k, float d){ auto x = g(k); return x.isVoid() ? d : (float)(double)x; };
    s.showNewYork = gb("showNY", s.showNewYork);
    s.showLondon = gb("showLDN", s.showLondon);
    s.showTokyo = gb("showTYO", s.showTokyo);
    if (!s.showNewYork && !s.showLondon && !s.showTokyo) s.showNewYork = true; // an empty bar is a bug, not a preference
    s.use24h = gb("use24h", s.use24h);
    s.themeName = g("theme").toString().toStdString();
    if (s.themeName.empty()) s.themeName = "Midnight Kids";
    s.backgroundStyle = (sessionbar::BackgroundStyle)juce::jlimit(0, 2, gi("bgStyle", (int)s.backgroundStyle));
    s.backgroundOpacity = juce::jlimit(0.15f, 1.0f, gf("bgOpacity", s.backgroundOpacity));
    s.pinned = gb("pinned", s.pinned);
    s.reserveScreenSpace = gb("reserveSpace", s.reserveScreenSpace);
    s.dockEdge = (sessionbar::DockEdge)juce::jlimit(0, 1, gi("dockEdge", (int)s.dockEdge));
    s.windowX = gi("winX", s.windowX); s.windowY = gi("winY", s.windowY);
    s.windowW = juce::jlimit(420, 3840, gi("winW", s.windowW));
    s.windowH = juce::jlimit(56, 300, gi("winH", s.windowH));
    return s;
}
inline sessionbar::SessionBarState load() {
    auto f = settingsFile();
    if (!f.existsAsFile()) return sessionbar::SessionBarState{};
    auto parsed = juce::JSON::parse(f.loadFileAsString());
    return fromVar(parsed);
}
inline void save(const sessionbar::SessionBarState& s) {
    settingsFile().replaceWithText(juce::JSON::toString(toVar(s)));
}
} // namespace settingsstore
