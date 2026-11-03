#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// TEXT POP (v0.4.16 — Jackson's CORRECTED geometry, 2026-11-02
// 19:40 PT, superseding MeterBar v0.26.1's arrangement). Every
// piece of text SessionBar draws over its transparent / visual
// bar background gets the doubled-letter treatment from his
// reference picture, in the order he specified:
//   1. the text's OWN COLOUR copy is drawn FIRST, at the rect
//      translated (+off, +off) — bottom-right, BEHIND;
//   2. a WHITE copy is drawn at the untranslated rect — top-left
//      relative to the colour copy, IN FRONT.
// The offset scales with the font (~20% of the glyph height,
// clamped 2-7 px) so it always clears the glyph strokes.
// Mirrors MeterBar's app/Source/TextPop.h in structure and
// coverage (bar-drawn text + toolbar button/label text via the
// popped LookAndFeels in MainComponent.h), with the draw order
// and colours corrected per above. Emoji/icon glyph text must
// NOT go through here — the glyph buttons keep their single-draw
// IconButtonLookAndFeel (Jackson: the emoji was good as it was).
// Text inside the opaque Settings/dialog windows is not popped.
namespace textpop {

// Offset for a given font height: ~20% of the glyph, clamped so
// small readouts still clear their strokes and large text does
// not fly away from its letters.
inline float offsetFor(float fontHeight) {
    return juce::jlimit(2.f, 7.f, fontHeight * 0.20f);
}

// drawText with the pop. The caller sets the font first, exactly
// as with Graphics::drawText — the offset derives from it. Both
// copies keep the text colour's own alpha, so a dim readout gets
// a proportionally dim pop.
inline void draw(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                 juce::Justification j, juce::Colour colour) {
    const float off = offsetFor(g.getCurrentFont().getHeight());
    g.setColour(colour); // own colour, bottom-right, behind
    g.drawText(text, r.translated(off, off), j);
    g.setColour(juce::Colours::white.withAlpha(colour.getFloatAlpha())); // white, top-left, in front
    g.drawText(text, r, j);
}
inline void draw(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r,
                 juce::Justification j, juce::Colour colour) {
    draw(g, text, r.toFloat(), j, colour);
}

// drawFittedText with the pop (toolbar buttons / labels, whose
// stock LookAndFeel draws fitted text) — same corrected order.
inline void drawFitted(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r,
                       juce::Justification j, int maxLines, float minScale, juce::Colour colour) {
    const int off = juce::roundToInt(offsetFor(g.getCurrentFont().getHeight()));
    g.setColour(colour);
    g.drawFittedText(text, r.translated(off, off), j, maxLines, minScale);
    g.setColour(juce::Colours::white.withAlpha(colour.getFloatAlpha()));
    g.drawFittedText(text, r, j, maxLines, minScale);
}

} // namespace textpop
