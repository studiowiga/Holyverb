#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "HolyverbDSP.h"

namespace ParamID
{
    static const juce::String decay      { "decay" };
    static const juce::String tone       { "tone" };
    static const juce::String level      { "level" };
    static const juce::String mode       { "mode" };
    static const juce::String preDelay   { "predelay" };
    static const juce::String expression { "expression" };
    static const juce::String on         { "on" };
    static const juce::String trails     { "trails" };
}

// Ordre du sélecteur MODE de la façade (sens horaire, de 7 h 30 à 4 h 30)
// index paramètre : 0 CHURCH, 1 PLATE, 2 HALL, 3 ROOM, 4 SHIMMER, 5 LOFI, 6 MOD, 7 SPRING
namespace ModeOrder
{
    inline const char* name (int i)
    {
        static const char* n[] = { "CHURCH", "PLATE", "HALL", "ROOM", "SHIMMER", "LOFI", "MOD", "SPRING" };
        return n[juce::jlimit (0, 7, i)];
    }
    // index du sélecteur -> mode du moteur DSP
    inline int toEngine (int i)
    {
        using M = holyverb::Mode;
        static const M m[] = { M::Church, M::Plate, M::Hall, M::Room, M::Shimmer, M::Lofi, M::Mod, M::Spring };
        return (int) m[juce::jlimit (0, 7, i)];
    }
    constexpr int defaultIndex = 2; // HALL
}

struct HolyverbPreset
{
    const char* name;
    int   mode;          // index du sélecteur
    float decay, tone, level;
    bool  preLong;
    bool  trails;
};

class HolyverbAudioProcessor : public juce::AudioProcessor
{
public:
    HolyverbAudioProcessor();
    ~HolyverbAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Holyverb"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    // Presets usine : ils apparaissent dans le menu déroulant de presets du DAW
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // --- Presets ---
    static int getNumFactoryPresets();
    static const HolyverbPreset& getFactoryPreset (int i);
    void applyFactoryPreset (int i);
    static juce::File userPresetFile (int slot);
    bool hasUserPreset (int slot) const { return userPresetFile (slot).existsAsFile(); }
    void saveUserPreset (int slot);
    void loadUserPreset (int slot);

    juce::AudioProcessorValueTreeState apvts;

    // -1 = réglages libres, 0..N-1 = preset usine, 101..109 = preset utilisateur
    std::atomic<int> currentPreset { -1 };
    std::atomic<int> presetCounter { 0 };          // pour afficher le nom du preset chargé

    // Pour l'animation de la LED (lu par l'éditeur)
    std::atomic<float> ledLevel { 0.0f };

private:
    void setParam (const juce::String& id, float plain);

    holyverb::Engine engine;

    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pLevel = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pPre = nullptr;
    std::atomic<float>* pExpr = nullptr;
    std::atomic<float>* pOn = nullptr;
    std::atomic<float>* pTrails = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HolyverbAudioProcessor)
};
