#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <sessionbar/State.hpp>
// Theme values are MeterBar's "Midnight Kids" palette VERBATIM (Jackson's
// chosen family — see meters-app/app/Source/Theme.h): deep violet-black
// chrome, pink brand accent, teal secondary, warm off-white text.
// SessionBar wears the same colours so the bars look like siblings on
// one screen. "Clean Dark" is the family's near-black blue-grey variant.
struct Theme {
    juce::Colour bg, panel, deepest, accent, accent2, text, dim, good, warn, bad;
    juce::Colour border, grid;
    juce::String name;
};
inline Theme midnightKidsTheme() {
    return { juce::Colour(0xff17141f), juce::Colour(0xff231f31), juce::Colour(0xff0e0c15),
        juce::Colour(0xffe04c89), juce::Colour(0xff40aac0), juce::Colour(0xfff6eff3), juce::Colour(0xffa79fc0),
        juce::Colour(0xff2fd68a), juce::Colour(0xfffad927), juce::Colour(0xfffe5a3c),
        juce::Colour(0xff3d3654), juce::Colour(0xff494164), "Midnight Kids" };
}
inline Theme cleanDarkTheme() {
    return { juce::Colour(0xff14171c), juce::Colour(0xff1d2129), juce::Colour(0xff0b0d10),
        juce::Colour(0xff40aac0), juce::Colour(0xffe04c89), juce::Colour(0xffeef2f5), juce::Colour(0xff8b95a5),
        juce::Colour(0xff2fd68a), juce::Colour(0xfffad927), juce::Colour(0xfffe5a3c),
        juce::Colour(0xff2c313a), juce::Colour(0xff232830), "Clean Dark" };
}
inline Theme themeByName(const std::string& n) {
    return n == "Clean Dark" ? cleanDarkTheme() : midnightKidsTheme();
}
// Background styles (MeterBar v0.16 helper, family version): CleanDark =
// near-black blue-grey gradient; MidnightKids = violet/plum gradient;
// Flat = plain theme.bg. Painted at alpha=opacity.
inline void fillStyledBackground(juce::Graphics& g, juce::Rectangle<float> r,
                                 sessionbar::BackgroundStyle style, const Theme& th, float alpha) {
    alpha = juce::jlimit(0.15f, 1.0f, alpha);
    juce::Colour top, bottom;
    switch (style) {
    case sessionbar::BackgroundStyle::CleanDark:
        top = juce::Colour(0xff272c36); bottom = juce::Colour(0xff101216); break;
    case sessionbar::BackgroundStyle::MidnightKids:
        top = juce::Colour(0xff2b1a4d); bottom = juce::Colour(0xff150f24); break;
    default: // Flat
        g.fillAll(th.bg.withAlpha(alpha)); return;
    }
    juce::ColourGradient grad(top.withAlpha(alpha), r.getX(), r.getY(),
                              bottom.withAlpha(alpha), r.getX(), r.getBottom(), false);
    if (style == sessionbar::BackgroundStyle::MidnightKids)
        grad.addColour(0.55, juce::Colour(0xff3d1d43).withAlpha(alpha)); // magenta-plum mid-stop
    g.setGradientFill(grad);
    g.fillRect(r);
}
