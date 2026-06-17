#include <JuceHeader.h>
// StandaloneFilterWindow is skipped by JUCE when using a custom app — include it manually
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

// Custom standalone entry point — enables systray + custom window behaviour.
// Compiled only for the Standalone target (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1).

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
        juce::Image img(juce::Image::ARGB, 16, 16, true);
        {
            juce::Graphics g(img);
            g.setColour(juce::Colour(0xFFE8A020));
            g.fillEllipse(1.f, 1.f, 14.f, 14.f);
            g.setColour(juce::Colours::black);
            g.setFont(juce::Font(juce::FontOptions().withHeight(9.f).withStyle("Bold")));
            g.drawText("M", 0, 1, 16, 13, juce::Justification::centred);
        }
        setIconImage(img, img);
        setIconTooltip("M3K Normalizator");
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            toggleWindow();
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
