#include "PresetManager.h"

namespace vc
{
namespace
{
struct Ov { ParamIndex idx; float value; };

FactoryPreset make (const char* name, std::initializer_list<Ov> overrides)
{
    FactoryPreset p { name, defaultValues() };
    for (const auto& o : overrides)
        p.values[(size_t) o.idx] = o.value;
    return p;
}

enum { Transparent = 0, Opto, FET, VCA, VariMu };
}

const std::vector<FactoryPreset>& factoryPresets()
{
    // 値は「実用的な出発点」です。実際の歌声での最終調整はお使いの素材に合わせてください。
    static const std::vector<FactoryPreset> presets = {
        make ("Vocal Natural",     { {P_model, Transparent}, {P_threshold, -22}, {P_ratio, 2.5f}, {P_attack, 15}, {P_release, 150}, {P_knee, 9},
                                     {P_blend, 50}, {P_autoRelease, 1}, {P_autoMakeup, 1} }),
        make ("Vocal Smooth",      { {P_model, Opto}, {P_threshold, -24}, {P_ratio, 3}, {P_attack, 20}, {P_release, 250}, {P_knee, 12},
                                     {P_blend, 60}, {P_autoRelease, 1}, {P_autoMakeup, 1} }),
        make ("Vocal Pop",         { {P_model, VCA}, {P_threshold, -26}, {P_ratio, 4}, {P_attack, 8}, {P_release, 120}, {P_knee, 6},
                                     {P_blend, 40}, {P_autoRelease, 1}, {P_autoMakeup, 1}, {P_vSib, 40} }),
        make ("Vocal Aggressive",  { {P_model, FET}, {P_threshold, -30}, {P_ratio, 8}, {P_attack, 1}, {P_release, 80}, {P_knee, 2},
                                     {P_blend, 20}, {P_autoRelease, 0}, {P_autoMakeup, 1}, {P_os, 1}, {P_vPlosive, 30} }),
        make ("Vocal Soft",        { {P_model, VariMu}, {P_threshold, -20}, {P_ratio, 2}, {P_attack, 25}, {P_release, 300}, {P_knee, 15},
                                     {P_blend, 70}, {P_autoRelease, 1}, {P_autoMakeup, 1} }),
        make ("Vocal Lead",        { {P_model, VCA}, {P_threshold, -26}, {P_ratio, 3.5f}, {P_attack, 10}, {P_release, 110}, {P_knee, 6},
                                     {P_blend, 45}, {P_autoRelease, 1}, {P_autoMakeup, 1}, {P_lookahead, 1}, {P_vSib, 30}, {P_vSyl, 30} }),
        make ("Vocal Background",  { {P_model, Opto}, {P_threshold, -28}, {P_ratio, 5}, {P_attack, 5}, {P_release, 200}, {P_knee, 10},
                                     {P_blend, 60}, {P_autoRelease, 1}, {P_autoMakeup, 1}, {P_vLevel, 30} }),
        make ("Vocal Whisper",     { {P_model, Transparent}, {P_threshold, -40}, {P_ratio, 3}, {P_attack, 8}, {P_release, 120}, {P_knee, 12},
                                     {P_blend, 50}, {P_autoRelease, 1}, {P_autoMakeup, 1}, {P_weighting, 1}, {P_vPlosive, 50}, {P_vSib, 50} }),
        make ("Vocal Broadcast",   { {P_model, VCA}, {P_threshold, -28}, {P_ratio, 6}, {P_attack, 5}, {P_release, 150}, {P_knee, 6},
                                     {P_blend, 50}, {P_autoRelease, 1}, {P_autoMakeup, 1}, {P_lookahead, 2}, {P_weighting, 1}, {P_scHpf, 80},
                                     {P_vLevel, 40}, {P_vPlosive, 40} }),
        make ("Vocal Transparent", { {P_model, Transparent}, {P_threshold, -18}, {P_ratio, 1.8f}, {P_attack, 30}, {P_release, 200}, {P_knee, 12},
                                     {P_blend, 70}, {P_autoRelease, 1}, {P_autoMakeup, 1} }),
        make ("Vocal Heavy",       { {P_model, FET}, {P_threshold, -34}, {P_ratio, 12}, {P_attack, 0.8f}, {P_release, 60}, {P_knee, 3},
                                     {P_blend, 30}, {P_autoRelease, 0}, {P_autoMakeup, 1}, {P_os, 2}, {P_vPlosive, 40}, {P_vSib, 30} }),
        make ("Vocal Leveling",    { {P_model, Opto}, {P_threshold, -30}, {P_ratio, 2.5f}, {P_attack, 40}, {P_release, 500}, {P_knee, 18},
                                     {P_blend, 80}, {P_autoRelease, 1}, {P_autoMakeup, 1}, {P_vLevel, 80}, {P_vPhrase, 60}, {P_vAdapt, 50} }),
    };
    return presets;
}

//==============================================================================
PresetManager::PresetManager (juce::File userPresetDir) : dir (std::move (userPresetDir)) {}

juce::File PresetManager::defaultUserDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("VocalCompressor").getChildFile ("Presets");
}

juce::String PresetManager::sanitise (const juce::String& name)
{
    // ファイル名に使えない文字(Windows の禁止文字と制御文字)を除く。日本語などはそのまま使える。
    juce::String out;
    for (auto c : name.trim())
    {
        const auto u = (juce::juce_wchar) c;
        if (u < 32 || juce::String ("\\/:*?\"<>|").containsChar (u)) continue;
        out += juce::String::charToString (u);
    }
    out = out.trim();
    while (out.endsWithChar ('.')) out = out.dropLastCharacters (1).trim();   // Windows は末尾のドットを許さない
    if (out.length() > 64) out = out.substring (0, 64).trim();
    return out;
}

juce::File PresetManager::fileFor (const juce::String& name) const
{
    return dir.getChildFile (sanitise (name) + extension);
}

juce::StringArray PresetManager::getFactoryNames() const
{
    juce::StringArray a;
    for (const auto& p : factoryPresets()) a.add (p.name);
    return a;
}

bool PresetManager::isFactory (const juce::String& name) const
{
    for (const auto& p : factoryPresets())
        if (p.name.equalsIgnoreCase (name.trim())) return true;
    return false;
}

bool PresetManager::userPresetExists (const juce::String& name) const
{
    const auto s = sanitise (name);
    return s.isNotEmpty() && fileFor (s).existsAsFile();
}

juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray names;
    if (! dir.isDirectory()) return names;
    for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

std::unique_ptr<juce::XmlElement> PresetManager::toXml (const juce::String& name, const ParamValues& v)
{
    auto xml = std::make_unique<juce::XmlElement> ("VocalCompressorPreset");
    xml->setAttribute ("version", 1);
    xml->setAttribute ("name", name);
    for (int i = 0; i < P_COUNT; ++i)
    {
        auto* e = xml->createNewChildElement ("Param");
        e->setAttribute ("id", paramDefs()[(size_t) i].id);
        e->setAttribute ("value", (double) v[(size_t) i]);
    }
    return xml;
}

std::optional<ParamValues> PresetManager::fromXml (const juce::XmlElement& xml, juce::String* nameOut)
{
    if (! xml.hasTagName ("VocalCompressorPreset")) return std::nullopt;
    ParamValues v = defaultValues();             // 欠けているパラメーターは既定値(将来の拡張に対応)
    for (const auto* e : xml.getChildWithTagNameIterator ("Param"))
    {
        const auto id = e->getStringAttribute ("id");
        for (int i = 0; i < P_COUNT; ++i)
        {
            const auto& d = paramDefs()[(size_t) i];
            if (id == d.id)
            {
                const double x = e->getDoubleAttribute ("value", (double) d.def);
                v[(size_t) i] = std::isfinite (x) ? std::clamp ((float) x, d.min, d.max) : d.def;
            }
        }
    }
    if (nameOut != nullptr) *nameOut = xml.getStringAttribute ("name");
    return v;
}

std::optional<ParamValues> PresetManager::load (const juce::String& name) const
{
    for (const auto& p : factoryPresets())
        if (p.name.equalsIgnoreCase (name.trim())) return p.values;

    const auto f = fileFor (name);
    if (! f.existsAsFile()) return std::nullopt;
    if (auto xml = juce::XmlDocument::parse (f))
        return fromXml (*xml);
    return std::nullopt;
}

juce::Result PresetManager::save (const juce::String& name, const ParamValues& values)
{
    const auto s = sanitise (name);
    if (s.isEmpty()) return juce::Result::fail ("Invalid preset name");
    if (isFactory (s)) return juce::Result::fail ("This name is used by a factory preset");
    if (! dir.createDirectory()) return juce::Result::fail ("Cannot create preset folder: " + dir.getFullPathName());
    const auto xml = toXml (s, values);
    // 一時ファイルに書いてから置き換える(書き込み途中の破損を避ける)
    juce::TemporaryFile tmp (fileFor (s));
    if (! xml->writeTo (tmp.getFile())) return juce::Result::fail ("Cannot write preset file");
    if (! tmp.overwriteTargetFileWithTemporary()) return juce::Result::fail ("Cannot replace preset file");
    return juce::Result::ok();
}

juce::Result PresetManager::remove (const juce::String& name)
{
    if (isFactory (name)) return juce::Result::fail ("Factory presets cannot be deleted");
    const auto f = fileFor (name);
    if (! f.existsAsFile()) return juce::Result::fail ("Preset not found");
    return f.deleteFile() ? juce::Result::ok() : juce::Result::fail ("Cannot delete file");
}

juce::Result PresetManager::rename (const juce::String& oldName, const juce::String& newName)
{
    if (isFactory (oldName)) return juce::Result::fail ("Factory presets cannot be renamed");
    const auto n = sanitise (newName);
    if (n.isEmpty()) return juce::Result::fail ("Invalid preset name");
    if (isFactory (n)) return juce::Result::fail ("This name is used by a factory preset");
    const auto from = fileFor (oldName), to = fileFor (n);
    if (! from.existsAsFile()) return juce::Result::fail ("Preset not found");
    if (to.existsAsFile() && to != from) return juce::Result::fail ("A preset with this name already exists");
    auto v = load (oldName);
    if (! v) return juce::Result::fail ("Cannot read preset");
    const auto xml = toXml (n, *v);
    if (! xml->writeTo (to)) return juce::Result::fail ("Cannot write preset file");
    if (to != from) from.deleteFile();
    return juce::Result::ok();
}

juce::Result PresetManager::exportFactoryPresets (const juce::File& destDir) const
{
    if (! destDir.createDirectory()) return juce::Result::fail ("Cannot create folder");
    for (const auto& p : factoryPresets())
    {
        const auto xml = toXml (p.name, p.values);
        if (! xml->writeTo (destDir.getChildFile (sanitise (p.name) + extension)))
            return juce::Result::fail ("Cannot write " + p.name);
    }
    return juce::Result::ok();
}

} // namespace vc
