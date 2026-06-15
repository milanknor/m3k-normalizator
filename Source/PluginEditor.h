#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class M3KNormalizatorEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit M3KNormalizatorEditor(M3KNormalizatorProcessor&);
    ~M3KNormalizatorEditor() override;

    void resized() override;

private:
    // Window kept compact; controls at original size, graph/VU take the rest
    static constexpr int kDesignW = 544, kDesignH = 470;
    static constexpr float kUiScale = 1.0f;

    void timerCallback() override;
    void paintCanvas(juce::Graphics& g);
    void layoutCanvas();
    void canvasClicked(juce::Point<int> p);
    const juce::Rectangle<int> logoBounds { 10, 3, 40, 40 };
    void drawGraph(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawVuMeter(juce::Graphics& g, juce::Rectangle<int> bounds,
                     float dbL, float dbR, const juce::String& label);

    // Inner component holding the whole UI at design size (transformed by kUiScale)
    struct Canvas : public juce::Component
    {
        M3KNormalizatorEditor& ed;
        explicit Canvas(M3KNormalizatorEditor& e) : ed(e) {}
        void paint(juce::Graphics& g) override   { ed.paintCanvas(g); }
        void resized() override                  { ed.layoutCanvas(); }
        void mouseDown(const juce::MouseEvent& e) override { ed.canvasClicked(e.getPosition()); }
        void mouseMove(const juce::MouseEvent& e) override
        {
            setMouseCursor(ed.logoBounds.contains(e.getPosition())
                           ? juce::MouseCursor::PointingHandCursor
                           : juce::MouseCursor::NormalCursor);
        }
    };
    Canvas canvas { *this };

    M3KNormalizatorProcessor& processor;

    juce::Slider targetLufsSlider, releaseSlider, windowSlider;
    juce::ToggleButton normalizeButton { "NORMALIZE" };
    juce::TextButton   modeButtons[8];
    juce::TextButton   resetButton { "RESET I" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> targetAttach, releaseAttach, windowAttach;
    std::unique_ptr<ButtonAttachment> normAttach;

    // Smoothed display values
    float dispM  = -70.f, dispS  = -70.f, dispI  = -70.f;
    float dispNG = 0.f;
    float dispVuInL = -70.f, dispVuInR = -70.f;
    float dispVuOutL = -70.f, dispVuOutR = -70.f;
    float dispLraIn = 0.f, dispLraOut = 0.f;
    unsigned int lastBlockCounter = 0; // detect when audio callbacks stop

    // Graph history — 60s at 30fps = 1800 pts, showing OUTPUT LUFS
    static constexpr int kHistSize = 1800;
    float histM[kHistSize]{}, histS[kHistSize]{}, histI[kHistSize]{};
    int   histPos = 0, histFilled = 0;

    juce::Rectangle<int> graphBounds, vuInBounds, vuOutBounds, knobArea;
    juce::Rectangle<int> lraInBadge, lraOutBadge;

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
