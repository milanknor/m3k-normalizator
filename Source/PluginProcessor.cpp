#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout M3KNormalizatorProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "targetLufs", "Target LUFS",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -14.0f,
        juce::AudioParameterFloatAttributes().withLabel("LUFS")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "releaseMs", "Up Speed",
        juce::NormalisableRange<float>(10.0f, 4000.0f, 1.0f, 0.5f), 300.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "attackMs", "Down Speed",
        juce::NormalisableRange<float>(5.0f, 2000.0f, 1.0f, 0.5f), 80.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "customMs", "Window",
        juce::NormalisableRange<float>(10.0f, 10000.0f, 1.0f, 0.4f), 1000.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "ceiling", "Limiter",
        juce::NormalisableRange<float>(-6.0f, 0.0f, 0.1f), -0.3f,
        juce::AudioParameterFloatAttributes().withLabel("dBFS")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "inputGain", "Input Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    // Intermediate (glue) compressor — loudness-neutral leveler between AGC and limiter
    layout.add(std::make_unique<juce::AudioParameterBool>("compOn", "Compressor", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "compThresh", "Comp Threshold",
        juce::NormalisableRange<float>(-40.0f, 0.0f, 0.1f), -20.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "compRatio", "Comp Ratio",
        juce::NormalisableRange<float>(1.0f, 8.0f, 0.1f), 3.0f,
        juce::AudioParameterFloatAttributes().withLabel(":1")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "compAttack", "Comp Attack",
        juce::NormalisableRange<float>(1.0f, 100.0f, 0.1f, 0.5f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "compRelease", "Comp Release",
        juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f, 0.5f), 150.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    // Master output volume (post-limiter attenuation) — final playback level
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "outputVol", "Output Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        "normalize", "Auto Normalize", true));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "mode", "Mode",
        juce::StringArray { "Momentary","Short","Integrated","Custom",
                            "Momentary C","Short C","Integrated C","Custom C" }, 2));

    return layout;
}

M3KNormalizatorProcessor::M3KNormalizatorProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

bool M3KNormalizatorProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

// ---- K-weighting (ITU-R BS.1770-4 / EBU R128) ----
// The loudness weighting used for all LUFS measurements.
void M3KNormalizatorProcessor::computeKWeightingCoefficients(double fs)
{
    // Stage 1: high-shelf pre-filter
    {
        const double Vh=1.58486, Vb=1.25872, Q=0.71054, fc=1681.974450955533;
        double K=std::tan(juce::MathConstants<double>::pi*fc/fs), Ks=K*K, d=1+K/Q+Ks;
        kHsB[0]=(Vh+Vb*K/Q+Ks)/d; kHsB[1]=2*(Ks-Vh)/d;  kHsB[2]=(Vh-Vb*K/Q+Ks)/d;
        kHsA[0]=1;                 kHsA[1]=2*(Ks-1)/d;    kHsA[2]=(1-K/Q+Ks)/d;
    }
    // Stage 2: RLB high-pass filter
    {
        const double Q=0.5003270373238773, fc=38.13547087602444;
        double K=std::tan(juce::MathConstants<double>::pi*fc/fs), Ks=K*K, d=1+K/Q+Ks;
        kRlbB[0]=1/d; kRlbB[1]=-2/d; kRlbB[2]=1/d;
        kRlbA[0]=1;   kRlbA[1]=2*(Ks-1)/d; kRlbA[2]=(1-K/Q+Ks)/d;
    }
}

// ---- C-weighting (IEC 61672-1) ----
// H_C(s) = [s*a4 / ((s+a1)*(s+a4))]^2, a1=2π*20.6, a4=2π*12194
// Implemented as two cascaded identical biquads using bilinear transform.
void M3KNormalizatorProcessor::computeCWeightingCoefficients(double fs)
{
    const double a1 = 2.0 * juce::MathConstants<double>::pi * 20.6;
    const double a4 = 2.0 * juce::MathConstants<double>::pi * 12194.0;
    const double K  = 2.0 * fs;

    double D  = K*K + (a1+a4)*K + a1*a4;
    double g  = a4 * K / D;

    cB[0] =  g;
    cB[1] =  0.0;
    cB[2] = -g;
    cA[0] =  1.0;
    cA[1] =  (2.0*a1*a4 - 2.0*K*K) / D;
    cA[2] =  (K*K - (a1+a4)*K + a1*a4) / D;
}

double M3KNormalizatorProcessor::processBiquad(double x, BiquadState& s,
                                           const double b[3], const double a[3])
{
    double y = b[0]*x + s.z1;
    s.z1 = b[1]*x - a[1]*y + s.z2;
    s.z2 = b[2]*x - a[2]*y;
    return y;
}

// ---- Loudness Range (EBU Tech 3342) from a short-term-loudness histogram ----
float M3KNormalizatorProcessor::computeLra(const long long* hist) const
{
    auto binToLufs = [](int i){ return kLraMinDb + (i + 0.5) * kLraBinW; };

    // Energy-weighted mean of all (absolute-gated) blocks
    long long total = 0; double energy = 0.0;
    for (int i = 0; i < kLraBins; ++i)
        if (hist[i] > 0)
        {
            total  += hist[i];
            energy += (double)hist[i] * std::pow(10.0, binToLufs(i) / 10.0);
        }
    if (total < 10) return 0.0f;            // need ~1 s of data

    double meanL     = 10.0 * std::log10(energy / (double)total);
    double relThresh = meanL - 20.0;        // relative gate: -20 LU

    long long n = 0;
    for (int i = 0; i < kLraBins; ++i)
        if (hist[i] > 0 && binToLufs(i) >= relThresh) n += hist[i];
    if (n < 2) return 0.0f;

    long long p10c = (long long)std::ceil(0.10 * n);
    long long p95c = (long long)std::ceil(0.95 * n);
    double lo = relThresh, hi = relThresh;
    bool gotLo = false, gotHi = false;
    long long cum = 0;
    for (int i = 0; i < kLraBins; ++i)
    {
        if (hist[i] <= 0 || binToLufs(i) < relThresh) continue;
        cum += hist[i];
        if (!gotLo && cum >= p10c) { lo = binToLufs(i); gotLo = true; }
        if (!gotHi && cum >= p95c) { hi = binToLufs(i); gotHi = true; break; }
    }
    return (float)std::max(0.0, hi - lo);
}

// EBU R128 integrated loudness from a histogram of 400 ms momentary blocks:
// absolute gate at -70 LUFS, then a relative gate at (energy-mean - 10 LU).
float M3KNormalizatorProcessor::computeIntegratedGated(const long long* hist) const
{
    auto binToLufs = [](int i){ return kLraMinDb + (i + 0.5) * kLraBinW; };

    // Pass 1: energy-mean over absolute-gated blocks (histogram already starts at -70).
    long long total = 0; double energy = 0.0;
    for (int i = 0; i < kLraBins; ++i)
        if (hist[i] > 0)
        {
            total  += hist[i];
            energy += (double)hist[i] * std::pow(10.0, binToLufs(i) / 10.0);
        }
    if (total < 4) return -70.0f;               // need ~400 ms of gated signal

    double meanL     = 10.0 * std::log10(energy / (double)total);
    double relThresh = meanL - 10.0;            // EBU relative gate: -10 LU

    // Pass 2: energy-mean over blocks above the relative threshold.
    long long n = 0; double e2 = 0.0;
    for (int i = 0; i < kLraBins; ++i)
        if (hist[i] > 0 && binToLufs(i) >= relThresh)
        {
            n  += hist[i];
            e2 += (double)hist[i] * std::pow(10.0, binToLufs(i) / 10.0);
        }
    if (n < 1) return (float)meanL;
    return (float)(10.0 * std::log10(e2 / (double)n));
}

void M3KNormalizatorProcessor::pushSpectrumSample(float s)
{
    fftFifo[(size_t)fftFifoIdx] = s;
    if (++fftFifoIdx >= kFftSize) { computeSpectrum(); fftFifoIdx = 0; }
}

void M3KNormalizatorProcessor::computeSpectrum()
{
    std::fill(fftBuf.begin(), fftBuf.end(), 0.0f);
    std::copy(fftFifo.begin(), fftFifo.end(), fftBuf.begin());
    fftWindow.multiplyWithWindowingTable(fftBuf.data(), (size_t)kFftSize);
    fft.performFrequencyOnlyForwardTransform(fftBuf.data());   // magnitudes in [0..kFftSize/2]

    const double nyq = sampleRate_ * 0.5;
    const float  norm = 2.0f / (float)kFftSize;
    for (int b = 0; b < kSpecBins; ++b)
    {
        // 20 Hz .. 20 kHz, log spaced (20 * 1000^(b/(N-1)))
        const double f0 = 20.0 * std::pow(1000.0, (double)b      / (kSpecBins));
        const double f1 = 20.0 * std::pow(1000.0, (double)(b+1)  / (kSpecBins));
        int i0 = juce::jlimit(0, kFftSize/2, (int)(f0 / nyq * (kFftSize/2)));
        int i1 = juce::jlimit(i0+1, kFftSize/2, (int)(f1 / nyq * (kFftSize/2)));
        float mag = 0.0f;
        for (int i = i0; i < i1; ++i) mag = std::max(mag, fftBuf[(size_t)i]);
        float db = juce::Decibels::gainToDecibels(mag * norm, -100.0f);
        float prev = spectrum[b].load();
        spectrum[b].store(prev + 0.4f * (db - prev));   // light smoothing
    }
}

void M3KNormalizatorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    prepared_ = false;
    sampleRate_ = sampleRate;

    computeKWeightingCoefficients(sampleRate);
    computeCWeightingCoefficients(sampleRate);

    for (int ch = 0; ch < 2; ++ch)
        kHsState[ch] = kRlbState[ch] = cState1[ch] = cState2[ch]
            = kHsStateOut[ch] = kRlbStateOut[ch] = {};

    momSamples       = std::max(1, (int)(sampleRate * momentaryWindowMs / 1000.0));
    stSamples        = std::max(1, (int)(sampleRate * shortTermWindowMs / 1000.0));
    customSamplesMax = std::max(1, (int)(sampleRate * maxCustomMs       / 1000.0));

    kMomBuf   .setSize(2, momSamples,       false, true, false); kMomBuf   .clear();
    kStBuf    .setSize(2, stSamples,        false, true, false); kStBuf    .clear();
    kCustomBuf.setSize(2, customSamplesMax, false, true, false); kCustomBuf.clear();
    cMomBuf   .setSize(2, momSamples,       false, true, false); cMomBuf   .clear();
    cStBuf    .setSize(2, stSamples,        false, true, false); cStBuf    .clear();
    cCustomBuf.setSize(2, customSamplesMax, false, true, false); cCustomBuf.clear();
    kStBufOut .setSize(2, stSamples,        false, true, false); kStBufOut .clear();
    stPosOut = 0;

    momPos = stPos = customPos = 0;
    kIntSum=0; kIntCount=0;
    cIntSum=0; cIntCount=0;

    std::fill(std::begin(lraHistIn),  std::end(lraHistIn),  0LL);
    std::fill(std::begin(lraHistOut), std::end(lraHistOut), 0LL);
    std::fill(std::begin(intHistK),   std::end(intHistK),   0LL);
    std::fill(std::begin(intHistC),   std::end(intHistC),   0LL);
    std::fill(std::begin(dispHistK),  std::end(dispHistK),  0LL);
    std::fill(std::begin(dispHistC),  std::end(dispHistC),  0LL);
    lraSampleCounter = 0;

    normGainSmooth = 1.0;
    faderG[0]=faderG[1]=faderG[2]=faderG[3]=1.0;
    limG1 = limG2 = 1.0;
    fftFifoIdx = 0;
    for (int b = 0; b < kSpecBins; ++b) spectrum[b].store(-100.0f);
    compEnv = -120.0; compAvgGr = 0.0;

    // True-peak limiter: 4x oversampling + lookahead, operating in the oversampled domain
    osChannels = juce::jlimit(1, 2, getTotalNumOutputChannels());
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        osChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler->initProcessing((size_t)samplesPerBlock);
    osRate = sampleRate * oversampler->getOversamplingFactor();
    limLookahead = std::max(1, (int)(osRate * 0.0015)); // ~1.5 ms in oversampled samples
    limDelay.setSize(2, limLookahead, false, true, false);
    limDelay.clear();
    limWritePos = 0;
    // Monotonic-deque ring for the sliding-window minimum (capacity = window + slack)
    limDqVal.assign((size_t)limLookahead + 2, 1.0f);
    limDqIdx.assign((size_t)limLookahead + 2, 0LL);
    limDqHead = 0; limDqCount = 0; limSampleIdx = 0;
    setLatencySamples((int)oversampler->getLatencyInSamples()
                      + (int)(limLookahead / oversampler->getOversamplingFactor()));
    activeSamples = 0;
    silentSamples = 0;
    vuInLevel[0]=vuInLevel[1]=vuOutLevel[0]=vuOutLevel[1]=0.0;

    meterCounter = 0;
    prepared_ = true;

    juce::ignoreUnused(samplesPerBlock);
}

void M3KNormalizatorProcessor::releaseResources()
{
    prepared_ = false;
}

void M3KNormalizatorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (!prepared_) return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jlimit(1, 2, buffer.getNumChannels());
    if (numSamples <= 0) return;

    blockCounter.fetch_add(1, std::memory_order_relaxed); // heartbeat for the GUI

    const float target     = *apvts.getRawParameterValue("targetLufs");
    const float releaseMs  = *apvts.getRawParameterValue("releaseMs");
    const float customMs   = *apvts.getRawParameterValue("customMs");
    const bool  doNorm     = *apvts.getRawParameterValue("normalize") > 0.5f;
    const int   mode       = (int)*apvts.getRawParameterValue("mode");
    const bool  isC        = (mode >= kMomentaryC);

    // ---- Input gain (trim) — applied before measurement & processing ----
    // A fully transparent input trim: it behaves exactly as if the source recording
    // changed level (used to simulate loudness changes), so it must NOT reset or retune
    // anything downstream. Ramped from the previous block to avoid zipper noise.
    const double inGain = juce::Decibels::decibelsToGain(
                              (double)*apvts.getRawParameterValue("inputGain"));
    buffer.applyGainRamp(0, numSamples, (float)inGainPrev, (float)inGain);
    inGainPrev = inGain;

    // ---- Compute per-channel input peak for VU (before any gain) ----
    float inputPeak[2] = { 0.0f, 0.0f };
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            inputPeak[ch] = std::max(inputPeak[ch], std::abs(buffer.getSample(ch, i)));
    if (numChannels < 2) inputPeak[1] = inputPeak[0]; // mono → mirror to both bars

    // ---- K-weighted and C-weighted accumulation into ring buffers ----
    double blockKSum = 0.0, blockCSum = 0.0;   // channel-summed power for this block
    for (int i = 0; i < numSamples; ++i)
    {
        double kSq = 0.0, cSq = 0.0;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            double x = (double)buffer.getSample(ch, i);

            // K-weighting (high-shelf + RLB high-pass)
            double xk = processBiquad(x,  kHsState[ch],  kHsB,  kHsA);
            xk        = processBiquad(xk, kRlbState[ch], kRlbB, kRlbA);
            kMomBuf   .setSample(ch, momPos,    (float)xk);
            kStBuf    .setSample(ch, stPos,     (float)xk);
            kCustomBuf.setSample(ch, customPos, (float)xk);
            kSq += xk * xk;

            // C-weighting (two cascaded stages)
            double xc = processBiquad(x,  cState1[ch], cB, cA);
            xc        = processBiquad(xc, cState2[ch], cB, cA);
            cMomBuf   .setSample(ch, momPos,    (float)xc);
            cStBuf    .setSample(ch, stPos,     (float)xc);
            cCustomBuf.setSample(ch, customPos, (float)xc);
            cSq += xc * xc;
        }

        if (++momPos    >= momSamples)       momPos    = 0;
        if (++stPos     >= stSamples)        stPos     = 0;
        if (++customPos >= customSamplesMax) customPos = 0;

        // Sum channel power (EBU R128 sums channels, does NOT average)
        blockKSum += kSq;
        blockCSum += cSq;
    }

    // Block loudness — used to gate silence out of Integrated and of adaptation
    double blockKLoud = blockKSum > 1e-20 ? -0.691+10.0*std::log10(blockKSum/numSamples) : -200.0;
    double blockCLoud = blockCSum > 1e-20 ? -0.691+10.0*std::log10(blockCSum/numSamples) : -200.0;
    const bool kActive = blockKLoud > -70.0;   // absolute gate (EBU R128)
    const bool cActive = blockCLoud > -70.0;
    if (kActive) { kIntSum += blockKSum; kIntCount += numSamples; }
    if (cActive) { cIntSum += blockCSum; cIntCount += numSamples; }
    const bool signalPresent = isC ? cActive : kActive;

    // Track continuous-signal / silence length
    if (signalPresent) { activeSamples += numSamples; silentSamples = 0;
                         integratedSamples.fetch_add(numSamples); }
    else               { activeSamples = 0; silentSamples += numSamples; }

    // After a real gap, reset Integrated — treat resume as a new track so a stale
    // integrated value from the previous track can't boost a louder new one.
    if (silentSamples > (long long)(sampleRate_ * 0.2))
    {
        kIntSum = 0.0; kIntCount = 0;
        cIntSum = 0.0; cIntCount = 0;
        std::fill(std::begin(intHistK), std::end(intHistK), 0LL);
        std::fill(std::begin(intHistC), std::end(intHistC), 0LL);
    }

    // ---- Compute LUFS values (channel-summed mean power) ----
    auto lufsOf = [&](const juce::AudioBuffer<float>& buf, int nSmp, int nCh) -> double
    {
        if (nSmp <= 0) return -70.0;
        double s = 0.0;
        for (int ch = 0; ch < nCh; ++ch)
            for (int i = 0; i < nSmp; ++i)
                s += (double)buf.getSample(ch,i) * (double)buf.getSample(ch,i);
        s /= (double)nSmp;   // sum over channels, average over time
        return s > 1e-10 ? -0.691 + 10.0*std::log10(s) : -70.0;
    };

    double kMom = lufsOf(kMomBuf, momSamples, numChannels);
    double kSt  = lufsOf(kStBuf,  stSamples,  numChannels);
    double kInt = (double)computeIntegratedGated(intHistK);  // EBU R128 relative-gated

    double cMom = lufsOf(cMomBuf, momSamples, numChannels);
    double cSt  = lufsOf(cStBuf,  stSamples,  numChannels);
    double cInt = (double)computeIntegratedGated(intHistC);  // EBU R128 relative-gated

    // Custom-window LUFS — only summed when a Custom mode is active
    // Custom-window LUFS — computed every block (not only in Custom mode) so the
    // Custom curve / fader preview is available regardless of the active mode.
    double kCustomVal = -70.0, cCustomVal = -70.0;
    {
        const int n = juce::jlimit(1, customSamplesMax,
                                   (int)(sampleRate_ * (double)customMs / 1000.0));
        double sk = 0.0, sc = 0.0;
        for (int k = 0; k < n; ++k)
        {
            int idx = (customPos - 1 - k + customSamplesMax) % customSamplesMax;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                double vk = kCustomBuf.getSample(ch, idx); sk += vk*vk;
                double vc = cCustomBuf.getSample(ch, idx); sc += vc*vc;
            }
        }
        sk /= (double)n; sc /= (double)n;
        kCustomVal = sk > 1e-10 ? -0.691+10.0*std::log10(sk) : -70.0;
        cCustomVal = sc > 1e-10 ? -0.691+10.0*std::log10(sc) : -70.0;
        customLufs.store((float)(isC ? cCustomVal : kCustomVal));
    }

    // ---- Compute target normGain ----
    // During silence, target unity (0 dB) so the gain can't "wind up" and blast
    // when playback resumes after a pause. After silence we also wait until the
    // active measurement window is filled with real signal before adapting, so a
    // half-silent buffer can't trigger a huge boost on resume.
    long long windowNeeded = momSamples;
    switch (mode)
    {
        case kShort:  case kShortC:       windowNeeded = stSamples; break;
        case kIntegrated: case kIntegratedC:
            windowNeeded = (long long)(sampleRate_ * 0.4); break;
        case kCustom: case kCustomC:
            windowNeeded = juce::jlimit((long long)1, (long long)customSamplesMax,
                                        (long long)(sampleRate_ * (double)customMs / 1000.0));
            break;
        default: break; // momentary
    }
    const bool windowReady = (activeSamples >= windowNeeded);

    double targetNormGain = 1.0;
    bool   cutOnly = false;   // warm-up cut: drives the gain down extra-fast
    if (doNorm && signalPresent)
    {
        double ref = -70.0;
        const long long intMin = (long long)(sampleRate_ * 0.4);

        if (windowReady)
        {
            switch (mode)
            {
                case kMomentary:   ref = kMom; break;
                case kShort:       ref = kSt;  break;
                case kIntegrated:  if (kIntCount > intMin) ref = kInt; break;
                case kCustom:      ref = kCustomVal; break;
                case kMomentaryC:  ref = cMom; break;
                case kShortC:      ref = cSt;  break;
                case kIntegratedC: if (cIntCount > intMin) ref = cInt; break;
                case kCustomC:     ref = cCustomVal; break;
            }
        }
        else
        {
            // Window not yet filled after silence: use the INSTANTANEOUS block
            // loudness (~10 ms) to tame loud material immediately. Cut only — never
            // boost during warm-up (a quiet fade-in must not trigger a boost).
            ref = isC ? blockCLoud : blockKLoud;
            cutOnly = true;
        }

        if (ref > -69.0)
        {
            double diffDb = (double)target - ref;
            // Boost only after >=400 ms of continuous signal, so a quiet fade-in
            // right after silence can't trigger a boost burst.
            const bool allowBoost = !cutOnly && (activeSamples >= momSamples);
            if (!allowBoost)
            {
                diffDb = std::min(0.0, diffDb);
            }
            else if (diffDb > 0.0)
            {
                // Cap the boost by the stable per-track Integrated loudness so the
                // gain can't chase quiet passages/gaps upward and pump the limiter.
                // Boost should bring the whole track to target, not every quiet
                // moment. (No-op in Integrated mode, where ref already is integrated.)
                //
                // NOT applied in Custom modes: there the user-set Window is meant to
                // fully govern responsiveness, so the boost must follow the windowed
                // measurement (and thus react to sustained level changes) without the
                // long-memory Integrated holding it down.
                const bool isCustom = (mode == kCustom || mode == kCustomC);
                if (!isCustom)
                {
                    const double    intLoud = isC ? cInt      : kInt;
                    const long long ic      = isC ? cIntCount : kIntCount;
                    if (ic > intMin && intLoud > -69.0)
                        diffDb = std::min(diffDb, (double)target - intLoud + 3.0);
                }
                diffDb = std::max(0.0, diffDb);
            }
            diffDb = juce::jlimit(-40.0, 24.0, diffDb);      // boost capped at +24 dB
            targetNormGain = juce::Decibels::decibelsToGain((float)diffDb);
        }
    }
    else if (doNorm)
    {
        // Silence: hold any cut, but relax boosts back to unity. Holding the cut
        // means a paused track resumes at the right (quiet) level with no burst.
        targetNormGain = std::min(normGainSmooth, 1.0);
    }

    // Asymmetric smoothing: gain comes DOWN at the user's "Down Speed" (attenuation of
    // loud surges), UP at the "Up Speed" (gentle boost of quiet material). Warm-up cut
    // stays extra-fast regardless, as a safety against post-pause bursts.
    const double downMs    = (double)*apvts.getRawParameterValue("attackMs");
    const double downCoeff = std::exp(-(double)numSamples / (sampleRate_ * downMs / 1000.0));
    const double warmCoeff = std::exp(-(double)numSamples / (sampleRate_ * 0.005)); // ~5 ms safety

    // Program-dependent UP (release): large sustained gaps boost at the user's Up Speed;
    // small corrections are slowed up to ~3x to avoid audible micro-pumping near target.
    const double upGapDb = juce::Decibels::gainToDecibels((float)std::max(1e-6, targetNormGain))
                         - juce::Decibels::gainToDecibels((float)std::max(1e-6, normGainSmooth));
    const double upMs    = (double)releaseMs * (1.0 + 2.0 * std::exp(-std::abs(upGapDb) / 3.0));
    const double upCoeff = std::exp(-(double)numSamples / (sampleRate_ * upMs / 1000.0));

    const double coeff = (targetNormGain < normGainSmooth)
                         ? (cutOnly ? warmCoeff : downCoeff)
                         : upCoeff;
    normGainSmooth = normGainSmooth * coeff + targetNormGain * (1.0 - coeff);
    normGainDb.store(juce::Decibels::gainToDecibels((float)normGainSmooth));

    // ---- Per-mode fader preview (display only) — same gating/cap/smoothing as above,
    //      run for all four base modes so the graph shows what each would do without spikes.
    {
        const long long intMinP = (long long)(sampleRate_ * 0.4);
        const double instLoud   = isC ? blockCLoud : blockKLoud;
        const double intLoudP   = isC ? cInt : kInt;
        const long long icP     = isC ? cIntCount : kIntCount;
        const double refs[4] = { isC?cMom:kMom, isC?cSt:kSt,
                                 (icP>intMinP ? intLoudP : -70.0), isC?cCustomVal:kCustomVal };
        const long long wins[4] = { (long long)momSamples, (long long)stSamples,
                                    (long long)(sampleRate_*0.4),
                                    juce::jlimit((long long)1,(long long)customSamplesMax,
                                        (long long)(sampleRate_*(double)customMs/1000.0)) };
        for (int m = 0; m < 4; ++m)
        {
            double tgtG = 1.0; bool cutOnlyP = false;
            if (doNorm && signalPresent)
            {
                double r = refs[m];
                if (activeSamples < wins[m]) { r = instLoud; cutOnlyP = true; }
                if (r > -69.0)
                {
                    double diffDb = (double)target - r;
                    const bool allowBoost = !cutOnlyP && (activeSamples >= momSamples);
                    if (!allowBoost) diffDb = std::min(0.0, diffDb);
                    else if (diffDb > 0.0)
                    {
                        if (m != 3 && icP > intMinP && intLoudP > -69.0)
                            diffDb = std::min(diffDb, (double)target - intLoudP + 3.0);
                        diffDb = std::max(0.0, diffDb);
                    }
                    diffDb = juce::jlimit(-40.0, 24.0, diffDb);
                    tgtG = juce::Decibels::decibelsToGain((float)diffDb);
                }
            }
            else if (doNorm) { tgtG = std::min(faderG[m], 1.0); }

            const double upGap = juce::Decibels::gainToDecibels((float)std::max(1e-6, tgtG))
                               - juce::Decibels::gainToDecibels((float)std::max(1e-6, faderG[m]));
            const double upMsP = (double)releaseMs * (1.0 + 2.0 * std::exp(-std::abs(upGap) / 3.0));
            const double upCP  = std::exp(-(double)numSamples / (sampleRate_ * upMsP / 1000.0));
            const double cf    = (tgtG < faderG[m]) ? (cutOnlyP ? warmCoeff : downCoeff) : upCP;
            faderG[m] = faderG[m] * cf + tgtG * (1.0 - cf);
            faderDb[m].store(juce::Decibels::gainToDecibels((float)faderG[m]));
        }
    }

    // ---- Apply normalization gain ----
    for (int ch = 0; ch < numChannels; ++ch)
        buffer.applyGain(ch, 0, numSamples, (float)normGainSmooth);

    // ---- Intermediate compressor (glue/leveler) — loudness-neutral ----
    // Reduces medium-term dynamics; running-average GR is added back as makeup so the
    // average loudness (the AGC target) is preserved. Sits before the true-peak limiter.
    if (*apvts.getRawParameterValue("compOn") > 0.5f)
    {
        const double thr   = (double)*apvts.getRawParameterValue("compThresh");
        const double ratio = (double)*apvts.getRawParameterValue("compRatio");
        const double knee  = 6.0;
        const double atkC  = std::exp(-1.0 / (sampleRate_ * (double)*apvts.getRawParameterValue("compAttack")  / 1000.0));
        const double relC  = std::exp(-1.0 / (sampleRate_ * (double)*apvts.getRawParameterValue("compRelease") / 1000.0));
        const double avgC  = std::exp(-1.0 / (sampleRate_ * 0.300));   // makeup average
        double minGrDb = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            double l = std::abs((double)buffer.getSample(0, i));
            if (numChannels > 1) l = std::max(l, std::abs((double)buffer.getSample(1, i)));
            double lvl = l <= 1e-12 ? -120.0 : 20.0 * std::log10(l);
            double c = lvl > compEnv ? atkC : relC;
            compEnv = compEnv * c + lvl * (1.0 - c);
            double over = compEnv - thr, gr;
            if      (over <= -knee*0.5) gr = 0.0;
            else if (over >=  knee*0.5) gr = over * (1.0/ratio - 1.0);
            else { double e = over + knee*0.5; gr = (1.0/ratio - 1.0) * (e*e) / (2.0*knee); }
            compAvgGr = compAvgGr * avgC + gr * (1.0 - avgC);
            double gdb = gr - compAvgGr;                 // makeup = -avgGr (neutral)
            double g = juce::Decibels::decibelsToGain(gdb);
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample(ch, i, (float)(buffer.getSample(ch, i) * g));
            minGrDb = std::min(minGrDb, gr);
        }
        compGrDb.store((float)minGrDb);
    }
    else compGrDb.store(0.0f);

    // ---- True-peak limiter: oversample 4x, limit, downsample ----
    // Sliding-window minimum of the required gain over the lookahead window (so the gain
    // is already down when a peak emerges from the delay line — fixes the mistimed,
    // hard-clipping behaviour), then two cascaded one-poles smooth the gain envelope.
    const double ceiling = juce::Decibels::decibelsToGain(
                                (float)*apvts.getRawParameterValue("ceiling"));
    const double atk  = 1.0 - std::exp(-4.0 / (double)limLookahead);  // reaches target within lookahead
    const double atk2 = 1.0 - std::exp(-6.0 / (double)limLookahead);  // 2nd stage, rounds the envelope
    const double rel  = 1.0 - std::exp(-1.0 / (osRate * 0.05));       // ~50 ms release
    const float  ceilF = (float)ceiling;
    const int    cap   = (int)limDqVal.size();

    double minGain = 1.0;
    if (oversampler != nullptr && numChannels == osChannels && cap > 0)
    {
        juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(),
                                           (size_t)numChannels, (size_t)numSamples);
        auto os = oversampler->processSamplesUp(block);
        const int osN = (int)os.getNumSamples();
        for (int i = 0; i < osN; ++i)
        {
            double peak = 0.0;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = std::max(peak, std::abs((double)os.getSample(ch, i)));
            const float gReq = (peak > ceiling) ? (float)(ceiling / peak) : 1.0f;

            // push gReq into the monotonic deque (drop ≥ values from the back)
            while (limDqCount > 0
                   && limDqVal[(limDqHead + limDqCount - 1) % cap] >= gReq)
                --limDqCount;
            const int tail = (limDqHead + limDqCount) % cap;
            limDqVal[tail] = gReq;
            limDqIdx[tail] = limSampleIdx;
            ++limDqCount;
            // drop entries that fell out of the lookahead window from the front
            while (limDqCount > 0 && limDqIdx[limDqHead] <= limSampleIdx - limLookahead)
            { limDqHead = (limDqHead + 1) % cap; --limDqCount; }

            const double gMin = limDqVal[limDqHead];   // front = min gain over the window
            limG1 += (gMin - limG1) * (gMin < limG1 ? atk  : rel);
            limG2 += (limG1 - limG2) * (limG1 < limG2 ? atk2 : rel);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float in      = os.getSample(ch, i);
                float delayed = limDelay.getSample(ch, limWritePos);
                limDelay.setSample(ch, limWritePos, in);
                os.setSample(ch, i, juce::jlimit(-ceilF, ceilF, (float)(delayed * limG2)));
            }
            if (++limWritePos >= limLookahead) limWritePos = 0;
            minGain = std::min(minGain, limG2);
            ++limSampleIdx;
        }
        oversampler->processSamplesDown(block);
    }
    limGrDb.store(juce::Decibels::gainToDecibels((float)minGain, -60.0f));

    // ---- Master output volume (final playback level, post-limiter) ----
    {
        const float outVol = *apvts.getRawParameterValue("outputVol");
        if (outVol < 0.9999f)
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.applyGain(ch, 0, numSamples, outVol);
    }

    // ---- VU metering — measure per-channel output peak after gain ----
    float outputPeak[2] = { 0.0f, 0.0f };
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            outputPeak[ch] = std::max(outputPeak[ch], std::abs(buffer.getSample(ch, i)));
    if (numChannels < 2) outputPeak[1] = outputPeak[0];

    // ---- Spectrum analyzer — feed the mono-summed output ----
    if (numChannels >= 2)
        for (int i = 0; i < numSamples; ++i)
            pushSpectrumSample(0.5f * (buffer.getSample(0,i) + buffer.getSample(1,i)));
    else
        for (int i = 0; i < numSamples; ++i)
            pushSpectrumSample(buffer.getSample(0,i));

    // Peak meter ballistics: instant attack, fast release (~120ms decay)
    const double vuRelBlock = std::exp(-(double)numSamples / (sampleRate_ * 0.120));

    for (int ch = 0; ch < 2; ++ch)
    {
        vuInLevel[ch]  = ((double)inputPeak[ch]  >= vuInLevel[ch])
                       ? (double)inputPeak[ch]  : vuInLevel[ch]  * vuRelBlock;
        vuOutLevel[ch] = ((double)outputPeak[ch] >= vuOutLevel[ch])
                       ? (double)outputPeak[ch] : vuOutLevel[ch] * vuRelBlock;
    }

    vuInputDbL .store(juce::Decibels::gainToDecibels((float)vuInLevel[0],  -70.0f));
    vuInputDbR .store(juce::Decibels::gainToDecibels((float)vuInLevel[1],  -70.0f));
    vuOutputDbL.store(juce::Decibels::gainToDecibels((float)vuOutLevel[0], -70.0f));
    vuOutputDbR.store(juce::Decibels::gainToDecibels((float)vuOutLevel[1], -70.0f));

    // ---- Update LUFS display ----
    meterCounter += numSamples;
    if (meterCounter >= meterUpdateInterval)
    {
        meterCounter = 0;
        // Display Integrated uses the CONTINUOUS histogram (matches the INT timer);
        // the normalization above keeps the per-track-resetting kInt/cInt.
        double inInt = isC ? (double)computeIntegratedGated(dispHistC)
                           : (double)computeIntegratedGated(dispHistK);
        momentaryLufs .store((float)(isC ? cMom : kMom));
        shortTermLufs .store((float)(isC ? cSt  : kSt));
        integratedLufs.store((float)inInt);
        // Output integrated ≈ input integrated + applied gain (for compliance check)
        outIntegratedLufs.store(inInt <= -69.0 ? -70.0f
            : (float)(inInt + juce::Decibels::gainToDecibels((float)normGainSmooth)));
    }

    // ---- Loudness Range — sample short-term loudness every 100 ms ----
    // K-weight the actual OUTPUT into its own short-term ring so the output LRA reflects
    // the processed signal (compressor/limiter), not just input + a gain offset.
    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            double x  = (double)buffer.getSample(ch, i);
            double xk = processBiquad(x,  kHsStateOut[ch],  kHsB,  kHsA);
            xk        = processBiquad(xk, kRlbStateOut[ch], kRlbB, kRlbA);
            kStBufOut.setSample(ch, stPosOut, (float)xk);
        }
        if (++stPosOut >= stSamples) stPosOut = 0;
    }
    const double outST = lufsOf(kStBufOut, stSamples, numChannels);

    lraSampleCounter += numSamples;
    const int lraHop = std::max(1, (int)(sampleRate_ * 0.1));
    if (lraSampleCounter >= lraHop)
    {
        lraSampleCounter -= lraHop;
        auto addSample = [this](long long* hist, double L)
        {
            if (L < kLraMinDb) return;                 // absolute gate at -70 LUFS
            int idx = (int)((L - kLraMinDb) / kLraBinW);
            hist[juce::jlimit(0, kLraBins - 1, idx)]++;
        };
        addSample(lraHistIn,  kSt);
        addSample(lraHistOut, outST);
        lraInputLU .store(computeLra(lraHistIn));
        lraOutputLU.store(computeLra(lraHistOut));

        // EBU R128 integrated: feed 400 ms momentary blocks (K and C) every 100 ms.
        // Resetting histograms (normalization anti-burst) + continuous ones (display).
        addSample(intHistK,  kMom);  addSample(intHistC,  cMom);
        addSample(dispHistK, kMom);  addSample(dispHistC, cMom);
    }
}

juce::AudioProcessorEditor* M3KNormalizatorProcessor::createEditor()
{
    return new M3KNormalizatorEditor(*this);
}

void M3KNormalizatorProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void M3KNormalizatorProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new M3KNormalizatorProcessor();
}
