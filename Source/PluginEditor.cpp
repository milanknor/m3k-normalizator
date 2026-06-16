#include "PluginEditor.h"

static juce::Font pf(float sz, bool bold=false)
{
    return juce::Font(juce::FontOptions().withHeight(sz).withStyle(bold?"Bold":"Regular"));
}
static juce::Colour amber()  { return juce::Colour(0xFFE8A020); }
static juce::Colour dimCol() { return juce::Colour(0xFF555555); }
static juce::Colour txtCol() { return juce::Colour(0xFFCCCCCC); }
static juce::Colour colM()   { return juce::Colour(0xFF30C870); }
static juce::Colour colS()   { return juce::Colour(0xFF2090FF); }
static juce::Colour colI()   { return amber(); }

// ---- M3K logo (embedded SVG) ----
static std::unique_ptr<juce::Drawable> makeLogo()
{
    static const char* logoSvg =
        "<svg viewBox=\"222 100 236 96\" xmlns=\"http://www.w3.org/2000/svg\" width=\"236\" height=\"96\">"
        "<defs><clipPath id=\"wideclip\"><rect x=\"140\" y=\"100\" width=\"400\" height=\"96\"/></clipPath></defs>"
        "<g clip-path=\"url(#wideclip)\">"
        "<circle cx=\"340\" cy=\"148\" r=\"52\" fill=\"#1a1a24\"/>"
        "<circle cx=\"340\" cy=\"148\" r=\"74\" fill=\"none\" stroke=\"#F97316\" stroke-width=\"1.8\"/>"
        "<circle cx=\"340\" cy=\"148\" r=\"52\" fill=\"none\" stroke=\"#F97316\" stroke-width=\"2.5\"/>"
        "<circle cx=\"340\" cy=\"148\" r=\"32\" fill=\"none\" stroke=\"#F97316\" stroke-width=\"3.5\"/>"
        "<circle cx=\"340\" cy=\"148\" r=\"96\" fill=\"none\" stroke=\"#F97316\" stroke-width=\"1.3\"/>"
        "<circle cx=\"340\" cy=\"148\" r=\"118\" fill=\"none\" stroke=\"#F97316\" stroke-width=\"1.5\"/>"
        "</g>"
        "<path fill=\"#ffffff\" transform=\"translate(283.32,170) scale(0.0640,-0.0640)\" d=\"M857 702V0H686V421L529 0H391L233 422V0H62V702H264L461 216L656 702Z\"/>"
        "<path fill=\"#ffffff\" transform=\"translate(352.07,170) scale(0.0640,-0.0640)\" d=\"M469 0 233 310V0H62V702H233V394L467 702H668L396 358L678 0Z\"/>"
        "<path fill=\"#F97316\" transform=\"translate(339.05,140) scale(0.0265,-0.0265)\" d=\"M300 747Q375 747 428.5 721.0Q482 695 509.5 650.0Q537 605 537 549Q537 483 504.0 441.5Q471 400 427 385V381Q484 362 517.0 318.0Q550 274 550 205Q550 143 521.5 95.5Q493 48 438.5 21.0Q384 -6 309 -6Q189 -6 117.5 53.0Q46 112 42 231H208Q209 187 233.0 161.5Q257 136 303 136Q342 136 363.5 158.5Q385 181 385 218Q385 266 354.5 287.5Q324 309 257 309H225V448H257Q308 448 339.5 465.5Q371 483 371 528Q371 564 351.0 584.0Q331 604 296 604Q258 604 239.5 581.0Q221 558 218 524H51Q55 631 121.0 689.0Q187 747 300 747Z\"/>"
        "</svg>";
    return juce::Drawable::createFromImageData(logoSvg, std::strlen(logoSvg));
}

// ---- "About" popup content ----
struct AboutComponent : public juce::Component
{
    std::unique_ptr<juce::Drawable> logo = makeLogo();
    AboutComponent() { setSize(250, 196); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF161616));
        g.setColour(amber()); g.drawRect(getLocalBounds(), 1);
        if(logo) logo->drawWithin(g, juce::Rectangle<float>(getWidth()*0.5f-46,14,92,92),
                                  juce::RectanglePlacement::centred, 1.0f);
        g.setColour(amber()); g.setFont(pf(15,true));
        g.drawText("M3K NORMALIZATOR", 0,112,getWidth(),20, juce::Justification::centred);
        g.setColour(txtCol()); g.setFont(pf(12,true));
        g.drawText("verze " JucePlugin_VersionString, 0,136,getWidth(),16, juce::Justification::centred);
        g.setColour(dimCol()); g.setFont(pf(9));
        g.drawText("LUFS normalizace  -  EBU R128 / IEC 61672",
                   0,160,getWidth(),14, juce::Justification::centred);
        g.drawText(juce::String::fromUTF8("(c) MilanAudio"),
                   0,174,getWidth(),14, juce::Justification::centred);
    }
};

// ---- LookAndFeel ----
void M3KNormalizatorEditor::AmberLAF::drawRotarySlider(
    juce::Graphics& g, int x, int y, int w, int h,
    float pos, float rotStart, float rotEnd, juce::Slider& s)
{
    float cx=x+w*.5f, cy=y+h*.5f, r=std::min(w,h)*.5f-4.f;
    if(r<=0) return;
    auto arc=[&](float f, float t, juce::Colour c, float th){
        juce::Path p; p.addCentredArc(cx,cy,r,r,0,f,t,true);
        g.setColour(c); g.strokePath(p,juce::PathStrokeType(th,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    };
    arc(rotStart,rotEnd,juce::Colour(0xFF333333),3);
    float angle=rotStart+pos*(rotEnd-rotStart);
    arc(rotStart,angle,amber(),3);
    float dx=cx+(r-2)*std::cos(angle-juce::MathConstants<float>::halfPi);
    float dy=cy+(r-2)*std::sin(angle-juce::MathConstants<float>::halfPi);
    g.setColour(juce::Colours::white); g.fillEllipse(dx-3,dy-3,6,6);
    g.setColour(juce::Colour(0xFF1E1E1E));
    g.fillEllipse(cx-r+9,cy-r+9,(r-9)*2,(r-9)*2);

    // Value drawn inside the knob
    int decimals = (s.getInterval() > 0.0 && s.getInterval() < 1.0) ? 1 : 0;
    juce::String val = juce::String(s.getValue(), decimals);
    g.setColour(juce::Colours::white);
    g.setFont(pf(std::min(17.f, (r-9)*0.7f), true));
    g.drawText(val, juce::Rectangle<float>(cx-r, cy-8, 2*r, 16),
               juce::Justification::centred);
}
void M3KNormalizatorEditor::AmberLAF::drawButtonBackground(
    juce::Graphics& g, juce::Button& btn, const juce::Colour&, bool, bool)
{
    auto b=btn.getLocalBounds().toFloat().reduced(.5f);
    bool on=btn.getToggleState();
    g.setColour(on?amber().withAlpha(.25f):juce::Colour(0xFF1A1A1A));
    g.fillRoundedRectangle(b,4);
    g.setColour(on?amber():juce::Colour(0xFF444444));
    g.drawRoundedRectangle(b,4,1);
}
void M3KNormalizatorEditor::AmberLAF::drawButtonText(
    juce::Graphics& g, juce::TextButton& btn, bool, bool)
{
    bool on=btn.getToggleState();
    g.setColour(on?amber():txtCol()); g.setFont(pf(10,on));
    g.drawText(btn.getButtonText(),btn.getLocalBounds(),juce::Justification::centred);
}
void M3KNormalizatorEditor::AmberLAF::drawTickBox(
    juce::Graphics& g, juce::Component&, float x, float y, float w, float h,
    bool ticked, bool, bool, bool)
{
    // Orange filled rounded square (checked) / dark outline (unchecked) — per reference UI
    float s = std::min(w, h);
    juce::Rectangle<float> box(x, y + (h - s) * .5f, s, s);
    box = box.reduced(1.f);
    if (ticked)
    {
        g.setColour(amber());
        g.fillRoundedRectangle(box, 3.f);
    }
    else
    {
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillRoundedRectangle(box, 3.f);
        g.setColour(juce::Colour(0xFF555555));
        g.drawRoundedRectangle(box, 3.f, 1.2f);
    }
}

// ---- Editor ----
M3KNormalizatorEditor::M3KNormalizatorEditor(M3KNormalizatorProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // Canvas holds the UI at design size and is scaled down uniformly
    addAndMakeVisible(canvas);
    canvas.setBounds(0, 0, kDesignW, kDesignH);
    canvas.setTransform(juce::AffineTransform::scale(kUiScale));
    setSize(juce::roundToInt(kDesignW * kUiScale),
            juce::roundToInt(kDesignH * kUiScale));

    logo = makeLogo();

    std::fill(histM,histM+kHistSize,-70.f);
    std::fill(histS,histS+kHistSize,-70.f);
    std::fill(histI,histI+kHistSize,-70.f);

    auto setupKnob=[&](juce::Slider& s){
        s.setLookAndFeel(&laf);
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0); // value drawn inside the knob
        canvas.addAndMakeVisible(s);
    };
    setupKnob(targetLufsSlider);
    setupKnob(releaseSlider);
    setupKnob(windowSlider);
    setupKnob(ceilingSlider);

    normalizeButton.setLookAndFeel(&laf);
    normalizeButton.setColour(juce::ToggleButton::textColourId,txtCol());
    normalizeButton.setColour(juce::ToggleButton::tickColourId,amber());
    normalizeButton.setColour(juce::ToggleButton::tickDisabledColourId,dimCol());
    canvas.addAndMakeVisible(normalizeButton);

    targetAttach  = std::make_unique<SliderAttachment>(processor.apvts,"targetLufs",targetLufsSlider);
    releaseAttach = std::make_unique<SliderAttachment>(processor.apvts,"releaseMs",  releaseSlider);
    windowAttach  = std::make_unique<SliderAttachment>(processor.apvts,"customMs",   windowSlider);
    ceilingAttach = std::make_unique<SliderAttachment>(processor.apvts,"ceiling",    ceilingSlider);
    normAttach    = std::make_unique<ButtonAttachment>(processor.apvts,"normalize",  normalizeButton);

    static const char* mnames[]={"Momentary","Short","Integrated","Custom",
                                 "Momentary C","Short C","Integrated C","Custom C"};
    for(int i=0;i<8;++i){
        modeButtons[i].setButtonText(mnames[i]);
        modeButtons[i].setLookAndFeel(&laf);
        modeButtons[i].setClickingTogglesState(false);
        modeButtons[i].onClick=[this,i](){
            if(auto* pa=dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter("mode")))
                pa->setValueNotifyingHost(pa->convertTo0to1((float)i));
        };
        canvas.addAndMakeVisible(modeButtons[i]);
    }

    resetButton.setLookAndFeel(&laf);
    resetButton.onClick=[this](){ processor.resetIntegrated(); };
    canvas.addAndMakeVisible(resetButton);

    // Preset menu (factory standards + save/load)
    presetButton.setLookAndFeel(&laf);
    presetButton.onClick=[this](){ showPresetMenu(); };
    canvas.addAndMakeVisible(presetButton);

    startTimerHz(30);
}

M3KNormalizatorEditor::~M3KNormalizatorEditor()
{
    stopTimer();
    targetLufsSlider.setLookAndFeel(nullptr);
    releaseSlider   .setLookAndFeel(nullptr);
    windowSlider    .setLookAndFeel(nullptr);
    normalizeButton .setLookAndFeel(nullptr);
    resetButton     .setLookAndFeel(nullptr);
    presetButton    .setLookAndFeel(nullptr);
    for(auto& b:modeButtons) b.setLookAndFeel(nullptr);
}

juce::File M3KNormalizatorEditor::presetDir()
{
    auto d=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
              .getChildFile("M3K Normalizator Presets");
    if(!d.exists()) d.createDirectory();
    return d;
}

void M3KNormalizatorEditor::setParam(const juce::String& id, float value)
{
    if(auto* p=processor.apvts.getParameter(id))
        p->setValueNotifyingHost(processor.apvts.getParameterRange(id).convertTo0to1(value));
}

void M3KNormalizatorEditor::applyFactoryPreset(float lufs, float ceiling, float speed)
{
    setParam("targetLufs", lufs);
    setParam("ceiling",    ceiling);
    setParam("releaseMs",  speed);  // plynulá adaptace pro normalizaci
    setParam("mode",       2.0f);   // Integrated (standardní pro normy)
    setParam("normalize",  1.0f);   // zapnout
}

void M3KNormalizatorEditor::showPresetMenu()
{
    struct FP { const char* name; float lufs; float ceil; float speed; };
    static const FP stream[]={{"Spotify  (-14)",-14,-1,1000},{"Spotify Loud  (-11)",-11,-1,1000},
        {"Apple Music  (-16)",-16,-1,1000},{"YouTube  (-14)",-14,-1,1000},{"Amazon Music  (-14)",-14,-1,1000},
        {"Tidal  (-14)",-14,-1,1000},{"Deezer  (-15)",-15,-1,1000}};
    static const FP broad[]={{"EBU R128  (-23)",-23,-1,1500},{"ATSC A/85  (-24)",-24,-2,1500},
        {"TR-B32  (-24)",-24,-1,1500}};
    static const FP pod[]={{"Podcast  (-16)",-16,-1,1200},{"Mluvene slovo  (-19)",-19,-1,1200}};
    static const FP club[]={{"Klub / DJ max  (-8)",-8,-1,500},{"Na doraz  (-6)",-6,-0.3f,300}};

    juce::PopupMenu menu, mS, mB, mP, mC;
    std::vector<FP> all;
    auto fill=[&](juce::PopupMenu& m, const FP* arr, int n){
        for(int i=0;i<n;++i){ all.push_back(arr[i]); m.addItem((int)all.size(), arr[i].name); }
    };
    fill(mS, stream, 7); fill(mB, broad, 3); fill(mP, pod, 2); fill(mC, club, 2);
    menu.addSectionHeader("Tovarni predvolby (Integrated)");
    menu.addSubMenu("Streaming hudba", mS);
    menu.addSubMenu("Broadcast / TV",  mB);
    menu.addSubMenu("Podcast",         mP);
    menu.addSubMenu("Klub / DJ",       mC);
    menu.addSeparator();
    menu.addItem(1000, "Ulozit preset...");
    menu.addItem(1001, "Nacist preset...");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presetButton),
        [this,all](int r)
        {
            if(r>=1 && r<=(int)all.size())
                applyFactoryPreset(all[r-1].lufs, all[r-1].ceil, all[r-1].speed);
            else if(r==1000)
            {
                chooser=std::make_unique<juce::FileChooser>("Uložit preset",presetDir(),"*.m3kpreset");
                chooser->launchAsync(juce::FileBrowserComponent::saveMode
                                    | juce::FileBrowserComponent::canSelectFiles,
                    [this](const juce::FileChooser& fc){
                        auto f=fc.getResult();
                        if(f!=juce::File()) processor.savePreset(f.withFileExtension("m3kpreset"));
                    });
            }
            else if(r==1001)
            {
                chooser=std::make_unique<juce::FileChooser>("Načíst preset",presetDir(),"*.m3kpreset");
                chooser->launchAsync(juce::FileBrowserComponent::openMode
                                    | juce::FileBrowserComponent::canSelectFiles,
                    [this](const juce::FileChooser& fc){
                        auto f=fc.getResult();
                        if(f.existsAsFile()) processor.loadPreset(f);
                    });
            }
        });
}

void M3KNormalizatorEditor::canvasClicked(juce::Point<int> p)
{
    if(lraInBadge.contains(p))  { processor.resetLraInput();  return; } // reset LRA IN
    if(lraOutBadge.contains(p)) { processor.resetLraOutput(); return; } // reset LRA OUT
    if(logoBounds.contains(p))             // logo opens the About box
    {
        auto content = std::make_unique<AboutComponent>();
        juce::CallOutBox::launchAsynchronously(std::move(content), logoBounds, &canvas);
    }
}

void M3KNormalizatorEditor::timerCallback()
{
    const float a=.25f;
    auto sm=[&](float& d,float t){ d+=a*(t-d); };
    sm(dispM,  processor.momentaryLufs .load());
    sm(dispS,  processor.shortTermLufs .load());
    sm(dispI,  processor.integratedLufs.load());
    sm(dispNG, processor.normGainDb    .load());
    // VU meters: while audio is flowing the processor handles ballistics; if the host
    // stops calling processBlock (transport stopped), pull the meters down to zero.
    unsigned int bc = processor.blockCounter.load();
    bool audioFresh = (bc != lastBlockCounter);
    lastBlockCounter = bc;
    auto vu=[&](float& d,float v){ d = audioFresh ? v : d + (-70.f - d)*0.35f; };
    vu(dispVuInL,  processor.vuInputDbL .load());
    vu(dispVuInR,  processor.vuInputDbR .load());
    vu(dispVuOutL, processor.vuOutputDbL.load());
    vu(dispVuOutR, processor.vuOutputDbR.load());
    dispLraIn  = processor.lraInputLU .load();
    dispLraOut = processor.lraOutputLU.load();

    // Graph stores OUTPUT LUFS = input LUFS + normGain
    float outM = juce::jlimit(-70.f,  6.f, dispM + dispNG);
    float outS = juce::jlimit(-70.f,  6.f, dispS + dispNG);
    float outI = juce::jlimit(-70.f,  6.f, dispI + dispNG);
    histM[histPos]=outM; histS[histPos]=outS; histI[histPos]=outI;
    histPos=(histPos+1)%kHistSize;
    if(histFilled<kHistSize) ++histFilled;

    // Sync mode buttons
    int cur=(int)*processor.apvts.getRawParameterValue("mode");
    for(int i=0;i<8;++i) modeButtons[i].setToggleState(i==cur,juce::dontSendNotification);

    canvas.repaint();
}

// ---- VU Meter (stereo: L + R bars) ----
void M3KNormalizatorEditor::drawVuMeter(juce::Graphics& g, juce::Rectangle<int> b,
                                    float dbL, float dbR, const juce::String& label)
{
    const float minDb=-60.f, maxDb=6.f;

    // Background
    g.setColour(juce::Colour(0xFF111111));
    g.fillRoundedRectangle(b.toFloat(),4);
    g.setColour(juce::Colour(0xFF2A2A2A));
    g.drawRoundedRectangle(b.toFloat().reduced(.5f),4,1);

    // Label top
    g.setFont(pf(8.5f,true)); g.setColour(dimCol());
    g.drawText(label,b.withHeight(14),juce::Justification::centred);

    auto bars=b.reduced(3,16).withTrimmedBottom(12);

    // Two side-by-side bars (L / R)
    const int gap=2;
    const int colW=(bars.getWidth()-gap)/2;
    auto barL=bars.withWidth(colW);
    auto barR=bars.withX(bars.getRight()-colW).withWidth(colW);

    auto drawBar=[&](juce::Rectangle<int> bar, float db, const char* ch)
    {
        g.setColour(juce::Colour(0xFF0A0A0A));
        g.fillRoundedRectangle(bar.toFloat(),2);

        float norm=juce::jlimit(0.f,1.f,(db-minDb)/(maxDb-minDb));
        int fillH=(int)(norm*bar.getHeight());
        if(fillH>0)
        {
            auto fill=bar.withTop(bar.getBottom()-fillH);
            // Gradient anchored to the WHOLE bar so colour reflects absolute level:
            // green low, yellow approaching 0 dB, red only at the very top.
            juce::ColourGradient grad(
                juce::Colour(0xFFFF2020), (float)bar.getX(), (float)bar.getY(),
                juce::Colour(0xFF30C870), (float)bar.getX(), (float)bar.getBottom(), false);
            grad.addColour(0.10, juce::Colour(0xFFFF2020)); // red down to ~0 dB
            grad.addColour(0.20, juce::Colour(0xFFFFCC00)); // yellow ~ -6 dB
            grad.addColour(0.42, juce::Colour(0xFF50DD50)); // green below
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fill.toFloat(),2);
        }
        // L/R marker under bar
        g.setFont(pf(7.f,true)); g.setColour(dimCol());
        g.drawText(ch, bar.getX(), bars.getBottom()+1, bar.getWidth(), 9,
                   juce::Justification::centred);
    };
    drawBar(barL, dbL, "L");
    drawBar(barR, dbR, "R");

    // Scale ticks across both bars
    g.setFont(pf(7.f));
    for(float tick : {0.f,-6.f,-12.f,-24.f,-48.f})
    {
        float n=(tick-minDb)/(maxDb-minDb);
        int ty=bars.getBottom()-(int)(n*bars.getHeight());
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.drawHorizontalLine(ty,(float)bars.getX(),(float)bars.getRight());
    }

    // peak dB value (max of L/R) at bottom
    float db=std::max(dbL,dbR);
    float norm=juce::jlimit(0.f,1.f,(db-minDb)/(maxDb-minDb));
    juce::String val = db<=-59.f ? "-inf" : juce::String(db,1);
    g.setFont(pf(8.f,true));
    g.setColour(norm>0.05f ? (db>0.f ? juce::Colour(0xFFFF4040) : juce::Colour(0xFF80EE80)) : dimCol());
    g.drawText(val, b.withTop(b.getBottom()-12), juce::Justification::centred);
}

// ---- Graph ----
void M3KNormalizatorEditor::drawGraph(juce::Graphics& g, juce::Rectangle<int> b)
{
    const float minDb=-60.f, maxDb=6.f;

    g.setColour(juce::Colour(0xFF111111));
    g.fillRoundedRectangle(b.toFloat(),5);
    g.setColour(juce::Colour(0xFF2A2A2A));
    g.drawRoundedRectangle(b.toFloat().reduced(.5f),5,1);

    const int scW=32;
    auto area=b.withTrimmedLeft(scW).withTrimmedRight(2).withTrimmedTop(2).withTrimmedBottom(14);
    float gx=area.getX(), gy=area.getY(), gw=area.getWidth(), gh=area.getHeight();

    // Target LUFS line
    float targetLufs=*processor.apvts.getRawParameterValue("targetLufs");
    {
        float n=(targetLufs-minDb)/(maxDb-minDb);
        float ty=gy+gh-n*gh;
        g.setColour(amber().withAlpha(.35f));
        float dash[]={6,4};
        juce::Path p; p.startNewSubPath(gx,ty); p.lineTo(gx+gw,ty);
        g.strokePath(p,juce::PathStrokeType(1),juce::AffineTransform());
        // draw dashes manually
        g.setColour(amber().withAlpha(.5f));
        for(float dx=gx; dx<gx+gw; dx+=10)
            g.drawHorizontalLine((int)ty,dx,std::min(dx+6,gx+gw));
        g.setFont(pf(8,true)); g.setColour(amber().withAlpha(.8f));
        g.drawText(juce::String(targetLufs,1)+" LUFS", (int)(gx+gw-56),(int)ty-10,55,10,
                   juce::Justification::centredRight);
    }

    // Grid lines + scale
    const float gridDbs[]={6,0,-6,-12,-18,-23,-30,-40,-50,-60};
    for(float db : gridDbs)
    {
        float n=(db-minDb)/(maxDb-minDb);
        float y=gy+gh-n*gh;
        bool ref=(db==-14||db==-23||db==0);
        g.setColour(ref?juce::Colour(0xFF303020):juce::Colour(0xFF1E1E1E));
        g.drawHorizontalLine((int)y,gx,gx+gw);
        g.setFont(pf(8)); g.setColour(ref?amber().withAlpha(.6f):dimCol());
        g.drawText(juce::String((int)db), b.getX(),(int)y-5,scW-3,10,juce::Justification::centredRight);
    }

    // Time labels
    g.setFont(pf(8)); g.setColour(dimCol());
    g.drawText("now",  (int)gx,       b.getBottom()-13, 30, 12, juce::Justification::centredLeft);
    g.drawText("-60s", (int)(gx+gw)-30, b.getBottom()-13, 30, 12, juce::Justification::centredRight);

    g.saveState();
    g.reduceClipRegion(area);

    int pts=std::min(histFilled,(int)gw);
    if(pts>=2)
    {
        auto makePath=[&](const float* hist, juce::Colour col)
        {
            juce::Path path; bool started=false;
            for(int px=0; px<pts; ++px)
            {
                int idx=(histPos-1-px+kHistSize)%kHistSize;
                float db=juce::jlimit(minDb,maxDb,hist[idx]);
                float n=(db-minDb)/(maxDb-minDb);
                float x=gx+gw-1.f-(float)px;
                float y=gy+gh-n*gh;
                if(!started){path.startNewSubPath(x,y);started=true;}
                else path.lineTo(x,y);
            }
            g.setColour(col);
            g.strokePath(path,juce::PathStrokeType(1.5f));
        };
        makePath(histI,colI());
        makePath(histS,colS());
        makePath(histM,colM());
    }
    g.restoreState();

    // Legend
    struct{juce::Colour c;const char* l;}leg[]={{colM(),"M"},{colS(),"S"},{colI(),"I"}};
    int lx=(int)(gx+gw)-72, ly=b.getY()+6;
    for(auto& l:leg){
        g.setColour(l.c); g.fillRect(lx,ly+4,12,2);
        g.setFont(pf(9,true)); g.drawText(l.l,lx+15,ly,20,10,juce::Justification::centredLeft);
        lx+=25;
    }
}

// ---- paint (drawn into the design-size canvas) ----
void M3KNormalizatorEditor::paintCanvas(juce::Graphics& g)
{
    const int W=kDesignW;

    g.fillAll(juce::Colour(0xFF141414));

    // Header
    g.setColour(juce::Colour(0xFF1A1A1A));
    g.fillRect(0,0,W,44);
    g.setColour(amber()); g.fillRect(0,43,W,1);
    // Logo (clickable — opens the About box)
    if(logo)
        logo->drawWithin(g, logoBounds.toFloat(),
                         juce::RectanglePlacement::centred, 1.0f);
    g.setFont(pf(14,true)); g.setColour(amber());
    g.drawText("M3K NORMALIZATOR",56,0,150,44,juce::Justification::centredLeft);
    // version shown bottom-left; SAVE/LOAD + normalize toggle are child components
    g.setFont(pf(8)); g.setColour(dimCol());
    g.drawText("v" JucePlugin_VersionString, 6, kDesignH-12, 70, 11,
               juce::Justification::centredLeft);

    // Current value strip
    const int sY=48, sH=28, sw=(W-28)/4;
    struct{float v;const char* l;juce::Colour c;} strips[]={
        {dispM,"M",colM()},{dispS,"S",colS()},{dispI,"I",colI()},
        {dispNG,"GAIN",dispNG>0?colM():juce::Colour(0xFFFF6040)}
    };
    for(int i=0;i<4;++i){
        int sx=14+i*sw;
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillRoundedRectangle((float)sx,(float)sY,(float)(sw-4),(float)sH,3);
        g.setFont(pf(8,true)); g.setColour(dimCol());
        g.drawText(strips[i].l, sx+5,sY,22,sH,juce::Justification::centredLeft);
        g.setFont(pf(12,true)); g.setColour(strips[i].c);
        juce::String val=strips[i].v<=-69.f?"-inf":
            (i==3&&strips[i].v>0?"+":"")+juce::String(strips[i].v,1);
        g.drawText(val,sx,sY,sw-4,sH,juce::Justification::centred);
    }

    // VU meters (stereo)
    drawVuMeter(g, vuInBounds,  dispVuInL,  dispVuInR,  "IN");
    drawVuMeter(g, vuOutBounds, dispVuOutL, dispVuOutR, "OUT");

    // LRA badges (round, matching the knobs) — under the VU meters
    auto drawLra=[&](juce::Rectangle<int> box, float lra, const char* lbl)
    {
        float cx=box.getCentreX(), cy=box.getCentreY();
        float r=box.getWidth()*0.5f;
        // ring (amber, like the knobs)
        g.setColour(juce::Colour(0xFF333333));
        g.drawEllipse(cx-r,cy-r,2*r,2*r,3.f);
        g.setColour(amber());
        g.drawEllipse(cx-r,cy-r,2*r,2*r,2.f);
        // inner dark face
        g.setColour(juce::Colour(0xFF1E1E1E));
        g.fillEllipse(cx-(r-6),cy-(r-6),2*(r-6),2*(r-6));
        // value
        juce::String v = lra<=0.05f ? "--" : juce::String(lra,1);
        g.setColour(juce::Colours::white); g.setFont(pf(20,true));
        g.drawText(v, box.withTrimmedBottom(10), juce::Justification::centred);
        g.setFont(pf(9,true)); g.setColour(juce::Colour(0xFFA8A8A8));
        g.drawText("LU", box.getX(), (int)(cy+r*0.30f), box.getWidth(), 11,
                   juce::Justification::centred);
        // top label
        g.setFont(pf(10.5f,true)); g.setColour(txtCol());
        g.drawText(lbl, box.getX(), knobArea.getY()-23, box.getWidth(), 13,
                   juce::Justification::centred);
    };
    drawLra(lraInBadge,  dispLraIn,  "LRA IN");
    drawLra(lraOutBadge, dispLraOut, "LRA OUT");

    // Graph
    drawGraph(g, graphBounds);

    // Controls divider — sits just below the graph, with a gap before the labels
    int ctrlTop=knobArea.getY()-32;
    g.setColour(juce::Colour(0xFF282828)); g.fillRect(0,ctrlTop,W,1);

    // Knob labels (4 knobs) — labels can be a touch wider than the knob box
    const int kW=72, kGap=24;
    const int kX=(W-(4*kW+3*kGap))/2;
    const int labelY=knobArea.getY()-23;       // shared baseline for all top labels
    const struct{const char* top,*unit;} kl[]=
        {{"TARGET LUFS","LUFS"},{"SPEED","ms"},{"WINDOW","ms"},{"CEILING","dBFS"}};
    for(int i=0;i<4;++i){
        int cx=kX+i*(kW+kGap)+kW/2;             // centre of this knob
        g.setFont(pf(9.5f,true)); g.setColour(txtCol());
        g.drawText(kl[i].top, cx-48,labelY,96,13,juce::Justification::centred);
        g.setFont(pf(9.f)); g.setColour(juce::Colour(0xFFA8A8A8));
        g.drawText(kl[i].unit,cx-48,knobArea.getBottom()+5,96,12,juce::Justification::centred);
    }
}

// ---- editor resized: keep the scaled canvas anchored top-left ----
void M3KNormalizatorEditor::resized()
{
    canvas.setBounds(0, 0, kDesignW, kDesignH);
    canvas.setTransform(juce::AffineTransform::scale(kUiScale));
}

// ---- canvas layout (design-size coordinates) ----
void M3KNormalizatorEditor::layoutCanvas()
{
    const int W=kDesignW, H=kDesignH, pad=14;

    // Normalize toggle + Reset — top-right of header (above the orange line)
    normalizeButton.setBounds(W-150, 11, 136, 22);
    resetButton.setBounds(W-150-58, 12, 52, 20);
    // PRESET menu — header, between title and Reset
    presetButton.setBounds(214, 12, 92, 20);

    // Mode buttons — two rows of 4 (row1 = A-weighted, row2 = C-weighted)
    const int mBW=96, mBH=22, mBGapX=6, mBGapY=4;
    const int mTW=4*mBW+3*mBGapX, mStartX=(W-mTW)/2;
    const int modeY=48+28+8;
    for(int i=0;i<8;++i){
        int row=i/4, col=i%4;
        modeButtons[i].setBounds(mStartX+col*(mBW+mBGapX),
                                 modeY+row*(mBH+mBGapY), mBW, mBH);
    }

    // Layout areas — knob box sized so its drawn circle (min(kW,kH)-8) == LRA badge (64)
    const int kH=72, kW=72, kGap=24;
    const int ctrlH=kH+48;
    const int graphTop=modeY+2*mBH+mBGapY+8;
    const int graphH=H-graphTop-ctrlH-34;   // extra gap above the control labels

    // VU meters (stereo — wider to fit L/R bars)
    const int vuW=46;
    vuInBounds  = {pad,         graphTop, vuW, graphH};
    vuOutBounds = {W-pad-vuW,   graphTop, vuW, graphH};
    graphBounds = {pad+vuW+4,   graphTop, W-2*pad-2*vuW-8, graphH};

    // Knobs (4) — narrower so they fit between the LRA badges
    const int kX=(W-(4*kW+3*kGap))/2;
    const int kY=H-ctrlH+8;
    knobArea={kX,kY,4*kW+3*kGap,kH};
    targetLufsSlider.setBounds(kX+0*(kW+kGap), kY,kW,kH);
    releaseSlider   .setBounds(kX+1*(kW+kGap), kY,kW,kH);
    windowSlider    .setBounds(kX+2*(kW+kGap), kY,kW,kH);
    ceilingSlider   .setBounds(kX+3*(kW+kGap), kY,kW,kH);

    // LRA badges — same diameter & vertical centre as the knobs, under each VU meter
    const int badgeD  = 64;
    const int cy      = kY + kH/2;              // knob centre (no text box now)
    const int cxIn    = vuInBounds.getCentreX();
    const int cxOut   = vuOutBounds.getCentreX();
    lraInBadge  = {cxIn -badgeD/2, cy-badgeD/2, badgeD, badgeD};
    lraOutBadge = {cxOut-badgeD/2, cy-badgeD/2, badgeD, badgeD};
}
