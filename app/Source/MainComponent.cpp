#include "MainComponent.h"
#include <sessionbar/CellLayout.hpp>
#include <algorithm>
#include <sessionbar/WindowGeom.hpp>
#include <BinaryData.h>
#include "SettingsStore.h"
#include "PresetIO.h"
#include "SettingsPalette.h"
#if JUCE_WINDOWS
 #include <windows.h> // native rects for the AppBar sync (physical pixels)
#endif
static const char* kVersion = "v0.3.0";
static const char* kVersionNum = "0.3.0";
static juce::String dockEdgeName(sessionbar::DockEdge e) {
    switch (e) {
        case sessionbar::DockEdge::Top: return "Top";
        case sessionbar::DockEdge::Bottom: return "Bottom";
        case sessionbar::DockEdge::Left: return "Left";
        case sessionbar::DockEdge::Right: return "Right";
    }
    return "Top";
}

// =====================================================================
// MascotView — the Anime-theme mascot cell (v0.3.0, item 18): the
// transparent-cutout character in her own cell at the END of the
// strip, hidden entirely outside the Anime theme. Ported from
// MeterBar's ModuleViews.h; the ARTWORK is MeterBar's own bundled
// files (byte-identical copies in this repo's assets/). Triggers are
// SessionBar's own: she hops on any session state change and cheers
// when a session OPENS.
// =====================================================================
class MascotView : public juce::Component {
public:
    bool excited = false;
    float bobPx = 0.f;
    MascotView() { setInterceptsMouseClicks(false, false); setOpaque(false); }
    void paint(juce::Graphics& g) override {
        ensureLoaded();
        auto& img = excited ? imgExcited : imgCalm;
        if (!img.isValid()) return;
        auto b = getLocalBounds().toFloat();
        float scale = std::min(b.getWidth() / (float)img.getWidth(), b.getHeight() / (float)img.getHeight());
        float w = img.getWidth() * scale, h = img.getHeight() * scale;
        float x = b.getCentreX() - w * 0.5f;
        float y = b.getBottom() - h + bobPx;
        g.setColour(juce::Colour(0x28000000));
        g.fillEllipse(b.getCentreX() - w * 0.30f, b.getBottom() - 6.f, w * 0.60f, 5.f);
        g.drawImage(img, x, y, w, h, 0, 0, img.getWidth(), img.getHeight());
    }
private:
    juce::Image imgCalm, imgExcited; bool loaded = false;
    void ensureLoaded() {
        if (loaded) return; loaded = true;
        imgCalm = juce::ImageFileFormat::loadFrom(BinaryData::animemascotcalmclear_png,
                                                  (size_t)BinaryData::animemascotcalmclear_pngSize);
        imgExcited = juce::ImageFileFormat::loadFrom(BinaryData::animemascotexcitedclear_png,
                                                     (size_t)BinaryData::animemascotexcitedclear_pngSize);
    }
};

// =====================================================================
// CellSplitter — the draggable divider between two cells (v0.3.0,
// item 10, MeterBar's ModuleSplitter pattern): drag reports the axis
// delta; the owner adjusts the pair via the core (CellLayout.hpp
// adjustPairWidths) and persists the widths by cell id.
// =====================================================================
class CellSplitter : public juce::Component {
public:
    MainComponent* owner = nullptr;
    int pairIndex = 0;
    bool verticalBar = false;
    std::function<void(int, int)> onDragDelta;
    CellSplitter() { setRepaintsOnMouseActivity(true); }
    void paint(juce::Graphics& g) override {
        const auto& th = owner->theme();
        auto b = getLocalBounds().toFloat();
        g.setColour(isMouseOver() ? th.accent : th.dim.withAlpha(0.6f));
        const float cx = b.getCentreX(), cy = b.getCentreY();
        if (verticalBar) {
            g.fillEllipse(cx - 8.f, cy - 1.5f, 3.f, 3.f);
            g.fillEllipse(cx + 5.f, cy - 1.5f, 3.f, 3.f);
        } else {
            g.fillEllipse(cx - 1.5f, cy - 8.f, 3.f, 3.f);
            g.fillEllipse(cx - 1.5f, cy + 5.f, 3.f, 3.f);
        }
    }
    void mouseDown(const juce::MouseEvent&) override {
        lastPos_ = verticalBar ? getMouseXYRelative().getY() : getMouseXYRelative().getX();
        setMouseCursor(verticalBar ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::LeftRightResizeCursor);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        auto rel = e.getPosition();
        const int pos = verticalBar ? rel.getY() : rel.getX();
        const int delta = pos - lastPos_;
        lastPos_ = pos;
        if (delta != 0 && onDragDelta) onDragDelta(pairIndex, delta);
    }
private:
    int lastPos_ = 0;
};

// =====================================================================
// SessionCell — one exchange: city, its local clock, state, countdown.
// A drag moves the whole bar (no click action in v0.1).
// =====================================================================
class SessionCell : public juce::Component, public juce::SettableTooltipClient {
public:
    MainComponent* owner = nullptr;
    const sessionbar::SessionDef* def = nullptr;
    std::string id; // "NY" / "LDN" / "TYO" — order + width persistence key
    sessionbar::SessionStatus status;
    bool has = false;

    explicit SessionCell(MainComponent* o) : owner(o) {}
    void setStatus(const sessionbar::SessionStatus& s) { status = s; has = true; repaint(); }

    void mouseDown(const juce::MouseEvent& e) override { owner->cellDragBegin(this, e); }
    void mouseDrag(const juce::MouseEvent& e) override { owner->cellDragMove(this, e); }
    void mouseUp(const juce::MouseEvent& e) override { owner->cellDragEnd(this, e); }

    void paint(juce::Graphics& g) override {
        const auto& th = owner->theme();
        auto b = getLocalBounds().toFloat().reduced(1.f);
        g.setColour(th.panel.withAlpha(owner->fxActive() ? 0.55f : 0.92f));
        g.fillRoundedRectangle(b, 8.f);
        g.setColour(th.border);
        g.drawRoundedRectangle(b, 8.f, 1.f);
        auto r = b.reduced(10.f, 6.f);
        // top row: city (left) + the session's own local time (right)
        auto top = r.removeFromTop(18.f);
        g.setColour(th.text); g.setFont(juce::Font(13.5f, juce::Font::bold));
        g.drawText(def ? juce::String(def->city) : juce::String("--"), top, juce::Justification::centredLeft);
        g.setColour(th.dim); g.setFont(juce::Font(13.f));
        g.drawText(has ? juce::String(sessionbar::formatLocalTime(status, owner->state.use24h)) : juce::String("--:--"),
                   top, juce::Justification::centredRight);
        // middle row: state dot + state word, colour-coded
        auto mid = r.removeFromTop(24.f);
        // v0.3.0 item 6: the state colour comes from the selected
        // session-state colour scheme (core SessionColors).
        const juce::Colour sc = !has ? th.dim : owner->stateColour(status.state);
        auto dot = mid.removeFromLeft(16.f).toFloat().withSizeKeepingCentre(9.f, 9.f);
        g.setColour(sc.withAlpha(0.25f)); g.fillEllipse(dot.expanded(1.5f));
        g.setColour(sc); g.fillEllipse(dot);
        g.setFont(juce::Font(16.5f, juce::Font::bold));
        g.drawText(has ? juce::String(sessionbar::stateName(status.state)) : juce::String("..."),
                   mid, juce::Justification::centredLeft);
        // bottom row: the countdown to the next state change
        g.setColour(th.dim); g.setFont(12.f);
        g.drawText(has ? juce::String(sessionbar::countdownText(status)) : juce::String(""),
                   r, juce::Justification::centredLeft);
    }
};

// =====================================================================
// Settings — one content component, rebuilt on open (family idiom).
// =====================================================================
class SettingsContent : public juce::Component {
public:
    MainComponent* owner = nullptr;
    sessionbar::SessionBarState* st = nullptr;
    std::function<void()> onChanged;
    SettingsContent() { setSize(560, 600); }
    void paint(juce::Graphics& g) override { g.fillAll(settingsPalette::base()); } // v0.3.0: the fixed pink-plum sheet

    void build() {
        removeAllChildren(); owned_.clear(); y_ = 10;
        if (!owner || !st) return;
        th_ = owner->theme();
        auto changed = [this]{ if (onChanged) onChanged(); };

        heading("Sessions shown");
        toggle("New York — NYSE 9:30-16:00 local (+ PRE 4:00-9:30, POST 16:00-20:00)", st->showNewYork, changed);
        toggle("London — LSE 8:00-16:30 local", st->showLondon, changed);
        toggle("Tokyo — TSE 9:00-15:00 local (lunch break 11:30-12:30)", st->showTokyo, changed);
        note("Exchange holidays and early closes ARE modeled (v0.2.0): NYSE / LSE / JPX calendars for 2026-2028 are embedded in the app — fully offline. Tokyo 2028 and the two LSE 2028 early closes are derived from statutory holidays (see BUILD_LOG).");
        heading("Clock");
        toggle("24-hour clock (off = 12-hour AM/PM)", st->use24h, changed);
        heading("Appearance");
        comboRow("Theme", { "Midnight Kids", "Midnight Pink", "Deep Teal", "Daylight", "Anime", "Clean Dark" }, st->themeName, changed);
        {
            auto* l = add(new juce::Label({}, "Background style")); placeLabel(l);
            auto* cb = add(new juce::ComboBox());
            cb->addItem("Clean Dark", 1); cb->addItem("Midnight Kids", 2); cb->addItem("Flat", 3); cb->addItem("Pastel", 4);
            cb->setSelectedId((int)st->backgroundStyle + 1, juce::dontSendNotification);
            cb->setBounds(24, y_, 220, 24);
            cb->onChange = [this, cb, changed]{ st->backgroundStyle = (sessionbar::BackgroundStyle)(cb->getSelectedId() - 1); changed(); };
            y_ += 32;
        }
        {
            auto* l = add(new juce::Label({}, "Background opacity")); placeLabel(l);
            auto* sl = add(new juce::Slider());
            sl->setRange(0.15, 1.0, 0.01);
            sl->setValue(st->backgroundOpacity, juce::dontSendNotification);
            sl->setBounds(24, y_, 300, 24);
            sl->onValueChange = [this, sl, changed]{ st->backgroundOpacity = (float)sl->getValue(); changed(); };
            y_ += 32;
        }
        {
            auto* l = add(new juce::Label({}, "Session-state colour scheme")); placeLabel(l);
            auto* cb = add(new juce::ComboBox());
            cb->addItem("Family", 1); cb->addItem("Plasma", 2); cb->addItem("Inferno", 3); cb->addItem("Viridis", 4); cb->addItem("Magma", 5);
            cb->setSelectedId(st->stateColorScheme + 1, juce::dontSendNotification);
            cb->setBounds(24, y_, 220, 24);
            cb->onChange = [this, cb, changed]{ st->stateColorScheme = cb->getSelectedId() - 1; changed(); };
            y_ += 32;
        }
        note("State colours drive each cell's dot + state word. Family = the shipping green/amber/red/teal; the others are stops from MeterBar's colour maps.");
        {
            auto* l = add(new juce::Label({}, "Background effect")); placeLabel(l);
            auto* cb = add(new juce::ComboBox());
            cb->addItem("Styled background", 1); cb->addItem("Visualizer particles", 2); cb->addItem("Reactive glow", 3);
            cb->setSelectedId(st->bgEffect + 1, juce::dontSendNotification);
            cb->setBounds(24, y_, 220, 24);
            cb->onChange = [this, cb, changed]{ st->bgEffect = cb->getSelectedId() - 1; changed(); };
            y_ += 32;
        }
        note("Visualizer: MeterBar's particle brushes. Reactive glow: the MiniMeters-style blurred bloom — linked to MeterBar it pulses with the music; on its own it breathes gently. Both render UNDER the cells.");
        sliderRow("Effect palette hue", 0.0, 1.0, st->vizHue, changed);
        sliderRow("Effect palette saturation", 0.0, 1.0, st->vizSat, changed);
        sliderRow("Effect palette spread", 0.0, 1.0, st->vizSpread, changed);
        sliderRow("Effect cycle seconds (0 = static)", 0.0, 60.0, st->vizCycle, changed);
        heading("Sound");
        toggle("UI click sounds on the bar's buttons", st->clickSounds, changed);
        note("MeterBar's synthesized clicks (pop / bloop / sparkle) on button presses. Default off.");
        heading("Window");
        toggle("Always on top", st->pinned, changed);
        {
            auto* cyc = add(new juce::TextButton("Cycle dock edge (Top / Bottom / Left / Right)"));
            cyc->setBounds(24, y_, 330, 24);
            cyc->onClick = [this]{ owner->cycleDockEdgeFromSettings(); rebuildSoon(); };
            y_ += 32;
        }
        note("Docked to a side edge, the bar turns vertical. Screen-space reservation stays a top/bottom feature (Windows AppBar + this bar's reserve machine); on a side edge the bar docks and stacks but does not reserve.");
        {
            auto* stk = add(new juce::TextButton("Stack all linked bars as one block"));
            stk->setBounds(24, y_, 330, 24);
            stk->onClick = [this]{ owner->doStackAll(); };
            y_ += 32;
        }
        toggle("Reserve screen space (Windows AppBar: maximised windows stop at the bar)", st->reserveScreenSpace, changed);
        {
            auto* ar = add(new juce::TextButton("Arrange windows"));
            ar->setBounds(24, y_, 140, 24);
            ar->onClick = [this]{ owner->doArrange(); rebuildSoon(); };
            auto* rs = add(new juce::TextButton("Restore windows"));
            rs->setBounds(172, y_, 140, 24);
            rs->onClick = [this]{ owner->doRestore(); rebuildSoon(); };
            y_ += 30;
        }
        heading("About");
        {
            auto* cu = add(new juce::TextButton("Check for updates"));
            cu->setBounds(24, y_, 200, 24);
            cu->onClick = [this]{ owner->checkForUpdates(); };
            y_ += 32;
        }
        note("SessionBar v0.3.0 — session clocks for New York / London / Tokyo with exchange calendars embedded (2026-2028).");
        status_ = add(new juce::Label({}, owner->screenSpaceStatus()));
        status_->setFont(11.f); status_->setColour(juce::Label::textColourId, settingsPalette::dimText());
        status_->setBounds(24, y_, 512, 18);
        y_ += 24;
        setSize(560, y_ + 8);
    }
private:
    template <typename T> T* add(T* c) { addAndMakeVisible(c); owned_.push_back(std::unique_ptr<juce::Component>(c)); return c; }
    void rebuildSoon() { juce::MessageManager::callAsync([this]{ build(); }); }
    void heading(const juce::String& t) {
        auto* l = add(new juce::Label({}, t));
        l->setFont(juce::Font(14.f, juce::Font::bold)); l->setColour(juce::Label::textColourId, settingsPalette::accent());
        l->setBounds(16, y_, 520, 20); y_ += 24;
    }
    void note(const juce::String& t) {
        auto* l = add(new juce::Label({}, t));
        l->setFont(11.f); l->setColour(juce::Label::textColourId, settingsPalette::dimText());
        l->setBounds(24, y_, 512, 16); y_ += 22;
    }
    void placeLabel(juce::Label* l) {
        l->setFont(12.5f); l->setColour(juce::Label::textColourId, settingsPalette::text());
        l->setBounds(24, y_, 300, 18); y_ += 20;
    }
    void toggle(const juce::String& text, bool& field, std::function<void()> changed) {
        auto* t = add(new juce::ToggleButton(text));
        t->setToggleState(field, juce::dontSendNotification);
        t->setColour(juce::ToggleButton::textColourId, settingsPalette::text());
        t->setBounds(24, y_, 512, 22);
        t->onClick = [&field, t, changed]{ field = t->getToggleState(); changed(); };
        y_ += 26;
    }
    void sliderRow(const juce::String& label, double mn, double mx, float& field, std::function<void()> changed) {
        auto* l = add(new juce::Label({}, label)); placeLabel(l);
        auto* sl = add(new juce::Slider());
        sl->setRange(mn, mx, 0.01);
        sl->setValue(field, juce::dontSendNotification);
        sl->setBounds(24, y_, 300, 24);
        sl->onValueChange = [&field, sl, changed]{ field = (float)sl->getValue(); changed(); };
        y_ += 32;
    }
    void comboRow(const juce::String& label, std::vector<const char*> items, std::string& field, std::function<void()> changed) {
        auto* l = add(new juce::Label({}, label)); placeLabel(l);
        auto* cb = add(new juce::ComboBox());
        int sel = 1, i = 1;
        for (auto* it : items) { cb->addItem(it, i); if (field == it) sel = i; ++i; }
        cb->setSelectedId(sel, juce::dontSendNotification);
        cb->setBounds(24, y_, 220, 24);
        cb->onChange = [&field, cb, changed]{ field = cb->getText().toStdString(); changed(); };
        y_ += 32;
    }
    Theme th_ = midnightKidsTheme();
    juce::Label* status_ = nullptr;
    std::vector<std::unique_ptr<juce::Component>> owned_;
    int y_ = 10;
};

class SessionSettingsWindow : public juce::DocumentWindow {
public:
    SettingsLookAndFeel settingsLnf; // declared before content so it outlives it
    SettingsContent content;
    SessionSettingsWindow() : juce::DocumentWindow("SessionBar Settings", settingsPalette::base(), juce::DocumentWindow::closeButton) {
        content.setLookAndFeel(&settingsLnf);
        viewport.setLookAndFeel(&settingsLnf);
        content.build();
        viewport.setViewedComponent(&content, false);
        viewport.setScrollBarsShown(true, false);
        setContentNonOwned(&viewport, true);
        setSize(580, 640); setResizable(true, false); setUsingNativeTitleBar(true);
    }
    ~SessionSettingsWindow() override {
        content.setLookAndFeel(nullptr);
        viewport.setLookAndFeel(nullptr);
    }
    void visibilityChanged() override {
        if (isVisible()) settingsPalette::applyDarkTitleBar(this);
        juce::DocumentWindow::visibilityChanged();
    }
    void closeButtonPressed() override { setVisible(false); }
private:
    juce::Viewport viewport;
};

// =====================================================================
// MainComponent
// =====================================================================
MainComponent::MainComponent() {
    state = settingsstore::load();
    // v0.3.0 migration: the top toolbar strip needs height the old
    // centred cluster didn't, and the wider cluster needs width —
    // adjust the saved size ONCE (uiVer).
    if (state.uiVer < 3) {
        state.windowH = juce::jlimit(56, 1200, state.windowH + 24);
        state.windowW = juce::jlimit(200, 3840, std::max(state.windowW, 1280));
        state.uiVer = 3;
    }
    theme_ = themeByName(state.themeName);
    setOpaque(false); // the window is non-opaque too: Background Opacity shows the desktop through
    setTooltip("SessionBar - New York / London / Tokyo market session clocks. Exchange holidays + early closes modeled (2026-2028, embedded); New York PRE/POST states shown.");

    wordmark_.setText("SessionBar", juce::dontSendNotification);
    wordmark_.setFont(juce::Font(15.f, juce::Font::bold));
    versionLabel_.setText(kVersion, juce::dontSendNotification);
    versionLabel_.setFont(11.f);
    for (auto* c : { (juce::Component*)&wordmark_, (juce::Component*)&versionLabel_ }) {
        addAndMakeVisible(c);
        c->setInterceptsMouseClicks(false, false); // clicks fall through to the bar drag
    }
    reserveBtn_.setClickingTogglesState(true);
    reserveBtn_.setToggleState(state.reserveScreenSpace, juce::dontSendNotification);
    reserveBtn_.setTooltip("Reserve screen space: Windows keeps maximised windows clear of the docked bar (top/bottom edges)");
    reserveBtn_.onClick = [this]{ uiClick(ClickSounds::Which::Pop); state.reserveScreenSpace = reserveBtn_.getToggleState(); saveSettings(); syncScreenSpace(); };
    linkBtn_.setClickingTogglesState(true);
    linkBtn_.setToggleState(state.linkEnabled, juce::dontSendNotification);
    linkBtn_.setTooltip("Link: snap, drag, dock and share the background with the other family bars (MeterBar / TickerBar) when their Link is on too");
    linkBtn_.onClick = [this]{ uiClick(ClickSounds::Which::Pop); state.linkEnabled = linkBtn_.getToggleState(); saveSettings(); };
    arrangeBtn_.setTooltip("Arrange windows: slide other windows clear of the bar's docked edge");
    arrangeBtn_.onClick = [this]{ uiClick(ClickSounds::Which::Pop); doArrange(); };
    restoreBtn_.setTooltip("Restore windows moved by Arrange");
    restoreBtn_.onClick = [this]{ uiClick(ClickSounds::Which::Pop); doRestore(); };
    presetBtn_.setTooltip("Presets: named full configurations - appearance, sessions AND window position");
    presetBtn_.onClick = [this]{ uiClick(ClickSounds::Which::Pop); showPresetMenu(); };
    stackBtn_.setTooltip("Stack: turn Link on, snap this bar to its dock edge, and line every linked bar up flush against it as one block");
    stackBtn_.onClick = [this]{ uiClick(ClickSounds::Which::Sparkle); doStackAll(); };
    dockBtn_.setTooltip("Dock edge: click to cycle Top / Bottom / Left / Right - the bar snaps to that edge (side edges turn it vertical)");
    dockBtn_.onClick = [this]{ uiClick(ClickSounds::Which::Pop); cycleDockEdgeFromSettings(); };
    settingsBtn_.onClick = [this]{
        uiClick(ClickSounds::Which::Pop);
        settingsWin_->content.build();
        settingsWin_->setVisible(true); settingsWin_->toFront(true);
    };
    updateBtn_.setVisible(false);
    updateBtn_.onClick = [this]{ uiClick(ClickSounds::Which::Pop);
        if (pendingRelease_.ok && pendingRelease_.newer) showUpdatePanel(pendingRelease_); };
    minBtn_.onClick = [this]{ if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
                                 if (auto* peer = w->getPeer()) peer->setMinimised(true); };
    quitBtn_.onClick = []{ juce::JUCEApplication::getInstance()->systemRequestedQuit(); };
    updateDockBtn();
    for (auto* b : { &reserveBtn_, &linkBtn_, &arrangeBtn_, &restoreBtn_, &presetBtn_, &stackBtn_,
                     &dockBtn_, &settingsBtn_, &updateBtn_, &minBtn_, &quitBtn_ }) addAndMakeVisible(b);
    applyChrome();
    // v0.2.0 (k): family link wiring (protocol: core BarLink.hpp).
    linkClient_.getBounds = [this]{ return currentWindowBounds(); };
    linkClient_.setBounds = [this](juce::Rectangle<int> r){
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) w->setBounds(r);
    };
    linkClient_.getBg = [this]{ return BarLinkClient::Bg{ (int)state.backgroundStyle, state.backgroundOpacity }; };
    linkClient_.applyBg = [this](const BarLinkClient::Bg& bg){
        state.backgroundStyle = (sessionbar::BackgroundStyle)juce::jlimit(0, 3, bg.style);
        state.backgroundOpacity = juce::jlimit(0.15f, 1.0f, bg.opacity);
        saveSettings(); repaint();
        // v0.2.1: an open Settings window must not keep showing the
        // pre-adoption values — rebuild it so the adopted style /
        // opacity are what its controls display.
        if (settingsWin_ && settingsWin_->isVisible()) settingsWin_->content.build();
    };
    linkClient_.getDockedEdge = [this]{
        // the reserve machine is top/bottom-only: side edges report
        // "not reserve-docked" even when the bar sits at one.
        if (!reserveMachine_.reserved()) return -1;
        return state.dockEdge == sessionbar::DockEdge::Top ? 0
             : state.dockEdge == sessionbar::DockEdge::Bottom ? 1 : -1;
    };
    linkClient_.isLinked = [this]{ return state.linkEnabled; };
    linkClient_.start();

    settingsWin_ = std::make_unique<SessionSettingsWindow>();
    settingsWin_->content.owner = this;
    settingsWin_->content.st = &state;
    settingsWin_->content.onChanged = [this]{ settingsChanged(); };
    settingsWin_->content.build();

    mascot_ = std::make_unique<MascotView>();
    addChildComponent(mascot_.get()); // Anime theme only (applyTheme drives visibility)
    fxPump_.onTick = [this]{ stepFx(); };

    applyTheme();
    rebuildCells();
    refreshCells();
    syncScreenSpace();
    syncFxPump();
    startTimerHz(1); // countdowns tick every second; the core math is three evaluate() calls
    setSize(state.windowW, state.windowH);
    // v0.3.0 item 1: silent background update check on launch.
    if (sbupdate::configured()) startUpdateCheck(true);
}

MainComponent::~MainComponent() {
    stopTimer();
    saveSettings();
    screenSpace_.release();
}

void MainComponent::paint(juce::Graphics& g) {
    if (state.bgEffect == 1) drawVisualizerBackground(g);
    else if (state.bgEffect == 2) drawGlowBackground(g);
    else fillStyledBackground(g, getLocalBounds().toFloat(), state.backgroundStyle, theme_,
                              juce::jlimit(0.15f, 1.0f, state.backgroundOpacity));
    g.setColour(theme_.border);
    g.drawRect(getLocalBounds().toFloat(), 1.f);
}

void MainComponent::resized() {
    auto area = getLocalBounds();
    const bool sideDock = state.dockEdge == sessionbar::DockEdge::Left || state.dockEdge == sessionbar::DockEdge::Right;
    vertical_ = sideDock && area.getHeight() > area.getWidth();
    struct BtnW { juce::TextButton* b; int w; };
    const BtnW cluster[] = {
        { &reserveBtn_, 64 }, { &linkBtn_, 48 }, { &arrangeBtn_, 66 }, { &restoreBtn_, 62 },
        { &presetBtn_, 60 }, { &stackBtn_, 52 }, { &dockBtn_, 62 }, { &settingsBtn_, 72 },
        { &updateBtn_, 68 }, { &minBtn_, 30 }, { &quitBtn_, 52 },
    };
    auto btnVisible = [this](juce::TextButton* b) { return b != &updateBtn_ || updateAvailable_; };
    updateBtn_.setVisible(updateAvailable_);
    auto r = area.reduced(8, 6);
    const int n = (int)cells_.size();
    const bool anime = state.themeName == "Anime";
    if (!vertical_) {
        auto strip = r.removeFromTop(30);
        int clusterW = 0;
        for (auto& bw : cluster) if (btnVisible(bw.b)) clusterW += bw.w + 4;
        auto cl = strip.removeFromRight(clusterW);
        for (int i = (int)std::size(cluster) - 1; i >= 0; --i) {
            if (!btnVisible(cluster[i].b)) continue;
            auto cellRect = cl.removeFromRight(cluster[i].w);
            cluster[i].b->setBounds(cellRect.withSizeKeepingCentre(cluster[i].w, 24));
            cl.removeFromRight(4);
        }
        wordmark_.setBounds(strip.removeFromLeft(104).withSizeKeepingCentre(104, 20));
        versionLabel_.setBounds(strip.withSizeKeepingCentre(strip.getWidth(), 14));
        r.removeFromTop(4);
        if (anime && mascot_) { mascot_->setBounds(r.removeFromRight(72)); r.removeFromRight(6); }
        if (n > 0) {
            const int boundary = 16; // 10 px splitter + 6 px air
            std::vector<int> expl((size_t)n, 0);
            for (int i = 0; i < n; ++i) {
                auto it = state.cellWidths.find(cells_[(size_t)i]->id);
                if (it != state.cellWidths.end()) expl[(size_t)i] = it->second;
            }
            auto widths = sessionbar::solveCellWidths(r.getWidth(), boundary, expl, 120);
            int x = r.getX();
            for (int i = 0; i < n; ++i) {
                cells_[(size_t)i]->setBounds(x, r.getY(), widths[(size_t)i], r.getHeight());
                x += widths[(size_t)i];
                if (i + 1 < n && i < (int)splitters_.size()) {
                    splitters_[(size_t)i]->verticalBar = false;
                    splitters_[(size_t)i]->setBounds(x + 3, r.getY(), 10, r.getHeight());
                }
                x += boundary;
            }
        }
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
            w->setResizeLimits(1180, 92, 3840, 1200);
    } else {
        auto wm = r.removeFromTop(24);
        wordmark_.setBounds(wm.removeFromLeft(104).withSizeKeepingCentre(104, 20));
        versionLabel_.setBounds(wm.withSizeKeepingCentre(wm.getWidth(), 14));
        r.removeFromTop(4);
        int bx = r.getX(), by = r.getY();
        for (auto& bw : cluster) {
            if (!btnVisible(bw.b)) continue;
            if (bx + bw.w > r.getRight()) { bx = r.getX(); by += 28; }
            bw.b->setBounds(bx, by, bw.w, 24);
            bx += bw.w + 4;
        }
        r.removeFromTop(by - r.getY() + 28);
        const int boundary = 16;
        if (n > 0) {
            std::vector<int> expl((size_t)n, 0);
            for (int i = 0; i < n; ++i) {
                auto it = state.cellWidths.find(cells_[(size_t)i]->id);
                if (it != state.cellWidths.end()) expl[(size_t)i] = it->second;
            }
            auto heights = sessionbar::solveCellWidths(r.getHeight(), boundary, expl, 56);
            for (int i = 0; i < n; ++i) {
                cells_[(size_t)i]->setBounds(r.getX(), r.getY(), r.getWidth(), heights[(size_t)i]);
                r.removeFromTop(heights[(size_t)i]);
                if (i + 1 < n && i < (int)splitters_.size()) {
                    splitters_[(size_t)i]->verticalBar = true;
                    splitters_[(size_t)i]->setBounds(r.getX(), r.getY() + 3, r.getWidth(), 10);
                }
                r.removeFromTop(boundary);
            }
        }
        if (anime && mascot_) mascot_->setBounds(r.removeFromTop(96));
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
            w->setResizeLimits(200, 340, 1200, 3840);
    }
    notifyWindowGeometryChanged(); // a resize is a reserve-machine event (family discipline)
}

void MainComponent::mouseDown(const juce::MouseEvent& e) { windowDragStarted(e); }
void MainComponent::mouseDrag(const juce::MouseEvent& e) { windowDragged(e); }
void MainComponent::windowDragStarted(const juce::MouseEvent& e) {
    // ComponentDragger works in screen coordinates, so events forwarded
    // from child cells drag the top-level window correctly.
    if (auto* top = getTopLevelComponent()) dragger_.startDraggingComponent(top, e);
}
void MainComponent::windowDragged(const juce::MouseEvent& e) {
    if (auto* top = getTopLevelComponent()) dragger_.dragComponent(top, e, nullptr);
}

void MainComponent::rebuildCells() {
    cells_.clear();
    auto defFor = [](const std::string& id) -> const sessionbar::SessionDef* {
        if (id == "NY") return &sessionbar::newYorkDef();
        if (id == "LDN") return &sessionbar::londonDef();
        return &sessionbar::tokyoDef();
    };
    auto shown = [&](const std::string& id) {
        return id == "NY" ? state.showNewYork : id == "LDN" ? state.showLondon : state.showTokyo;
    };
    auto tipFor = [](const std::string& id) -> const char* {
        if (id == "NY") return "NYSE regular hours 9:30 AM - 4:00 PM New York time - drag sideways to reorder";
        if (id == "LDN") return "LSE regular hours 8:00 AM - 4:30 PM London time - drag sideways to reorder";
        return "TSE regular hours 9:00 AM - 3:00 PM Tokyo time, lunch break 11:30 AM - 12:30 PM - drag sideways to reorder";
    };
    for (auto& id : state.cellOrder) {
        if (!shown(id)) continue;
        auto c = std::make_unique<SessionCell>(this);
        c->def = defFor(id);
        c->id = id;
        c->setTooltip(tipFor(id));
        addAndMakeVisible(c.get());
        cells_.push_back(std::move(c));
    }
    rebuildSplitters();
    resized();
}

void MainComponent::rebuildSplitters() {
    splitters_.clear();
    for (int i = 0; i + 1 < (int)cells_.size(); ++i) {
        auto sp = std::make_unique<CellSplitter>();
        sp->owner = this;
        sp->pairIndex = i;
        sp->onDragDelta = [this](int pair, int delta) {
            if (pair < 0 || pair + 1 >= (int)cells_.size()) return;
            const int w1 = vertical_ ? cells_[(size_t)pair]->getHeight() : cells_[(size_t)pair]->getWidth();
            const int w2 = vertical_ ? cells_[(size_t)pair + 1]->getHeight() : cells_[(size_t)pair + 1]->getWidth();
            auto pr = sessionbar::adjustPairWidths(w1, w2, delta, vertical_ ? 56 : 120);
            state.cellWidths[cells_[(size_t)pair]->id] = pr.first;
            state.cellWidths[cells_[(size_t)pair + 1]->id] = pr.second;
            saveSettings();
            resized();
        };
        addAndMakeVisible(sp.get());
        splitters_.push_back(std::move(sp));
    }
}

// --- cell drag arbitration (v0.3.0 item 9): direction decides —
// mostly-along-the-bar past a threshold becomes a cell reorder,
// anything else the ordinary window drag (started at the decision
// point: no jump, just a few px of dead zone). ---
void MainComponent::cellDragBegin(juce::Component* cell, const juce::MouseEvent& e) {
    dragCell_ = cell;
    dragMode_ = 0;
    dragStartScreen_ = e.getScreenPosition();
    dragCellStartBounds_ = cell->getBounds();
}
void MainComponent::cellDragMove(juce::Component* cell, const juce::MouseEvent& e) {
    if (cell != dragCell_) return;
    const auto now = e.getScreenPosition();
    const int dx = now.x - dragStartScreen_.x, dy = now.y - dragStartScreen_.y;
    const int axis = vertical_ ? dy : dx, cross = vertical_ ? dx : dy;
    if (dragMode_ == 0) {
        if (std::abs(axis) > 14 && std::abs(axis) > 1.6 * std::abs(cross)) {
            dragMode_ = 2;
            cell->toFront(false);
        } else if (std::abs(dx) + std::abs(dy) > 6) {
            dragMode_ = 1;
            windowDragStarted(e);
            windowDragged(e);
            return;
        } else return;
    }
    if (dragMode_ == 1) { windowDragged(e); return; }
    auto b = dragCellStartBounds_;
    if (vertical_) b.setY(b.getY() + dy); else b.setX(b.getX() + dx);
    cell->setBounds(b);
}
bool MainComponent::cellDragEnd(juce::Component* cell, const juce::MouseEvent&) {
    const bool wasReorder = dragMode_ == 2 && cell == dragCell_;
    dragCell_ = nullptr;
    dragMode_ = 0;
    if (!wasReorder) return false;
    int from = -1;
    for (int i = 0; i < (int)cells_.size(); ++i) if (cells_[(size_t)i].get() == cell) from = i;
    if (from < 0) { resized(); return true; }
    std::vector<int> centres;
    for (int i = 0; i < (int)cells_.size(); ++i) {
        if (i == from) continue;
        auto b = cells_[(size_t)i]->getBounds();
        centres.push_back(vertical_ ? b.getCentreY() : b.getCentreX());
    }
    auto db = cell->getBounds();
    const int slot = sessionbar::dropSlot(centres, vertical_ ? db.getCentreY() : db.getCentreX());
    auto order = sessionbar::indexOrderAfterMove((int)cells_.size(), from, slot);
    std::vector<std::string> shownOrder;
    for (int idx : order) shownOrder.push_back(cells_[(size_t)idx]->id);
    // write back: shown cells take their new sequence; hidden cells
    // keep their previous relative order after them.
    std::vector<std::string> newOrder = shownOrder;
    for (auto& id : state.cellOrder)
        if (std::find(newOrder.begin(), newOrder.end(), id) == newOrder.end()) newOrder.push_back(id);
    if (newOrder != state.cellOrder) {
        state.cellOrder = newOrder;
        saveSettings();
    }
    rebuildCells();
    refreshCells();
    return true;
}

void MainComponent::refreshCells() {
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    for (auto& c : cells_) {
        if (!c->def) continue;
        auto st = sessionbar::evaluate(*c->def, now);
        // item 18 triggers: any state change hops; a change INTO
        // Open (the bell) cheers.
        auto it = prevStates_.find(c->id);
        if (it != prevStates_.end() && it->second != st.state) {
            const long long nowMs = juce::Time::currentTimeMillis();
            mascotAnim_.triggerBob(nowMs);
            if (st.state == sessionbar::SessionState::Open) mascotAnim_.triggerExcited(nowMs, 3000);
        }
        prevStates_[c->id] = st.state;
        c->setStatus(st);
    }
}

void MainComponent::timerCallback() {
    pollUpdater();
    refreshCells();
    auto b = currentWindowBounds();
    if (b != lastSavedBounds_ && !b.isEmpty()) { lastSavedBounds_ = b; saveSettings(); }
}

juce::Rectangle<int> MainComponent::currentWindowBounds() const {
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) return w->getBounds();
    return {};
}

void MainComponent::saveSettings() {
    auto b = currentWindowBounds();
    if (!b.isEmpty()) { state.windowX = b.getX(); state.windowY = b.getY(); state.windowW = b.getWidth(); state.windowH = b.getHeight(); }
    settingsstore::save(state);
}

// v0.2.1 (family chrome): the control cluster wears MeterBar's
// toolbar colours verbatim (panel face, deepest "on" face, accent
// text off, body text on) — v0.2.0 shipped the buttons in stock JUCE
// colours, which is most of why SessionBar read as a different
// program next to MeterBar. Called on construction and whenever the
// theme changes.
void MainComponent::applyChrome() {
    for (auto* b : { &reserveBtn_, &linkBtn_, &arrangeBtn_, &restoreBtn_, &presetBtn_, &stackBtn_,
                     &dockBtn_, &settingsBtn_, &minBtn_, &quitBtn_ }) {
        b->setColour(juce::TextButton::buttonColourId, theme_.panel);
        b->setColour(juce::TextButton::buttonOnColourId, theme_.accent);
        b->setColour(juce::TextButton::textColourOffId, theme_.text);
        b->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }
    updateBtn_.setColour(juce::TextButton::buttonColourId, theme_.accent);
    updateBtn_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    updateBtn_.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    wordmark_.setColour(juce::Label::textColourId, theme_.accent);
    versionLabel_.setColour(juce::Label::textColourId, theme_.dim);
}

void MainComponent::updateDockBtn() {
    dockBtn_.setButtonText(dockEdgeName(state.dockEdge));
}

void MainComponent::cycleDockEdgeFromSettings() {
    state.dockEdge = (sessionbar::DockEdge)(((int)state.dockEdge + 1) % 4);
    snapToDockEdge();
}

void MainComponent::settingsChanged() {
    applyTheme();
    linkClient_.notifyLocalSettings();
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) w->setAlwaysOnTop(state.pinned);
    if (reserveBtn_.getToggleState() != state.reserveScreenSpace)
        reserveBtn_.setToggleState(state.reserveScreenSpace, juce::dontSendNotification);
    if (linkBtn_.getToggleState() != state.linkEnabled)
        linkBtn_.setToggleState(state.linkEnabled, juce::dontSendNotification);
    updateDockBtn();
    rebuildCells();
    refreshCells();
    saveSettings();
    syncScreenSpace();
    syncFxPump();
    repaint();
}

void MainComponent::snapToDockEdge() {
    auto* w = findParentComponentOfClass<juce::DocumentWindow>();
    if (!w) return;
    auto display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(w->getBounds());
    if (display == nullptr) return;
    auto area = display->totalArea;
    auto b = w->getBounds();
    const bool side = state.dockEdge == sessionbar::DockEdge::Left || state.dockEdge == sessionbar::DockEdge::Right;
    // Entering a side edge with a landscape shape (or a top/bottom
    // edge with a portrait shape) swaps the window's axes so the bar
    // actually turns (item 8's "true vertical orientation").
    if (side && b.getWidth() > b.getHeight()) {
        const int nw = juce::jlimit(200, 1200, b.getHeight());
        const int nh = juce::jlimit(340, 1600, b.getWidth());
        b.setSize(nw, nh);
    } else if (!side && b.getHeight() > b.getWidth()) {
        const int nw = juce::jlimit(420, 3840, b.getHeight());
        const int nh = juce::jlimit(92, 300, b.getWidth());
        b.setSize(nw, nh);
    }
    switch (state.dockEdge) {
        case sessionbar::DockEdge::Top:    b.setY(area.getY()); break;
        case sessionbar::DockEdge::Bottom: b.setY(area.getBottom() - b.getHeight()); break;
        case sessionbar::DockEdge::Left:   b.setX(area.getX()); break;
        case sessionbar::DockEdge::Right:  b.setX(area.getRight() - b.getWidth()); break;
    }
    b.setX(juce::jlimit(area.getX(), area.getRight() - b.getWidth(), b.getX()));
    b.setY(juce::jlimit(area.getY(), area.getBottom() - b.getHeight(), b.getY()));
    w->setBounds(b);
    updateDockBtn(); // the Settings dock control lands here too — the bar button must agree
    saveSettings();
    syncScreenSpace();
}

// Reserve sync (MeterBar v0.18.1 pattern, edge-generalised): while the
// toggle is ON and the bar sits flush at its docked edge of its monitor,
// register as an AppBar. This function only FEEDS one observation to
// sessionbar::ReserveMachine (hysteresis: dock at <= 2 px, release only
// past 8 px; actions are transitions) and performs the single action
// the machine returns. It runs on real events only — the toggle,
// Settings changes, dock commands, and the window's own moved/resized
// notifications (MainWindow forwards them via
// notifyWindowGeometryChanged). No timer touches other windows' space.
void MainComponent::syncScreenSpace() {
#if JUCE_WINDOWS
    auto* w = findParentComponentOfClass<juce::DocumentWindow>();
    HWND hwnd = (w && w->getPeer()) ? static_cast<HWND>(w->getPeer()->getNativeHandle()) : nullptr;
    if (hwnd) {
        RECT wr{}; GetWindowRect(hwnd, &wr);
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{}; mi.cbSize = sizeof mi; GetMonitorInfoW(mon, &mi);
        const bool sideEdge = state.dockEdge == sessionbar::DockEdge::Left || state.dockEdge == sessionbar::DockEdge::Right;
        const bool top = state.dockEdge == sessionbar::DockEdge::Top;
        const int gap = sideEdge ? 9999 // side edges: the machine only ever releases (see below)
                        : top ? std::abs(wr.top - mi.rcMonitor.top)
                              : std::abs(wr.bottom - mi.rcMonitor.bottom);
        const sessionbar::IntRect rect{ wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top };
        const long long monId = (long long)(uintptr_t)mon;
        // v0.3.0: reservation is a top/bottom feature — on a side
        // edge the machine is fed toggle-OFF so any live reservation
        // releases cleanly and none can register.
        switch (reserveMachine_.update(state.reserveScreenSpace && !sideEdge, top ? 0 : 1, gap, rect, monId)) {
            case sessionbar::ReserveAction::Register:
            case sessionbar::ReserveAction::Update:
                screenSpace_.reserve(hwnd, top ? 0 : 1, rect.x, rect.y, rect.w, rect.h);
                break;
            case sessionbar::ReserveAction::Release:
                screenSpace_.release();
                // Toggle-OFF is a full stop: the reservation is removed
                // AND any arranged windows go home in the same action.
                if (!state.reserveScreenSpace) screenSpace_.restore();
                break;
            case sessionbar::ReserveAction::None:
                break;
        }
    }
#else
    screenSpace_.release();
#endif
    if (reserveBtn_.getToggleState() != state.reserveScreenSpace)
        reserveBtn_.setToggleState(state.reserveScreenSpace, juce::dontSendNotification);
}
void MainComponent::notifyWindowGeometryChanged() { syncScreenSpace(); linkClient_.notifyLocalGeometry(); }

void MainComponent::doArrange() {
#if JUCE_WINDOWS
    auto* w = findParentComponentOfClass<juce::DocumentWindow>();
    HWND hwnd = (w && w->getPeer()) ? static_cast<HWND>(w->getPeer()->getNativeHandle()) : nullptr;
    if (hwnd) {
        RECT wr{}; GetWindowRect(hwnd, &wr);
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{}; mi.cbSize = sizeof mi; GetMonitorInfoW(mon, &mi);
        const bool top = state.dockEdge == sessionbar::DockEdge::Top;
        const int strip = top ? (wr.bottom - mi.rcMonitor.top) : (mi.rcMonitor.bottom - wr.top);
        screenSpace_.arrange(hwnd, strip, top ? 0 : 1);
        return;
    }
#endif
    screenSpace_.arrange(nullptr, 0, 0); // stub: records the honest "unavailable" status
}
void MainComponent::doRestore() { screenSpace_.restore(); }

// =====================================================================
// v0.3.0: theme application
// =====================================================================
void MainComponent::applyTheme() {
    theme_ = themeByName(state.themeName);
    applyChrome();
    if (mascot_) mascot_->setVisible(state.themeName == "Anime");
    for (auto& c : cells_) c->repaint();
    resized(); // the mascot cell (Anime) and cell softening depend on the theme
    repaint();
}
void MainComponent::uiClick(ClickSounds::Which w) {
    if (state.clickSounds) clicks_.play(w); // the toggle is the gate (item 5)
}
juce::Colour MainComponent::stateColour(sessionbar::SessionState s) const {
    auto c = sessionbar::sessionStateColour(state.stateColorScheme, s);
    return juce::Colour((juce::uint8)c.r, (juce::uint8)c.g, (juce::uint8)c.b);
}

// =====================================================================
// v0.3.0 item 12: one-click stack (see TickerBar's twin).
// =====================================================================
void MainComponent::doStackAll() {
    if (!state.linkEnabled) {
        state.linkEnabled = true;
        linkBtn_.setToggleState(true, juce::dontSendNotification);
        saveSettings();
    }
    snapToDockEdge();
    linkClient_.requestStackAll((int)state.dockEdge, currentWindowBounds());
}

// =====================================================================
// v0.3.0 item 2: named presets — full configurations, position
// included; loading one moves the bar (clamped on-screen).
// =====================================================================
void MainComponent::showPresetMenu() {
    enum { idNew = 1, idSave, idSaveAs, idOpenFolder, idLoadBase = 100 };
    juce::PopupMenu m;
    m.addItem(idNew, "New Preset (defaults)");
    m.addItem(idSave, "Save", lastPresetName_.isNotEmpty());
    m.addItem(idSaveAs, "Save As...");
    m.addItem(idOpenFolder, "Open Preset Folder");
    m.addSeparator();
    auto files = presetio::listPresets();
    for (int i = 0; i < files.size(); ++i) m.addItem(idLoadBase + i, files[i].getFileNameWithoutExtension());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presetBtn_), [this, files](int r) {
        if (r == 1) {
            auto keepTheme = state.themeName;
            state = sessionbar::SessionBarState{};
            state.themeName = keepTheme;
            lastPresetName_.clear();
            settingsChanged();
        }
        else if (r == 2) savePresetInteractive(false);
        else if (r == 3) savePresetInteractive(true);
        else if (r == 4) presetio::presetFolder().revealToUser();
        else if (r >= 100 && r < 100 + files.size()) loadPresetFile(files[r - 100]);
    });
}
void MainComponent::savePresetInteractive(bool saveAs) {
    saveSettings(); // refresh the live window rect into state first — a preset includes position
    if (!saveAs && lastPresetName_.isNotEmpty()) { presetio::save(state, lastPresetName_); return; }
    auto w = std::make_shared<juce::AlertWindow>("Save Preset", "Preset name:", juce::AlertWindow::NoIcon);
    w->addTextEditor("name", lastPresetName_.isEmpty() ? juce::String("My Preset") : lastPresetName_);
    w->addButton("Save", 1); w->addButton("Cancel", 0);
    w->enterModalState(true, juce::ModalCallbackFunction::create([this, w](int r) {
        if (r == 1) {
            lastPresetName_ = w->getTextEditorContents("name");
            presetio::save(state, lastPresetName_);
        }
    }));
}
void MainComponent::loadPresetFile(const juce::File& f) {
    sessionbar::SessionBarState loaded;
    juce::String name;
    if (!presetio::load(f, loaded, name)) return;
    state = loaded;
    lastPresetName_ = name;
    const auto rect = juce::Rectangle<int>(state.windowX, state.windowY, state.windowW, state.windowH);
    settingsChanged(); // its saveSettings() re-stamps the CURRENT window rect...
    state.windowX = rect.getX(); state.windowY = rect.getY(); // ...so restore the preset's rect, then move
    state.windowW = rect.getWidth(); state.windowH = rect.getHeight();
    applyWindowRectFromState();
    saveSettings();
    uiClick(ClickSounds::Which::Sparkle);
}
void MainComponent::applyWindowRectFromState() {
    auto* w = findParentComponentOfClass<juce::DocumentWindow>();
    if (!w) return;
    std::vector<sessionbar::IntRect> areas;
    for (auto& d : juce::Desktop::getInstance().getDisplays().displays) {
        auto ua = d.userArea;
        areas.push_back({ ua.getX(), ua.getY(), ua.getWidth(), ua.getHeight() });
    }
    auto clamped = sessionbar::clampRectToWorkAreas(
        { state.windowX, state.windowY, state.windowW, state.windowH }, areas);
    w->setBounds(clamped.x, clamped.y, clamped.w, clamped.h);
}

// =====================================================================
// v0.3.0 item 1: the in-app updater — MeterBar's flow ported
// (check -> notes -> download -> size/SHA-256 verify -> install ->
// relaunch), with the honest private-repo fallback.
// =====================================================================
void MainComponent::startUpdateCheck(bool fromLaunch) {
    if (updateCheckPending_) return;
    updateState_ = std::make_shared<sbupdate::SharedState>();
    updateCheckPending_ = true;
    updateCheckFromLaunch_ = fromLaunch;
    sbupdate::checkLatestAsync(updateState_, juce::String(kVersionNum));
}
void MainComponent::checkForUpdates() {
    if (!sbupdate::configured()) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Updates",
            "SessionBar " + juce::String(kVersion) + " - updates are not configured in this build.");
        return;
    }
    if (updateCheckPending_) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Updates",
            "An update check is already running - its result will appear when it finishes.");
        return;
    }
    startUpdateCheck(false);
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Updates",
        "Checking GitHub for the latest SessionBar release...\nThe result will pop up here when the check finishes.");
}
void MainComponent::pollUpdater() {
    if (updateCheckPending_ && updateState_ && updateState_->done.load(std::memory_order_acquire)) {
        updateCheckPending_ = false;
        const bool wasLaunch = updateCheckFromLaunch_;
        updateCheckFromLaunch_ = false;
        const auto r = updateState_->result;
        if (wasLaunch) {
            if (r.ok && r.newer) {
                pendingRelease_ = r;
                updateAvailable_ = true;
                resized();
                repaint();
            }
        } else {
            presentUpdateResult(r);
        }
    }
    if (updateDownloadPending_ && updateDownload_ && updateDownload_->isFinished()) {
        updateDownloadPending_ = false;
        const int dlStatus = updateDownload_->statusCode();
        const bool dlOk = !updateDownload_->hadError() && dlStatus == 200;
        updateDownload_.reset();
        if (!dlOk) {
            if (sessionbar::httpStatusBlocksAuth(dlStatus)) {
                showPrivateRepoFallback("The installer download itself was refused (HTTP "
                    + juce::String(dlStatus) + ") - the same repository-visibility limit as the check.");
                return;
            }
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Update",
                "The update download did not complete.\nSessionBar has NOT been changed - you are still on " + juce::String(kVersion) + ".");
            return;
        }
        if (pendingRelease_.assetSize > 0 && updateInstallerFile_.getSize() != pendingRelease_.assetSize) {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Update",
                "The downloaded installer is the wrong size (expected " + juce::String(pendingRelease_.assetSize)
                + " bytes, got " + juce::String(updateInstallerFile_.getSize()) + ").\nIt will NOT be run. SessionBar has not been changed.");
            return;
        }
        if (pendingRelease_.assetDigest.isNotEmpty()) {
            juce::String want = pendingRelease_.assetDigest;
            if (want.startsWithIgnoreCase("sha256:")) want = want.substring(7);
            const juce::String got = sbupdate::sha256HexOfFile(updateInstallerFile_);
            if (!got.equalsIgnoreCase(want)) {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Update",
                    "The downloaded installer's SHA-256 does not match the one GitHub published for this release.\n"
                    "It will NOT be run. SessionBar has not been changed.");
                return;
            }
        }
        juce::ChildProcess installer;
        const bool launched = installer.start({ updateInstallerFile_.getFullPathName(),
                                                "/SILENT", "/CLOSEAPPLICATIONS", "/RESTARTAPPLICATIONS", "/NORESTART" });
        if (!launched) {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Update",
                "The installer was downloaded to\n" + updateInstallerFile_.getFullPathName()
                + "\nbut could not be started. You can run it by hand;\nSessionBar is still on " + juce::String(kVersion) + ".");
            return;
        }
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
}
void MainComponent::presentUpdateResult(const sbupdate::ReleaseInfo& r) {
    pendingRelease_ = r;
    if (r.authBlocked) {
        showPrivateRepoFallback("SessionBar asked GitHub for the latest release, but GitHub "
            "answers only signed-in users for a repository it treats as private - and SessionBar "
            "deliberately carries no sign-in token.");
        return;
    }
    if (!r.ok) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Updates",
            "Update check failed.\n" + r.error + "\nYou are still on SessionBar " + juce::String(kVersion) + ".");
        return;
    }
    if (!r.newer) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Updates",
            "You are up to date.\nSessionBar " + juce::String(kVersion) + " is the latest release (" + r.tag + ").");
        return;
    }
    updateAvailable_ = true;
    resized();
    showUpdatePanel(r);
}
void MainComponent::showUpdatePanel(const sbupdate::ReleaseInfo& r) {
    auto* w = new juce::AlertWindow("Update available",
        "SessionBar " + r.tag + " is available (you are on " + juce::String(kVersion) + ").\nRelease notes:",
        juce::AlertWindow::NoIcon);
    auto* notesBox = new juce::TextEditor();
    notesBox->setMultiLine(true, true);
    notesBox->setReadOnly(true);
    notesBox->setScrollbarsShown(true);
    notesBox->setCaretVisible(false);
    notesBox->setText(r.notes.trim().isNotEmpty() ? r.notes : "(This release was published without notes.)", false);
    notesBox->setSize(520, 190);
    w->addCustomComponent(notesBox);
    w->addTextBlock("\nDownload & install will close SessionBar, run the installer,\n"
                    "and relaunch at the new version. Your settings and presets\n"
                    "are stored outside the app folder and are kept.");
    w->addButton("Download & install", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Later", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create([this](int result) {
        if (result == 1) startUpdateDownload();
    }), true);
}
void MainComponent::showPrivateRepoFallback(const juce::String& detail) {
    auto* w = new juce::AlertWindow("Updates",
        "Automatic update check isn't available while the repository reads as private.\n\n"
        + detail + "\n\n"
        "You are still on SessionBar " + juce::String(kVersion) + ". Releases can be downloaded "
        "from the releases page in your browser instead:",
        juce::AlertWindow::InfoIcon);
    w->addButton("Open releases page", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Close", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create([](int result) {
        if (result == 1) juce::URL(sbupdate::releasesPageUrl()).launchInDefaultBrowser();
    }), true);
}
void MainComponent::startUpdateDownload() {
    if (updateDownloadPending_) return;
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("SessionBar-Update");
    dir.createDirectory();
    updateInstallerFile_ = dir.getChildFile("SessionBar-Setup.exe");
    updateInstallerFile_.deleteFile();
    updateDownload_ = juce::URL(pendingRelease_.assetUrl).downloadToFile(updateInstallerFile_, juce::URL::DownloadTaskOptions());
    if (updateDownload_ == nullptr) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Update",
            "The update download could not be started.\nSessionBar has NOT been changed - you are still on " + juce::String(kVersion) + ".");
        return;
    }
    updateDownloadPending_ = true;
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Update",
        "Downloading SessionBar " + pendingRelease_.tag + "...\n"
        "When the download finishes, SessionBar will close, install,\nand relaunch itself at the new version.");
}

// =====================================================================
// v0.3.0 items 7/17/18: animated backgrounds + mascot drive (the
// twin of TickerBar's; bands from MeterBar's BarLink broadcast when
// linked, else the core's ambient driver).
// =====================================================================
sessionbar::VisualizerSettings MainComponent::vizSettings() const {
    sessionbar::VisualizerSettings vs;
    vs.enabled = true;
    vs.baseHue = state.vizHue;
    vs.saturation = state.vizSat;
    vs.paletteSpread = state.vizSpread;
    vs.cycleSeconds = state.vizCycle;
    return vs;
}
sessionbar::GlowSettings MainComponent::glowSettings() const {
    sessionbar::GlowSettings gs;
    gs.baseHue = state.vizHue;
    gs.saturation = state.vizSat;
    gs.spread = state.vizSpread;
    gs.cycleSeconds = state.vizCycle;
    return gs;
}
void MainComponent::syncFxPump() {
    fxPump_.setRunning(state.bgEffect != 0 || (mascot_ && mascot_->isVisible()));
}
void MainComponent::stepFx() {
    const float dt = 1.f / 30.f;
    fxClock_ += dt;
    float bands[3] = { 0.f, 0.f, 0.f }, level = 0.f;
    float pkt[4];
    if (state.linkEnabled && linkClient_.peerAudio(sessionbar::kLinkAppMeter, pkt)) {
        bands[0] = pkt[0]; bands[1] = pkt[1]; bands[2] = pkt[2]; level = pkt[3];
    } else {
        sessionbar::ambientBands(fxClock_, bands, level);
    }
    if (state.bgEffect == 1) viz_.update(dt, bands, vizSettings());
    else if (state.bgEffect == 2) glow_.update(dt, bands, level);
    if (mascot_ && mascot_->isVisible()) {
        const long long now = juce::Time::currentTimeMillis();
        mascot_->excited = mascotAnim_.excited(now);
        mascot_->bobPx = mascotAnim_.bobPx(now);
        mascot_->repaint();
    }
    if (state.bgEffect != 0) repaint();
}
void MainComponent::drawVisualizerBackground(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colours::black);
    if (bounds.isEmpty()) return;
    const int iw = juce::jlimit(1, 480, bounds.getWidth() / 2);
    const int ih = juce::jlimit(1, 270, bounds.getHeight() / 2);
    if (!vizCache_.isValid() || vizCache_.getWidth() != iw || vizCache_.getHeight() != ih) {
        vizCache_ = juce::Image(juce::Image::RGB, iw, ih, true);
        vizBlur_ = juce::Image(juce::Image::RGB, iw, ih, true);
    }
    auto vs = vizSettings();
    {
        juce::Graphics ig(vizCache_);
        ig.fillAll(juce::Colours::black);
        for (auto& pt : viz_.particles()) {
            auto c = viz_.colorFor(pt, vs);
            ig.setColour(juce::Colour(c.r, c.g, c.b));
            const float px = pt.x * (float)iw, py = pt.y * (float)ih;
            const float r = pt.size;
            ig.fillEllipse(px - r, py - r, r * 2.f, r * 2.f);
        }
    }
    vizBlur_ = vizCache_;
    vizBlur_.duplicateIfShared();
    juce::ImageConvolutionKernel kernel(5);
    kernel.createGaussianBlur(1.8f);
    kernel.applyToImage(vizBlur_, vizCache_, vizBlur_.getBounds());
    const float op = juce::jlimit(0.15f, 1.0f, state.backgroundOpacity);
    g.setOpacity(op);
    g.drawImage(vizBlur_, bounds.toFloat());
    g.setOpacity(0.9f * op);
    g.drawImage(vizCache_, bounds.toFloat());
    g.setOpacity(1.0f);
}
void MainComponent::drawGlowBackground(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colours::black);
    if (bounds.isEmpty()) return;
    const int iw = juce::jlimit(8, 240, bounds.getWidth() / 4);
    const int ih = juce::jlimit(8, 135, bounds.getHeight() / 4);
    if (!glowCache_.isValid() || glowCache_.getWidth() != iw || glowCache_.getHeight() != ih)
        glowCache_ = juce::Image(juce::Image::RGB, iw, ih, true);
    auto group = linkClient_.groupRect();
    auto own = getScreenBounds();
    if (group.isEmpty() || own.isEmpty()) group = own;
    auto gs = glowSettings();
    {
        juce::Image::BitmapData bd(glowCache_, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < ih; ++y) {
            const float sy = (float)own.getY() + (y + 0.5f) / (float)ih * (float)own.getHeight();
            const float gy = group.getHeight() > 0 ? (sy - (float)group.getY()) / (float)group.getHeight() : 0.5f;
            for (int x = 0; x < iw; ++x) {
                const float sx = (float)own.getX() + (x + 0.5f) / (float)iw * (float)own.getWidth();
                const float gx = group.getWidth() > 0 ? (sx - (float)group.getX()) / (float)group.getWidth() : 0.5f;
                auto c = glow_.sample(gx, gy, gs);
                bd.setPixelColour(x, y, juce::Colour(c.r, c.g, c.b));
            }
        }
    }
    g.setOpacity(juce::jlimit(0.15f, 1.0f, state.backgroundOpacity));
    g.drawImage(glowCache_, bounds.toFloat());
    const float groupW = group.getWidth() > 0 ? (float)group.getWidth() : 1.f;
    g.setColour(juce::Colours::white);
    for (int lx = 0; lx < bounds.getWidth(); lx += 2) {
        const float gx = ((float)own.getX() + (float)lx - (float)group.getX()) / groupW;
        const float a = sessionbar::GlowField::pinstripe(gx);
        if (a > 0.f) { g.setOpacity(a); g.drawVerticalLine(lx, 0.f, (float)bounds.getHeight()); }
    }
    g.setOpacity(1.0f);
}
