#include "MainComponent.h"
#include "SettingsStore.h"
#if JUCE_WINDOWS
 #include <windows.h> // native rects for the AppBar sync (physical pixels)
#endif
static const char* kVersion = "v0.2.0";

// =====================================================================
// SessionCell — one exchange: city, its local clock, state, countdown.
// A drag moves the whole bar (no click action in v0.1).
// =====================================================================
class SessionCell : public juce::Component, public juce::SettableTooltipClient {
public:
    MainComponent* owner = nullptr;
    const sessionbar::SessionDef* def = nullptr;
    sessionbar::SessionStatus status;
    bool has = false;

    explicit SessionCell(MainComponent* o) : owner(o) {}
    void setStatus(const sessionbar::SessionStatus& s) { status = s; has = true; repaint(); }

    void mouseDown(const juce::MouseEvent& e) override { owner->windowDragStarted(e); }
    void mouseDrag(const juce::MouseEvent& e) override { owner->windowDragged(e); }

    void paint(juce::Graphics& g) override {
        const auto& th = owner->theme();
        auto b = getLocalBounds().toFloat().reduced(1.f);
        g.setColour(th.panel.withAlpha(0.92f));
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
        const juce::Colour sc = !has ? th.dim
            : status.state == sessionbar::SessionState::Open ? th.good
            : status.state == sessionbar::SessionState::Break ? th.warn
            : status.state == sessionbar::SessionState::Pre ? th.warn   // v0.2.0: pre-market = warming up (amber)
            : status.state == sessionbar::SessionState::Post ? th.accent2 // post-market = winding down (teal)
            : th.bad;
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
        comboRow("Theme", { "Midnight Kids", "Clean Dark" }, st->themeName, changed);
        {
            auto* l = add(new juce::Label({}, "Background style")); placeLabel(l);
            auto* cb = add(new juce::ComboBox());
            cb->addItem("Clean Dark", 1); cb->addItem("Midnight Kids", 2); cb->addItem("Flat", 3);
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
        heading("Window");
        toggle("Always on top", st->pinned, changed);
        {
            auto* top = add(new juce::TextButton("Dock top"));
            top->setBounds(24, y_, 110, 24);
            top->onClick = [this]{ st->dockEdge = sessionbar::DockEdge::Top; owner->snapToDockEdge(); };
            auto* bot = add(new juce::TextButton("Dock bottom"));
            bot->setBounds(142, y_, 110, 24);
            bot->onClick = [this]{ st->dockEdge = sessionbar::DockEdge::Bottom; owner->snapToDockEdge(); };
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
        status_ = add(new juce::Label({}, owner->screenSpaceStatus()));
        status_->setFont(11.f); status_->setColour(juce::Label::textColourId, th_.dim);
        status_->setBounds(24, y_, 512, 18);
        y_ += 24;
        setSize(560, y_ + 8);
    }
private:
    template <typename T> T* add(T* c) { addAndMakeVisible(c); owned_.push_back(std::unique_ptr<juce::Component>(c)); return c; }
    void rebuildSoon() { juce::MessageManager::callAsync([this]{ build(); }); }
    void heading(const juce::String& t) {
        auto* l = add(new juce::Label({}, t));
        l->setFont(juce::Font(14.f, juce::Font::bold)); l->setColour(juce::Label::textColourId, th_.accent);
        l->setBounds(16, y_, 520, 20); y_ += 24;
    }
    void note(const juce::String& t) {
        auto* l = add(new juce::Label({}, t));
        l->setFont(11.f); l->setColour(juce::Label::textColourId, th_.dim);
        l->setBounds(24, y_, 512, 16); y_ += 22;
    }
    void placeLabel(juce::Label* l) {
        l->setFont(12.5f); l->setColour(juce::Label::textColourId, th_.text);
        l->setBounds(24, y_, 300, 18); y_ += 20;
    }
    void toggle(const juce::String& text, bool& field, std::function<void()> changed) {
        auto* t = add(new juce::ToggleButton(text));
        t->setToggleState(field, juce::dontSendNotification);
        t->setColour(juce::ToggleButton::textColourId, th_.text);
        t->setBounds(24, y_, 512, 22);
        t->onClick = [&field, t, changed]{ field = t->getToggleState(); changed(); };
        y_ += 26;
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
    SettingsContent content;
    SessionSettingsWindow() : juce::DocumentWindow("SessionBar Settings", juce::Colour(0xff231f31), juce::DocumentWindow::closeButton) {
        content.build();
        viewport.setViewedComponent(&content, false);
        viewport.setScrollBarsShown(true, false);
        setContentNonOwned(&viewport, true);
        setSize(580, 640); setResizable(true, false); setUsingNativeTitleBar(true);
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
    theme_ = themeByName(state.themeName);
    setOpaque(false); // the window is non-opaque too: Background Opacity shows the desktop through
    setTooltip("SessionBar - New York / London / Tokyo market session clocks. Exchange holidays + early closes modeled (2026-2028, embedded); New York PRE/POST states shown.");

    wordmark_.setText("SessionBar", juce::dontSendNotification);
    wordmark_.setFont(juce::Font(15.f, juce::Font::bold)); wordmark_.setColour(juce::Label::textColourId, theme_.accent);
    versionLabel_.setText(kVersion, juce::dontSendNotification);
    versionLabel_.setFont(10.f); versionLabel_.setColour(juce::Label::textColourId, theme_.dim);
    for (auto* c : { (juce::Component*)&wordmark_, (juce::Component*)&versionLabel_ }) {
        addAndMakeVisible(c);
        c->setInterceptsMouseClicks(false, false); // clicks fall through to the bar drag
    }
    reserveBtn_.setClickingTogglesState(true);
    reserveBtn_.setToggleState(state.reserveScreenSpace, juce::dontSendNotification);
    reserveBtn_.setTooltip("Reserve screen space: Windows keeps maximised windows clear of the docked bar");
    reserveBtn_.onClick = [this]{ state.reserveScreenSpace = reserveBtn_.getToggleState(); saveSettings(); syncScreenSpace(); };
    settingsBtn_.onClick = [this]{
        settingsWin_->content.build();
        settingsWin_->setVisible(true); settingsWin_->toFront(true);
    };
    minBtn_.onClick = [this]{ if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
                                 if (auto* peer = w->getPeer()) peer->setMinimised(true); };
    quitBtn_.onClick = []{ juce::JUCEApplication::getInstance()->systemRequestedQuit(); };
    linkBtn_.setClickingTogglesState(true);
    linkBtn_.setToggleState(state.linkEnabled, juce::dontSendNotification);
    linkBtn_.setTooltip("Link: snap, drag, dock and share the background with the other family bars (MeterBar / TickerBar) when their Link is on too");
    linkBtn_.onClick = [this]{ state.linkEnabled = linkBtn_.getToggleState(); saveSettings(); };
    for (auto* b : { &reserveBtn_, &linkBtn_, &settingsBtn_, &minBtn_, &quitBtn_ }) addAndMakeVisible(b);
    // v0.2.0 (k): family link wiring (protocol: core BarLink.hpp).
    // SessionBar's reserve machine is top/bottom-only with int edge
    // codes — exactly the protocol's dockedEdge ordinal.
    linkClient_.getBounds = [this]{ return currentWindowBounds(); };
    linkClient_.setBounds = [this](juce::Rectangle<int> r){
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) w->setBounds(r);
    };
    linkClient_.getBg = [this]{ return BarLinkClient::Bg{ (int)state.backgroundStyle, state.backgroundOpacity }; };
    linkClient_.applyBg = [this](const BarLinkClient::Bg& bg){
        state.backgroundStyle = (sessionbar::BackgroundStyle)juce::jlimit(0, 2, bg.style);
        state.backgroundOpacity = juce::jlimit(0.15f, 1.0f, bg.opacity);
        saveSettings(); repaint();
    };
    linkClient_.getDockedEdge = [this]{
        return reserveMachine_.reserved() ? (state.dockEdge == sessionbar::DockEdge::Top ? 0 : 1) : -1;
    };
    linkClient_.isLinked = [this]{ return state.linkEnabled; };
    linkClient_.start();

    settingsWin_ = std::make_unique<SessionSettingsWindow>();
    settingsWin_->content.owner = this;
    settingsWin_->content.st = &state;
    settingsWin_->content.onChanged = [this]{ settingsChanged(); };
    settingsWin_->content.build();

    rebuildCells();
    refreshCells();
    syncScreenSpace();
    startTimerHz(1); // countdowns tick every second; the core math is three evaluate() calls
    setSize(state.windowW, state.windowH);
}

MainComponent::~MainComponent() {
    stopTimer();
    saveSettings();
    screenSpace_.release();
}

void MainComponent::paint(juce::Graphics& g) {
    fillStyledBackground(g, getLocalBounds().toFloat(), state.backgroundStyle, theme_,
                         juce::jlimit(0.15f, 1.0f, state.backgroundOpacity));
    g.setColour(theme_.border);
    g.drawRect(getLocalBounds().toFloat(), 1.f);
}

void MainComponent::resized() {
    auto r = getLocalBounds().reduced(8, 6);
    auto left = r.removeFromLeft(92);
    wordmark_.setBounds(left.removeFromTop(20));
    versionLabel_.setBounds(left.removeFromTop(12));
    quitBtn_.setBounds(r.removeFromRight(26).withSizeKeepingCentre(26, 24));
    minBtn_.setBounds(r.removeFromRight(26).withSizeKeepingCentre(26, 24));
    settingsBtn_.setBounds(r.removeFromRight(76).withSizeKeepingCentre(72, 24));
    reserveBtn_.setBounds(r.removeFromRight(80).withSizeKeepingCentre(76, 24));
    linkBtn_.setBounds(r.removeFromRight(56).withSizeKeepingCentre(52, 24));
    const int n = (int)cells_.size();
    if (n > 0) {
        const int gap = 6;
        const int w = (r.getWidth() - gap * (n - 1)) / n;
        for (int i = 0; i < n; ++i) {
            auto cellR = r.removeFromLeft(w);
            cells_[(size_t)i]->setBounds(cellR);
            r.removeFromLeft(gap);
        }
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
    auto addCell = [&](const sessionbar::SessionDef& def, bool show, const char* tip) {
        if (!show) return;
        auto c = std::make_unique<SessionCell>(this);
        c->def = &def;
        c->setTooltip(tip);
        addAndMakeVisible(c.get());
        cells_.push_back(std::move(c));
    };
    addCell(sessionbar::newYorkDef(), state.showNewYork, "NYSE regular hours 9:30 AM - 4:00 PM New York time");
    addCell(sessionbar::londonDef(), state.showLondon, "LSE regular hours 8:00 AM - 4:30 PM London time");
    addCell(sessionbar::tokyoDef(), state.showTokyo, "TSE regular hours 9:00 AM - 3:00 PM Tokyo time, lunch break 11:30 AM - 12:30 PM");
    resized();
}

void MainComponent::refreshCells() {
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    for (auto& c : cells_)
        if (c->def) c->setStatus(sessionbar::evaluate(*c->def, now));
}

void MainComponent::timerCallback() {
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

void MainComponent::settingsChanged() { linkClient_.notifyLocalSettings();
    theme_ = themeByName(state.themeName);
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) w->setAlwaysOnTop(state.pinned);
    if (reserveBtn_.getToggleState() != state.reserveScreenSpace)
        reserveBtn_.setToggleState(state.reserveScreenSpace, juce::dontSendNotification);
    rebuildCells();
    refreshCells();
    saveSettings();
    syncScreenSpace();
    repaint();
}

void MainComponent::snapToDockEdge() {
    auto* w = findParentComponentOfClass<juce::DocumentWindow>();
    if (!w) return;
    auto display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(w->getBounds());
    if (display == nullptr) return;
    auto area = display->totalArea;
    auto b = w->getBounds();
    b.setY(state.dockEdge == sessionbar::DockEdge::Top ? area.getY() : area.getBottom() - b.getHeight());
    b.setX(juce::jlimit(area.getX(), area.getRight() - b.getWidth(), b.getX()));
    w->setBounds(b);
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
        const bool top = state.dockEdge == sessionbar::DockEdge::Top;
        const int gap = top ? std::abs(wr.top - mi.rcMonitor.top)
                            : std::abs(wr.bottom - mi.rcMonitor.bottom);
        const sessionbar::IntRect rect{ wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top };
        const long long monId = (long long)(uintptr_t)mon;
        switch (reserveMachine_.update(state.reserveScreenSpace, top ? 0 : 1, gap, rect, monId)) {
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
