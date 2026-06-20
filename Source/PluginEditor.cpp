#include "PluginEditor.h"

static juce::Font pf(float sz, bool bold=false)
{
    return juce::Font(juce::FontOptions().withHeight(sz).withStyle(bold?"Bold":"Regular"));
}
static juce::Colour amber()  { return juce::Colour(0xFFE8A020); }
static juce::Colour dimCol() { return juce::Colour(0xFF888888); }
static juce::Colour txtCol() { return juce::Colour(0xFFCCCCCC); }
static juce::Colour colM()   { return juce::Colour(0xFF30C870); }
static juce::Colour colS()   { return juce::Colour(0xFF2090FF); }
static juce::Colour colI()   { return amber(); }
static juce::Colour colC()   { return juce::Colour(0xFF40D0C0); }  // Custom (teal)
static juce::Colour colG()   { return juce::Colour(0xFFC060E0); }  // applied gain (purple)

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
        g.drawText(juce::String::fromUTF8("(c) Milan Knor"),
                   0,174,getWidth(),14, juce::Justification::centred);
    }
};

// ---- "Help" popup content (scrollable usage guide) ----
struct HelpComponent : public juce::Component
{
    juce::TextEditor te;
    HelpComponent()
    {
        setSize(380, 460);
        addAndMakeVisible(te);
        te.setMultiLine(true);
        te.setReadOnly(true);
        te.setScrollbarsShown(true);
        te.setCaretVisible(false);
        te.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF161616));
        te.setColour(juce::TextEditor::textColourId,       juce::Colour(0xFFCCCCCC));
        te.setColour(juce::TextEditor::outlineColourId,    juce::Colours::transparentBlack);
        te.setFont(juce::Font(juce::FontOptions().withHeight(12.5f)));
        te.setText(juce::String::fromUTF8(
            "M3K NORMALIZATOR \xe2\x80\x93 napoveda\n"
            "================================\n\n"
            "K cemu slouzi: meri hlasitost (LUFS dle EBU R128 / ITU-R BS.1770)\n"
            "a v realnem case normalizuje signal na zvolenou cilovou hlasitost.\n\n"
            "OVLADACE (kulate knoby dole)\n"
            "- TARGET LUFS: cilova hlasitost, na kterou se signal dorovnava.\n"
            "- DOWN: jak rychle se UBIRA zisk pri skoku nahlas (srazeni naporu).\n"
            "- UP: jak pomalu/nenapadne se PRIDAVA zisk u ticheho zakladu.\n"
            "- WINDOW: delka mericiho okna pro rezimy Custom (10-10000 ms).\n"
            "- LIMITER: strop vystupnich spicek (dBTP, true-peak). Vystup ho neprekroci.\n"
            "- IN GAIN (male kolecko nad levym VU): vstupni trim \xe2\x80\x93 pro simulaci\n"
            "  zmen hlasitosti nahravky. Nic neresetuje.\n"
            "Dvojklik na kolecko = zadat hodnotu rucne. Ctrl+klik = vychozi hodnota.\n\n"
            "REZIMY (tlacitka)\n"
            "- Momentary (400 ms), Short (3 s), Integrated (od resetu), Custom (okno).\n"
            "- Radek 'C' = stejne, ale C-vazeni (IEC 61672) misto K-vazeni.\n"
            "- NORMALIZE: zapina/vypina automatickou normalizaci.\n"
            "- RESET I: vynuluje pouze Integrated mereni (a jeho cas).\n\n"
            "GRAF\n"
            "- Prepinac nahore 'LOUDNESS / FADER':\n"
            "  * LOUDNESS = vystupni hlasitost (M/S/I/C) vuci cili.\n"
            "  * FADER = jak by jel zisk v jednotlivych rezimech \xe2\x80\x93 aktivni\n"
            "    plnou carou, ostatni prerusovane (pro porovnani rezimu).\n"
            "- Klik na pismena M / S / I / C / FADER (horni pruh) skryje/zobrazi krivku.\n"
            "- Cislo 'INT' vlevo nahore = cas zapocitany do Integrated.\n\n"
            "INDIKATORY\n"
            "- VU metry IN/OUT (stereo, peak). LRA kolecka = rozsah dynamiky;\n"
            "  klik na kterekoli vynuluje obe.\n"
            "- Kontrolka 'CIL' nad pravym VU: zelena = vystup sedi na cil.\n\n"
            "PRESET\n"
            "- Tovarni predvolby dle norem (Spotify, EBU R128, ...) i ulozeni/nacteni vlastnich.\n\n"
            "(c) Milan Knor"), juce::dontSendNotification);
    }
    void resized() override { te.setBounds(getLocalBounds().reduced(1)); }
    void paint(juce::Graphics& g) override
    {
        g.setColour(amber()); g.drawRect(getLocalBounds(), 1);
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

    // Value drawn inside the knob — skipped for small knobs (trim), value shown beside it
    if (w >= 50)
    {
        int decimals = (s.getInterval() > 0.0 && s.getInterval() < 1.0) ? 1 : 0;
        juce::String val = juce::String(s.getValue(), decimals);
        g.setColour(juce::Colours::white);
        g.setFont(pf(std::min(17.f, (r-9)*0.7f), true));
        g.drawText(val, juce::Rectangle<float>(cx-r, cy-8, 2*r, 16),
                   juce::Justification::centred);
    }
}
void M3KNormalizatorEditor::AmberLAF::drawButtonBackground(
    juce::Graphics& g, juce::Button& btn, const juce::Colour&, bool, bool)
{
    auto b=btn.getLocalBounds().toFloat().reduced(.5f);
    bool on=btn.getToggleState();
    auto ac=btn.findColour(juce::TextButton::buttonOnColourId);
    bool hasAccent=!ac.isTransparent();
    juce::Colour acc=hasAccent?ac:amber();
    g.setColour(on?acc.withAlpha(.25f):juce::Colour(0xFF1A1A1A));
    g.fillRoundedRectangle(b,4);
    g.setColour(on?acc:(hasAccent?acc.withAlpha(.40f):juce::Colour(0xFF444444)));
    g.drawRoundedRectangle(b,4,1);
}
void M3KNormalizatorEditor::AmberLAF::drawButtonText(
    juce::Graphics& g, juce::TextButton& btn, bool, bool)
{
    bool on=btn.getToggleState();
    auto ac=btn.findColour(juce::TextButton::buttonOnColourId);
    bool hasAccent=!ac.isTransparent();
    g.setColour(on ? (hasAccent?ac:amber())
                   : (hasAccent?ac.withAlpha(.55f):txtCol()));
    g.setFont(pf(10,on));
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

    // Dark popup-menu colours to match the main window
    laf.setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xFF1A1A1A));
    laf.setColour(juce::PopupMenu::textColourId,                  txtCol());
    laf.setColour(juce::PopupMenu::headerTextColourId,            amber());
    laf.setColour(juce::PopupMenu::highlightedBackgroundColourId, amber().withAlpha(0.30f));
    laf.setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);

    std::fill(histM,histM+kHistSize,-70.f);
    std::fill(histS,histS+kHistSize,-70.f);
    std::fill(histI,histI+kHistSize,-70.f);
    std::fill(histC,histC+kHistSize,-70.f);
    std::fill(histG,histG+kHistSize,0.f);
    std::fill(histFM,histFM+kHistSize,0.f); std::fill(histFS,histFS+kHistSize,0.f);
    std::fill(histFI,histFI+kHistSize,0.f); std::fill(histFC,histFC+kHistSize,0.f);

    auto setupKnob=[&](juce::Slider& s){
        s.setLookAndFeel(&laf);
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0); // value drawn inside the knob
        canvas.addAndMakeVisible(s);
    };
    setupKnob(targetLufsSlider);
    setupKnob(releaseSlider);
    setupKnob(attackSlider);
    setupKnob(windowSlider);
    setupKnob(ceilingSlider);
    setupKnob(inputGainSlider);

    // Ctrl/Cmd+click resets each knob to its parameter default.
    auto setDefault=[&](ValueKnob& k, const juce::String& id){
        if(auto* p=processor.apvts.getParameter(id)){
            k.defaultValue = processor.apvts.getParameterRange(id)
                                .convertFrom0to1(p->getDefaultValue());
            k.hasDefault = true;
        }
    };
    setDefault(targetLufsSlider,"targetLufs"); setDefault(releaseSlider,"releaseMs");
    setDefault(attackSlider,"attackMs");       setDefault(windowSlider,"customMs");
    setDefault(ceilingSlider,"ceiling");       setDefault(inputGainSlider,"inputGain");

    normalizeButton.setLookAndFeel(&laf);
    normalizeButton.setColour(juce::ToggleButton::textColourId,txtCol());
    normalizeButton.setColour(juce::ToggleButton::tickColourId,amber());
    normalizeButton.setColour(juce::ToggleButton::tickDisabledColourId,dimCol());
    canvas.addAndMakeVisible(normalizeButton);

    targetAttach  = std::make_unique<SliderAttachment>(processor.apvts,"targetLufs",targetLufsSlider);
    releaseAttach = std::make_unique<SliderAttachment>(processor.apvts,"releaseMs",  releaseSlider);
    attackAttach  = std::make_unique<SliderAttachment>(processor.apvts,"attackMs",   attackSlider);
    windowAttach  = std::make_unique<SliderAttachment>(processor.apvts,"customMs",   windowSlider);
    ceilingAttach = std::make_unique<SliderAttachment>(processor.apvts,"ceiling",    ceilingSlider);
    inputGainAttach = std::make_unique<SliderAttachment>(processor.apvts,"inputGain", inputGainSlider);
    normAttach    = std::make_unique<ButtonAttachment>(processor.apvts,"normalize",  normalizeButton);

    // ---- České tooltipy (nájezd myší) ----
    targetLufsSlider.setTooltip(juce::String::fromUTF8(
        "Cilova hlasitost (LUFS) – na jakou prumernou loudness se signal normalizuje."));
    releaseSlider.setTooltip(juce::String::fromUTF8(
        "Zesileni (UP) – jak rychle se POMALU pridava zisk, kdyz je zaklad potichu. "
        "Vyssi = nenapadnejsi, plynulejsi nabeh."));
    attackSlider.setTooltip(juce::String::fromUTF8(
        "Ztiseni (DOWN) – jak rychle se ubira zisk pri skoku NAHORU v hlasitosti. "
        "Nizsi = rychle srazi hlasity napor, vyssi = mekci."));
    windowSlider.setTooltip(juce::String::fromUTF8(
        "Window – delka mericiho okna pro rezimy Custom (10–10000 ms)."));
    ceilingSlider.setTooltip(juce::String::fromUTF8(
        "Limiter – strop vystupnich spicek (dBFS, true-peak). Vystup nikdy neprekroci."));
    inputGainSlider.setTooltip(juce::String::fromUTF8(
        "Vstupni gain (trim) – zesili/zeslabi vstupni signal pred merenim i zpracovanim. "
        "Slouzi k simulaci zmen hlasitosti nahravky. "
        "Dvojklik = zadat hodnotu, Ctrl+klik = vychozi (0 dB)."));
    normalizeButton.setTooltip(juce::String::fromUTF8(
        "Zapnuti / vypnuti automaticke normalizace na cilovou hlasitost."));
    resetButton.setTooltip(juce::String::fromUTF8(
        "Vynuluje pouze Integrated mereni (zacne pocitat znovu od nuly)."));
    presetButton.setTooltip(juce::String::fromUTF8(
        "Tovarni predvolby podle norem (Spotify, EBU R128…) + ulozeni/nacteni vlastnich."));
    static const char* mtt[8]={
        "Momentary – K-vazeni, okno 400 ms (rychla loudness).",
        "Short – K-vazeni, okno 3 s.",
        "Integrated – K-vazeni, prumer od resetu (standard pro normalizaci).",
        "Custom – K-vazeni, vlastni okno (knob Window).",
        "Momentary C – C-vazeni (IEC 61672), okno 400 ms.",
        "Short C – C-vazeni, okno 3 s.",
        "Integrated C – C-vazeni, prumer od resetu.",
        "Custom C – C-vazeni, vlastni okno."};
    for(int i=0;i<8;++i) modeButtons[i].setTooltip(juce::String::fromUTF8(mtt[i]));

    static const char* mnames[]={"Momentary","Short","Integrated","Custom",
                                 "Momentary C","Short C","Integrated C","Custom C"};
    const juce::Colour modeCols[4]={colM(),colS(),colI(),colC()}; // match the graph curves
    for(int i=0;i<8;++i){
        modeButtons[i].setButtonText(mnames[i]);
        modeButtons[i].setColour(juce::TextButton::buttonOnColourId, modeCols[i%4]);
        modeButtons[i].setLookAndFeel(&laf);
        modeButtons[i].setClickingTogglesState(false);
        modeButtons[i].onClick=[this,i](){
            if(auto* pa=dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter("mode")))
                pa->setValueNotifyingHost(pa->convertTo0to1((float)i));
        };
        canvas.addAndMakeVisible(modeButtons[i]);
    }

    // Amber accent so they stay clearly visible (LookAndFeel_V4 has a default
    // buttonOnColourId, so we must set ours explicitly).
    resetButton.setColour(juce::TextButton::buttonOnColourId, amber());
    resetButton.setLookAndFeel(&laf);
    resetButton.onClick=[this](){ processor.resetIntegrated(); };
    canvas.addAndMakeVisible(resetButton);

    // Preset menu (factory standards + save/load)
    presetButton.setColour(juce::TextButton::buttonOnColourId, amber());
    presetButton.setLookAndFeel(&laf);
    presetButton.onClick=[this](){ showPresetMenu(); };
    canvas.addAndMakeVisible(presetButton);

    // Help button ("?") — top-left, opens the usage guide
    helpButton.setColour(juce::TextButton::buttonOnColourId, amber());
    helpButton.setLookAndFeel(&laf);
    helpButton.setTooltip(juce::String::fromUTF8("N\xc3\xa1pov\xc4\x9b" "da \xe2\x80\x93 jak plugin pouzivat."));
    helpButton.onClick=[this](){
        auto c=std::make_unique<HelpComponent>();
        juce::CallOutBox::launchAsynchronously(std::move(c), helpButton.getBounds(), &canvas);
    };
    canvas.addAndMakeVisible(helpButton);

    startTimerHz(30);
}

M3KNormalizatorEditor::~M3KNormalizatorEditor()
{
    stopTimer();
    targetLufsSlider.setLookAndFeel(nullptr);
    releaseSlider   .setLookAndFeel(nullptr);
    attackSlider    .setLookAndFeel(nullptr);
    windowSlider    .setLookAndFeel(nullptr);
    ceilingSlider   .setLookAndFeel(nullptr);
    inputGainSlider .setLookAndFeel(nullptr);
    normalizeButton .setLookAndFeel(nullptr);
    resetButton     .setLookAndFeel(nullptr);
    presetButton    .setLookAndFeel(nullptr);
    helpButton      .setLookAndFeel(nullptr);
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

void M3KNormalizatorEditor::applyFactoryPreset(float lufs, float ceiling, float speed, float down)
{
    setParam("targetLufs", lufs);
    setParam("ceiling",    ceiling);
    setParam("releaseMs",  speed);  // UP – plynulý náběh
    setParam("attackMs",   down);   // DOWN – sražení hlasitého náporu
    setParam("mode",       2.0f);   // Integrated (standardní pro normy)
    setParam("normalize",  1.0f);   // zapnout
}

void M3KNormalizatorEditor::showPresetMenu()
{
    // speed = UP (releaseMs, pomalý náběh), down = DOWN (attackMs, sražení náporu)
    struct FP { const char* name; float lufs; float ceil; float speed; float down; };
    static const FP stream[]={{"Spotify  (-14)",-14,-1,1000,150},{"Spotify Loud  (-11)",-11,-1,1000,150},
        {"Apple Music  (-16)",-16,-1,1000,150},{"YouTube  (-14)",-14,-1,1000,150},{"Amazon Music  (-14)",-14,-1,1000,150},
        {"Tidal  (-14)",-14,-1,1000,150},{"Deezer  (-15)",-15,-1,1000,150}};
    static const FP broad[]={{"EBU R128  (-23)",-23,-1,1500,250},{"ATSC A/85  (-24)",-24,-2,1500,250},
        {"TR-B32  (-24)",-24,-1,1500,250}};
    static const FP pod[]={{"Podcast  (-16)",-16,-1,1200,120},{"Mluvene slovo  (-19)",-19,-1,1200,120}};
    static const FP club[]={{"Klub / DJ max  (-8)",-8,-1,500,50},{"Na doraz  (-6)",-6,-0.3f,300,30}};

    juce::PopupMenu menu, mS, mB, mP, mC;
    std::vector<FP> all;
    auto fill=[&](juce::PopupMenu& m, const FP* arr, int n){
        for(int i=0;i<n;++i){ all.push_back(arr[i]); m.addItem((int)all.size(), arr[i].name); }
    };
    fill(mS, stream, 7); fill(mB, broad, 3); fill(mP, pod, 2); fill(mC, club, 2);
    for(auto* mm : { &menu,&mS,&mB,&mP,&mC }) mm->setLookAndFeel(&laf);
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
                applyFactoryPreset(all[r-1].lufs, all[r-1].ceil, all[r-1].speed, all[r-1].down);
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

juce::String M3KNormalizatorEditor::tooltipForPoint(juce::Point<int> p)
{
    auto U=[](const char* s){ return juce::String::fromUTF8(s); };
    // Value strip cells (M, S, I, GAIN, LIM) — click M/S/I/GAIN to toggle its graph curve
    if(p.y>=46 && p.y<46+34 && p.x>=14)
    {
        int sw=(kDesignW-28)/6, i=(p.x-14)/sw;
        switch(i){
            case 0: return U("Momentary loudness (400 ms). Klik = skryt/zobrazit krivku v grafu.");
            case 1: return U("Short-term loudness (3 s). Klik = skryt/zobrazit krivku v grafu.");
            case 2: return U("Integrated loudness (od resetu). Klik = skryt/zobrazit krivku.");
            case 3: return U("Custom loudness (okno = knob WINDOW). Klik = skryt/zobrazit krivku.");
            case 4: return U("FADER – pohyb hlasitosti (kolik dB normalizace prave pridava/ubira). Klik = krivka v grafu.");
            case 5: return U("LIM – kolik dB prave ubira limiter (gain reduction).");
        }
    }
    if(vuInBounds .contains(p)) return U("Vstupni VU metr (peak, L/R) pred zpracovanim.");
    if(vuOutBounds.contains(p)) return U("Vystupni VU metr (peak, L/R) po normalizaci a limiteru.");
    if(lraInBadge .contains(p)) return U("LRA vstupu (rozsah dynamiky, EBU 3342). Klik = vynulovat obe LRA.");
    if(lraOutBadge.contains(p)) return U("LRA vystupu (rozsah dynamiky). Klik = vynulovat obe LRA.");
    juce::Rectangle<int> led(vuOutBounds.getX()-6, vuOutBounds.getY()-42,
                             vuOutBounds.getWidth()+12, 42);
    if(led.contains(p)) return U("Kontrolka: jestli vystupni Integrated sedi na cil (zelena = v cili).");
    if(viewToggle.contains(p)) return U("Prepinac grafu: LOUDNESS (vystupni hlasitost) / FADER (pohyb zisku po rezimech – aktivni plne, ostatni prerusovane).");
    if(graphBounds.contains(p)) return U("Prubeh za 60 s. Klik na pismena M/S/I/C/FADER = skryt/zobrazit krivku.");
    if(logoBounds .contains(p)) return U("O aplikaci (logo, verze).");
    return {};
}

void M3KNormalizatorEditor::canvasClicked(juce::Point<int> p)
{
    // Graph view switch (Loudness / Fader)
    if(viewToggle.contains(p)){ graphView ^= 1; canvas.repaint(); return; }

    // Value strip: M/S/I/C/GAIN cells toggle their graph curve
    if(stripCell[0].contains(p)){ showM=!showM; canvas.repaint(); return; }
    if(stripCell[1].contains(p)){ showS=!showS; canvas.repaint(); return; }
    if(stripCell[2].contains(p)){ showI=!showI; canvas.repaint(); return; }
    if(stripCell[3].contains(p)){ showC=!showC; canvas.repaint(); return; }
    if(stripCell[4].contains(p)){ showG=!showG; canvas.repaint(); return; }

    if(lraInBadge.contains(p) || lraOutBadge.contains(p))
    {
        processor.resetLraInput();
        processor.resetLraOutput();
        return;
    }
    if(logoBounds.contains(p))             // logo → About box
    {
        auto c=std::make_unique<AboutComponent>();
        juce::CallOutBox::launchAsynchronously(std::move(c), logoBounds, &canvas);
    }
}

void M3KNormalizatorEditor::timerCallback()
{
    const float a=.25f;
    auto sm=[&](float& d,float t){ d+=a*(t-d); };
    sm(dispM,  processor.momentaryLufs .load());
    sm(dispS,  processor.shortTermLufs .load());
    sm(dispI,  processor.integratedLufs.load());
    sm(dispC,  processor.customLufs    .load());
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
    sm(dispGr, processor.limGrDb.load());          // smoothed gain-reduction
    dispOutInt = processor.outIntegratedLufs.load();

    // Graph stores OUTPUT LUFS = input LUFS + normGain
    float outM = juce::jlimit(-70.f,  6.f, dispM + dispNG);
    float outS = juce::jlimit(-70.f,  6.f, dispS + dispNG);
    float outI = juce::jlimit(-70.f,  6.f, dispI + dispNG);
    float outC = juce::jlimit(-70.f,  6.f, dispC + dispNG);
    histM[histPos]=outM; histS[histPos]=outS; histI[histPos]=outI; histC[histPos]=outC;
    histG[histPos]=dispNG;                       // applied gain (dB) — dynamic curve
    // Per-mode fader preview (already gated/smoothed in the processor)
    histFM[histPos]=processor.faderDb[0].load(); histFS[histPos]=processor.faderDb[1].load();
    histFI[histPos]=processor.faderDb[2].load(); histFC[histPos]=processor.faderDb[3].load();
    histPos=(histPos+1)%kHistSize;
    ++histTotal;
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
    g.setFont(pf(8.5f,true)); g.setColour(txtCol());
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
            juce::ColourGradient grad(
                juce::Colour(0xFFFF2020), (float)bar.getX(), (float)bar.getY(),
                juce::Colour(0xFF30C870), (float)bar.getX(), (float)bar.getBottom(), false);
            grad.addColour(0.10, juce::Colour(0xFFFF2020));
            grad.addColour(0.20, juce::Colour(0xFFFFCC00));
            grad.addColour(0.42, juce::Colour(0xFF50DD50));
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

    // Scale ticks + numeric labels overlaid on bars
    g.setFont(pf(9.f, true));
    for(float tick : {0.f,-6.f,-12.f,-24.f,-48.f})
    {
        float n=(tick-minDb)/(maxDb-minDb);
        int ty=bars.getBottom()-(int)(n*bars.getHeight());
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.drawHorizontalLine(ty,(float)bars.getX(),(float)bars.getRight());
        juce::String lbl = (tick == 0.f) ? "0" : juce::String((int)tick);
        g.setColour(juce::Colour(0xFFAAAAAA));
        g.drawText(lbl, bars.getX(), ty-10, bars.getWidth(), 10,
                   juce::Justification::centred);
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
    float gx=(float)area.getX(), gy=(float)area.getY(), gw=(float)area.getWidth(), gh=(float)area.getHeight();

    const bool fader = (graphView == 1);
    const float gMin=-24.f, gMax=24.f;
    auto yLufs=[&](float db){ float n=(juce::jlimit(minDb,maxDb,db)-minDb)/(maxDb-minDb);
                              return gy+gh-n*gh; };
    auto yGain=[&](float db){ float n=(juce::jlimit(gMin,gMax,db)-gMin)/(gMax-gMin);
                              return gy+gh-n*gh; };
    float targetLufs=*processor.apvts.getRawParameterValue("targetLufs");

    if(!fader)
    {
        // Target LUFS line
        float ty=yLufs(targetLufs);
        g.setColour(amber().withAlpha(.5f));
        for(float dx=gx; dx<gx+gw; dx+=10)
            g.drawHorizontalLine((int)ty,dx,std::min(dx+6,gx+gw));
        g.setFont(pf(8,true)); g.setColour(amber().withAlpha(.8f));
        g.drawText(juce::String(targetLufs,1)+" LUFS", (int)(gx+gw-56),(int)ty-10,55,10,
                   juce::Justification::centredRight);
        // LUFS grid + scale
        const float gridDbs[]={6,0,-6,-12,-18,-23,-30,-40,-50,-60};
        for(float db : gridDbs)
        {
            float y=yLufs(db);
            bool ref=(db==-14||db==-23||db==0);
            g.setColour(ref?juce::Colour(0xFF303020):juce::Colour(0xFF1E1E1E));
            g.drawHorizontalLine((int)y,gx,gx+gw);
            g.setFont(pf(8)); g.setColour(ref?amber().withAlpha(.6f):dimCol());
            g.drawText(juce::String((int)db), b.getX(),(int)y-5,scW-3,10,juce::Justification::centredRight);
        }
    }
    else
    {
        // Fader view: gain dB grid (+-24), zero line emphasised
        for(float db : {24.f,12.f,0.f,-12.f,-24.f})
        {
            float y=yGain(db); bool zero=(db==0.f);
            g.setColour(zero?juce::Colour(0xFF353028):juce::Colour(0xFF1E1E1E));
            g.drawHorizontalLine((int)y,gx,gx+gw);
            g.setFont(pf(8)); g.setColour(zero?colG().withAlpha(.7f):dimCol());
            g.drawText((db>0?"+":"")+juce::String((int)db), b.getX(),(int)y-5,scW-3,10,
                       juce::Justification::centredRight);
        }
        g.setFont(pf(7.f)); g.setColour(dimCol());
        g.drawText("dB", b.getX()+2,(int)gy+1, scW-4,9, juce::Justification::centredLeft);
    }

    // Time labels (bottom) — both views
    g.setFont(pf(8)); g.setColour(dimCol());
    g.drawText("now",  (int)gx,         b.getBottom()-13, 30, 12, juce::Justification::centredLeft);
    g.drawText("-60s", (int)(gx+gw)-30, b.getBottom()-13, 30, 12, juce::Justification::centredRight);

    // Integrated elapsed time (top-left)
    {
        long long samps = processor.integratedSamples.load();
        double sr = processor.getSampleRate();
        long long totalSec = (samps > 0 && sr > 0.0) ? (long long)(samps / sr) : 0LL;
        juce::String elapsed = juce::String::formatted("%d:%02d", (int)(totalSec/60), (int)(totalSec%60));
        g.setFont(pf(9.f, true)); g.setColour(amber().withAlpha(0.7f));
        g.drawText(elapsed, (int)gx+2, b.getY()+4, 46, 11, juce::Justification::centredLeft);
        g.setFont(pf(7.5f)); g.setColour(dimCol());
        g.drawText("INT", (int)gx+2, b.getY()+15, 46, 9, juce::Justification::centredLeft);
    }

    // View toggle (top-centre) — switch Loudness / Fader
    {
        int tw=150, tx=(int)(gx+gw*0.5f)-tw/2, ty=b.getY()+4;
        viewToggle = { tx, ty, tw, 13 };
        g.setFont(pf(8.5f,true));
        g.setColour(graphView==0?amber():dimCol());
        g.drawText("LOUDNESS", tx, ty, tw/2-6, 13, juce::Justification::centredRight);
        g.setColour(dimCol()); g.drawText("|", tx+tw/2-4, ty, 8, 13, juce::Justification::centred);
        g.setColour(graphView==1?amber():dimCol());
        g.drawText("FADER", tx+tw/2+4, ty, tw/2-6, 13, juce::Justification::centredLeft);
    }

    g.saveState();
    g.reduceClipRegion(area);

    int pts=std::min(histFilled,(int)gw);
    if(pts>=2)
    {
        auto drawCurve=[&](std::function<float(int)> valAt, juce::Colour col,
                           bool isGain, float thick, bool dashed)
        {
            g.setColour(col);
            if(!dashed){
                juce::Path path; bool started=false;
                for(int px=0; px<pts; ++px){
                    int idx=(histPos-1-px+kHistSize)%kHistSize;
                    float x=gx+gw-1.f-(float)px;
                    float y=isGain? yGain(valAt(idx)) : yLufs(valAt(idx));
                    if(!started){path.startNewSubPath(x,y);started=true;} else path.lineTo(x,y);
                }
                g.strokePath(path,juce::PathStrokeType(thick));
            } else {
                // dashes anchored to the DATA (absolute index), so gaps scroll with the
                // curve instead of staying fixed on screen.
                const long long dlen=6, period=2*dlen;
                float px0=0,py0=0; bool have=false;
                for(int px=0; px<pts; ++px){
                    int idx=(histPos-1-px+kHistSize)%kHistSize;
                    float x=gx+gw-1.f-(float)px;
                    float y=isGain? yGain(valAt(idx)) : yLufs(valAt(idx));
                    long long a=histTotal-1-(long long)px;
                    bool on=(((a%period)+period)%period) < dlen;
                    if(have && on) g.drawLine(px0,py0,x,y,thick);
                    px0=x; py0=y; have=true;
                }
            }
        };

        if(!fader)
        {
            // Loudness view: output LUFS curves + applied-gain (own scale)
            if(showG) drawCurve([this](int i){return histG[i];}, colG(), true, 1.2f, false);
            if(showC) drawCurve([this](int i){return histC[i];}, colC(), false, 1.5f, false);
            if(showI) drawCurve([this](int i){return histI[i];}, colI(), false, 1.5f, false);
            if(showS) drawCurve([this](int i){return histS[i];}, colS(), false, 1.5f, false);
            if(showM) drawCurve([this](int i){return histM[i];}, colM(), false, 1.5f, false);
        }
        else
        {
            // Fader view: per-mode DESIRED gain = target - inputLoudness(mode)
            //   inputLoudness = output(histX) - appliedGain(histG)
            // Active mode solid, others dashed.
            int base=((int)*processor.apvts.getRawParameterValue("mode"))%4; // 0=M,1=S,2=I,3=C
            const float* fArr[4]={histFM,histFS,histFI,histFC};
            juce::Colour cols[4]={colM(),colS(),colI(),colC()};
            bool shows[4]={showM,showS,showI,showC};
            for(int m=0;m<4;++m){
                if(!shows[m]) continue;
                const float* fh=fArr[m];
                bool act=(m==base);
                drawCurve([fh](int i){ return fh[i]; }, cols[m], true, act?1.9f:1.2f, !act);
            }
        }
    }
    g.restoreState();

    // GAIN scale (right edge) — loudness view only, when gain curve visible
    if(!fader && showG)
    {
        g.setFont(pf(7.f));
        for(float gdb : {24.f,12.f,0.f,-12.f,-24.f}){
            float y=yGain(gdb);
            g.setColour(colG().withAlpha(gdb==0.f?0.5f:0.32f));
            g.drawText((gdb>0?"+":"")+juce::String((int)gdb), (int)(gx+gw)-26,(int)y-5,24,10,
                       juce::Justification::centredRight);
        }
    }

    // Legend
    if(!fader){
        struct{juce::Colour c;const char* l;bool on;}leg[]={
            {colM(),"M",showM},{colS(),"S",showS},{colI(),"I",showI},
            {colC(),"C",showC},{colG(),"F",showG}};
        int lx=(int)(gx+gw)-120, ly=b.getY()+6;
        for(auto& l:leg){
            g.setColour(l.on ? l.c : dimCol().withAlpha(0.5f)); g.fillRect(lx,ly+4,12,2);
            g.setFont(pf(9,true)); g.setColour(l.on ? l.c : dimCol().withAlpha(0.6f));
            g.drawText(l.l,lx+15,ly,18,10,juce::Justification::centredLeft);
            lx+=24;
        }
    } else {
        // Fader legend: active = solid swatch, others = dashed swatch
        int base=((int)*processor.apvts.getRawParameterValue("mode"))%4;
        struct{juce::Colour c;const char* l;bool on;}leg[]={
            {colM(),"M",showM},{colS(),"S",showS},{colI(),"I",showI},{colC(),"C",showC}};
        int lx=(int)(gx+gw)-96, ly=b.getY()+6;
        for(int m=0;m<4;++m){
            bool act=(m==base);
            g.setColour(leg[m].on ? leg[m].c : dimCol().withAlpha(0.5f));
            if(act) g.fillRect(lx,ly+4,12,2);
            else { for(float dx=0;dx<12;dx+=4) g.fillRect((int)(lx+dx),ly+4,2,2); }
            g.setFont(pf(9,act)); g.setColour(leg[m].on ? leg[m].c : dimCol().withAlpha(0.6f));
            g.drawText(leg[m].l,lx+15,ly,16,10,juce::Justification::centredLeft);
            lx+=22;
        }
    }
}

// ---- paint (drawn into the design-size canvas) ----
void M3KNormalizatorEditor::paintCanvas(juce::Graphics& g)
{
    const int W=kDesignW, pad=14;

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

    // Current value strip (6 cells: M, S, I, C, GAIN, LIM). M/S/I/C/GAIN toggle their
    // graph curve when clicked; a coloured underline shows the curve colour & on/off state.
    const int sY=46, sH=34, sw=(W-28)/6;
    struct{float v;const char* l;juce::Colour c;bool gain;bool gr;bool curve;bool on;} strips[]={
        {dispM,"M",colM(),false,false,true,showM},
        {dispS,"S",colS(),false,false,true,showS},
        {dispI,"I",colI(),false,false,true,showI},
        {dispC,"C",colC(),false,false,true,showC},
        {dispNG,"FADER",colG(),true,false,true,showG},   // barva sjednocena s gain krivkou
        {dispGr,"LIM",dispGr<-0.3f?juce::Colour(0xFFFF6040):dimCol(),false,true,false,true}
    };
    for(int i=0;i<6;++i){
        int sx=14+i*sw;
        stripCell[i] = { sx, sY, sw-4, sH };
        const bool dimmed = strips[i].curve && !strips[i].on;
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillRoundedRectangle((float)sx,(float)sY,(float)(sw-4),(float)sH,3);
        // letter — in curve colour when the curve is on, dim when off
        g.setFont(pf(9,true));
        g.setColour(strips[i].curve ? (strips[i].on ? strips[i].c : dimCol()) : txtCol());
        g.drawText(strips[i].l, sx+6,sY+2,sw-12,12,juce::Justification::centredLeft);
        // value
        g.setFont(pf(14,true));
        g.setColour(dimmed ? dimCol() : strips[i].c);
        juce::String val;
        if(strips[i].gr)        val = strips[i].v>-0.1f ? "0.0" : juce::String(strips[i].v,1);
        else if(strips[i].v<=-69.f) val = "-inf";
        else                    val = (strips[i].gain&&strips[i].v>0?"+":"")+juce::String(strips[i].v,1);
        g.drawText(val,sx,sY+9,sw-4,sH-12,juce::Justification::centred);
        // curve on/off underline (only for the four curve cells)
        if(strips[i].curve){
            float uw=(float)(sw-16), ux=(float)(sx+6), uy=(float)(sY+sH-5);
            g.setColour(strips[i].on ? strips[i].c : juce::Colour(0xFF333333));
            if(strips[i].on) g.fillRoundedRectangle(ux,uy,uw,2.5f,1.f);
            else             g.drawRoundedRectangle(ux,uy,uw,2.5f,1.f,0.8f);
        }
    }

    // Compliance light (LED) above the OUT VU meter — output loudness vs target
    {
        float tgt=*processor.apvts.getRawParameterValue("targetLufs");
        int mode=(int)*processor.apvts.getRawParameterValue("mode");
        // Use the same reference as the active mode for fast response
        float oi;
        switch(mode){
            case 0: case 4: oi = dispM + dispNG; break;  // Momentary
            case 1: case 5: oi = dispS + dispNG; break;  // Short
            case 3: case 7: { float cv = processor.customLufs.load();
                              oi = cv > -69.f ? cv + dispNG : -70.f; break; } // Custom
            default:        oi = dispOutInt;     break;  // Integrated
        }
        juce::Colour col; juce::String sub;
        if(oi<=-69.f){ col=juce::Colour(0xFF444444); sub="--"; }
        else {
            float dev=oi-tgt;
            if(std::abs(dev)<0.5f){ col=juce::Colour(0xFF30C870); sub="V CILI"; }
            else if(std::abs(dev)<1.5f){ col=amber();             sub=(dev>0?"+":"")+juce::String(dev,1); }
            else { col=juce::Colour(0xFFFF5030); sub=(dev>0?"+":"")+juce::String(dev,1); }
        }
        float cx=(float)vuOutBounds.getCentreX();
        float cy=(float)vuOutBounds.getY()-26.f;
        g.setColour(dimCol()); g.setFont(pf(7.5f,true));
        g.drawText("CIL", (int)cx-24, (int)cy-16, 48, 9, juce::Justification::centred);
        g.setColour(col.withAlpha(0.25f)); g.fillEllipse(cx-9,cy-9,18,18);   // glow
        g.setColour(col);                  g.fillEllipse(cx-5.5f,cy-5.5f,11,11); // LED
        g.setColour(juce::Colours::white.withAlpha(0.5f)); g.drawEllipse(cx-5.5f,cy-5.5f,11,11,1.f);
        g.setColour(col); g.setFont(pf(8.f,true));
        g.drawText(juce::String::fromUTF8(sub.toRawUTF8()), (int)cx-26, (int)cy+8, 52, 10,
                   juce::Justification::centred);
    }

    // Input-gain trim: label above + value below the small knob (knob draws itself)
    {
        float ig = *processor.apvts.getRawParameterValue("inputGain");
        g.setColour(dimCol()); g.setFont(pf(7.5f,true));
        g.drawText("IN GAIN", inGainBounds.getX()-8, inGainBounds.getY()-10,
                   inGainBounds.getWidth()+16, 9, juce::Justification::centred);
        juce::String v = (ig>0?"+":"")+juce::String(ig,1)+" dB";
        g.setColour(std::abs(ig)<0.05f ? dimCol()
                    : (ig>0 ? colM() : juce::Colour(0xFFFF6040)));
        g.setFont(pf(8.f,true));
        g.drawText(v, inGainBounds.getX()-8, inGainBounds.getBottom()+1,
                   inGainBounds.getWidth()+16, 10, juce::Justification::centred);
    }

    // VU meters (stereo)
    drawVuMeter(g, vuInBounds,  dispVuInL,  dispVuInR,  "IN");
    drawVuMeter(g, vuOutBounds, dispVuOutL, dispVuOutR, "OUT");

    // LRA badges (round, matching the knobs) — under the VU meters
    auto drawLra=[&](juce::Rectangle<int> box, float lra, const char* lbl)
    {
        float cx=(float)box.getCentreX(), cy=(float)box.getCentreY();
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

    // Knob labels (5 knobs) — same even-spacing formula as layoutCanvas (circles 1..5)
    const int kW=72;
    const float step7 = (float)(W - 2*pad - kW) / 6.0f;
    auto knobCx = [&](int i){ return pad + kW/2 + juce::roundToInt((i+1) * step7); };
    const int labelY=knobArea.getY()-23;
    const struct{const char* top,*unit;} kl[]=
        {{"TARGET LUFS","LUFS"},{"DOWN","ms"},{"UP","ms"},{"WINDOW","ms"},{"LIMITER","dBFS"}};
    for(int i=0;i<5;++i){
        int cx=knobCx(i);
        g.setFont(pf(9.5f,true)); g.setColour(txtCol());
        g.drawText(kl[i].top, cx-46,labelY,92,13,juce::Justification::centred);
        g.setFont(pf(9.f)); g.setColour(juce::Colour(0xFFA8A8A8));
        g.drawText(kl[i].unit,cx-46,knobArea.getBottom()+5,92,12,juce::Justification::centred);
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

    // Help "?" — far top-right edge, next to NORMALIZE
    helpButton.setBounds(W-32, 12, 22, 20);
    // Normalize toggle + Reset — top-right of header (above the orange line)
    normalizeButton.setBounds(W-172, 11, 136, 22);
    resetButton.setBounds(W-172-58, 12, 52, 20);
    // PRESET menu — header, between title and Reset
    presetButton.setBounds(214, 12, 92, 20);

    // Mode buttons — two rows of 4 (row1 = A-weighted, row2 = C-weighted)
    const int mBW=96, mBH=22, mBGapX=6, mBGapY=4;
    const int mTW=4*mBW+3*mBGapX, mStartX=(W-mTW)/2;
    const int modeY=46+34+8;   // below the (enlarged) value strip
    for(int i=0;i<8;++i){
        int row=i/4, col=i%4;
        modeButtons[i].setBounds(mStartX+col*(mBW+mBGapX),
                                 modeY+row*(mBH+mBGapY), mBW, mBH);
    }

    // Layout areas
    const int kH=72, kW=72;
    const int ctrlH=kH+48;
    const int graphTop=modeY+2*mBH+mBGapY+8;
    const int graphH=H-graphTop-ctrlH-34;

    // VU meters (stereo — wider to fit L/R bars)
    const int vuW=46;
    vuInBounds  = {pad,         graphTop, vuW, graphH};
    vuOutBounds = {W-pad-vuW,   graphTop, vuW, graphH};
    graphBounds = {pad+vuW+4,   graphTop, W-2*pad-2*vuW-8, graphH};

    // Input-gain trim knob — small, centered above the IN VU meter (mirrors CIL LED on the right)
    const int igD = 40;
    const int igCx = vuInBounds.getCentreX();
    const int igCy = (76 + graphTop) / 2;       // band between value strip (76) and VU top
    inGainBounds = { igCx - igD/2, igCy - igD/2, igD, igD };
    inputGainSlider.setBounds(inGainBounds);

    // 7 circles evenly spaced: LRA-IN | TARGET | DOWN | UP | WINDOW | LIMITER | LRA-OUT
    const int kY=H-ctrlH+8;
    const int cy = kY + kH/2;
    const int badgeD = 64;
    const float step = (float)(W - 2*pad - kW) / 6.0f;
    auto cx7 = [&](int i){ return pad + kW/2 + juce::roundToInt(i * step); };

    lraInBadge       = {cx7(0)-badgeD/2, cy-badgeD/2, badgeD, badgeD};
    targetLufsSlider.setBounds(cx7(1)-kW/2, kY, kW, kH);
    attackSlider    .setBounds(cx7(2)-kW/2, kY, kW, kH);
    releaseSlider   .setBounds(cx7(3)-kW/2, kY, kW, kH);
    windowSlider    .setBounds(cx7(4)-kW/2, kY, kW, kH);
    ceilingSlider   .setBounds(cx7(5)-kW/2, kY, kW, kH);
    lraOutBadge      = {cx7(6)-badgeD/2, cy-badgeD/2, badgeD, badgeD};

    knobArea = {cx7(1)-kW/2, kY, cx7(5)+kW/2 - (cx7(1)-kW/2), kH};
}
