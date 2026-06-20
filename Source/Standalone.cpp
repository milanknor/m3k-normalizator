#include <JuceHeader.h>
// StandaloneFilterWindow is skipped by JUCE when using a custom app — include it manually
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "PluginProcessor.h"

// Custom standalone entry point — enables systray + custom window behaviour.
// Compiled only for the Standalone target (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1).

// ---- Volume popup (tray flyout): output volume + target LUFS ----
struct VolumePopup : public juce::Component
{
    juce::Slider volSlider, tgtSlider;
    juce::RangedAudioParameter* volP = nullptr;
    juce::RangedAudioParameter* tgtP = nullptr;
    VolumePopup(juce::RangedAudioParameter* v, juce::RangedAudioParameter* t) : volP(v), tgtP(t)
    {
        setSize(248, 104);
        auto style=[this](juce::Slider& s){
            addAndMakeVisible(s);
            s.setSliderStyle(juce::Slider::LinearHorizontal);
            s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 46, 22);
            s.setColour(juce::Slider::trackColourId,     juce::Colour(0xFFE8A020));
            s.setColour(juce::Slider::thumbColourId,     juce::Colour(0xFFE8A020));
            s.setColour(juce::Slider::backgroundColourId,juce::Colour(0xFF3A3A3A));
            s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
            s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        };
        style(volSlider); style(tgtSlider);

        volSlider.setRange(0.0, 100.0, 1.0);
        volSlider.setValue(volP ? volP->getValue()*100.0 : 100.0, juce::dontSendNotification);
        volSlider.onValueChange=[this]{ if(volP) volP->setValueNotifyingHost((float)(volSlider.getValue()/100.0)); };

        auto r = tgtP ? tgtP->getNormalisableRange() : juce::NormalisableRange<float>(-60.f, 0.f);
        tgtSlider.setRange(r.start, r.end, 0.1);
        tgtSlider.setValue(tgtP ? r.convertFrom0to1(tgtP->getValue()) : -14.0, juce::dontSendNotification);
        tgtSlider.onValueChange=[this]{ if(tgtP){ auto rr=tgtP->getNormalisableRange();
            tgtP->setValueNotifyingHost(rr.convertTo0to1((float)tgtSlider.getValue())); } };
    }
    void resized() override
    {
        tgtSlider.setBounds(10, 20, getWidth()-20, 22);   // target on top
        volSlider.setBounds(10, 66, getWidth()-20, 22);   // output volume below
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFFE8A020)); g.drawRect(getLocalBounds(),1);
        g.setColour(juce::Colour(0xFFCCCCCC)); g.setFont(juce::Font(juce::FontOptions().withHeight(11.5f).withStyle("Bold")));
        g.drawText(juce::String::fromUTF8("C\xc3\xadlov\xc3\xa1 hlasitost (LUFS)"), 10, 4, 228, 14, juce::Justification::centredLeft);
        g.drawText(juce::String::fromUTF8("Hlasitost v\xc3\xbdstupu"), 10, 50, 228, 14, juce::Justification::centredLeft);
    }
};

// ---- App / tray icon (dark disc, orange rings, white N) ----------------
static juce::Image makeAppIcon(int sz)
{
    juce::Image img(juce::Image::ARGB, sz, sz, true);
    juce::Graphics g(img);
    const float s = (float)sz;
    g.setColour(juce::Colour(0xFF1A1A24)); g.fillEllipse(s*0.02f, s*0.02f, s*0.96f, s*0.96f);
    g.setColour(juce::Colour(0xFFF97316));
    g.drawEllipse(s*0.14f, s*0.14f, s*0.72f, s*0.72f, juce::jmax(1.0f, s*0.055f));
    g.drawEllipse(s*0.04f, s*0.04f, s*0.92f, s*0.92f, juce::jmax(1.0f, s*0.02f));
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions().withHeight(s*0.55f).withStyle("Bold")));
    g.drawText("N", 0, (int)(s*0.02f), sz, sz, juce::Justification::centred);
    return img;
}

// ---- Main window -------------------------------------------------------
class M3KMainWindow : public juce::StandaloneFilterWindow
{
public:
    M3KMainWindow()
        : juce::StandaloneFilterWindow(
              juce::JUCEApplication::getInstance()->getApplicationName(),
              juce::Colour(0xFF141414), nullptr, true)
    {
        setUsingNativeTitleBar(true);
        setResizable(false, false);
        setIcon(makeAppIcon(64));   // taskbar / title-bar icon
    }

    // Hide to tray instead of closing / minimising
    void closeButtonPressed()    override { asComp()->setVisible(false); }
    void minimiseButtonPressed() override { asComp()->setVisible(false); }

    void showAudioSettings()
    {
        // Built-in dialog (now dark via the global LookAndFeel) — same as the window's
        // "Settings..." button; keeps the feedback-loop mute option for the bridge use.
        pluginHolder->showAudioSettingsDialog();
    }

    juce::Component* asComp() { return static_cast<juce::Component*>(this); }
};

// ---- Tray icon ---------------------------------------------------------
class M3KTrayIcon : public juce::SystemTrayIconComponent
{
public:
    explicit M3KTrayIcon(M3KMainWindow& w) : window(w)
    {
        // Small amber "M" icon (16×16)
        juce::Image img = makeAppIcon(20);   // "N" (Normalizer), matching the app icon
        setIconImage(img, img);
        setIconTooltip("M3K Normalizator");
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            showVolume();
        }
        else if (e.mods.isRightButtonDown())
        {
            juce::PopupMenu menu;
            menu.addItem(1, window.asComp()->isVisible()
                            ? juce::String::fromUTF8("Skr\xc3\xbdt okno")
                            : juce::String::fromUTF8("Zobrazit okno"));
            menu.addItem(2, juce::String::fromUTF8("Nastaven\xc3\xad zvuku\xe2\x80\xa6"));
            menu.addSeparator();
            menu.addItem(3, juce::String::fromUTF8("Ukon\xc4\x8dit"));

            menu.showMenuAsync(juce::PopupMenu::Options(),
                [this](int r)
                {
                    if      (r == 1) toggleWindow();
                    else if (r == 2) window.showAudioSettings();
                    else if (r == 3) juce::JUCEApplication::getInstance()->systemRequestedQuit();
                });
        }
    }

private:
    M3KMainWindow& window;

    void toggleWindow()
    {
        auto* c = window.asComp();
        if (c->isVisible()) { c->setVisible(false); }
        else                { c->setVisible(true);  c->toFront(true); }
    }

    void showVolume()
    {
        juce::RangedAudioParameter* vol = nullptr;
        juce::RangedAudioParameter* tgt = nullptr;
        if (auto* proc = dynamic_cast<M3KNormalizatorProcessor*>(
                window.pluginHolder->processor.get()))
        {
            vol = proc->apvts.getParameter("outputVol");
            tgt = proc->apvts.getParameter("targetLufs");
        }
        auto content = std::make_unique<VolumePopup>(vol, tgt);
        auto m = juce::Desktop::getMousePosition();   // anchor at the tray icon / cursor
        juce::CallOutBox::launchAsynchronously(std::move(content),
            { m.x, m.y, 1, 1 }, nullptr);
    }
};

// ---- Application -------------------------------------------------------
class M3KApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName()    override { return JucePlugin_Name; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed()          override { return false; }

    void initialise(const juce::String&) override
    {
        // Dark theme for all app dialogs (audio settings, menus, popups) to match the UI
        using CS = juce::LookAndFeel_V4::ColourScheme;
        darkLaf = std::make_unique<juce::LookAndFeel_V4>(juce::LookAndFeel_V4::getDarkColourScheme());
        CS scheme = darkLaf->getCurrentColourScheme();
        scheme.setUIColour(CS::windowBackground,    juce::Colour(0xFF161616));
        scheme.setUIColour(CS::widgetBackground,    juce::Colour(0xFF242424));
        scheme.setUIColour(CS::menuBackground,      juce::Colour(0xFF1A1A1A));
        scheme.setUIColour(CS::outline,             juce::Colour(0xFF3A3A3A));
        scheme.setUIColour(CS::defaultText,         juce::Colour(0xFFCCCCCC));
        scheme.setUIColour(CS::defaultFill,         juce::Colour(0xFF2A2A2A));
        scheme.setUIColour(CS::menuText,            juce::Colour(0xFFCCCCCC));
        scheme.setUIColour(CS::highlightedText,     juce::Colours::white);
        scheme.setUIColour(CS::highlightedFill,     juce::Colour(0xFFE8A020));
        darkLaf->setColourScheme(scheme);
        // ResizableWindow background (dialog bg) — match the plugin
        darkLaf->setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xFF161616));
        juce::LookAndFeel::setDefaultLookAndFeel(darkLaf.get());

        mainWindow = std::make_unique<M3KMainWindow>();
        mainWindow->setVisible(true);
        tray = std::make_unique<M3KTrayIcon>(*mainWindow);
    }

    void shutdown() override
    {
        tray       = nullptr;
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        darkLaf    = nullptr;
    }

    void systemRequestedQuit() override { quit(); }

    void anotherInstanceStarted(const juce::String&) override
    {
        if (mainWindow) { mainWindow->asComp()->setVisible(true); mainWindow->asComp()->toFront(true); }
    }

private:
    std::unique_ptr<juce::LookAndFeel_V4> darkLaf;
    std::unique_ptr<M3KMainWindow> mainWindow;
    std::unique_ptr<M3KTrayIcon>   tray;
};

START_JUCE_APPLICATION(M3KApplication)
