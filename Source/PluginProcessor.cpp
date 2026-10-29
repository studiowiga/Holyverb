#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Presets usine (mode = index du sélecteur : 0 CHURCH, 1 PLATE, 2 HALL, 3 ROOM, 4 SHIMMER, 5 LOFI, 6 MOD, 7 SPRING)
    const HolyverbPreset factoryPresets[] =
    {
        //  nom                 mode decay  tone  level  preLong trails
        { "Concert Hall",        2, 0.50f, 0.50f, 0.50f, false, true },
        { "Studio Room",         3, 0.35f, 0.55f, 0.45f, false, true },
        { "Tight Ambience",      3, 0.10f, 0.70f, 0.35f, false, true },
        { "Vintage Plate",       1, 0.50f, 0.60f, 0.50f, false, true },
        { "Dark Plate",          1, 0.75f, 0.20f, 0.50f, true,  true },
        { "Surf Spring",         7, 0.55f, 0.65f, 0.60f, false, true },
        { "Drippy Spring",       7, 0.85f, 0.80f, 0.70f, false, true },
        { "Cathedral",           0, 0.70f, 0.40f, 0.55f, true,  true },
        { "Endless Church",      0, 1.00f, 0.35f, 0.60f, true,  true },
        { "Chorus Hall",         6, 0.60f, 0.55f, 0.50f, false, true },
        { "Lofi Tape Room",      5, 0.45f, 0.40f, 0.55f, false, true },
        { "Heaven Shimmer",      4, 0.65f, 0.60f, 0.55f, true,  true },
        { "Ambient Wash",        4, 0.90f, 0.45f, 0.70f, true,  true },
        { "Huge Long Hall",      2, 0.90f, 0.45f, 0.55f, true,  true },
    };
}

HolyverbAudioProcessor::HolyverbAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HolyverbState", createLayout())
{
    pDecay  = apvts.getRawParameterValue (ParamID::decay);
    pTone   = apvts.getRawParameterValue (ParamID::tone);
    pLevel  = apvts.getRawParameterValue (ParamID::level);
    pMode   = apvts.getRawParameterValue (ParamID::mode);
    pPre    = apvts.getRawParameterValue (ParamID::preDelay);
    pExpr   = apvts.getRawParameterValue (ParamID::expression);
    pOn     = apvts.getRawParameterValue (ParamID::on);
    pTrails = apvts.getRawParameterValue (ParamID::trails);
}

juce::AudioProcessorValueTreeState::ParameterLayout HolyverbAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; });

    StringArray modes;
    for (int i = 0; i < 8; ++i)
        modes.add (String (ModeOrder::name (i)).substring (0, 1) + String (ModeOrder::name (i)).substring (1).toLowerCase());

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::decay, 1 }, "Decay",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::tone, 1 }, "Tone",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::level, 1 }, "Level",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::mode, 1 }, "Mode",
                                                        modes, ModeOrder::defaultIndex));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::preDelay, 1 }, "Pre-Delay",
                                                        StringArray { "Short", "Long" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::expression, 1 }, "Expression",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::on, 1 }, "On", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::trails, 1 }, "Tails", true));
    return layout;
}

bool HolyverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono())
        return false;
    // Mono -> stéréo autorisé ; stéréo -> mono refusé
    return ! (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::mono());
}

void HolyverbAudioProcessor::prepareToPlay (double sampleRate, int)
{
    holyverb::Params p;
    p.decay = pDecay->load (std::memory_order_relaxed); p.tone = pTone->load (std::memory_order_relaxed); p.level = pLevel->load (std::memory_order_relaxed);
    p.mode = ModeOrder::toEngine ((int) pMode->load (std::memory_order_relaxed)); p.preLong = pPre->load (std::memory_order_relaxed) > 0.5f;
    p.expression = pExpr->load (std::memory_order_relaxed); p.on = pOn->load (std::memory_order_relaxed) > 0.5f; p.spillover = pTrails->load (std::memory_order_relaxed) > 0.5f;
    engine.setParams (p);
    engine.prepare (sampleRate);
}

void HolyverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();

    // Entrée mono -> on duplique sur la sortie stéréo
    if (numIn == 1 && numOut > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    holyverb::Params p;
    p.decay      = pDecay->load (std::memory_order_relaxed);
    p.tone       = pTone->load (std::memory_order_relaxed);
    p.level      = pLevel->load (std::memory_order_relaxed);
    p.mode       = ModeOrder::toEngine (juce::roundToInt (pMode->load (std::memory_order_relaxed)));
    p.preLong    = pPre->load (std::memory_order_relaxed) > 0.5f;
    p.expression = pExpr->load (std::memory_order_relaxed);
    p.on         = pOn->load (std::memory_order_relaxed) > 0.5f;
    p.spillover  = pTrails->load (std::memory_order_relaxed) > 0.5f;
    engine.setParams (p);

    float* L = buffer.getWritePointer (0);
    float* R = numOut > 1 ? buffer.getWritePointer (1) : L;
    engine.process (L, R, n);

    ledLevel.store (engine.getMeter());
}

//==============================================================================
// Presets
//==============================================================================
int HolyverbAudioProcessor::getNumFactoryPresets()
{
    return (int) (sizeof (factoryPresets) / sizeof (factoryPresets[0]));
}

const HolyverbPreset& HolyverbAudioProcessor::getFactoryPreset (int i)
{
    return factoryPresets[juce::jlimit (0, getNumFactoryPresets() - 1, i)];
}

int HolyverbAudioProcessor::getNumPrograms()              { return getNumFactoryPresets(); }
int HolyverbAudioProcessor::getCurrentProgram()           { return juce::jmax (0, juce::jmin (currentPreset.load(), getNumFactoryPresets() - 1)); }
void HolyverbAudioProcessor::setCurrentProgram (int i)    { if (i >= 0 && i < getNumFactoryPresets()) applyFactoryPreset (i); }
const juce::String HolyverbAudioProcessor::getProgramName (int i)
{
    return (i >= 0 && i < getNumFactoryPresets()) ? juce::String (getFactoryPreset (i).name) : juce::String();
}

void HolyverbAudioProcessor::setParam (const juce::String& id, float plain)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void HolyverbAudioProcessor::applyFactoryPreset (int i)
{
    const auto& p = getFactoryPreset (i);
    setParam (ParamID::mode,       (float) p.mode);
    setParam (ParamID::decay,      p.decay);
    setParam (ParamID::tone,       p.tone);
    setParam (ParamID::level,      p.level);
    setParam (ParamID::preDelay,   p.preLong ? 1.0f : 0.0f);
    setParam (ParamID::trails,     p.trails ? 1.0f : 0.0f);
    setParam (ParamID::expression, 0.0f);
    currentPreset = i;
    ++presetCounter;
}

juce::File HolyverbAudioProcessor::userPresetFile (int slot)
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Studio Wiga").getChildFile ("Holyverb")
             .getChildFile ("User " + juce::String (slot) + ".xml");
}

void HolyverbAudioProcessor::saveUserPreset (int slot)
{
    auto f = userPresetFile (slot);
    f.getParentDirectory().createDirectory();
    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (f);
    currentPreset = 100 + slot;
    ++presetCounter;
}

void HolyverbAudioProcessor::loadUserPreset (int slot)
{
    if (auto xml = juce::XmlDocument::parse (userPresetFile (slot)))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto tree = juce::ValueTree::fromXml (*xml);
            for (auto* id : { &ParamID::mode, &ParamID::decay, &ParamID::tone, &ParamID::level,
                              &ParamID::preDelay, &ParamID::trails, &ParamID::expression })
            {
                for (int c = 0; c < tree.getNumChildren(); ++c)
                {
                    const auto child = tree.getChild (c);
                    if (child.getProperty ("id").toString() == *id)
                        setParam (*id, (float) child.getProperty ("value"));
                }
            }
            currentPreset = 100 + slot;
            ++presetCounter;
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* HolyverbAudioProcessor::createEditor()
{
    return new HolyverbAudioProcessorEditor (*this);
}

void HolyverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", currentPreset.load(), nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void HolyverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        currentPreset = (int) tree.getProperty ("currentPreset", -1);
        apvts.replaceState (tree);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HolyverbAudioProcessor();
}
