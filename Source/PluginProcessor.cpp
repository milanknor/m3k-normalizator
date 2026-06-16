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
        "releaseMs", "Speed",
        juce::NormalisableRange<float>(10.0f, 4000.0f, 1.0f, 0.5f), 300.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "customMs", "Window",
        juce::NormalisableRange<float>(10.0f, 10000.0f, 1.0f, 0.4f), 1000.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "ceiling", "Ceiling",
        juce::NormalisableRange<float>(-6.0f, 0.0f, 0.1f), -0.3f,
        juce::AudioParameterFloatAttributes().withLabel("dBFS")));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        "normalize", "Auto Normalize", true));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        "bypass", "Bypass", false));

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

void M3KNormalizatorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    prepared_ = false;
    sampleRate_ = sampleRate;

    computeKWeightingCoefficients(sampleRate);
    computeCWeightingCoefficients(sampleRate);

    for (int ch = 0; ch < 2; ++ch)
        kHsState[ch] = kRlbState[ch] = cState1[ch] = cState2[ch] = {};

    momSamples       = std::max(1, (int)(sampleRate * momentaryWindowMs / 1000.0));
    stSamples        = std::max(1, (int)(sampleRate * shortTermWindowMs / 1000.0));
    customSamplesMax = std::max(1, (int)(sampleRate * maxCustomMs       / 1000.0));

    kMomBuf   .setSize(2, momSamples,       false, true, false); kMomBuf   .clear();
    kStBuf    .setSize(2, stSamples,        false, true, false); kStBuf    .clear();
    kCustomBuf.setSize(2, customSamplesMax, false, true, false); kCustomBuf.clear();
    cMomBuf   .setSize(2, momSamples,       false, true, false); cMomBuf   .clear();
    cStBuf    .setSize(2, stSamples,        false, true, false); cStBuf    .clear();
    cCustomBuf.setSize(2, customSamplesMax, false, true, false); cCustomBuf.clear();

    momPos = stPos = customPos = 0;
    kIntSum=0; kIntCount=0;
    cIntSum=0; cIntCount=0;

    std::fill(std::begin(lraHistIn),  std::end(lraHistIn),  0LL);
    std::fill(std::begin(lraHistOut), std::end(lraHistOut), 0LL);
    lraSampleCounter = 0;

    normGainSmooth = 1.0;
    limGain = 1.0;

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

    const bool  bypass     = *apvts.getRawParameterValue("bypass") > 0.5f;
    const float target     = *apvts.getRawParameterValue("targetLufs");
    const float releaseMs  = *apvts.getRawParameterValue("releaseMs");
    const float customMs   = *apvts.getRawParameterValue("customMs");
    const bool  doNorm     = *apvts.getRawParameterValue("normalize") > 0.5f;
    const int   mode       = (int)*apvts.getRawParameterValue("mode");
    const bool  isC        = (mode >= kMomentaryC);

    // Per-block smoothing coefficient for normGain
    const double smoothCoeff = std::exp(-1.0 / (sampleRate_ * (double)releaseMs / 1000.0
                                                / std::max(1, numSamples)));

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
    if (signalPresent) { activeSamples += numSamples; silentSamples = 0; }
    else               { activeSamples = 0; silentSamples += numSamples; }

    // After a real gap, reset Integrated — treat resume as a new track so a stale
    // integrated value from the previous track can't boost a louder new one.
    if (silentSamples > (long long)(sampleRate_ * 0.2))
    {
        kIntSum = 0.0; kIntCount = 0;
        cIntSum = 0.0; cIntCount = 0;
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
    double kInt = (kIntCount>0 && kIntSum>1e-20)
                ? -0.691+10.0*std::log10(kIntSum/(double)kIntCount) : -70.0;

    double cMom = lufsOf(cMomBuf, momSamples, numChannels);
    double cSt  = lufsOf(cStBuf,  stSamples,  numChannels);
    double cInt = (cIntCount>0 && cIntSum>1e-20)
                ? -0.691+10.0*std::log10(cIntSum/(double)cIntCount) : -70.0;

    // Custom-window LUFS — only summed when a Custom mode is active
    double kCustomVal = -70.0, cCustomVal = -70.0;
    if (mode == kCustom || mode == kCustomC)
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
                const double    intLoud = isC ? cInt      : kInt;
                const long long ic      = isC ? cIntCount : kIntCount;
                if (ic > intMin && intLoud > -69.0)
                    diffDb = std::min(diffDb, (double)target - intLoud + 3.0);
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

    // Asymmetric smoothing: gain comes DOWN fast (extra-fast during warm-up cut to
    // kill any post-pause burst), and UP at the user's Speed setting.
    const double downCoeff = std::exp(-(double)numSamples / (sampleRate_ * 0.010)); // ~10 ms
    const double warmCoeff = std::exp(-(double)numSamples / (sampleRate_ * 0.005)); // ~5 ms
    const double coeff = (targetNormGain < normGainSmooth)
                         ? (cutOnly ? warmCoeff : downCoeff)
                         : smoothCoeff;
    if (bypass) { targetNormGain = 1.0; normGainSmooth = 1.0; }
    normGainSmooth = normGainSmooth * coeff + targetNormGain * (1.0 - coeff);
    if (bypass) normGainSmooth = 1.0;
    normGainDb.store(bypass ? 0.0f : juce::Decibels::gainToDecibels((float)normGainSmooth));

    float limGr = 0.0f;
    if (!bypass)
    {
        // ---- Apply normalization gain ----
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.applyGain(ch, 0, numSamples, (float)normGainSmooth);

        // ---- True-peak limiter: oversample 4x, limit, downsample ----
        const double ceiling  = juce::Decibels::decibelsToGain(
                                    (float)*apvts.getRawParameterValue("ceiling"));
        const double atkCoeff = 1.0 - std::exp(-3.0 / (double)limLookahead);
        const double relCoeff = 1.0 - std::exp(-(double)1.0 / (osRate * 0.15)); // ~150 ms

        double minGain = 1.0;
        if (oversampler != nullptr && numChannels == osChannels)
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
                double gReq = (peak > ceiling) ? ceiling / peak : 1.0;
                limGain += (gReq - limGain) * (gReq < limGain ? atkCoeff : relCoeff);
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    float in      = os.getSample(ch, i);
                    float delayed = limDelay.getSample(ch, limWritePos);
                    limDelay.setSample(ch, limWritePos, in);
                    os.setSample(ch, i, juce::jlimit(-1.0f, 1.0f, (float)(delayed * limGain)));
                }
                if (++limWritePos >= limLookahead) limWritePos = 0;
                minGain = std::min(minGain, limGain);
            }
            oversampler->processSamplesDown(block);
        }
        limGr = juce::Decibels::gainToDecibels((float)minGain, -60.0f);
    }
    limGrDb.store(limGr);

    // ---- VU metering — measure per-channel output peak after gain ----
    float outputPeak[2] = { 0.0f, 0.0f };
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            outputPeak[ch] = std::max(outputPeak[ch], std::abs(buffer.getSample(ch, i)));
    if (numChannels < 2) outputPeak[1] = outputPeak[0];

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
        double inInt = isC ? cInt : kInt;
        momentaryLufs .store((float)(isC ? cMom : kMom));
        shortTermLufs .store((float)(isC ? cSt  : kSt));
        integratedLufs.store((float)inInt);
        // Output integrated ≈ input integrated + applied gain (for compliance check)
        outIntegratedLufs.store(inInt <= -69.0 ? -70.0f
            : (float)(inInt + (bypass ? 0.0 : juce::Decibels::gainToDecibels((float)normGainSmooth))));
    }

    // ---- Loudness Range — sample short-term loudness every 100 ms ----
    // LRA uses K-weighting; output ST ≈ input ST + applied gain (linear, slow gain)
    lraSampleCounter += numSamples;
    const int lraHop = std::max(1, (int)(sampleRate_ * 0.1));
    if (lraSampleCounter >= lraHop)
    {
        lraSampleCounter -= lraHop;
        const double gainDb = juce::Decibels::gainToDecibels((float)normGainSmooth);
        auto addSample = [this](long long* hist, double L)
        {
            if (L < kLraMinDb) return;                 // absolute gate at -70 LUFS
            int idx = (int)((L - kLraMinDb) / kLraBinW);
            hist[juce::jlimit(0, kLraBins - 1, idx)]++;
        };
        addSample(lraHistIn,  kSt);
        addSample(lraHistOut, kSt + gainDb);
        lraInputLU .store(computeLra(lraHistIn));
        lraOutputLU.store(computeLra(lraHistOut));
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
