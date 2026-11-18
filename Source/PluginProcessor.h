#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include "CompressorDSP.h"
#include "CompressorParameters.h"
#include "PresetManager.h"

class VocalCompressorProcessor : public juce::AudioProcessor,
                                 private juce::AsyncUpdater
{
public:
    VocalCompressorProcessor();
    ~VocalCompressorProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Vocal Compressor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // GUI 用(すべてメッセージスレッドから呼ぶ)
    vc::MeterData& getMeters() noexcept { return compressor.meters; }
    int getReportedLatency() const noexcept { return compressor.getLatencySamples(); }
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }

    vc::ParamValues getCurrentValues() const;                    // 現在の全パラメーター値(物理単位)
    void applyValues (const vc::ParamValues& v, const juce::String& undoName);
    void resetToDefaults();

    // プリセット
    vc::PresetManager& getPresetManager() noexcept { return presets; }
    juce::String getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName (const juce::String& n) { currentPresetName = n; }
    bool loadPreset (const juce::String& name);

    // A/B 比較
    void toggleAB();
    int getActiveSlot() const noexcept { return abActive; }
    void copyActiveToOther();

    // UI サイズ(状態に保存)
    void setUiSize (int w, int h) { uiW = w; uiH = h; }
    int getUiWidth() const noexcept { return uiW; }
    int getUiHeight() const noexcept { return uiH; }

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;                           // レイテンシー更新(メッセージスレッド)
    static juce::String valuesToString (const vc::ParamValues& v);
    static bool stringToValues (const juce::String& s, vc::ParamValues& v);

    std::array<std::atomic<float>*, vc::P_COUNT> raw {};
    juce::AudioParameterBool* bypassParam = nullptr;

    vc::Compressor compressor;
    vc::PresetManager presets;
    double currentSampleRate = 44100.0;
    int maxBlock = 512;
    std::atomic<int> reportedLatency { -1 };

    juce::String currentPresetName { "Vocal Natural" };
    std::array<vc::ParamValues, 2> slots {};
    std::array<bool, 2> slotValid { false, false };
    int abActive = 0;
    int uiW = 960, uiH = 640;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalCompressorProcessor)
};
