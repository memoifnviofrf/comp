#include "PluginEditor.h"
#include "CompressorDSP.h"

using namespace vcgui;

namespace
{
juce::Font fontOf (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

float dbToX (float db, juce::Rectangle<float> r, float lo = -60.0f, float hi = 0.0f)
{
    return r.getX() + r.getWidth() * (juce::jlimit (lo, hi, db) - lo) / (hi - lo);
}

float dbToY (float db, juce::Rectangle<float> r, float lo = -60.0f, float hi = 0.0f)
{
    return r.getBottom() - r.getHeight() * (juce::jlimit (lo, hi, db) - lo) / (hi - lo);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (col::dim);
    g.setFont (fontOf (11.0f, true));
    g.drawText (title, r.reduced (8.0f, 4.0f).removeFromTop (14.0f), juce::Justification::centredLeft);
}
}

//==============================================================================
VcLookAndFeel::VcLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxBackgroundColourId, col::panel2);
    setColour (juce::Slider::textBoxOutlineColourId, col::line);
    setColour (juce::Slider::textBoxHighlightColourId, col::accent.withAlpha (0.4f));
    setColour (juce::ComboBox::backgroundColourId, col::panel2);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::arrowColourId, col::dim);
    setColour (juce::PopupMenu::backgroundColourId, col::panel2);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, col::text);
    setColour (juce::TextButton::buttonColourId, col::panel2);
    setColour (juce::TextButton::buttonOnColourId, col::accent.withAlpha (0.55f));
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, col::text);
    setColour (juce::TooltipWindow::backgroundColourId, col::panel2);
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::text);
    setColour (juce::TextEditor::backgroundColourId, col::panel2);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::outlineColourId, col::line);
}

void VcLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    if (radius < 6.0f) return;
    const auto c = bounds.getCentre();
    const float trackR = radius - 3.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto colour = s.findColour (juce::Slider::rotarySliderFillColourId);

    g.setColour (col::panel2);
    g.fillEllipse (c.x - radius + 5.0f, c.y - radius + 5.0f, (radius - 5.0f) * 2.0f, (radius - 5.0f) * 2.0f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, startAngle, endAngle, true);
    g.setColour (col::line);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, startAngle, angle, true);
    g.setColour (s.isEnabled() ? colour : col::dim);
    g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = radius - 12.0f;
    g.setColour (col::text);
    g.drawLine (c.x + std::sin (angle) * inner * 0.35f, c.y - std::cos (angle) * inner * 0.35f,
                c.x + std::sin (angle) * inner,         c.y - std::cos (angle) * inner, 2.0f);
}

void VcLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hl, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? col::accent.withAlpha (0.28f) : col::panel2);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? col::accent : (hl ? col::dim : col::line));
    g.drawRoundedRectangle (r, 5.0f, 1.2f);
    g.setColour (on ? col::text : col::dim);
    g.setFont (fontOf (12.0f, on));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void VcLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool hl, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (down ? bg.brighter (0.25f) : (hl ? bg.brighter (0.12f) : bg));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& caption, juce::Colour colour)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 18);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setTooltip (caption + " (double-click: default, click the number to type a value)");
    addAndMakeVisible (slider);
    label.setText (caption, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (fontOf (12.0f));
    label.setColour (juce::Label::textColourId, col::dim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

Choice::Choice (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& caption)
{
    label.setText (caption, juce::dontSendNotification);
    label.setFont (fontOf (11.0f));
    label.setColour (juce::Label::textColourId, col::dim);
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    box.setBounds (r.reduced (0, 1));
}

Toggle::Toggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& caption)
{
    setButtonText (caption);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, id, *this);
}

//==============================================================================
void MeterBar::setValues (float p, float r, float h)
{
    if (std::abs (p - peak) > 0.05f || std::abs (r - rms) > 0.05f || std::abs (h - hold) > 0.05f)
    {
        peak = p; rms = r; hold = h;
        repaint();
    }
}

void MeterBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::dim);
    g.setFont (fontOf (11.0f, true));
    g.drawText (title, r.removeFromTop (14.0f), juce::Justification::centred);
    r.removeFromBottom (2.0f);

    const float labelW = 20.0f;
    auto scale = r.removeFromLeft (labelW);
    auto bar = r.reduced (2.0f, 0.0f);

    g.setColour (col::panel2);
    g.fillRoundedRectangle (bar, 3.0f);

    g.setFont (fontOf (9.0f));
    if (mode == Mode::Level)
    {
        for (int db = 0; db >= -60; db -= 12)
        {
            const float y = dbToY ((float) db, bar);
            g.setColour (col::line);
            g.drawHorizontalLine ((int) y, bar.getX(), bar.getRight());
            g.setColour (col::dim);
            g.drawText (juce::String (db), juce::Rectangle<float> (scale.getX(), y - 6.0f, labelW - 2.0f, 12.0f), juce::Justification::centredRight);
        }
        auto fill = [&] (float db, juce::Colour c)
        {
            const float y = dbToY (db, bar);
            g.setColour (c);
            g.fillRect (juce::Rectangle<float> (bar.getX() + 1.0f, y, bar.getWidth() - 2.0f, bar.getBottom() - y));
        };
        fill (peak, (peak > -1.0f ? col::hot : (peak > -9.0f ? col::warn : col::good)).withAlpha (0.45f));
        fill (rms, col::good.withAlpha (0.9f));
        const float y = dbToY (hold, bar);
        g.setColour (hold > -1.0f ? col::hot : col::text);
        g.fillRect (juce::Rectangle<float> (bar.getX() + 1.0f, y - 1.0f, bar.getWidth() - 2.0f, 2.0f));
    }
    else
    {
        for (int db = 0; db <= 24; db += 6)
        {
            const float y = bar.getY() + bar.getHeight() * (float) db / 24.0f;
            g.setColour (col::line);
            g.drawHorizontalLine ((int) y, bar.getX(), bar.getRight());
            g.setColour (col::dim);
            g.drawText (juce::String (db), juce::Rectangle<float> (scale.getX(), y - 6.0f, labelW - 2.0f, 12.0f), juce::Justification::centredRight);
        }
        const float gr = juce::jlimit (0.0f, 24.0f, peak);   // peak に GR(正の dB)を入れて使う
        g.setColour (col::gr);
        g.fillRect (juce::Rectangle<float> (bar.getX() + 1.0f, bar.getY(), bar.getWidth() - 2.0f, bar.getHeight() * gr / 24.0f));
        const float y = bar.getY() + bar.getHeight() * juce::jlimit (0.0f, 24.0f, hold) / 24.0f;
        g.setColour (col::text);
        g.fillRect (juce::Rectangle<float> (bar.getX() + 1.0f, y - 1.0f, bar.getWidth() - 2.0f, 2.0f));
    }
}

//==============================================================================
void CurveDisplay::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    drawPanel (g, full, "COMPRESSION CURVE (after Input Gain)");
    auto r = full.reduced (22.0f, 22.0f).withTrimmedBottom (4.0f);
    r = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight()));
    if (! getParams) return;
    const vc::Params p = getParams();

    g.setColour (col::bg);
    g.fillRect (r);
    for (int db = -60; db <= 0; db += 12)
    {
        g.setColour (col::line);
        g.drawVerticalLine ((int) dbToX ((float) db, r), r.getY(), r.getBottom());
        g.drawHorizontalLine ((int) dbToY ((float) db, r), r.getX(), r.getRight());
    }
    g.setColour (col::line.brighter (0.3f));
    g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.0f);            // 1:1

    const double makeup = p.autoMakeup ? vc::autoMakeupDb (p) : p.makeupDb;
    juce::Path curvePath;
    for (int i = 0; i <= 120; ++i)
    {
        const double in = -60.0 + 60.0 * i / 120.0;
        const double out = in - vc::computeGainReduction (in, p.thresholdDb, p.ratio, p.kneeDb, p.model) + makeup;
        const float x = dbToX ((float) in, r), y = dbToY ((float) out, r);
        if (i == 0) curvePath.startNewSubPath (x, y); else curvePath.lineTo (x, y);
    }
    // しきい値
    const float tx = dbToX ((float) p.thresholdDb, r);
    g.setColour (col::warn.withAlpha (0.55f));
    g.drawVerticalLine ((int) tx, r.getY(), r.getBottom());
    g.setFont (fontOf (10.0f));
    g.drawText (juce::String (p.thresholdDb, 1) + " dB", juce::Rectangle<float> (tx + 3.0f, r.getY() + 2.0f, 60.0f, 12.0f), juce::Justification::centredLeft);

    g.setColour (col::accent);
    g.strokePath (curvePath, juce::PathStrokeType (2.2f));

    // 現在の動作点
    if (liveInDb > -59.0f)
    {
        const float outDb = liveInDb - liveGrDb + (float) makeup;
        g.setColour (col::gr);
        g.fillEllipse (dbToX (liveInDb, r) - 4.0f, dbToY (outDb, r) - 4.0f, 8.0f, 8.0f);
    }
    g.setColour (col::dim);
    g.setFont (fontOf (9.0f));
    g.drawText ("in", juce::Rectangle<float> (r.getRight() - 30.0f, r.getBottom() + 1.0f, 30.0f, 10.0f), juce::Justification::centredRight);
    g.drawText ("out", juce::Rectangle<float> (r.getX() - 20.0f, r.getY() - 12.0f, 30.0f, 10.0f), juce::Justification::centredLeft);
}

//==============================================================================
void BallisticsDisplay::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    drawPanel (g, full, "ATTACK / RELEASE (ideal 1-pole)");
    auto r = full.reduced (14.0f, 24.0f).withTrimmedBottom (6.0f);
    if (! getParams) return;
    const vc::Params p = getParams();

    g.setColour (col::bg);
    g.fillRect (r);

    const double A = p.attackMs, R = p.releaseMs;
    const double tA = 4.0 * A, tR = 4.0 * R, total = tA + tR;
    auto X = [&] (double t) { return r.getX() + r.getWidth() * (float) (t / total); };
    auto Y = [&] (double v) { return r.getBottom() - 6.0f - (r.getHeight() - 14.0f) * (float) v; };

    // 入力ステップ(圧縮が必要なレベルが tA の間だけ入る、という想定)
    g.setColour (col::dim.withAlpha (0.6f));
    juce::Path step;
    step.startNewSubPath (r.getX(), Y (0.0));
    step.lineTo (X (0.0), Y (0.0));
    step.lineTo (X (0.0), Y (1.0));
    step.lineTo (X (tA), Y (1.0));
    step.lineTo (X (tA), Y (0.0));
    step.lineTo (r.getRight(), Y (0.0));
    g.strokePath (step, juce::PathStrokeType (1.0f, juce::PathStrokeType::mitered));

    juce::Path resp;
    resp.startNewSubPath (X (0.0), Y (0.0));
    const int N = 160;
    for (int i = 1; i <= N; ++i)
    {
        const double t = total * i / N;
        const double v = t <= tA ? 1.0 - std::exp (-t / A)
                                 : (1.0 - std::exp (-tA / A)) * std::exp (-(t - tA) / R);
        resp.lineTo (X (t), Y (v));
    }
    g.setColour (col::gr);
    g.strokePath (resp, juce::PathStrokeType (2.2f));

    // 63% / 37% のマーカー
    g.setColour (col::accent.withAlpha (0.8f));
    g.fillEllipse (X (A) - 3.0f, Y (1.0 - std::exp (-1.0)) - 3.0f, 6.0f, 6.0f);
    g.fillEllipse (X (tA + R) - 3.0f, Y ((1.0 - std::exp (-4.0)) * std::exp (-1.0)) - 3.0f, 6.0f, 6.0f);
    g.setColour (col::text);
    g.setFont (fontOf (11.0f));
    g.drawText ("Attack " + juce::String (p.attackMs, 1) + " ms", r.removeFromTop (14.0f).reduced (4.0f, 0.0f), juce::Justification::centredLeft);
    g.drawText ("Release " + juce::String (p.releaseMs, 0) + " ms" + (p.autoRelease ? " (auto)" : ""),
                juce::Rectangle<float> (r.getX() + 4.0f, r.getY(), r.getWidth() - 8.0f, 14.0f), juce::Justification::centredRight);
}

//==============================================================================
void HistoryDisplay::push (float in, float g)
{
    inDb[(size_t) pos] = in;
    gr[(size_t) pos] = g;
    pos = (pos + 1) % kPoints;
    repaint();
}

void HistoryDisplay::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    drawPanel (g, full, "LEVEL / GAIN REDUCTION (8 s)");
    auto r = full.reduced (12.0f, 24.0f).withTrimmedBottom (6.0f);
    g.setColour (col::bg);
    g.fillRect (r);

    juce::Path levelPath, grPath;
    for (int i = 0; i < kPoints; ++i)
    {
        const size_t idx = (size_t) ((pos + i) % kPoints);
        const float x = r.getX() + r.getWidth() * (float) i / (float) (kPoints - 1);
        const float yl = dbToY (inDb[idx], r);
        const float yg = r.getY() + r.getHeight() * juce::jlimit (0.0f, 24.0f, gr[idx]) / 24.0f;
        if (i == 0) { levelPath.startNewSubPath (x, yl); grPath.startNewSubPath (x, r.getY()); }
        else levelPath.lineTo (x, yl);
        grPath.lineTo (x, yg);
    }
    grPath.lineTo (r.getRight(), r.getY());
    grPath.closeSubPath();
    g.setColour (col::gr.withAlpha (0.55f));
    g.fillPath (grPath);
    g.setColour (col::good);
    g.strokePath (levelPath, juce::PathStrokeType (1.5f));

    if (getParams)
    {
        const float y = dbToY ((float) getParams().thresholdDb, r);
        g.setColour (col::warn.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
    }
}

//==============================================================================
VocalCompressorEditor::VocalCompressorEditor (VocalCompressorProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      osChoice (p.apvts, "os", "Oversampling"),
      bypassToggle (p.apvts, "bypass", "Bypass"),
      kThreshold (p.apvts, "threshold", "Threshold"), kRatio (p.apvts, "ratio", "Ratio"),
      kAttack (p.apvts, "attack", "Attack"), kRelease (p.apvts, "release", "Release"),
      kKnee (p.apvts, "knee", "Knee"), kMakeup (p.apvts, "makeup", "Makeup", vcgui::col::good),
      meterIn (MeterBar::Mode::Level, "IN"), meterOut (MeterBar::Mode::Level, "OUT"),
      meterGr (MeterBar::Mode::GainReduction, "GR"),
      kIn (p.apvts, "in_gain", "Input", vcgui::col::good), kOut (p.apvts, "out_gain", "Output", vcgui::col::good),
      cModel (p.apvts, "model", "Model"), cTopology (p.apvts, "topology", "Topology"),
      tAutoRelease (p.apvts, "auto_rel", "Auto Release"), tAutoMakeup (p.apvts, "auto_mk", "Auto Makeup"),
      kBlend (p.apvts, "blend", "Peak <> RMS"), kLink (p.apvts, "link", "Stereo Link"),
      kMix (p.apvts, "mix", "Mix"), kLookahead (p.apvts, "lookahead", "Lookahead"),
      cScSource (p.apvts, "sc_source", "SC Source"), cScMode (p.apvts, "sc_mode", "SC Channels"),
      cWeighting (p.apvts, "weighting", "Detector Weighting"), tScMonitor (p.apvts, "sc_monitor", "SC Monitor"),
      kScHpf (p.apvts, "sc_hpf", "SC HPF"), kScLpf (p.apvts, "sc_lpf", "SC LPF"), kScGain (p.apvts, "sc_gain", "SC Gain"),
      kEqLf (p.apvts, "eq_lf", "Low Freq"), kEqLg (p.apvts, "eq_lg", "Low Gain"),
      kEqMf (p.apvts, "eq_mf", "Mid Freq"), kEqMg (p.apvts, "eq_mg", "Mid Gain"),
      kEqHf (p.apvts, "eq_hf", "High Freq"), kEqHg (p.apvts, "eq_hg", "High Gain"),
      kvLevel (p.apvts, "v_level", "Leveling"), kvPlosive (p.apvts, "v_plosive", "Plosive"),
      kvSib (p.apvts, "v_sib", "Sibilance"), kvSyl (p.apvts, "v_syl", "Syllable"),
      kvPhrase (p.apvts, "v_phrase", "Phrase"), kvAdapt (p.apvts, "v_adapt", "Adaptive")
{
    setLookAndFeel (&laf);

    titleLabel.setText ("VOCAL COMPRESSOR", juce::dontSendNotification);
    titleLabel.setFont (fontOf (17.0f, true));
    titleLabel.setColour (juce::Label::textColourId, vcgui::col::accent);
    addAndMakeVisible (titleLabel);

    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty() && ! proc.loadPreset (name))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset", "Could not load preset: " + name);
    };
    addAndMakeVisible (presetBox);

    for (auto* b : { &saveButton, &renameButton, &deleteButton, &abButton, &copyButton, &undoButton, &redoButton, &settingsButton })
        addAndMakeVisible (b);
    saveButton.onClick   = [this] { promptSavePreset(); };
    renameButton.onClick = [this] { promptRenamePreset(); };
    deleteButton.onClick = [this] { confirmDeletePreset(); };
    abButton.onClick     = [this] { proc.toggleAB(); abButton.setButtonText (proc.getActiveSlot() == 0 ? "A" : "B"); };
    copyButton.onClick   = [this] { proc.copyActiveToOther(); };
    undoButton.onClick   = [this] { proc.undoManager.undo(); };
    redoButton.onClick   = [this] { proc.undoManager.redo(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };
    abButton.setButtonText (proc.getActiveSlot() == 0 ? "A" : "B");
    abButton.setTooltip ("A/B: switch between two settings");
    copyButton.setTooltip ("Copy the current setting to the other slot");

    addAndMakeVisible (osChoice);
    addAndMakeVisible (bypassToggle);

    addAndMakeVisible (curve);
    addAndMakeVisible (ballistics);
    addAndMakeVisible (history);
    for (auto* d : { (std::function<vc::Params()>*) &curve.getParams, &ballistics.getParams, &history.getParams })
        *d = [this] { return currentParams(); };

    for (auto* k : { &kThreshold, &kRatio, &kAttack, &kRelease, &kKnee, &kMakeup, &kIn, &kOut })
        addAndMakeVisible (k);
    addAndMakeVisible (meterIn);
    addAndMakeVisible (meterOut);
    addAndMakeVisible (meterGr);

    // 下部タブ
    const char* tabNames[3] = { "Compressor", "Sidechain", "Vocal" };
    for (int i = 0; i < 3; ++i)
    {
        tabButtons[(size_t) i].setButtonText (tabNames[i]);
        tabButtons[(size_t) i].setClickingTogglesState (true);
        tabButtons[(size_t) i].setRadioGroupId (77);
        tabButtons[(size_t) i].onClick = [this, i] { showTab (i); };
        addAndMakeVisible (tabButtons[(size_t) i]);
        addChildComponent (tabPanels[(size_t) i]);
    }
    auto& t0 = tabPanels[0]; auto& t1 = tabPanels[1]; auto& t2 = tabPanels[2];
    for (auto* c : std::initializer_list<juce::Component*> { &cModel, &cTopology, &tAutoRelease, &tAutoMakeup, &kBlend, &kLink, &kMix, &kLookahead })
        t0.addAndMakeVisible (c);
    for (auto* c : std::initializer_list<juce::Component*> { &cScSource, &cScMode, &cWeighting, &tScMonitor, &kScHpf, &kScLpf, &kScGain,
                                                              &kEqLf, &kEqLg, &kEqMf, &kEqMg, &kEqHf, &kEqHg })
        t1.addAndMakeVisible (c);
    for (auto* c : std::initializer_list<juce::Component*> { &kvLevel, &kvPlosive, &kvSib, &kvSyl, &kvPhrase, &kvAdapt })
        t2.addAndMakeVisible (c);
    vocalNote.setText ("Vocal-specific detection. 0 % = off. Leveling: slow gain rider / Plosive: ignores low-frequency bursts / "
                       "Sibilance: ignores high-frequency energy / Syllable: protects onsets, recovers in gaps / "
                       "Phrase: slower release while singing, faster in rests / Adaptive: tracks phrase level and crest factor.",
                       juce::dontSendNotification);
    vocalNote.setFont (fontOf (11.0f));
    vocalNote.setColour (juce::Label::textColourId, vcgui::col::dim);
    vocalNote.setJustificationType (juce::Justification::topLeft);
    t2.addAndMakeVisible (vocalNote);
    tabButtons[0].setToggleState (true, juce::dontSendNotification);
    showTab (0);

    latencyLabel.setFont (fontOf (11.0f));
    latencyLabel.setColour (juce::Label::textColourId, vcgui::col::dim);
    latencyLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (latencyLabel);

    refreshPresetMenu();

    constrainer.setMinimumSize (900, 600);
    constrainer.setMaximumSize (2400, 1600);
    constrainer.setFixedAspectRatio (900.0 / 600.0);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (juce::jlimit (900, 2400, proc.getUiWidth()), juce::jlimit (600, 1600, proc.getUiHeight()));

    startTimerHz (30);
}

VocalCompressorEditor::~VocalCompressorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

vc::Params VocalCompressorEditor::currentParams() const
{
    const auto v = proc.getCurrentValues();
    return vc::toParams (v.data());
}

void VocalCompressorEditor::showTab (int index)
{
    currentTab = index;
    for (int i = 0; i < 3; ++i)
        tabPanels[(size_t) i].setVisible (i == index);
}

void VocalCompressorEditor::paint (juce::Graphics& g)
{
    g.fillAll (vcgui::col::bg);
}

void VocalCompressorEditor::resized()
{
    proc.setUiSize (getWidth(), getHeight());
    auto area = getLocalBounds().reduced (10);

    // --- 上部 ---
    auto top = area.removeFromTop (40);
    titleLabel.setBounds (top.removeFromLeft (170));
    presetBox.setBounds (top.removeFromLeft (190).withTrimmedTop (8).withTrimmedBottom (4));
    top.removeFromLeft (6);
    auto btn = [&] (juce::Component& c, int w) { c.setBounds (top.removeFromLeft (w).withTrimmedTop (8).withTrimmedBottom (4)); top.removeFromLeft (4); };
    btn (saveButton, 50); btn (renameButton, 62); btn (deleteButton, 56);
    top.removeFromLeft (8);
    btn (abButton, 34); btn (copyButton, 42);
    top.removeFromLeft (8);
    btn (undoButton, 50); btn (redoButton, 50);
    top.removeFromLeft (8);
    btn (settingsButton, 68);
    bypassToggle.setBounds (top.removeFromRight (70).withTrimmedTop (8).withTrimmedBottom (4));
    top.removeFromRight (6);
    osChoice.setBounds (top.removeFromRight (84).withTrimmedTop (1));

    area.removeFromTop (6);

    // --- 下部(タブ) ---
    auto bottom = area.removeFromBottom (area.getHeight() * 28 / 100);
    auto tabBar = bottom.removeFromTop (26);
    for (int i = 0; i < 3; ++i)
        tabButtons[(size_t) i].setBounds (tabBar.removeFromLeft (110).reduced (2, 1));
    latencyLabel.setBounds (tabBar);
    for (auto& p : tabPanels) p.setBounds (bottom);

    area.removeFromBottom (6);

    // --- 右(メーター + 入出力ノブ) ---
    auto right = area.removeFromRight (170);
    area.removeFromRight (8);
    auto knobsIO = right.removeFromBottom (right.getHeight() * 38 / 100);
    kIn.setBounds (knobsIO.removeFromLeft (knobsIO.getWidth() / 2));
    kOut.setBounds (knobsIO);
    right.removeFromBottom (4);
    const int mw = right.getWidth() / 3;
    meterIn.setBounds (right.removeFromLeft (mw));
    meterGr.setBounds (right.removeFromLeft (mw));
    meterOut.setBounds (right);

    // --- 中央 ---
    auto displays = area.removeFromTop (area.getHeight() * 52 / 100);
    area.removeFromTop (6);
    const int dw = (displays.getWidth() - 12) / 3;
    curve.setBounds (displays.removeFromLeft (dw));
    displays.removeFromLeft (6);
    ballistics.setBounds (displays.removeFromLeft (dw));
    displays.removeFromLeft (6);
    history.setBounds (displays);

    const int kw = area.getWidth() / 6;
    for (auto* k : { &kThreshold, &kRatio, &kAttack, &kRelease, &kKnee, &kMakeup })
        k->setBounds (area.removeFromLeft (kw));

    // --- タブ内 ---
    auto layoutRow = [] (juce::Rectangle<int> row, std::initializer_list<juce::Component*> items, int w, int gap)
    {
        for (auto* c : items) { c->setBounds (row.removeFromLeft (w)); row.removeFromLeft (gap); }
    };
    {   // Compressor
        auto r = tabPanels[0].getLocalBounds();
        auto strip = r.removeFromTop (40);
        cModel.setBounds (strip.removeFromLeft (170)); strip.removeFromLeft (10);
        cTopology.setBounds (strip.removeFromLeft (150)); strip.removeFromLeft (14);
        tAutoRelease.setBounds (strip.removeFromLeft (110).withTrimmedTop (14)); strip.removeFromLeft (8);
        tAutoMakeup.setBounds (strip.removeFromLeft (110).withTrimmedTop (14));
        const int w = juce::jmin (120, r.getWidth() / 4);
        layoutRow (r, { &kBlend, &kLink, &kMix, &kLookahead }, w, 0);
    }
    {   // Sidechain
        auto r = tabPanels[1].getLocalBounds();
        auto strip = r.removeFromTop (40);
        cScSource.setBounds (strip.removeFromLeft (130)); strip.removeFromLeft (10);
        cScMode.setBounds (strip.removeFromLeft (110)); strip.removeFromLeft (10);
        cWeighting.setBounds (strip.removeFromLeft (140)); strip.removeFromLeft (14);
        tScMonitor.setBounds (strip.removeFromLeft (110).withTrimmedTop (14));
        const int w = r.getWidth() / 9;
        layoutRow (r, { &kScHpf, &kScLpf, &kScGain, &kEqLf, &kEqLg, &kEqMf, &kEqMg, &kEqHf, &kEqHg }, w, 0);
    }
    {   // Vocal
        auto r = tabPanels[2].getLocalBounds();
        vocalNote.setBounds (r.removeFromTop (44));
        const int w = r.getWidth() / 6;
        layoutRow (r, { &kvLevel, &kvPlosive, &kvSib, &kvSyl, &kvPhrase, &kvAdapt }, w, 0);
    }
}

//==============================================================================
void VocalCompressorEditor::timerCallback()
{
    auto& m = proc.getMeters();
    const float dt = 1.0f / 30.0f;
    const float inPk = m.inPeakDb.load(), inRms = m.inRmsDb.load();
    const float outPk = m.outPeakDb.load(), outRms = m.outRmsDb.load();
    const float gr = m.grDb.load();

    balIn.update (inPk, inRms, dt);
    balOut.update (outPk, outRms, dt);
    balGr.update (gr, gr, dt);
    meterIn.setValues (balIn.getPeak(), balIn.getRms(), balIn.getPeakHold());
    meterOut.setValues (balOut.getPeak(), balOut.getRms(), balOut.getPeakHold());
    meterGr.setValues (gr, gr, balGr.getPeakHold());

    curve.liveInDb = inPk;   // メーターは Input Gain 後のレベル = 曲線の横軸と同じ基準
    curve.liveGrDb = gr;
    curve.repaint();
    ballistics.repaint();
    history.push (inPk, gr);

    const int lat = proc.getReportedLatency();
    const double sr = proc.getCurrentSampleRate();
    latencyLabel.setText ("Latency: " + juce::String (lat) + " samples (" + juce::String (sr > 0 ? 1000.0 * lat / sr : 0.0, 2) + " ms)",
                          juce::dontSendNotification);

    undoButton.setEnabled (proc.undoManager.canUndo());
    redoButton.setEnabled (proc.undoManager.canRedo());

    const auto cur = proc.getCurrentPresetName();
    if (presetBox.getText() != cur && cur.isNotEmpty() && ! presetBox.isPopupActive())
        refreshPresetMenu();
    const bool user = cur.isNotEmpty() && ! proc.getPresetManager().isFactory (cur);
    renameButton.setEnabled (user);
    deleteButton.setEnabled (user);
}

//==============================================================================
void VocalCompressorEditor::refreshPresetMenu()
{
    auto& pm = proc.getPresetManager();
    presetBox.clear (juce::dontSendNotification);
    int id = 1;
    presetBox.addSectionHeading ("Factory");
    for (const auto& n : pm.getFactoryNames()) presetBox.addItem (n, id++);
    const auto users = pm.getUserNames();
    if (users.size() > 0)
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("User");
        for (const auto& n : users) presetBox.addItem (n, id++);
    }
    const auto cur = proc.getCurrentPresetName();
    for (int i = 0; i < presetBox.getNumItems(); ++i)
        if (presetBox.getItemText (i) == cur) { presetBox.setSelectedItemIndex (i, juce::dontSendNotification); return; }
    presetBox.setText (cur.isEmpty() ? "Custom" : cur, juce::dontSendNotification);
}

void VocalCompressorEditor::promptSavePreset()
{
    auto* aw = new juce::AlertWindow ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", proc.getCurrentPresetName(), "");
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<VocalCompressorEditor> safe (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        const auto name = aw->getTextEditorContents ("name");
        auto& pm = safe->proc.getPresetManager();
        const auto res = pm.save (name, safe->proc.getCurrentValues());
        if (res.failed())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset", res.getErrorMessage());
            return;
        }
        safe->proc.setCurrentPresetName (vc::PresetManager::sanitise (name));
        safe->refreshPresetMenu();
    }), true);
}

void VocalCompressorEditor::promptRenamePreset()
{
    const auto old = proc.getCurrentPresetName();
    if (old.isEmpty() || proc.getPresetManager().isFactory (old)) return;
    auto* aw = new juce::AlertWindow ("Rename preset", "New name:", juce::MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", old, "");
    aw->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<VocalCompressorEditor> safe (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw, old] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        const auto name = aw->getTextEditorContents ("name");
        const auto res = safe->proc.getPresetManager().rename (old, name);
        if (res.failed())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Rename preset", res.getErrorMessage());
            return;
        }
        safe->proc.setCurrentPresetName (vc::PresetManager::sanitise (name));
        safe->refreshPresetMenu();
    }), true);
}

void VocalCompressorEditor::confirmDeletePreset()
{
    const auto name = proc.getCurrentPresetName();
    if (name.isEmpty() || proc.getPresetManager().isFactory (name)) return;
    juce::Component::SafePointer<VocalCompressorEditor> safe (this);
    juce::AlertWindow::showAsync (
        juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon, "Delete preset",
                                                      "Delete the preset \"" + name + "\"?", "Delete", "Cancel", this),
        [safe, name] (int result)
        {
            if (result == 0 || safe == nullptr) return;
            const auto res = safe->proc.getPresetManager().remove (name);
            if (res.failed())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Delete preset", res.getErrorMessage());
            safe->proc.setCurrentPresetName ({});
            safe->refreshPresetMenu();
        });
}

void VocalCompressorEditor::showSettingsMenu()
{
    juce::PopupMenu m;
    juce::PopupMenu sizes;
    const int pct[4] = { 100, 125, 150, 200 };
    for (int i = 0; i < 4; ++i)
        sizes.addItem (10 + i, juce::String (pct[i]) + " %", true, std::abs (getWidth() - 900 * pct[i] / 100) < 8);
    m.addSubMenu ("Window size", sizes);
    m.addSeparator();
    m.addItem (20, "Reset all parameters to default");
    m.addItem (21, "Open user preset folder");
    m.addSeparator();
    m.addItem (30, "About");
    juce::Component::SafePointer<VocalCompressorEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton), [safe, pct] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r >= 10 && r < 14)
            safe->setSize (900 * pct[r - 10] / 100, 600 * pct[r - 10] / 100);
        else if (r == 20)
        {
            safe->proc.resetToDefaults();
            safe->refreshPresetMenu();
        }
        else if (r == 21)
        {
            const auto d = safe->proc.getPresetManager().getUserDir();
            d.createDirectory();
            d.revealToUser();
        }
        else if (r == 30)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Vocal Compressor",
                "Vocal Compressor 1.0.0 (VST3)\nBuilt with JUCE.\n\nSee README / docs for the manual and known issues.");
    });
}
