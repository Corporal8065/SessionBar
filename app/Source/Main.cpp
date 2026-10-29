#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"
class MainWindow : public juce::DocumentWindow {
public:
    explicit MainWindow(const juce::String& name) : juce::DocumentWindow(name, juce::Colour(0x00000000), 0) {
        setUsingNativeTitleBar(false);
        setTitleBarHeight(0);
        // Per-pixel transparent top level (family pattern): the window
        // background is fully clear, so wherever the content paints
        // with alpha (the Background Opacity setting) the desktop
        // shows through. Translucency only — no DWM blur.
        setOpaque(false);
        auto* mc = new MainComponent();
        content = mc;
        setContentOwned(mc, true);
        setResizable(true, false);
        setResizeLimits(560, 64, 3840, 300);
        setBounds(mc->state.windowX, mc->state.windowY, mc->state.windowW, mc->state.windowH);
        setAlwaysOnTop(mc->state.pinned);
        setVisible(true);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    // The reserve machine is EVENT-driven (MeterBar v0.18.1 discipline):
    // the window's own moved/resized notifications are its geometry
    // feed — these fire for native drags and border resizes too.
    void moved() override { if (content != nullptr) content->notifyWindowGeometryChanged(); }
    void resized() override {
        juce::DocumentWindow::resized();
        if (content != nullptr) content->notifyWindowGeometryChanged();
    }
private:
    MainComponent* content = nullptr; // owned by the window (setContentOwned) — never delete here
};
class SessionBarApp : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "SessionBar"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String&) override {
        // JUCE free-license attribution splash (family pattern): small,
        // brief — "Made with JUCE" for ~1.5 s, click dismisses.
        juce::Image splashImg(juce::Image::RGB, 440, 160, true);
        {
            juce::Graphics g(splashImg);
            g.fillAll(juce::Colour(0xff17141f));
            g.setColour(juce::Colour(0xffe04c89)); g.setFont(juce::Font(30.f, juce::Font::bold));
            g.drawText("SessionBar", splashImg.getBounds().withTrimmedBottom(52), juce::Justification::centred);
            g.setColour(juce::Colour(0xfff6eff3)); g.setFont(15.f);
            g.drawText("Made with JUCE", splashImg.getBounds().withTrimmedTop(88), juce::Justification::centred);
        }
        // SplashScreen deletes ITSELF after the delay — never own it.
        splash = new juce::SplashScreen("SessionBar", splashImg, true);
        splash->deleteAfterDelay(juce::RelativeTime::milliseconds(1500), true);
        mainWindow = std::make_unique<MainWindow>("SessionBar");
    }
    void shutdown() override { mainWindow.reset(); }
    void systemRequestedQuit() override { quit(); }
private:
    std::unique_ptr<MainWindow> mainWindow;
    juce::SplashScreen* splash = nullptr; // self-deleting — never delete via this pointer
};
START_JUCE_APPLICATION(SessionBarApp)
