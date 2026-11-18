#pragma once

// パラメーター定義(JUCEに依存しない)。
// ・APVTS のレイアウト生成、プリセット、DSPテストが同じ表を共有します。
// ・DSP は Params 構造体(物理単位)だけを見ます。

#include <algorithm>
#include <array>

namespace vc
{
enum class Model     { Transparent = 0, Opto, FET, VCA, VariMu };
enum class Topology  { FeedForward = 0, FeedBack };
enum class Weighting { Off = 0, Voice, AWeight };
enum class ScSource  { Internal = 0, External };
enum class ScMode    { Stereo = 0, Mono };

enum ParamIndex : int
{
    P_bypass = 0, P_inGain, P_outGain, P_makeup, P_mix,
    P_threshold, P_ratio, P_attack, P_release, P_knee,
    P_model, P_topology, P_blend, P_autoRelease, P_autoMakeup, P_link, P_lookahead, P_os,
    P_weighting, P_scSource, P_scMode, P_scMonitor, P_scHpf, P_scLpf, P_scGain,
    P_eqLf, P_eqLg, P_eqMf, P_eqMg, P_eqHf, P_eqHg,
    P_vLevel, P_vPlosive, P_vSib, P_vSyl, P_vPhrase, P_vAdapt,
    P_COUNT
};

enum class ParamKind { Float, Choice, Bool };

struct ParamDef
{
    const char* id;
    const char* name;
    ParamKind kind;
    float min, max, def;
    float centre;        // >0 なら、この値がスライダー中央に来るスキュー
    float step;
    const char* unit;
    const char* choices; // "A|B|C"
};

inline const std::array<ParamDef, P_COUNT>& paramDefs()
{
    static const std::array<ParamDef, P_COUNT> defs = { {
        { "bypass",     "Bypass",              ParamKind::Bool,   0, 1, 0, 0, 1, "", "" },
        { "in_gain",    "Input Gain",          ParamKind::Float, -24, 24, 0, 0, 0.1f, " dB", "" },
        { "out_gain",   "Output Gain",         ParamKind::Float, -24, 24, 0, 0, 0.1f, " dB", "" },
        { "makeup",     "Makeup Gain",         ParamKind::Float, -24, 24, 0, 0, 0.1f, " dB", "" },
        { "mix",        "Mix",                 ParamKind::Float,   0, 100, 100, 0, 0.1f, " %", "" },

        { "threshold",  "Threshold",           ParamKind::Float, -60, 0, -20, 0, 0.1f, " dB", "" },
        { "ratio",      "Ratio",               ParamKind::Float,   1, 20, 3, 4, 0.01f, " :1", "" },
        { "attack",     "Attack",              ParamKind::Float, 0.1f, 200, 10, 10, 0.01f, " ms", "" },
        { "release",    "Release",             ParamKind::Float,   5, 1000, 100, 100, 0.1f, " ms", "" },
        { "knee",       "Knee",                ParamKind::Float,   0, 24, 6, 0, 0.1f, " dB", "" },

        { "model",      "Model",               ParamKind::Choice,  0, 4, 0, 0, 1, "", "Transparent|Opto|FET|VCA|Vari-Mu" },
        { "topology",   "Topology",            ParamKind::Choice,  0, 1, 0, 0, 1, "", "Feed Forward|Feed Back" },
        { "blend",      "Peak/RMS Blend",      ParamKind::Float,   0, 100, 50, 0, 0.1f, " %", "" },
        { "auto_rel",   "Auto Release",        ParamKind::Bool,    0, 1, 1, 0, 1, "", "" },
        { "auto_mk",    "Auto Makeup",         ParamKind::Bool,    0, 1, 0, 0, 1, "", "" },
        { "link",       "Stereo Link",         ParamKind::Float,   0, 100, 100, 0, 0.1f, " %", "" },
        { "lookahead",  "Lookahead",           ParamKind::Float,   0, 10, 0, 0, 0.1f, " ms", "" },
        { "os",         "Oversampling",        ParamKind::Choice,  0, 2, 0, 0, 1, "", "1x|2x|4x" },

        { "weighting",  "SC Weighting",        ParamKind::Choice,  0, 2, 0, 0, 1, "", "Off|Voice|A-weight" },
        { "sc_source",  "SC Source",           ParamKind::Choice,  0, 1, 0, 0, 1, "", "Internal|External" },
        { "sc_mode",    "SC Channels",         ParamKind::Choice,  0, 1, 0, 0, 1, "", "Stereo|Mono" },
        { "sc_monitor", "SC Monitor",          ParamKind::Bool,    0, 1, 0, 0, 1, "", "" },
        { "sc_hpf",     "SC HPF",              ParamKind::Float,  20, 1000, 20, 120, 1, " Hz", "" },
        { "sc_lpf",     "SC LPF",              ParamKind::Float, 1000, 20000, 20000, 6000, 1, " Hz", "" },
        { "sc_gain",    "SC Gain",             ParamKind::Float, -24, 24, 0, 0, 0.1f, " dB", "" },

        { "eq_lf",      "SC EQ Low Freq",      ParamKind::Float,  40, 500, 120, 0, 1, " Hz", "" },
        { "eq_lg",      "SC EQ Low Gain",      ParamKind::Float, -12, 12, 0, 0, 0.1f, " dB", "" },
        { "eq_mf",      "SC EQ Mid Freq",      ParamKind::Float, 300, 8000, 2000, 1500, 1, " Hz", "" },
        { "eq_mg",      "SC EQ Mid Gain",      ParamKind::Float, -12, 12, 0, 0, 0.1f, " dB", "" },
        { "eq_hf",      "SC EQ High Freq",     ParamKind::Float, 2000, 16000, 8000, 6000, 1, " Hz", "" },
        { "eq_hg",      "SC EQ High Gain",     ParamKind::Float, -12, 12, 0, 0, 0.1f, " dB", "" },

        { "v_level",    "Vocal Leveling",      ParamKind::Float,   0, 100, 0, 0, 0.1f, " %", "" },
        { "v_plosive",  "Plosive Guard",       ParamKind::Float,   0, 100, 0, 0, 0.1f, " %", "" },
        { "v_sib",      "Sibilance Guard",     ParamKind::Float,   0, 100, 0, 0, 0.1f, " %", "" },
        { "v_syl",      "Syllable Aware",      ParamKind::Float,   0, 100, 0, 0, 0.1f, " %", "" },
        { "v_phrase",   "Phrase Aware",        ParamKind::Float,   0, 100, 0, 0, 0.1f, " %", "" },
        { "v_adapt",    "Adaptive",            ParamKind::Float,   0, 100, 0, 0, 0.1f, " %", "" },
    } };
    return defs;
}

// DSP が使う物理単位のパラメーター
struct Params
{
    bool bypass = false;
    double inGainDb = 0, outGainDb = 0, makeupDb = 0, mix = 1.0;
    double thresholdDb = -20, ratio = 3, attackMs = 10, releaseMs = 100, kneeDb = 6;
    Model model = Model::Transparent;
    Topology topology = Topology::FeedForward;
    double blend = 0.5;                 // 0 = ピーク, 1 = RMS
    bool autoRelease = true, autoMakeup = false;
    double stereoLink = 1.0, lookaheadMs = 0;
    int oversampling = 1;               // 1, 2, 4
    Weighting weighting = Weighting::Off;
    ScSource scSource = ScSource::Internal;
    ScMode scMode = ScMode::Stereo;
    bool scMonitor = false;
    double scHpfHz = 20, scLpfHz = 20000, scGainDb = 0;
    double eqLowHz = 120, eqLowDb = 0, eqMidHz = 2000, eqMidDb = 0, eqHighHz = 8000, eqHighDb = 0;
    double vLevel = 0, vPlosive = 0, vSib = 0, vSyl = 0, vPhrase = 0, vAdapt = 0;
};

inline Params toParams (const float* v)
{
    Params p;
    p.bypass      = v[P_bypass] > 0.5f;
    p.inGainDb    = v[P_inGain];
    p.outGainDb   = v[P_outGain];
    p.makeupDb    = v[P_makeup];
    p.mix         = v[P_mix] * 0.01;
    p.thresholdDb = v[P_threshold];
    p.ratio       = v[P_ratio];
    p.attackMs    = v[P_attack];
    p.releaseMs   = v[P_release];
    p.kneeDb      = v[P_knee];
    p.model       = (Model) std::clamp ((int) (v[P_model] + 0.5f), 0, 4);
    p.topology    = (Topology) std::clamp ((int) (v[P_topology] + 0.5f), 0, 1);
    p.blend       = v[P_blend] * 0.01;
    p.autoRelease = v[P_autoRelease] > 0.5f;
    p.autoMakeup  = v[P_autoMakeup] > 0.5f;
    p.stereoLink  = v[P_link] * 0.01;
    p.lookaheadMs = v[P_lookahead];
    p.oversampling = 1 << std::clamp ((int) (v[P_os] + 0.5f), 0, 2);
    p.weighting   = (Weighting) std::clamp ((int) (v[P_weighting] + 0.5f), 0, 2);
    p.scSource    = (ScSource) std::clamp ((int) (v[P_scSource] + 0.5f), 0, 1);
    p.scMode      = (ScMode) std::clamp ((int) (v[P_scMode] + 0.5f), 0, 1);
    p.scMonitor   = v[P_scMonitor] > 0.5f;
    p.scHpfHz     = v[P_scHpf];
    p.scLpfHz     = v[P_scLpf];
    p.scGainDb    = v[P_scGain];
    p.eqLowHz = v[P_eqLf];  p.eqLowDb = v[P_eqLg];
    p.eqMidHz = v[P_eqMf];  p.eqMidDb = v[P_eqMg];
    p.eqHighHz = v[P_eqHf]; p.eqHighDb = v[P_eqHg];
    p.vLevel   = v[P_vLevel]   * 0.01;
    p.vPlosive = v[P_vPlosive] * 0.01;
    p.vSib     = v[P_vSib]     * 0.01;
    p.vSyl     = v[P_vSyl]     * 0.01;
    p.vPhrase  = v[P_vPhrase]  * 0.01;
    p.vAdapt   = v[P_vAdapt]   * 0.01;
    return p;
}

inline std::array<float, P_COUNT> defaultValues()
{
    std::array<float, P_COUNT> v {};
    for (int i = 0; i < P_COUNT; ++i)
        v[(size_t) i] = paramDefs()[(size_t) i].def;
    return v;
}

} // namespace vc
