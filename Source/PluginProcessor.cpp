#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace vc;

namespace
{
juce::NormalisableRange<float> makeRange (const ParamDef& d)
{
    juce::NormalisableRange<float> r (d.min, d.max, d.step);
    if (d.centre > d.min && d.centre < d.max)
        r.setSkewForCentre (d.centre);
    return r;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout VocalCompressorProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& d : paramDefs())
    {
        const juce::ParameterID pid { d.id, 1 };
        switch (d.kind)
        {
            case ParamKind::Float:
            {
                const int decimals = d.step >= 1.0f ? 0 : (d.step >= 0.1f ? 1 : 2);
                const juce::String unit (d.unit);
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, d.name, makeRange (d), d.def,
                    juce::AudioParameterFloatAttributes()
                        .withLabel (unit.trim())
                        .withStringFromValueFunction ([decimals, unit] (float v, int) { return juce::String (v, decimals) + unit; })
                        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); })));
                break;
            }
            case ParamKind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    pid, d.name, juce::StringArray::fromTokens (d.choices, "|", ""), (int) d.def));
                break;
            case ParamKind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f));
                break;
        }
    }
    return layout;
}

//==============================================================================
VocalCompressorProcessor::VocalCompressorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undoManager, "STATE", createLayout()),
      presets (PresetManager::defaultUserDir())
{
    for (int i = 0; i < P_COUNT; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (paramDefs()[(size_t) i].id);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (paramDefs()[P_bypass].id));
    undoManager.setMaxNumberOfStoredUnits (60000, 100);
}

VocalCompressorProcessor::~VocalCompressorProcessor()
{
    cancelPendingUpdate();
}

bool VocalCompressorProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (l.getMainInputChannelSet() != out) return false;
    const auto& sc = l.getChannelSet (true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

double VocalCompressorProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

void VocalCompressorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlock = samplesPerBlock;
    float v[P_COUNT];
    for (int i = 0; i < P_COUNT; ++i) v[i] = raw[(size_t) i]->load();
    compressor.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getMainBusNumInputChannels()));
    compressor.setParams (toParams (v));
    compressor.reset();
    reportedLatency.store (compressor.getLatencySamples());
    setLatencySamples (compressor.getLatencySamples());
}

void VocalCompressorProcessor::handleAsyncUpdate()
{
    setLatencySamples (reportedLatency.load());
}

void VocalCompressorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int nCh = juce::jmin (getMainBusNumInputChannels(), buffer.getNumChannels(), (int) Compressor::kMaxChannels);
    if (n <= 0 || nCh <= 0) return;

    float v[P_COUNT];
    for (int i = 0; i < P_COUNT; ++i) v[i] = raw[(size_t) i]->load (std::memory_order_relaxed);
    compressor.setParams (toParams (v));

    // レイテンシー(Lookahead / Oversampling で変わる)が変わったらホストへ通知(メッセージスレッドで)
    const int lat = compressor.getLatencySamples();
    if (lat != reportedLatency.load (std::memory_order_relaxed))
    {
        reportedLatency.store (lat, std::memory_order_relaxed);
        triggerAsyncUpdate();
    }

    // 外部サイドチェイン
    const float* scPtrs[2] = { nullptr, nullptr };
    int scCh = 0;
    auto* scBus = getBus (true, 1);
    if (scBus != nullptr && scBus->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        scCh = juce::jmin (sc.getNumChannels(), 2);
        for (int c = 0; c < scCh; ++c) scPtrs[c] = sc.getReadPointer (c);
    }

    // モノラルのサイドチェインは、右チャンネルでも L を使う(Compressor 側で処理)
    compressor.process (buffer.getArrayOfWritePointers(), nCh, n, scCh > 0 ? scPtrs : nullptr, scCh);

    for (int c = nCh; c < buffer.getNumChannels(); ++c)
        if (c < getMainBusNumOutputChannels()) buffer.clear (c, 0, n);
}

//==============================================================================
ParamValues VocalCompressorProcessor::getCurrentValues() const
{
    ParamValues v {};
    for (int i = 0; i < P_COUNT; ++i) v[(size_t) i] = raw[(size_t) i]->load();
    return v;
}

void VocalCompressorProcessor::applyValues (const ParamValues& v, const juce::String& undoName)
{
    undoManager.beginNewTransaction (undoName);
    for (int i = 0; i < P_COUNT; ++i)
    {
        if (auto* p = apvts.getParameter (paramDefs()[(size_t) i].id))
        {
            const float n = p->convertTo0to1 (v[(size_t) i]);
            if (std::abs (p->getValue() - n) > 1.0e-6f)
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (n);
                p->endChangeGesture();
            }
        }
    }
}

void VocalCompressorProcessor::resetToDefaults()
{
    applyValues (defaultValues(), "Reset");
    currentPresetName = {};
}

bool VocalCompressorProcessor::loadPreset (const juce::String& name)
{
    if (auto v = presets.load (name))
    {
        applyValues (*v, "Preset: " + name);
        currentPresetName = name;
        return true;
    }
    return false;
}

void VocalCompressorProcessor::toggleAB()
{
    slots[(size_t) abActive] = getCurrentValues();
    slotValid[(size_t) abActive] = true;
    const int other = 1 - abActive;
    if (! slotValid[(size_t) other])
    {
        slots[(size_t) other] = slots[(size_t) abActive];   // 初回は同じ内容から始める
        slotValid[(size_t) other] = true;
    }
    abActive = other;
    applyValues (slots[(size_t) abActive], abActive == 0 ? "Switch to A" : "Switch to B");
}

void VocalCompressorProcessor::copyActiveToOther()
{
    slots[(size_t) (1 - abActive)] = getCurrentValues();
    slotValid[(size_t) (1 - abActive)] = true;
}

//==============================================================================
juce::String VocalCompressorProcessor::valuesToString (const ParamValues& v)
{
    juce::StringArray a;
    for (float x : v) a.add (juce::String (x, 6));
    return a.joinIntoString (",");
}

bool VocalCompressorProcessor::stringToValues (const juce::String& s, ParamValues& v)
{
    const auto t = juce::StringArray::fromTokens (s, ",", "");
    if (t.size() != P_COUNT) return false;
    for (int i = 0; i < P_COUNT; ++i)
    {
        const double x = t[i].getDoubleValue();
        if (! std::isfinite (x)) return false;
        v[(size_t) i] = (float) x;
    }
    return true;
}

void VocalCompressorProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("uiW", uiW, nullptr);
    state.setProperty ("uiH", uiH, nullptr);
    state.setProperty ("preset", currentPresetName, nullptr);
    state.setProperty ("abActive", abActive, nullptr);
    slots[(size_t) abActive] = getCurrentValues();
    slotValid[(size_t) abActive] = true;
    for (int s = 0; s < 2; ++s)
        if (slotValid[(size_t) s])
            state.setProperty (s == 0 ? "slotA" : "slotB", valuesToString (slots[(size_t) s]), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void VocalCompressorProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    const auto state = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (state);

    uiW = juce::jlimit (640, 2400, (int) state.getProperty ("uiW", 960));
    uiH = juce::jlimit (440, 1600, (int) state.getProperty ("uiH", 640));
    currentPresetName = state.getProperty ("preset", "").toString();
    abActive = (int) state.getProperty ("abActive", 0) == 1 ? 1 : 0;
    for (int s = 0; s < 2; ++s)
    {
        ParamValues v {};
        slotValid[(size_t) s] = stringToValues (state.getProperty (s == 0 ? "slotA" : "slotB", "").toString(), v);
        if (slotValid[(size_t) s]) slots[(size_t) s] = v;
    }
    undoManager.clearUndoHistory();
}

juce::AudioProcessorEditor* VocalCompressorProcessor::createEditor()
{
    return new VocalCompressorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalCompressorProcessor();
}
