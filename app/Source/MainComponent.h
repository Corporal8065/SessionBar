#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <sessionbar/State.hpp>
#include <sessionbar/Sessions.hpp>
#include <sessionbar/ReserveState.hpp>
#include <sessionbar/SessionColors.hpp>
#include <sessionbar/Visualizer.hpp>
#include <sessionbar/Glow.hpp>
#include <sessionbar/Mascot.hpp>
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
    void onBgLinkToggled(); // v0.4.5: Settings "Link background" flipped — re-stamp for peers when it turns on
    void notifyWindowGeometryChanged(); // MainWindow moved/resized feed (reserve machine events)
    void snapToDockEdge();
    void cycleDockEdgeFromSettings(); // cycle Top -> Bottom -> Left -> Right + snap (Settings dock control; the bar's Dock button went at v0.4.8 — the Reposition submenu's ticked edges are its direct form)
    void doArrange();
    void doRestore();
    void doStackAll();                  // v0.3.0 item 12
    void checkForUpdates();             // v0.3.0 item 1
    juce::Colour stateColour(sessionbar::SessionState s) const; // v0.3.0 item 6
    bool fxActive() const { return state.bgEffect != 0; }
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
    IconButtonLookAndFeel iconLnf_; // v0.4.9: declared before the buttons so it outlives them
    // v0.4.8: icon-only cluster (see the class comment). The
    // Update! indicator stays text on purpose — it is temporary
    // and must be unmissable.
    juce::TextButton linkBtn_ { "🔗" }, settingsBtn_ { "⚙" }, burgerBtn_ { "☰" },
                     updateBtn_ { "Update!" };
    juce::ComponentDragger dragger_;
    juce::Rectangle<int> lastSavedBounds_;
    bool vertical_ = false;
    juce::String lastPresetName_;
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
    sessionbar::GlowField glow_;
    sessionbar::MascotAnim mascotAnim_;
    juce::Image vizCache_, vizBlur_, glowCache_;
    double fxClock_ = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
