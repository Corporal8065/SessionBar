#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <sessionbar/State.hpp>
#include <sessionbar/Sessions.hpp>
#include <sessionbar/ReserveState.hpp>
#include <vector>
#include "Theme.h"
#include "ScreenSpace.h"
#include "BarLink.h" // v0.2.0 (k)
class SessionCell;
class SessionSettingsWindow;

// The bar itself: wordmark on the left, one cell per visible session
// across the middle, Reserve / Settings / minimise / quit on the right.
// Borderless and always-on-top (state.pinned); drag any non-interactive
// surface to move it; position + every setting persist to the settings
// JSON on change. A 1 Hz timer re-evaluates the three session clocks
// (pure core math on the current instant) so the countdowns tick every
// second and can never be stale by more than a second.
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
    void settingsChanged(); // apply + persist (theme, cells, pin, reserve)
    void notifyWindowGeometryChanged(); // MainWindow moved/resized feed (reserve machine events)
    void snapToDockEdge();
    void doArrange();
    void doRestore();
    juce::String screenSpaceStatus() const { return juce::String(screenSpace_.statusInfo()); }

private:
    void timerCallback() override;
    void rebuildCells();
    void refreshCells();
    void syncScreenSpace();
    void saveSettings();
    juce::Rectangle<int> currentWindowBounds() const;

    Theme theme_ = midnightKidsTheme();
    ScreenSpace screenSpace_;
    BarLinkClient linkClient_{ sessionbar::kLinkAppSession }; // v0.2.0 (k)
    sessionbar::ReserveMachine reserveMachine_;
    std::vector<std::unique_ptr<SessionCell>> cells_;
    std::unique_ptr<SessionSettingsWindow> settingsWin_;

    juce::Label wordmark_, versionLabel_;
    juce::TextButton reserveBtn_ { "Reserve" }, linkBtn_ { "Link" }, settingsBtn_ { "Settings" },

                     minBtn_ { "-" }, quitBtn_ { "X" };
    juce::ComponentDragger dragger_;
    juce::Rectangle<int> lastSavedBounds_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
