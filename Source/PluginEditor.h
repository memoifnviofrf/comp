#pragma once

#include <array>
#include <functional>
#include <memory>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "Metering.h"
#include "PluginProcessor.h"

namespace vcgui
{
//==============================================================================
// 配色(独自デザイン)
namespace col
{
const juce::Colour bg      { 0xff12141a };
const juce::Colour panel   { 0xff1b1e26 };
const juce::Colour panel2  { 0xff232731 };
const juce::Colour line    { 0xff343a47 };
const juce::Colour text    { 0xffd5d9e0 };
const juce::Colour dim     { 0xff7d8596 };
const juce::Colour accent  { 0xff4cc3f0 };
const juce::Colour gr      { 0xffff9a3c };
const juce::Colour good    { 0xff5fd18a };
const juce::Colour warn    { 0xffffd166 };
const juce::Colour hot     { 0xffef5b5b };
}

class VcLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VcLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
};

//==============================================================================
// ノブ(キャプション + ロータリー + 数値入力ボックス)
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& caption,
          juce::Colour colour = col::accent);
    void resized() override;
    juce::Slider slider;
private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& caption);
    void resized() override;
    juce::ComboBox box;
private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class Toggle : public juce::ToggleButton
{
public:
    Toggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& caption);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

//==============================================================================
// レベルメーター(Level: 入出力 -60..0 dBFS / GainReduction: 0..24 dB を上から下へ)
class MeterBar : public juce::Component
{
public:
    enum class Mode { Level, GainReduction };
    MeterBar (Mode m, juce::String name) : mode (m), title (std::move (name)) {}
    void setValues (float peakDb, float rmsDb, float holdDb);
    void paint (juce::Graphics&) override;
private:
    Mode mode;
    juce::String title;
    float peak = -120.0f, rms = -120.0f, hold = -120.0f;
};

// 圧縮カーブ(静的カーブ + 現在の動作点)
class CurveDisplay : public juce::Component
{
public:
    std::function<vc::Params()> getParams;
    float liveInDb = -120.0f, liveGrDb = 0.0f;
    void paint (juce::Graphics&) override;
};

// Attack / Release の視覚的表示(1次系の理想ステップ応答)
class BallisticsDisplay : public juce::Component
{
public:
    std::function<vc::Params()> getParams;
    void paint (juce::Graphics&) override;
};

// 入力レベルとゲインリダクションの履歴(約 8 秒)
class HistoryDisplay : public juce::Component
{
public:
    static constexpr int kPoints = 240;
    std::function<vc::Params()> getParams;
    void push (float inPeakDb, float grDb);
    void paint (juce::Graphics&) override;
private:
    std::array<float, kPoints> inDb {}, gr {};
    int pos = 0;
};

} // namespace vcgui

//==============================================================================
class VocalCompressorEditor : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit VocalCompressorEditor (VocalCompressorProcessor&);
    ~VocalCompressorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresetMenu();
    void showTab (int index);
    void promptSavePreset();
    void promptRenamePreset();
    void confirmDeletePreset();
    void showSettingsMenu();
    vc::Params currentParams() const;

    VocalCompressorProcessor& proc;
    vcgui::VcLookAndFeel laf;
    juce::TooltipWindow tooltipWindow { this, 700 };
    juce::ComponentBoundsConstrainer constrainer;

    // 上部
    juce::Label titleLabel, latencyLabel;
    juce::ComboBox presetBox;
    juce::TextButton saveButton { "Save" }, renameButton { "Rename" }, deleteButton { "Delete" },
                     abButton { "A" }, copyButton { "A>B" }, undoButton { "Undo" }, redoButton { "Redo" },
                     settingsButton { "Settings" };
    vcgui::Choice osChoice;
    vcgui::Toggle bypassToggle;

    // 中央
    vcgui::CurveDisplay curve;
    vcgui::BallisticsDisplay ballistics;
    vcgui::HistoryDisplay history;
    vcgui::Knob kThreshold, kRatio, kAttack, kRelease, kKnee, kMakeup;

    // 右
    vcgui::MeterBar meterIn, meterOut, meterGr;
    vcgui::Knob kIn, kOut;
    vc::MeterBallistics balIn, balOut, balGr;

    // 下(タブ)
    std::array<juce::TextButton, 3> tabButtons;
    std::array<juce::Component, 3> tabPanels;
    int currentTab = 0;

    // タブ 0: Compressor
    vcgui::Choice cModel, cTopology;
    vcgui::Toggle tAutoRelease, tAutoMakeup;
    vcgui::Knob kBlend, kLink, kMix, kLookahead;
    // タブ 1: Sidechain
    vcgui::Choice cScSource, cScMode, cWeighting;
    vcgui::Toggle tScMonitor;
    vcgui::Knob kScHpf, kScLpf, kScGain, kEqLf, kEqLg, kEqMf, kEqMg, kEqHf, kEqHg;
    // タブ 2: Vocal
    vcgui::Knob kvLevel, kvPlosive, kvSib, kvSyl, kvPhrase, kvAdapt;
    juce::Label vocalNote;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalCompressorEditor)
};
