#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Potards : filmstrips haute définition, lumière fixe en haut à gauche.
//  - DECAY / TONE / LEVEL : le moletage tourne (8 phases suffisent, il a 76 dents),
//    l'index blanc et le point sont dessinés par le code à l'angle exact.
//  - MODE : 8 images, une par position du sélecteur.
// Une copie déjà à la taille d'affichage est gardée en cache (affichage fluide).
//==============================================================================
class StripKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
    void prepareScaled (int pixelSize);
    juce::Image strip, scaledStrip;
    int frames = 8, scaledSize = 0;
    bool drawPointer = true;      // index + point dessinés par le code
    float knobR = 1.0f;           // rayon extérieur du potard / demi-côté de l'image
};

//==============================================================================
// Mini-switch à levier (image haut / bas, éclairage fixe)
//==============================================================================
class LeverSwitch : public juce::Button
{
public:
    LeverSwitch() : juce::Button ("switch") { setClickingTogglesState (true); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void prepareScaled (int pixelSize);
    juce::Image strip, scaled;     // 2 images : haut, bas
    int scaledSize = 0;
};

//==============================================================================
// Zone cliquable du footswitch (dessiné sur l'image de fond)
//==============================================================================
class FootSwitchArea : public juce::Component
{
public:
    explicit FootSwitchArea (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;
    bool isOn() const { return value; }

private:
    bool value = true;
    bool pressed = false;
    juce::ParameterAttachment attachment;
};

//==============================================================================
// Transmet le clic droit des commandes vers le menu de l'éditeur
//==============================================================================
struct RightClickForwarder : public juce::MouseListener
{
    std::function<void()> callback;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && callback)
            callback();
    }
};

//==============================================================================
class HolyverbAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit HolyverbAudioProcessorEditor (HolyverbAudioProcessor&);
    ~HolyverbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void mouseDown (const juce::MouseEvent&) override;

    // Repère de référence (100 %) ; taille par défaut = taille minimale (identique à Echoflex)
    static constexpr int baseW = 760;
    static constexpr int baseH = 958;
    static constexpr int defaultH = 493;

private:
    void timerCallback() override;
    void useSoftwareRenderer();
    void showContextMenu();
    void setScaledHeight (int h);
    void showStatus (const juce::String& text);
    void drawLed (juce::Graphics&);
    void drawStatus (juce::Graphics&);
    void drawLogo (juce::Graphics&);
    void rebuildCaches (float scale = 0.0f);
    float physicalScale() const;
    juce::String paramText (const juce::String& id) const;
    juce::String baseStatusLine() const;
    juce::Rectangle<int> scaled (juce::Rectangle<float> r) const;

    HolyverbAudioProcessor& proc;
    StripKnobLookAndFeel knobLnf, modeLnf;
    RightClickForwarder rightClickFwd;
    juce::Image background, bgScaled, logo;
    juce::Font statusFont;
    int cacheW = 0, cacheH = 0;
    float cacheScale = 0.0f;
    int resizeSettle = 0;
    // anti-gel v2 : suivi de la fenêtre hôte, de l'échelle et des redessins
    void* lastPeer = nullptr;
    double lastPaintMs = 0.0, repaintAskedMs = 0.0, lastScaleCheckMs = 0.0, lastRendererCheckMs = 0.0;
    void askRepaint (juce::Rectangle<int> r);
    void visibilityChanged() override;

    juce::Slider decay, tone, level, mode;
    LeverSwitch preSwitch, tailsSwitch;
    FootSwitchArea footswitch;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> aDecay, aTone, aLevel, aMode;
    std::unique_ptr<BA> aPre, aTails;

    // Animation LED / ligne d'état
    juce::Random rng;
    float ledLevel = 1.0f, ledFlicker = 1.0f, reverbGlow = 0.0f, drawnLed = -1.0f;
    juce::String tempStatus, drawnStatusText;
    double statusUntil = 0.0;
    float tempAlpha = 0.0f, drawnTempAlpha = -1.0f;
    float lastValues[7] { -1, -1, -1, -1, -1, -1, -1 };
    int lastPresetCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HolyverbAudioProcessorEditor)
};
