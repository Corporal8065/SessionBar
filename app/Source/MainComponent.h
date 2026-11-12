#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <sessionbar/State.hpp>
#include <sessionbar/Sessions.hpp>
#include <sessionbar/ReserveState.hpp>
#include <sessionbar/SessionColors.hpp>
#include <sessionbar/Visualizer.hpp>
#include <sessionbar/Glow.hpp>
#include <sessionbar/CellPulse.hpp> // v0.4.16: the MeterBar-mechanic cell envelope (items 1/2)
#include <sessionbar/CloseSave.hpp> // v0.4.20: the close prompt's changed-since-open check
#include "TextPop.h" // v0.4.16: corrected-geometry text pop (item 6)
#include <sessionbar/Mascot.hpp>
#include <sessionbar/Glyphs.hpp> // v0.4.10: icon glyphs as explicit UTF-8 bytes
#include "SessionBarVersion.h" // v0.4.3: generated — single-source version
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Theme.h"
#include "ScreenSpace.h"
#include "BarLink.h" // v0.2.0 (k)
#include "ClickSounds.h"
#include "Updater.h"
#include "UpdaterConfig.h"
class SessionCell;
class SessionSettingsWindow;
class MascotView;
class CellSplitter;

// v0.4.9: the icon buttons' private LookAndFeel — stock V4 in every
// respect EXCEPT the font. The v0.4.8 glyph buttons render as tofu
// on real Windows (Jackson's TickerBar v0.6.0 field evidence): the
// default button typeface has no emoji glyphs. Segoe UI Emoji
// carries them (Segoe UI Symbol is the fallback face); if neither
// exists at runtime the default sans is used, which is the pre-fix
// behaviour. Applied per button, so every other button (Update!
// included) keeps the existing font.
// v0.4.16 (item 6): button text with the TEXT POP (TextPop.h,
// corrected geometry) — MeterBar v0.26.1's PoppedButtonLookAndFeel
// mirrored. Applied ONLY to the text "Update!" button: the glyph
// buttons keep IconButtonLookAndFeel below and stay single-draw
// (emoji/icon glyphs are NOT doubled, Jackson's word).
class PoppedButtonLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/) override {
        juce::Font font(getTextButtonFont(button, button.getHeight()));
        g.setFont(font);
        const auto colour = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                                      : juce::TextButton::textColourOffId)
                              .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f);
        const int yIndent = juce::jmin(4, button.proportionOfHeight(0.3f));
        const int cornerSize = juce::jmin(button.getHeight(), button.getWidth()) / 2;
        const int fontHeight = juce::roundToInt(font.getHeight() * 0.6f);
        const int leftIndent = juce::jmin(fontHeight, 2 + cornerSize / (button.isConnectedOnLeft() ? 4 : 2));
        const int rightIndent = juce::jmin(fontHeight, 2 + cornerSize / (button.isConnectedOnRight() ? 4 : 2));
        const int textWidth = button.getWidth() - leftIndent - rightIndent;
        if (textWidth > 0)
            textpop::drawFitted(g, button.getButtonText(),
                                juce::Rectangle<int>(leftIndent, yIndent, textWidth, button.getHeight() - yIndent * 2),
                                juce::Justification::centred, 2, 1.0f, colour);
    }
};

// v0.4.16 (item 6): the toolbar labels (wordmark / version) with
// the TEXT POP — mirrors LookAndFeel_V2::drawLabel, adding the
// corrected pop pass before the fitted text (MeterBar v0.26.1's
// PoppedLabelLookAndFeel mirrored).
class PoppedLabelLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawLabel(juce::Graphics& g, juce::Label& label) override {
        g.fillAll(label.findColour(juce::Label::backgroundColourId));
        if (!label.isBeingEdited()) {
            const auto alpha = label.isEnabled() ? 1.0f : 0.5f;
            const juce::Font font(getLabelFont(label));
            g.setFont(font);
            auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
            textpop::drawFitted(g, label.getText(), textArea, label.getJustificationType(),
                                juce::jmax(1, (int)((float)textArea.getHeight() / font.getHeight())),
                                label.getMinimumHorizontalScale(),
                                label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
            g.setColour(label.findColour(juce::Label::outlineColourId).withMultipliedAlpha(alpha));
        } else if (label.isEnabled()) {
            g.setColour(label.findColour(juce::Label::outlineColourId));
        }
        g.drawRect(label.getLocalBounds());
    }
};

class IconButtonLookAndFeel : public juce::LookAndFeel_V4 {
public:
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override {
        const float h = juce::jmin(16.0f, (float)buttonHeight * 0.62f);
        for (auto* name : { "Segoe UI Emoji", "Segoe UI Symbol" })
            if (juce::Font::findAllTypefaceNames().contains(name))
                return juce::Font(juce::FontOptions(juce::String(name), h, juce::Font::plain));
        return juce::Font(juce::FontOptions(h));
    }
};

// 30 Hz pump for the animated backgrounds + mascot (v0.3.0) — the
// bar's own timer runs at 1 Hz for the session clocks. Runs only
// while an effect is on or the mascot is visible (family pattern,
// shared with TickerBar).
class FxPump : private juce::Timer {
public:
    std::function<void()> onTick;
    void setRunning(bool run) {
        if (run) { if (!isTimerRunning()) startTimerHz(30); }
        else stopTimer();
    }
private:
    void timerCallback() override { if (onTick) onTick(); }
};

// The bar itself: a MeterBar-style TOP TOOLBAR STRIP (v0.3.0 — the
// v0.2.1 cluster sat vertically centred mid-bar; Jackson's field
// report) holding the wordmark on the left and the control cluster
// on the right. v0.4.8: the cluster is ICON-ONLY — Link | Settings
// | Update! | Menu (☰) — and Presets, Arrange, Restore, Stack,
// Reposition, Minimize and Quit live in the Menu. (The pre-v0.4.8
// text cluster ran Link | Arrange | Restore | Preset | Reposition
// | Stack | Dock | Settings | Update! | _ | Quit; Reserve had
// already moved into Reposition at v0.4.4.) Below the strip, one cell per visible
// session (drag sideways to reorder, v0.3.0), then the Anime-theme
// mascot. Borderless and always-on-top (state.pinned); position +
// every setting persist to the settings JSON on change. A 1 Hz
// timer re-evaluates the three session clocks (pure core math) so
// the countdowns tick every second.
class MainComponent : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;
    sessionbar::SessionBarState state; // loaded in the ctor; MainWindow positions itself from it

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

    // --- services for the child components (cells / settings) ---
    const Theme& theme() const { return theme_; }
    void windowDragStarted(const juce::MouseEvent& e);
    void windowDragged(const juce::MouseEvent& e);
    // Cell drag arbitration (v0.3.0): direction decides window drag vs reorder.
    void cellDragBegin(juce::Component* cell, const juce::MouseEvent& e);
    void cellDragMove(juce::Component* cell, const juce::MouseEvent& e);
    bool cellDragEnd(juce::Component* cell, const juce::MouseEvent& e);
    void settingsChanged(); // apply + persist (theme, cells, pin, reserve)
    void resetSettingsToDefaults(); // v0.4.16 (item 5): Settings "Reset settings to defaults" — factory SessionBarState, applied live
    // v0.4.20: the close-time save prompt (Jackson's ruling, 2026-11-03).
    // Called by the app on EVERY quit request instead of quit():
    // unchanged since open → quits at once, silently; changed → the
    // Save / Save As... / Close Without Saving / Cancel prompt, whose
    // answer completes (or vetoes) the quit. Async because JUCE 8 has
    // no modal loop — the prompt's callback issues the quit itself.
    void requestClose();
    void onBgLinkToggled(); // v0.4.5: Settings "Link background" flipped — re-stamp for peers when it turns on
    void notifyWindowGeometryChanged(); // MainWindow moved/resized feed (reserve machine events)
    void snapToDockEdge();
    void cycleDockEdgeFromSettings(); // cycle Top -> Bottom -> Left -> Right + snap (Settings dock control; the bar's Dock button went at v0.4.8 — the Reposition submenu's ticked edges are its direct form)
    void doArrange();
    void doRestore();
    // v0.4.22: doStackAll removed — Link All (doLinkAll) is the one gather
    void doLinkAll(int edgeOrdinal);    // v0.4.19 BLK7 §6: Link All request (0 Top / 1 Bottom)
    void checkForUpdates();             // v0.3.0 item 1
    juce::Colour stateColour(sessionbar::SessionState s) const; // v0.3.0 item 6
    bool fxActive() const { return state.bgEffect != 0; }
    // v0.4.16 (items 1/2): the cells' MeterBar-mechanic glow
    // envelope 0..1 — driven ONLY from MeterBar's linked audio
    // packet, through core CellPulse's attack/release smoothing
    // (never the raw packet). Zero when unlinked, so unlinked
    // cells render exactly as before.
    float cellGlow() const { return cellPulse_.glow(); }
    float cellDotScale() const { return cellPulse_.dotScale(); }
    // v0.4.19 BLK7 (§7): the fired pulse colour (0xRRGGBB from
    // MeterBar's stamp) — spent by the cell paint while the pump
    // is live; the theme accent is the fallback otherwise.
    bool cellPulseColourLive() const { return cellPulse_.firedColourLive(); }
    juce::Colour cellPulseColour() const {
        const int c = cellPulse_.firedColour();
        return juce::Colour((juce::uint8)((c >> 16) & 0xFF), (juce::uint8)((c >> 8) & 0xFF), (juce::uint8)(c & 0xFF));
    }
    // v0.4.14: this bar is link-grouped (Link on + at least one live
    // linked peer) — the state in which the shared background rules
    // (parent opacity under the effects, opacity-scaled cell panels)
    // apply. Unlinked rendering never consults it.
    bool linkGrouped() { return state.linkEnabled && linkClient_.liveLinkedPeers() > 0; }
    bool vertical() const { return vertical_; }
    void uiClick(ClickSounds::Which w); // v0.3.0 item 5
    juce::String screenSpaceStatus() const { return juce::String(screenSpace_.statusInfo()); }

private:
    void timerCallback() override;
    void rebuildCells();
    void rebuildSplitters();
    void refreshCells();
    void syncScreenSpace();
    void saveSettings();
    void applyTheme();
    void applyChrome();   // MeterBar toolbar colouring for the control cluster
    juce::Rectangle<int> currentWindowBounds() const;
    // updater (v0.3.0 item 1)
    void startUpdateCheck(bool fromLaunch);
    void pollUpdater();
    void presentUpdateResult(const sbupdate::ReleaseInfo& r);
    void showUpdatePanel(const sbupdate::ReleaseInfo& r);
    void showPrivateRepoFallback(const juce::String& detail);
    void startUpdateDownload();
    // v0.4.8: the burger Menu — holds the compressed toolbar:
    // the Presets submenu (v0.3.0 item 2), Arrange / Restore /
    // Stack items, the Reposition submenu (the v0.4.4 dropdown —
    // dock edges, pin, move-to-display, and the Reserve toggle),
    // then Minimize / Quit. The builders reproduce the old
    // standalone menus verbatim; showMainMenu's callback handles
    // the unified item ids.
    void showMainMenu();
    juce::PopupMenu buildPresetSubMenu(juce::Array<juce::File>& filesOut);
    juce::PopupMenu buildRepositionSubMenu();
    void doMoveToDisplay(int idx);
    void savePresetInteractive(bool saveAs);
    void performSave(bool saveAs, std::function<void(bool)> done); // v0.4.21: THE save function — the menu's Save / Save As and the close prompt's Save / Save As all route through it (Jackson's refinement: one kind of function for every save)
    void promptForPresetNameAsync(std::function<void(bool)> done); // v0.4.20: the shared name prompt inside performSave (sets state.activePresetName on success)
    void loadPresetFile(const juce::File& f);
    void applyWindowRectFromState();
    // animated backgrounds (items 7/17) + mascot (item 18)
    void stepFx();
    void drawVisualizerBackground(juce::Graphics& g);
    void drawGlowBackground(juce::Graphics& g);
    sessionbar::VisualizerSettings vizSettings() const;
    sessionbar::GlowSettings glowSettings() const;
    void syncFxPump();

    Theme theme_ = midnightKidsTheme();
    ScreenSpace screenSpace_;
    BarLinkClient linkClient_{ sessionbar::kLinkAppSession }; // v0.2.0 (k)
    sessionbar::ReserveMachine reserveMachine_;
    std::vector<std::unique_ptr<SessionCell>> cells_;
    std::vector<std::unique_ptr<CellSplitter>> splitters_;
    std::unique_ptr<MascotView> mascot_;
    std::unique_ptr<SessionSettingsWindow> settingsWin_;
    ClickSounds clicks_;

    juce::Label wordmark_, versionLabel_;
    PoppedLabelLookAndFeel popLabelLnf_;   // v0.4.16: wordmark/version text pop (declared before the labels' users below)
    PoppedButtonLookAndFeel popButtonLnf_; // v0.4.16: the text "Update!" button only — glyph buttons keep iconLnf_
    IconButtonLookAndFeel iconLnf_; // v0.4.9: declared before the buttons so it outlives them
    // v0.4.8: icon-only cluster (see the class comment). The
    // Update! indicator stays text on purpose — it is temporary
    // and must be unmissable.
    // v0.4.10: glyph text is built from the core's explicit UTF-8
    // byte escapes via fromUTF8 — the raw literals used up to
    // v0.4.9 were decoded as Windows-1252 by MSVC and reached the
    // buttons as mojibake (see sessionbar/Glyphs.hpp).
    juce::TextButton linkBtn_ { juce::String::fromUTF8(sessionbar::glyphLinkUtf8()) },
                     settingsBtn_ { juce::String::fromUTF8(sessionbar::glyphSettingsUtf8()) },
                     burgerBtn_ { juce::String::fromUTF8(sessionbar::glyphMenuUtf8()) },
                     updateBtn_ { "Update!" };
    juce::ComponentDragger dragger_;
    juce::Rectangle<int> lastSavedBounds_;
    bool vertical_ = false;
    // v0.4.20: the active preset's name lives in state.activePresetName
    // now (persisted; it was this RAM-only member before). The close
    // prompt's baseline: the state as captured on the first 1 Hz tick
    // (post-load, post window placement), plus the discard flag the
    // destructor honours when the prompt's answer was Close Without
    // Saving (it writes the baseline back over the auto-saved file).
    sessionbar::SessionBarState openBaseline_;
    bool openBaselineCaptured_ = false;
    bool discardOnClose_ = false;
    bool closePromptOpen_ = false; // a second quit request while the prompt is up is ignored
    std::map<std::string, sessionbar::SessionState> prevStates_; // mascot transition triggers
    // reorder gesture state
    juce::Component* dragCell_ = nullptr;
    juce::Rectangle<int> dragCellStartBounds_;
    juce::Point<int> dragStartScreen_;
    int dragMode_ = 0; // 0 undecided, 1 window drag, 2 cell reorder
    // updater state
    std::shared_ptr<sbupdate::SharedState> updateState_;
    bool updateCheckPending_ = false, updateCheckFromLaunch_ = false, updateAvailable_ = false;
    sbupdate::ReleaseInfo pendingRelease_;
    std::unique_ptr<juce::URL::DownloadTask> updateDownload_;
    bool updateDownloadPending_ = false;
    juce::File updateInstallerFile_;
    // fx state
    FxPump fxPump_;
    sessionbar::VisualizerEngine viz_;
    sessionbar::VisualizerLines vizLines_; // v0.4.7: the Lines style's ribbon model (pure core)
    sessionbar::VisualizerField vizField_; // v0.4.13: the Field style's vortex model (pure core)
    sessionbar::GlowField glow_;
    sessionbar::CellPulse cellPulse_; // v0.4.16: linked-audio cell envelope (items 1/2)
    bool cellPulseWasLit_ = false;    // v0.4.16: repaint cells while the envelope is up (and once as it dies)
    unsigned seenPulseSeq_ = 0;       // v0.4.19 BLK7 §7: last MeterBar pulse seq consumed (edge-detect)
    // v0.4.19 BLK7 §6d: pre-link appearance snapshot (in-memory only —
    // a follower that crashes mid-link keeps the adopted values, as
    // before) + the Link All geometry snapshot. Taken on the link
    // OFF->ON edge before the first adoption can land; restored and
    // cleared on the ON->OFF edge.
    struct AppearanceSnapshot {
        sessionbar::BackgroundStyle style = sessionbar::BackgroundStyle::MidnightKids;
        float opacity = 1.f;
        int bgEffect = 0, vizStyle = 0, scheme = 0;
        std::string themeName;
        float vizHue = 0.f, vizSat = 0.f, vizSpread = 0.f, vizCycle = 0.f;
    };
    AppearanceSnapshot appearanceSnapshot_;
    bool appearanceSnapshotHeld_ = false;
    juce::Rectangle<int> linkAllGeomSnapshot_;
    bool linkAllGeomSnapshotHeld_ = false;
    void snapshotAppearance();
    void restoreAppearanceOnUnlink();
    sessionbar::MascotAnim mascotAnim_;
    juce::Image vizCache_, vizBlur_, glowCache_;
    juce::Image vizBlurSrc_;                 // v0.4.12: quarter-res downsample the blur convolves (lag fix — MeterBar v0.23.1 twin)
    int vizBlurFrame_ = 0;                   // v0.4.12: blur recompute cadence — every 2nd frame, held between
    double fxClock_ = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
