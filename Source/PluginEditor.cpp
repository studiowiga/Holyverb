#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    // Positions dans le repère de référence 760 x 958 (même cadrage qu'Echoflex),
    // relevées sur le visuel Holyverb mis à l'échelle du fond (1520 x 1916 = 2 x 760 x 958)
    const Point<float> knobDecay { 196.78f, 175.53f };
    const Point<float> knobTone  { 381.14f, 176.29f };
    const Point<float> knobLevel { 563.22f, 176.29f };
    const float knobR      = 61.45f;      // rayon extérieur du moletage
    const float knobHalf   = 75.0f;       // demi-côté d'une image du filmstrip (ombre comprise)

    const Point<float> knobMode  { 380.76f, 404.65f };
    const float modeR      = 68.28f;
    const float modeHalf   = 83.25f;

    const Point<float> swPre     { 175.30f, 405.40f };
    const Point<float> swTails   { 589.24f, 406.70f };
    const float swHalf     = 57.25f;

    const Point<float> ledPos    { 380.83f, 553.35f };
    const float ledLensR   = 11.1f;     // lentille bleue (comme le visuel)
    const float ledGapR    = 12.9f;     // gorge sombre
    const float ledRingR   = 14.4f;     // fine bague chromée

    const Point<float> footPos   { 380.0f, 811.7f };
    const float footR      = 58.9f;

    // Ligne d'état : même police et même taille que « MODE HALL » du visuel
    const float statusCenterX  = 380.8f;
    const float statusBaseline = 708.5f;
    const float statusCap      = 12.14f;   // hauteur des capitales
    const Rectangle<float> statusRect { 140.0f, 690.0f, 482.0f, 26.0f };

    const Point<float> logoCentre { 380.4f, 899.3f };
    const float logoW = 118.0f;         // logo Studio Wiga horizontal (blanc)

    const Colour statusInk (0xfff2f2f4);
    const Colour bluCore (0xffe2f4ff);
    const Colour bluMid  (0xff2f8dff);
    const Colour bluDeep (0xff0b2f96);

    constexpr int   knurlTeeth = 76;
}

//==============================================================================
// POTARD
//==============================================================================
void StripKnobLookAndFeel::prepareScaled (int px)
{
    if (! strip.isValid() || px <= 0)
        return;
    if (px >= strip.getWidth())
    {
        scaledStrip = {};
        scaledSize = 0;
        return;
    }
    if (px == scaledSize && scaledStrip.isValid())
        return;
    scaledStrip = strip.rescaled (px, px * frames, Graphics::highResamplingQuality);
    scaledSize = px;
}

void StripKnobLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                             float pos, float startAngle, float endAngle, Slider&)
{
    const auto& img = scaledStrip.isValid() ? scaledStrip : strip;
    if (! img.isValid())
        return;

    const float angle = startAngle + pos * (endAngle - startAngle);    // radians, 0 = midi
    const int fs = img.getWidth();
    int frame;
    if (drawPointer)
    {
        // le moletage est périodique (76 dents) : on choisit sa phase
        const float period = MathConstants<float>::twoPi / (float) knurlTeeth;
        float ph = std::fmod (angle, period);
        if (ph < 0.0f) ph += period;
        frame = jlimit (0, frames - 1, (int) (ph / period * (float) frames));
    }
    else
    {
        frame = jlimit (0, frames - 1, roundToInt (pos * (float) (frames - 1)));
    }

    g.setImageResamplingQuality (scaledStrip.isValid() ? Graphics::mediumResamplingQuality
                                                       : Graphics::highResamplingQuality);
    g.drawImage (img, x, y, w, h, 0, frame * fs, fs, fs);

    if (! drawPointer)
        return;

    // Index blanc + point : tournent avec le potard
    const auto c  = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float R = (float) w * 0.5f * knobR;
    const float ux = std::sin (angle), uy = -std::cos (angle);

    Path line;
    line.startNewSubPath (c.x + ux * R * 0.10f, c.y + uy * R * 0.10f);
    line.lineTo          (c.x + ux * R * 0.60f, c.y + uy * R * 0.60f);
    const PathStrokeType stroke (R * 0.056f, PathStrokeType::curved, PathStrokeType::rounded);

    g.setColour (Colours::black.withAlpha (0.45f));          // ombre fine (lumière haut-gauche)
    g.strokePath (line, stroke, AffineTransform::translation (R * 0.012f, R * 0.018f));
    g.setGradientFill (ColourGradient (Colour (0xfff7f7f9), c.x - R * 0.4f, c.y - R * 0.4f,
                                       Colour (0xffc9c9cd), c.x + R * 0.4f, c.y + R * 0.4f, false));
    g.strokePath (line, stroke);

    const Point<float> d (c.x + ux * R * 0.845f, c.y + uy * R * 0.845f);
    const float dr = R * 0.042f;
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (d.x - dr + R * 0.01f, d.y - dr + R * 0.015f, dr * 2.0f, dr * 2.0f);
    g.setGradientFill (ColourGradient (Colours::white, d.x - dr * 0.4f, d.y - dr * 0.5f,
                                       Colour (0xffbdbdc2), d.x + dr, d.y + dr, true));
    g.fillEllipse (d.x - dr, d.y - dr, dr * 2.0f, dr * 2.0f);
}

//==============================================================================
// MINI-SWITCH
//==============================================================================
void LeverSwitch::prepareScaled (int px)
{
    if (! strip.isValid() || px <= 0)
        return;
    if (px >= strip.getWidth()) { scaled = {}; scaledSize = 0; return; }
    if (px == scaledSize && scaled.isValid()) return;
    scaled = strip.rescaled (px, px * 2, Graphics::highResamplingQuality);
    scaledSize = px;
}

void LeverSwitch::paintButton (Graphics& g, bool over, bool)
{
    const auto& img = scaled.isValid() ? scaled : strip;
    if (! img.isValid())
        return;
    const int fs = img.getWidth();
    const int frame = getToggleState() ? 0 : 1;      // 0 = levier en haut
    g.setImageResamplingQuality (scaled.isValid() ? Graphics::mediumResamplingQuality : Graphics::highResamplingQuality);
    g.drawImage (img, 0, 0, getWidth(), getHeight(), 0, frame * fs, fs, fs);
    if (over)
    {
        g.setColour (Colours::white.withAlpha (0.04f));
        g.fillEllipse (getLocalBounds().toFloat().reduced ((float) getWidth() * 0.3f));
    }
}

//==============================================================================
// FOOTSWITCH
//==============================================================================
FootSwitchArea::FootSwitchArea (RangedAudioParameter& p)
    : attachment (p, [this] (float v) { value = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool FootSwitchArea::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) getWidth() * 0.5f;
}

void FootSwitchArea::paint (Graphics& g)
{
    if (pressed)
    {
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillEllipse (getLocalBounds().toFloat().reduced ((float) getWidth() * 0.23f));
    }
}

void FootSwitchArea::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    pressed = true;
    repaint();
    attachment.setValueAsCompleteGesture (value ? 0.0f : 1.0f);
}

void FootSwitchArea::mouseUp (const MouseEvent&)
{
    pressed = false;
    repaint();
}

//==============================================================================
// ÉDITEUR
//==============================================================================
HolyverbAudioProcessorEditor::HolyverbAudioProcessorEditor (HolyverbAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      footswitch (*p.apvts.getParameter (ParamID::on))
{
    background = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    logo       = ImageCache::getFromMemory (BinaryData::logo_studio_wiga_png, BinaryData::logo_studio_wiga_pngSize);

    knobLnf.strip  = ImageCache::getFromMemory (BinaryData::knob_strip_png, BinaryData::knob_strip_pngSize);
    knobLnf.frames = 8;
    knobLnf.drawPointer = true;
    knobLnf.knobR = knobR / knobHalf;

    modeLnf.strip  = ImageCache::getFromMemory (BinaryData::mode_strip_png, BinaryData::mode_strip_pngSize);
    modeLnf.frames = 8;
    modeLnf.drawPointer = false;
    modeLnf.knobR = modeR / modeHalf;

    const auto toggleStrip = ImageCache::getFromMemory (BinaryData::toggle_strip_png, BinaryData::toggle_strip_pngSize);
    preSwitch.strip = tailsSwitch.strip = toggleStrip;

    // Police de la ligne d'état (même dessin que le visuel)
    {
        auto tf = Typeface::createSystemTypefaceFor (BinaryData::HolyverbSans_ttf, BinaryData::HolyverbSans_ttfSize);
        Font probe (FontOptions (tf).withHeight (100.0f));
        GlyphArrangement ga;
        ga.addLineOfText (probe, "H", 0.0f, 0.0f);
        Path glyph;
        ga.createPath (glyph);
        const float cap100 = jmax (1.0f, glyph.getBounds().getHeight());
        // même approche typographique que le visuel : capitales espacées
        statusFont = Font (FontOptions (tf).withHeight (100.0f * statusCap / cap100).withKerningFactor (0.063f));
    }

    rightClickFwd.callback = [this] { showContextMenu(); };

    auto setupKnob = [this] (Slider& s, StripKnobLookAndFeel& lnf)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s.setMouseDragSensitivity (220);
        s.setLookAndFeel (&lnf);
        s.setMouseCursor (MouseCursor::PointingHandCursor);
        s.addMouseListener (&rightClickFwd, false);
        s.onDragStart = [this] { proc.currentPreset = -1; };
        addAndMakeVisible (s);
    };
    setupKnob (decay, knobLnf);
    setupKnob (tone,  knobLnf);
    setupKnob (level, knobLnf);
    setupKnob (mode,  modeLnf);
    mode.setMouseDragSensitivity (160);
    decay.setDoubleClickReturnValue (true, 0.5);
    tone.setDoubleClickReturnValue  (true, 0.5);
    level.setDoubleClickReturnValue (true, 0.5);

    auto& st = proc.apvts;
    aDecay = std::make_unique<SA> (st, ParamID::decay, decay);
    aTone  = std::make_unique<SA> (st, ParamID::tone,  tone);
    aLevel = std::make_unique<SA> (st, ParamID::level, level);
    aMode  = std::make_unique<SA> (st, ParamID::mode,  mode);

    for (auto* b : { &preSwitch, &tailsSwitch })
    {
        b->addMouseListener (&rightClickFwd, false);
        b->onClick = [this] { proc.currentPreset = -1; };
        addAndMakeVisible (*b);
    }
    aPre   = std::make_unique<BA> (st, ParamID::preDelay, preSwitch);
    aTails = std::make_unique<BA> (st, ParamID::trails,   tailsSwitch);

    footswitch.addMouseListener (&rightClickFwd, false);
    addAndMakeVisible (footswitch);

    const char* ids[7] = { "decay", "tone", "level", "mode", "predelay", "trails", "expression" };
    for (int i = 0; i < 7; ++i)
        lastValues[i] = st.getRawParameterValue (ids[i])->load();
    lastPresetCounter = proc.presetCounter.load();

    // Taille par défaut = taille minimale (impossible de réduire en dessous)
    const int defaultW = roundToInt ((float) defaultH * (float) baseW / (float) baseH);
    setResizable (true, true);
    setResizeLimits (defaultW, defaultH, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* cons = getConstrainer())
        cons->setFixedAspectRatio ((double) baseW / (double) baseH);
    setOpaque (true);                      // anti-gel : l'hôte ne redessine jamais ce qui est derrière la fenêtre
    setSize (defaultW, defaultH);
    rebuildCaches();

    startTimerHz (30);
}

HolyverbAudioProcessorEditor::~HolyverbAudioProcessorEditor()
{
    stopTimer();
    for (auto* s : { &decay, &tone, &level, &mode })
    {
        s->removeMouseListener (&rightClickFwd);
        s->setLookAndFeel (nullptr);
    }
    preSwitch.removeMouseListener (&rightClickFwd);
    tailsSwitch.removeMouseListener (&rightClickFwd);
    footswitch.removeMouseListener (&rightClickFwd);
}

void HolyverbAudioProcessorEditor::setScaledHeight (int h)
{
    h = jmax ((int) defaultH, h);
    setSize (roundToInt ((float) h * (float) baseW / (float) baseH), h);
}

Rectangle<int> HolyverbAudioProcessorEditor::scaled (Rectangle<float> r) const
{
    const float s = (float) getHeight() / (float) baseH;
    return r.transformedBy (AffineTransform::scale (s)).getSmallestIntegerContainer();
}

float HolyverbAudioProcessorEditor::physicalScale() const
{
    return jlimit (1.0f, 4.0f, (float) Component::getApproximateScaleFactorForComponent (this));
}

//==============================================================================
// Anti-gel de la fenêtre (Windows) : JUCE 8 dessine par défaut avec Direct2D (carte graphique).
// Avec certains pilotes (Intel notamment) la fenêtre du plugin cesse de se rafraîchir alors que
// le DAW continue de tourner. Le moteur logiciel est stable partout et largement assez rapide ici.
//==============================================================================
// Anti-gel v2 (Studio Wiga) :
//  1. moteur de rendu logiciel forcé sur Windows (Direct2D peut figer la fenêtre dans certains hôtes),
//     appliqué à chaque nouvelle fenêtre hôte (peer) et revérifié toutes les secondes, jamais en boucle ;
//  2. aucun calcul lourd dans paint() : les caches d'images sont reconstruits par le timer seulement,
//     après stabilisation de la taille ou quand l'échelle d'affichage change (au plus toutes les 250 ms) ;
//  3. chien de garde : si un redessin demandé n'est pas arrivé au bout de 1,5 s alors que la fenêtre est
//     visible, le moteur de rendu est réappliqué et toute la fenêtre est redessinée ;
//  4. côté audio : aucun appel à l'hôte ni allocation depuis le thread audio (voir PluginProcessor).
void HolyverbAudioProcessorEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if (peer != lastPeer) { lastPeer = peer; repaint(); }
   #if JUCE_WINDOWS
    if (peer != nullptr)
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw)
            peer->setCurrentRenderingEngine (sw);
    }
   #endif
}

void HolyverbAudioProcessorEditor::parentHierarchyChanged()
{
    useSoftwareRenderer();
}

void HolyverbAudioProcessorEditor::visibilityChanged()
{
    useSoftwareRenderer();
    if (isShowing()) repaint();
}

void HolyverbAudioProcessorEditor::askRepaint (Rectangle<int> r)
{
    if (repaintAskedMs <= 0.0) repaintAskedMs = Time::getMillisecondCounterHiRes();
    repaint (r);
}

//==============================================================================
// Caches d'affichage : fond et images déjà à la bonne taille
//==============================================================================
void HolyverbAudioProcessorEditor::rebuildCaches (float sc)
{
    if (sc <= 0.0f)
        sc = cacheScale > 0.0f ? cacheScale : physicalScale();
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0)
        return;
    if (w == cacheW && h == cacheH && std::abs (sc - cacheScale) < 0.01f && bgScaled.isValid())
        return;

    cacheW = w; cacheH = h; cacheScale = sc;
    const int pw = roundToInt ((float) w * sc), ph = roundToInt ((float) h * sc);
    bgScaled = (pw >= background.getWidth()) ? background
                                             : background.rescaled (pw, ph, Graphics::highResamplingQuality);

    const float s = (float) h / (float) baseH;
    knobLnf.prepareScaled (roundToInt (knobHalf * 2.0f * s * sc));
    modeLnf.prepareScaled (roundToInt (modeHalf * 2.0f * s * sc));
    preSwitch.prepareScaled (roundToInt (swHalf * 2.0f * s * sc));
    tailsSwitch.prepareScaled (roundToInt (swHalf * 2.0f * s * sc));
    askRepaint (getLocalBounds());
}

void HolyverbAudioProcessorEditor::resized()
{
    // anti-gel : jamais de setSize ici (mise à l'échelle Windows 125 / 150 % : tolérance de quelques px) ;
    // les limites du constrainer suffisent, sinon boucle de redimensionnement avec l'hôte
    auto place = [&] (Component& c, Point<float> p, float half)
    {
        c.setBounds (scaled ({ p.x - half, p.y - half, half * 2.0f, half * 2.0f }));
    };
    place (decay, knobDecay, knobHalf);
    place (tone,  knobTone,  knobHalf);
    place (level, knobLevel, knobHalf);
    place (mode,  knobMode,  modeHalf);
    place (preSwitch,   swPre,   swHalf);
    place (tailsSwitch, swTails, swHalf);
    footswitch.setBounds (scaled ({ footPos.x - footR, footPos.y - footR, footR * 2.0f, footR * 2.0f }));

    resizeSettle = 4;
}

//==============================================================================
// Ligne d'état
//==============================================================================
String HolyverbAudioProcessorEditor::paramText (const String& id) const
{
    return proc.apvts.getParameter (id)->getCurrentValueAsText().toUpperCase();
}

String HolyverbAudioProcessorEditor::baseStatusLine() const
{
    String s = "MODE " + String (ModeOrder::name (roundToInt (proc.apvts.getRawParameterValue (ParamID::mode)->load())));
    if (proc.apvts.getRawParameterValue (ParamID::on)->load() < 0.5f)
        s << "  -  BYPASS";
    return s;
}

void HolyverbAudioProcessorEditor::showStatus (const String& text)
{
    tempStatus = text;
    statusUntil = Time::getMillisecondCounterHiRes() + 2500.0;
}

void HolyverbAudioProcessorEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    // au cas où la fenêtre a été recréée par l'hôte (changement d'écran, d'échelle…) ; sinon au plus 1 fois par seconde
    if (getPeer() != lastPeer || now - lastRendererCheckMs > 1000.0) { lastRendererCheckMs = now; useSoftwareRenderer(); }

    // caches : reconstruits ici seulement (jamais dans paint)
    if (resizeSettle > 0 && --resizeSettle == 0)
        rebuildCaches();
    else if (resizeSettle == 0 && now - lastScaleCheckMs > 250.0)
    {
        lastScaleCheckMs = now;
        const float sc = physicalScale();
        if (std::abs (sc - cacheScale) > 0.01f)
            rebuildCaches (sc);
    }

    // chien de garde : un redessin demandé n'arrive pas -> on relance le moteur de rendu et on redessine tout
    if (repaintAskedMs > 0.0 && isShowing() && now - repaintAskedMs > 1500.0 && lastPaintMs < repaintAskedMs)
    {
        repaintAskedMs = 0.0;
        lastPeer = nullptr;
        useSoftwareRenderer();
        repaint();
    }

    // Un réglage qui bouge (souris, automation, MIDI) s'affiche 2,5 s puis la ligne revient au mode
    {
        static const char* ids[7]   = { "decay", "tone", "level", "mode", "predelay", "trails", "expression" };
        static const char* names[7] = { "DECAY", "TONE", "LEVEL", "MODE", "PRE-DELAY", "TAILS", "EXPRESSION" };
        for (int i = 0; i < 7; ++i)
        {
            const float v = proc.apvts.getRawParameterValue (ids[i])->load();
            if (std::abs (v - lastValues[i]) > 1.0e-4f)
            {
                lastValues[i] = v;
                if (i == 3)       showStatus ("MODE " + String (ModeOrder::name (roundToInt (v))));
                else if (i == 5)  showStatus (v > 0.5f ? "TAILS ON" : "TAILS CUT");
                else              showStatus (String (names[i]) + " " + paramText (ids[i]));
            }
        }
        const int pc = proc.presetCounter.load();
        if (pc != lastPresetCounter)
        {
            lastPresetCounter = pc;
            const int cp = proc.currentPreset.load();
            if (cp >= 0 && cp < HolyverbAudioProcessor::getNumFactoryPresets())
                showStatus ("P" + String (cp + 1) + "  " + String (HolyverbAudioProcessor::getFactoryPreset (cp).name).toUpperCase());
            else if (cp > 100)
                showStatus ("USER " + String (cp - 100));
        }
        for (auto* s : { &decay, &tone, &level, &mode })
            if (s->isMouseButtonDown())
                statusUntil = jmax (statusUntil, now + 2500.0);

        const float target = now < statusUntil ? 1.0f : 0.0f;
        tempAlpha += (target - tempAlpha) * (target > tempAlpha ? 0.55f : 0.15f);
        if (tempAlpha < 0.005f) tempAlpha = 0.0f;
        if (tempAlpha > 0.995f) tempAlpha = 1.0f;
    }

    // LED bleue : allumage en fondu, lueur qui respire avec la reverb
    const bool on = footswitch.isOn();
    ledLevel += ((on ? 1.0f : 0.0f) - ledLevel) * (on ? 0.45f : 0.28f);
    if (ledLevel < 0.003f) ledLevel = 0.0f;
    if (ledLevel > 0.997f) ledLevel = 1.0f;
    const float wetNow = jlimit (0.0f, 1.0f, std::sqrt (proc.ledLevel.load()) * 2.4f);
    reverbGlow += (wetNow - reverbGlow) * (wetNow > reverbGlow ? 0.45f : 0.10f);
    ledFlicker = ledLevel > 0.0f ? 0.975f + 0.025f * rng.nextFloat() : 1.0f;

    const float ledNow = ledLevel * ledFlicker * (0.80f + 0.30f * reverbGlow);
    if (std::abs (ledNow - drawnLed) > 0.003f)
    {
        drawnLed = ledNow;
        askRepaint (scaled ({ ledPos.x - 60.0f, ledPos.y - 60.0f, 120.0f, 120.0f }));
    }

    const String text = tempAlpha > 0.0f ? tempStatus : String();
    const String base = baseStatusLine();
    if (std::abs (tempAlpha - drawnTempAlpha) > 0.003f || text + "|" + base != drawnStatusText)
    {
        drawnTempAlpha = tempAlpha;
        drawnStatusText = text + "|" + base;
        askRepaint (scaled (statusRect.expanded (4.0f)));
    }
}

//==============================================================================
void HolyverbAudioProcessorEditor::drawLed (Graphics& g)
{
    const auto c = ledPos;
    const float lvl = ledLevel;
    const float glow = ledFlicker * (0.80f + 0.30f * reverbGlow);

    // halo bleuté sur la façade
    if (lvl > 0.003f)
    {
        const float bloomR = 56.0f;
        ColourGradient bloom (bluMid.withAlpha (jmin (1.0f, 0.50f * lvl * glow)), c.x, c.y,
                              bluMid.withAlpha (0.0f), c.x + bloomR, c.y, true);
        bloom.addColour (0.28, bluMid.withAlpha (jmin (1.0f, 0.18f * lvl * glow)));
        g.setGradientFill (bloom);
        g.fillEllipse (c.x - bloomR, c.y - bloomR, bloomR * 2.0f, bloomR * 2.0f);
    }

    // fine bague chromée (lumière haut-gauche)
    const float rr = ledRingR;
    g.setColour (Colours::black.withAlpha (0.40f));
    g.fillEllipse (c.x - rr + 0.8f, c.y - rr + 1.3f, rr * 2.0f, rr * 2.0f);
    ColourGradient chrome (Colour (0xffe9ebef), c.x - rr * 0.8f, c.y - rr * 0.9f,
                           Colour (0xff4d5057), c.x + rr * 0.8f, c.y + rr * 0.9f, false);
    chrome.addColour (0.55, Colour (0xff9a9da5));
    g.setGradientFill (chrome);
    g.fillEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f);

    // gorge sombre
    const float gr = ledGapR;
    g.setGradientFill (ColourGradient (Colour (0xff010102), c.x - gr * 0.6f, c.y - gr * 0.7f,
                                       Colour (0xff1a1c22), c.x + gr * 0.7f, c.y + gr * 0.8f, false));
    g.fillEllipse (c.x - gr, c.y - gr, gr * 2.0f, gr * 2.0f);

    // lentille : éteinte (bleu nuit) puis allumée par-dessus en fondu
    const float r = ledLensR;
    g.setGradientFill (ColourGradient (Colour (0xff1b2b48), c.x - r * 0.3f, c.y - r * 0.4f,
                                       Colour (0xff060b16), c.x + r, c.y + r, true));
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    if (lvl > 0.003f)
    {
        const float hot = jlimit (0.0f, 1.0f, lvl * (0.75f + 0.35f * glow));
        ColourGradient body (Colour (0xfff2fbff).withAlpha (hot), c.x + r * 0.02f, c.y - r * 0.02f,
                             Colour (0xff0c3dbb).withAlpha (lvl), c.x + r, c.y, true);
        body.addColour (0.16, Colour (0xffcfeaff).withAlpha (hot));
        body.addColour (0.36, Colour (0xff3d9bff).withAlpha (lvl));
        body.addColour (0.72, Colour (0xff1f6ff0).withAlpha (lvl));
        g.setGradientFill (body);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }
    // bord intérieur de la lentille + reflet fixe
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.8f);
    ColourGradient sp (Colours::white.withAlpha (0.22f + 0.25f * lvl), c.x - r * 0.38f, c.y - r * 0.50f,
                       Colours::white.withAlpha (0.0f), c.x - r * 0.38f + r * 0.36f, c.y - r * 0.50f, true);
    g.setGradientFill (sp);
    g.fillEllipse (c.x - r * 0.74f, c.y - r * 0.78f, r * 0.72f, r * 0.56f);
}

void HolyverbAudioProcessorEditor::drawStatus (Graphics& g)
{
    g.setFont (statusFont);
    const String base = baseStatusLine();
    auto line = [&] (const String& t, float alpha)
    {
        if (alpha <= 0.0f || t.isEmpty())
            return;
        g.setColour (Colours::black.withAlpha (0.35f * alpha));        // légère ombre imprimée
        g.drawSingleLineText (t, roundToInt (statusCenterX + 0.6f), roundToInt (statusBaseline + 0.9f), Justification::horizontallyCentred);
        g.setColour (statusInk.withAlpha (alpha));
        g.drawSingleLineText (t, roundToInt (statusCenterX), roundToInt (statusBaseline), Justification::horizontallyCentred);
    };
    line (base, 1.0f - tempAlpha);
    line (tempStatus, tempAlpha);
}

void HolyverbAudioProcessorEditor::drawLogo (Graphics& g)
{
    if (! logo.isValid())
        return;
    const float w = logoW;
    const float h = w * (float) logo.getHeight() / (float) logo.getWidth();
    g.setOpacity (0.95f);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (logo, Rectangle<float> (logoCentre.x - w * 0.5f, logoCentre.y - h * 0.5f, w, h),
                 RectanglePlacement::centred);
    g.setOpacity (1.0f);
}

void HolyverbAudioProcessorEditor::paint (Graphics& g)
{
    lastPaintMs = Time::getMillisecondCounterHiRes();
    repaintAskedMs = 0.0;
    g.fillAll (Colours::black);
    // anti-gel : aucun rescale ici, on dessine le cache existant (ou l'original en attendant le timer)
    const Image& bg = bgScaled.isValid() ? bgScaled : background;
    g.setImageResamplingQuality (bg.getWidth() == background.getWidth() ? Graphics::highResamplingQuality
                                                                         : Graphics::lowResamplingQuality);
    if (bg.isValid())
        g.drawImage (bg, getLocalBounds().toFloat());

    const float s = (float) getHeight() / (float) baseH;
    g.addTransform (AffineTransform::scale (s));

    drawLed (g);
    drawStatus (g);
    drawLogo (g);
}

//==============================================================================
void HolyverbAudioProcessorEditor::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showContextMenu();
}

void HolyverbAudioProcessorEditor::showContextMenu()
{
    auto raw = [this] (const String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
    PopupMenu menu;

    PopupMenu sizes;
    const int defaultW = roundToInt ((float) defaultH * (float) baseW / (float) baseH);
    sizes.addItem (1, "Default (" + String (defaultW) + " x " + String (defaultH) + ")", true, getHeight() == defaultH);
    sizes.addItem (2, "75 %",  true, getHeight() == roundToInt (baseH * 0.75f));
    sizes.addItem (3, "100 %", true, getHeight() == baseH);
    menu.addSubMenu ("Window size", sizes);
    menu.addSeparator();

    PopupMenu factory;
    for (int i = 0; i < HolyverbAudioProcessor::getNumFactoryPresets(); ++i)
        factory.addItem (100 + i, String (i + 1) + ". " + HolyverbAudioProcessor::getFactoryPreset (i).name,
                         true, proc.currentPreset.load() == i);
    menu.addSubMenu ("Factory presets", factory);

    PopupMenu load, save;
    for (int slot = 1; slot <= 9; ++slot)
    {
        const bool exists = proc.hasUserPreset (slot);
        load.addItem (200 + slot, "User " + String (slot) + (exists ? "" : "  (empty)"), exists,
                      proc.currentPreset.load() == 100 + slot);
        save.addItem (300 + slot, "User " + String (slot) + (exists ? "  (overwrite)" : ""));
    }
    menu.addSubMenu ("Load user preset", load);
    menu.addSubMenu ("Save user preset", save);
    menu.addSeparator();

    PopupMenu modes;
    const int cur = roundToInt (raw (ParamID::mode));
    for (int i = 0; i < 8; ++i)
        modes.addItem (400 + i, ModeOrder::name (i), true, cur == i);
    menu.addSubMenu ("Mode", modes);
    menu.addItem (50, "Pre-delay long",            true, raw (ParamID::preDelay) > 0.5f);
    menu.addItem (51, "Tails (reverb rings out)",  true, raw (ParamID::trails) > 0.5f);
    menu.addItem (52, "Reset expression",          raw (ParamID::expression) > 0.001f);
    menu.addSeparator();
    menu.addItem (999, "Holyverb v1.3.0 - STUDIO WIGA", false);

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this),
        [safe = Component::SafePointer<HolyverbAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0)
                return;
            auto& ed = *safe;
            auto setP = [&ed] (const String& id, float plain)
            {
                if (auto* prm = ed.proc.apvts.getParameter (id))
                {
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
                    prm->endChangeGesture();
                }
            };
            auto rawv = [&ed] (const String& id) { return ed.proc.apvts.getRawParameterValue (id)->load(); };

            if      (r == 1) ed.setScaledHeight (defaultH);
            else if (r == 2) ed.setScaledHeight (roundToInt (baseH * 0.75f));
            else if (r == 3) ed.setScaledHeight (baseH);
            else if (r == 50) { setP (ParamID::preDelay, rawv (ParamID::preDelay) > 0.5f ? 0.0f : 1.0f); ed.proc.currentPreset = -1; }
            else if (r == 51) { setP (ParamID::trails,   rawv (ParamID::trails)   > 0.5f ? 0.0f : 1.0f); ed.proc.currentPreset = -1; }
            else if (r == 52) setP (ParamID::expression, 0.0f);
            else if (r >= 100 && r < 200) ed.proc.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  ed.proc.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  ed.proc.saveUserPreset (r - 300);
            else if (r >= 400 && r < 408) { setP (ParamID::mode, (float) (r - 400)); ed.proc.currentPreset = -1; }
        });
}
