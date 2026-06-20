#pragma once
#include <JuceHeader.h>

class M3KNormalizatorProcessor : public juce::AudioProcessor
{
public:
    M3KNormalizatorProcessor();
    ~M3KNormalizatorProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- Presets (per-instance; loading affects only this instance) ----
    void savePreset(const juce::File& f)
    {
        if (auto xml = apvts.copyState().createXml())
            xml->writeTo(f);
    }
    void loadPreset(const juce::File& f)
    {
        if (auto xml = juce::XmlDocument::parse(f))
            if (xml->hasTagName(apvts.state.getType()))
                apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }

    juce::AudioProcessorValueTreeState apvts;

    // Metering — written from audio thread, read from GUI
    std::atomic<float> momentaryLufs  { -70.0f };
    std::atomic<float> shortTermLufs  { -70.0f };
    std::atomic<float> integratedLufs { -70.0f };
    std::atomic<float> normGainDb     {   0.0f };
    std::atomic<float> vuInputDbL     { -70.0f };
    std::atomic<float> vuInputDbR     { -70.0f };
    std::atomic<float> vuOutputDbL    { -70.0f };
    std::atomic<float> vuOutputDbR    { -70.0f };
    std::atomic<float> lraInputLU     {   0.0f };
    std::atomic<float> lraOutputLU    {   0.0f };
    std::atomic<float> limGrDb        {   0.0f }; // limiter gain reduction (<=0)
    std::atomic<float> outIntegratedLufs { -70.0f }; // output integrated (for compliance)
    std::atomic<unsigned int> blockCounter { 0 }; // heartbeat: incremented each processBlock
    std::atomic<long long> integratedSamples { 0 }; // samples above gate (for elapsed time display)
    std::atomic<float> customLufs { -70.0f };       // current custom-window LUFS (active mode)
    // Per-mode fader preview (dB) — gain each mode WOULD apply (same gating/cap/smoothing
    // as the real fader). Index 0=Momentary,1=Short,2=Integrated,3=Custom (active weighting).
    std::atomic<float> faderDb[4] { {0.0f},{0.0f},{0.0f},{0.0f} };

    // Row 1 = A-weighted, Row 2 = C-weighted
    enum Mode { kMomentary=0, kShort, kIntegrated, kCustom,
                kMomentaryC, kShortC, kIntegratedC, kCustomC };

    void resetIntegrated()
    {
        // Resets ONLY the Integrated measurement (and its elapsed time).
        // LRA is intentionally left untouched — reset LRA by clicking the LRA badges.
        kIntSum = 0.0; kIntCount = 0;
        cIntSum = 0.0; cIntCount = 0;
        std::fill(std::begin(intHistK), std::end(intHistK), 0LL);
        std::fill(std::begin(intHistC), std::end(intHistC), 0LL);
        integratedLufs.store(-70.0f);
        integratedSamples.store(0);
    }

    void resetLraInput()
    {
        std::fill(std::begin(lraHistIn), std::end(lraHistIn), 0LL);
        lraInputLU.store(0.0f);
    }
    void resetLraOutput()
    {
        std::fill(std::begin(lraHistOut), std::end(lraHistOut), 0LL);
        lraOutputLU.store(0.0f);
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct BiquadState { double z1 = 0.0, z2 = 0.0; };
    double processBiquad(double x, BiquadState& s, const double b[3], const double a[3]);

    // ---- K-weighting (ITU-R BS.1770-4 / EBU R128) — two cascaded biquads ----
    BiquadState kHsState[2], kRlbState[2];
    double kHsB[3]{}, kHsA[3]{}, kRlbB[3]{}, kRlbA[3]{};
    void computeKWeightingCoefficients(double fs);

    // ---- C-weighting (IEC 61672-1) — two identical cascaded biquads ----
    BiquadState cState1[2], cState2[2];
    double cB[3]{}, cA[3]{};
    void computeCWeightingCoefficients(double fs);

    static constexpr int momentaryWindowMs = 400;
    static constexpr int shortTermWindowMs = 3000;
    static constexpr int maxCustomMs       = 10000;
    static constexpr int meterUpdateInterval = 512;

    // Measurement ring buffers (positions shared — all advance every sample)
    juce::AudioBuffer<float> kMomBuf, kStBuf, kCustomBuf;
    juce::AudioBuffer<float> cMomBuf, cStBuf, cCustomBuf;
    int momPos = 0, momSamples = 0;
    int stPos  = 0, stSamples  = 0;
    int customPos = 0, customSamplesMax = 0;

    // Integrated accumulators (sum of per-sample channel-summed power)
    double kIntSum = 0.0; long long kIntCount = 0;
    double cIntSum = 0.0; long long cIntCount = 0;

    // Loudness Range (EBU Tech 3342) — histograms of short-term loudness
    static constexpr int    kLraBins  = 751;     // -70.0 .. +5.0 LUFS in 0.1 LU bins
    static constexpr double kLraMinDb = -70.0;
    static constexpr double kLraBinW  = 0.1;
    long long lraHistIn [kLraBins] {};
    long long lraHistOut[kLraBins] {};
    int lraSampleCounter = 0;
    float computeLra(const long long* hist) const;

    // Integrated loudness — EBU R128 two-stage gating (absolute -70 LUFS + relative -10 LU).
    // Histograms of 400 ms momentary blocks sampled every 100 ms (75% overlap, per EBU).
    long long intHistK[kLraBins] {};
    long long intHistC[kLraBins] {};
    float computeIntegratedGated(const long long* hist) const;

    // Input gain (trim) — ramped per block to avoid zipper noise
    double inGainPrev = 1.0;

    // Smoothed normGain (prevents clicks on target changes)
    double normGainSmooth = 1.0;
    double faderG[4] = { 1.0, 1.0, 1.0, 1.0 };   // per-mode preview smoothing states

    // Safety output ceiling — oversampled (true-peak) lookahead limiter
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int    osChannels   = 2;
    double osRate       = 176400.0;
    juce::AudioBuffer<float> limDelay;   // lookahead delay line (oversampled domain)
    int    limWritePos  = 0;
    int    limLookahead = 0;
    // Lookahead peak limiter: sliding-window minimum of required gain (monotonic deque,
    // preallocated ring) + two-stage smoothing. Holds true-peak at the ceiling without
    // the hard 0 dBFS clipping that caused distortion at high loudness.
    double limG1 = 1.0, limG2 = 1.0;
    std::vector<float>     limDqVal;     // monotonic deque values (increasing → front=min)
    std::vector<long long> limDqIdx;     // sample index of each deque entry
    int       limDqHead  = 0;
    int       limDqCount = 0;
    long long limSampleIdx = 0;

    // How many consecutive non-silent samples — gates boosting until the
    // measurement window is filled with real signal (prevents post-pause bursts)
    long long activeSamples = 0;
    long long silentSamples = 0;   // continuous silence; resets Integrated after a gap

    // VU ballistic envelope (per channel L/R)
    double vuInLevel[2]  = { 0.0, 0.0 };
    double vuOutLevel[2] = { 0.0, 0.0 };

    int    meterCounter = 0;
    double sampleRate_  = 44100.0;
    bool   prepared_    = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(M3KNormalizatorProcessor)
};
