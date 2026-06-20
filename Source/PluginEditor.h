#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Rotary knob whose value is drawn inside it; double-clicking opens an inline
// text editor so the value can be typed exactly.
struct ValueKnob : public juce::Slider
{
    std::unique_ptr<juce::TextEditor> editor;
    double defaultValue = 0.0;
    bool   hasDefault   = false;

    // Ctrl/Cmd+click resets the knob to its default value.
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (hasDefault && (e.mods.isCtrlDown() || e.mods.isCommandDown()))
        {
            setValue(defaultValue, juce::sendNotificationSync);
            return;
        }
        juce::Slider::mouseDown(e);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        editor = std::make_unique<juce::TextEditor>();
        addAndMakeVisible(*editor);
        // Fit inside the knob so it isn't clipped (matters for the small gain knob)
        const int ew = juce::jmax(34, getWidth() - 6);
        editor->setBounds(getLocalBounds().withSizeKeepingCentre(ew, 18));
        editor->setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
        editor->setJustification(juce::Justification::centred);
        editor->setIndents(2, 1);
        editor->setInputRestrictions(0, "0123456789.,-");
        editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF101010));
        editor->setColour(juce::TextEditor::textColourId,       juce::Colours::white);
        editor->setColour(juce::TextEditor::outlineColourId,    juce::Colour(0xFFE8A020));
        editor->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xFFE8A020));
        const int dec = (getInterval() > 0.0 && getInterval() < 1.0) ? 1 : 0;
        editor->setText(juce::String(getValue(), dec), false);
        editor->selectAll();
        editor->grabKeyboardFocus();
        editor->onReturnKey = [this]{ commitEditor(); };
        editor->onEscapeKey = [this]{ closeEditor(); };
        editor->onFocusLost = [this]{ commitEditor(); };
    }

    void commitEditor()
    {
        if (editor == nullptr) return;
        const double v = editor->getText().replaceCharacter(',', '.').getDoubleValue();
        setValue(v, juce::sendNotificationSync);   // attachment clamps to range
        closeEditor();
    }

    void closeEditor()
    {
        if (editor == nullptr) return;
        editor->onReturnKey = nullptr;
        editor->onEscapeKey = nullptr;
        editor->onFocusLost = nullptr;
        juce::Component::SafePointer<ValueKnob> safe(this);
        juce::MessageManager::callAsync([safe]{ if (safe) safe->editor.reset(); });
    }
};

class M3KNormalizatorEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit M3KNormalizatorEditor(M3KNormalizatorProcessor&);
    ~M3KNormalizatorEditor() override;

    void resized() override;

private:
    // Window kept compact; controls at original size, graph/VU take the rest
    static constexpr int kDesignW = 624, kDesignH = 496;
    static constexpr float kUiScale = 1.0f;
    bool compExpanded = false;                 // compressor panel overlays the bottom controls
    void setCompExpanded(bool e);

    void timerCallback() override;
    void paintCanvas(juce::Graphics& g);
    void layoutCanvas();
    void canvasClicked(juce::Point<int> p);
    juce::String tooltipForPoint(juce::Point<int> p);
    const juce::Rectangle<int> logoBounds { 10, 3, 40, 40 };
    void drawGraph(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawVuMeter(juce::Graphics& g, juce::Rectangle<int> bounds,
                     float dbL, float dbR, const juce::String& label);

    // Inner component holding the whole UI at design size (transformed by kUiScale)
    struct Canvas : public juce::Component, public juce::TooltipClient
    {
        M3KNormalizatorEditor& ed;
        explicit Canvas(M3KNormalizatorEditor& e) : ed(e) {}
        void paint(juce::Graphics& g) override   { ed.paintCanvas(g); }
        void resized() override                  { ed.layoutCanvas(); }
        juce::String getTooltip() override       { return ed.tooltipForPoint(getMouseXYRelative()); }
        void mouseDown(const juce::MouseEvent& e) override { ed.canvasClicked(e.getPosition()); }
        void mouseMove(const juce::MouseEvent& e) override
        {
            auto p = e.getPosition();
            bool clickable = ed.logoBounds.contains(p)
                          || ed.lraInBadge.contains(p)
                          || ed.lraOutBadge.contains(p)
                          || ed.stripCell[0].contains(p) || ed.stripCell[1].contains(p)
                          || ed.stripCell[2].contains(p) || ed.stripCell[3].contains(p)
                          || ed.stripCell[4].contains(p) || ed.viewToggle.contains(p);
            setMouseCursor(clickable ? juce::MouseCursor::PointingHandCursor
                                     : juce::MouseCursor::NormalCursor);
        }
    };
    Canvas canvas { *this };

    M3KNormalizatorProcessor& processor;

    ValueKnob targetLufsSlider, releaseSlider, attackSlider, windowSlider, ceilingSlider;
    ValueKnob inputGainSlider;             // small trim knob above the IN VU meter
    ValueKnob threshSlider, ratioSlider, compAttackSlider, compReleaseSlider; // compressor row
    juce::ToggleButton normalizeButton { "NORMALIZE" };
    juce::TooltipWindow tooltips { this, 600 };
    juce::TextButton   modeButtons[8];
    juce::TextButton   resetButton  { "RESET I" };
    juce::TextButton   presetButton { "PRESET" };
    juce::TextButton   helpButton   { "?" };
    juce::TextButton   compButton   { "COMP" };
    juce::TextButton   compExpandButton;   // chevron: show/hide compressor settings
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File presetDir();
    void showPresetMenu();
    void applyFactoryPreset(float lufs, float ceiling, float speed, float down);
    void setParam(const juce::String& id, float value);

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> targetAttach, releaseAttach, attackAttach, windowAttach, ceilingAttach, inputGainAttach;
    std::unique_ptr<SliderAttachment> threshAttach, ratioAttach, compAttackAttach, compReleaseAttach;
    std::unique_ptr<ButtonAttachment> normAttach, compAttach;

    // Smoothed display values
    float dispM  = -70.f, dispS  = -70.f, dispI  = -70.f, dispC = -70.f;
    float dispNG = 0.f;
    float dispVuInL = -70.f, dispVuInR = -70.f;
    float dispVuOutL = -70.f, dispVuOutR = -70.f;
    float dispLraIn = 0.f, dispLraOut = 0.f;
    float dispGr = 0.f, dispOutInt = -70.f, dispCompGr = 0.f;
    unsigned int lastBlockCounter = 0; // detect when audio callbacks stop

    // Graph history — 60s at 30fps = 1800 pts. M/S/I = output LUFS, G = applied gain (dB)
    static constexpr int kHistSize = 1800;
    float histM[kHistSize]{}, histS[kHistSize]{}, histI[kHistSize]{};
    float histC[kHistSize]{}, histG[kHistSize]{};
    // Per-mode fader preview history (dB) — from processor.faderDb[0..3]
    float histFM[kHistSize]{}, histFS[kHistSize]{}, histFI[kHistSize]{}, histFC[kHistSize]{};
    int   histPos = 0, histFilled = 0;
    long long histTotal = 0;   // monotonic frame counter — anchors dashes to the data

    // Graph view: 0 = Loudness (output LUFS), 1 = Fader (per-mode desired gain).
    // In Fader view each mode's gain is drawn; the active mode solid, others dashed.
    int graphView = 0;
    juce::Rectangle<int> viewToggle;   // clickable Loudness/Fader switch (set in paint)

    // Per-curve visibility, toggled by clicking the M/S/I/C/GAIN letters in the value strip
    bool showM = true, showS = true, showI = true, showC = true, showG = true;
    // Cell hit-rects of the value strip (filled in paint), used for click toggling
    juce::Rectangle<int> stripCell[6];

    juce::Rectangle<int> graphBounds, vuInBounds, vuOutBounds, knobArea;
    juce::Rectangle<int> lraInBadge, lraOutBadge, inGainBounds, compGrBounds;

    std::unique_ptr<juce::Drawable> logo;

    struct AmberLAF : public juce::LookAndFeel_V4
    {
        void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                              float pos, float start, float end, juce::Slider&) override;
        void drawButtonBackground(juce::Graphics&, juce::Button&,
                                  const juce::Colour&, bool, bool) override;
        void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
        void drawTickBox(juce::Graphics&, juce::Component&, float x, float y, float w, float h,
                         bool ticked, bool enabled, bool over, bool down) override;
    } laf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(M3KNormalizatorEditor)
};
