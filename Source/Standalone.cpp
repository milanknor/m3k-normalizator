#include <JuceHeader.h>
// StandaloneFilterWindow is skipped by JUCE when using a custom app — include it manually
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "PluginProcessor.h"

// Custom standalone entry point — enables systray + custom window behaviour.
// Compiled only for the Standalone target (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1).

// ---- Volume popup (shown from the tray icon, like the Windows volume flyout) ----
struct VolumePopup : public juce::Component
{
    juce::Slider slider;
    juce::RangedAudioParameter* param = nullptr;
    explicit VolumePopup(juce::RangedAudioParameter* p) : param(p)
    {
        setSize(230, 56);
        addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setRange(0.0, 100.0, 1.0);
        slider.setValue(param ? param->getValue()*100.0 : 100.0, juce::dontSendNotification);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 22);
        slider.setColour(juce::Slider::trackColourId,     juce::Colour(0xFFE8A020));
        slider.setColour(juce::Slider::thumbColourId,     juce::Colour(0xFFE8A020));
        slider.setColour(juce::Slider::backgroundColourId,juce::Colour(0xFF3A3A3A));
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.onValueChange=[this]{ if(param) param->setValueNotifyingHost((float)(slider.getValue()/100.0)); };
    }
    void resized() override { slider.setBounds(getLocalBounds().reduced(10,4).withTrimmedTop(16)); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        g.setColour(juce::Colour(0xFFE8A020)); g.drawRect(getLocalBounds(),1);
        g.setColour(juce::Colour(0xFFCCCCCC)); g.setFont(juce::Font(juce::FontOptions().withHeight(12.f).withStyle("Bold")));
        g.drawText(juce::String::fromUTF8("Hlasitost v\xc3\xbdstupu"), 10, 3, 210, 14, juce::Justification::centredLeft);
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
        juce::RangedAudioParameter* p = nullptr;
        if (auto* proc = dynamic_cast<M3KNormalizatorProcessor*>(
                window.pluginHolder->processor.get()))
            p = proc->apvts.getParameter("outputVol");
        auto content = std::make_unique<VolumePopup>(p);
        auto m = juce::Desktop::getMousePosition();
        juce::CallOutBox::launchAsynchronously(std::move(content),
            { m.x - 115, m.y - 70, 1, 1 }, nullptr);
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
        mainWindow = std::make_unique<M3KMainWindow>();
        mainWindow->setVisible(true);
        tray = std::make_unique<M3KTrayIcon>(*mainWindow);
    }

    void shutdown() override
    {
        tray       = nullptr;
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override { quit(); }

    void anotherInstanceStarted(const juce::String&) override
    {
        if (mainWindow) { mainWindow->asComp()->setVisible(true); mainWindow->asComp()->toFront(true); }
    }

private:
    std::unique_ptr<M3KMainWindow> mainWindow;
    std::unique_ptr<M3KTrayIcon>   tray;
};

START_JUCE_APPLICATION(M3KApplication)
