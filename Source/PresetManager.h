#pragma once

// プリセット管理(juce_core のみに依存。GUI なしでテストできます)
//   ・工場出荷プリセット 12 種(コード内に定義)
//   ・ユーザープリセット: 保存 / 読み込み / 削除 / 名前変更(XML ファイル .vcpreset)
//   ・ファイル I/O はメッセージスレッドからのみ呼ぶこと(オーディオスレッドでは呼ばない)

#include <array>
#include <optional>
#include <vector>
#include <juce_core/juce_core.h>
#include "CompressorParameters.h"

namespace vc
{
using ParamValues = std::array<float, P_COUNT>;

struct FactoryPreset
{
    juce::String name;
    ParamValues values;
};

const std::vector<FactoryPreset>& factoryPresets();

class PresetManager
{
public:
    explicit PresetManager (juce::File userPresetDir);

    static juce::File defaultUserDir();
    static constexpr const char* extension = ".vcpreset";

    const juce::File& getUserDir() const noexcept { return dir; }

    juce::StringArray getFactoryNames() const;
    juce::StringArray getUserNames() const;                    // 名前順
    bool isFactory (const juce::String& name) const;
    bool userPresetExists (const juce::String& name) const;

    std::optional<ParamValues> load (const juce::String& name) const;
    juce::Result save (const juce::String& name, const ParamValues& values);
    juce::Result remove (const juce::String& name);
    juce::Result rename (const juce::String& oldName, const juce::String& newName);

    // 工場出荷プリセットを .vcpreset ファイルとして書き出す(配布用 presets/ フォルダー生成に使う)
    juce::Result exportFactoryPresets (const juce::File& destDir) const;

    // 名前に使えない文字を取り除く(空になったら空文字)
    static juce::String sanitise (const juce::String& name);

    // XML 変換(テスト用に公開)
    static std::unique_ptr<juce::XmlElement> toXml (const juce::String& name, const ParamValues& v);
    static std::optional<ParamValues> fromXml (const juce::XmlElement& xml, juce::String* nameOut = nullptr);

private:
    juce::File fileFor (const juce::String& name) const;
    juce::File dir;
};

} // namespace vc
