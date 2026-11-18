# Vocal Compressor(VST3 / Windows)

歌声向けのコンプレッサーです。JUCE 8.0.4 / C++17 で作られ、VST3 とスタンドアロンアプリを生成します。

> **先に読んでください(正直な現状)**
> - DSP(圧縮の計算部分)は、**実際の JUCE ライブラリにリンクしたテストプログラムで 200 項目すべて PASS** しています(5つのサンプルレートで測定。`docs/TEST_REPORT.md`)。
> - 一方、**Windows(MSVC)でのビルド、Cubase 15 Pro / REAPER での読み込み、GUI の見た目と操作は、この開発環境では実行できなかったため「未確認」です。** GUI のコードは実際の JUCE ヘッダーに対する構文チェックのみ通っています。
> - **Logic Pro / Cubase 標準コンプレッサーとの比較は行っていません**(Logic は macOS 専用、ブラインド試聴は人間が必要)。そのため「他製品を超えた」とは主張しません。
> - 詳細は `docs/KNOWN_ISSUES.md` を見てください。

---

## 1. ビルドして使う(Windows 10/11)

1. このフォルダーをそのまま(日本語やスペースを含まない場所が安全です。例 `C:\dev\VocalCompressor`)に置く。
2. **`build.bat` をダブルクリック**。
   - CMake / Git / Visual Studio 2022(C++ デスクトップ開発)が無い場合は、winget で入れるか尋ねます。Visual Studio を入れた直後は、もう一度 `build.bat` を実行してください。
   - 初回は JUCE のダウンロードで数分かかります。
3. 成功すると `C:\Program Files\Common Files\VST3\Vocal Compressor.vst3` にコピーされます。管理者権限が必要な場合は、画面の案内に従って `build\VocalCompressor_artefacts\Release\VST3\Vocal Compressor.vst3` を手動でコピーしてください(右クリック → 管理者として実行でも可)。
4. Cubase / REAPER でプラグインを再スキャンし、「Vocal Compressor」をインサートする。
5. DAW なしで動作確認したいときは `build\VocalCompressor_artefacts\Release\Standalone\Vocal Compressor.exe`。

### テストを自分の PC で実行する(任意・推奨)
`run_tests.bat` をダブルクリックすると、DSP テスト(GUI なし)をビルドして実行し、`docs\TEST_REPORT_local.md` を作ります。MSVC でも同じ結果になるかの確認になります(未確認)。

### CMake を直接使う場合
```
cmake -B build -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --config Release --parallel
```

---

## 2. 画面と操作

| 場所 | 内容 |
|---|---|
| 上部 | プリセット選択 / Save / Rename / Delete、A/B 切替、A>B(現在の設定をもう一方へコピー)、Undo / Redo、Settings、Oversampling、Bypass |
| 中央上 | 圧縮カーブ(橙の点が現在の動作点)/ Attack・Release の理想応答 / 入力レベルとゲインリダクションの履歴(約8秒) |
| 中央 | Threshold, Ratio, Attack, Release, Knee, Makeup |
| 右 | IN / GR / OUT メーター(ピーク・RMS・ピークホールド)、Input / Output ゲイン |
| 下(タブ) | **Compressor**: Model, Topology, Auto Release, Auto Makeup, Peak<>RMS, Stereo Link, Mix, Lookahead<br>**Sidechain**: Source(Internal/External), Channels, Weighting, Monitor, HPF, LPF, SC Gain, 3 バンド EQ<br>**Vocal**: Leveling / Plosive / Sibilance / Syllable / Phrase / Adaptive |

- ノブ: ドラッグで調整、**ダブルクリックで初期値**、下の数値をクリックして**数値を直接入力**できます。
- 右下のウィンドウ角をドラッグで**リサイズ**(縦横比固定、最小 900×600)。Settings → Window size で 100 / 125 / 150 / 200 %。
- 画面右下付近に現在のレイテンシー(サンプル数と ms)が出ます。

### 外部サイドチェイン
- Cubase: プラグインのサイドチェインを有効化し、送りたいトラックから「サイドチェイン」センドを設定 → Sidechain タブの Source を **External** に。
- REAPER: トラックを 4 チャンネルにし、ソース信号を 3/4 へ送る。Sidechain の入力は 3/4 チャンネル。
(どちらも本環境では**未確認**です。)

---

## 3. モデル(5種類)

| モデル | 検出・時間特性 | 非線形 | 特徴(設計意図) |
|---|---|---|---|
| Transparent | RMS窓 10 ms、1次系のアタック/リリース | なし | 設定値どおりに動く。透明 |
| Opto | RMS窓 50 ms、アタックに約6 msの下限、**リリースが直前のGRの量に依存**(深く長く圧縮した後ほど遅い) | なし | ゆるやかで滑らか。ニーは最低 10 dB |
| FET | RMS窓 1 ms、アタック ×0.25(最短 20 µs)、深いGRほどリリースがやや長い | あり(非対称 tanh、偶数次倍音) | 速くて元気。ニーは設定の 35% |
| VCA | RMS窓 5 ms、**リリースが直線(dB/秒一定)** | なし | 正確で素直 |
| Vari-Mu | RMS窓 30 ms、アタック下限 約10 ms、リリースはGR履歴依存、**深いほどレシオが上がる** | あり(非対称 tanh) | 太く滑らか。ニーは最低 12 dB |

Topology: **Feed Forward**(入力から検出)/ **Feed Back**(出力から検出。同じ設定でもGRが小さくなる)。Feed Back では先読みは働きません(Lookahead を設定しても遅延だけが加わるので、0 ms にしてください)。

> 「FET」「Opto」などは**特性の傾向を数式で再現した独自実装**で、特定の実機や既存プラグインのコピー・モデリングではありません。

---

## 4. ボーカル専用機能(0 % = 無効)

| 機能 | 動作 |
|---|---|
| Leveling | 約 250 ms の RMS を見て、Threshold 付近の目標レベルへ入力を ±12 dB の範囲で**ゆっくり**持ち上げ/下げる(上げは 0.8 s、下げは 0.3 s)。-55 dB 以下では動かない(ノイズを持ち上げない) |
| Plosive | 低域が支配的で急に立ち上がる成分(P/B の吹かれ)を検出し、その間、検出信号から低域の寄与を取り除く(= 吹かれで過剰に潰さない) |
| Sibilance | 高域の割合が高い区間(S/T)で、検出信号から高域の寄与を取り除く(= 歯擦音で過剰に潰さない。**これは de-esser ではなく、圧縮量が増えすぎないようにするもの**) |
| Syllable | 音節の立ち上がりではアタックを遅くして頭を通し、音節の切れ目ではリリースを速める |
| Phrase | 歌っている間はリリースを遅く、休符ではリリースを速める |
| Adaptive | フレーズが大きいほど実効しきい値を下げ、波高率(ピーク/RMS)が大きいほどレシオを上げる |

これらは**合成信号で「設計どおりの向きに効く」ことだけを確認**しています。**実際の歌声での効果・音質は未確認**です。しきい値などは固定値のヒューリスティックなので、素材によっては期待どおりに働かないことがあります。

### Detector Weighting / Sidechain
Weighting: Off / Voice(低域と超高域を落とし、声の帯域を強調)/ A-weight。サイドチェインは HPF・LPF・3 バンド EQ・ゲインを備え、**SC Monitor** で検出に使われている信号を出力に出して聴けます(外部サイドチェイン使用時は外部入力)。

---

## 5. Oversampling と Lookahead(レイテンシー)

- **Oversampling 1x / 2x / 4x は FET と Vari-Mu だけに効きます。** 理由: ゲインリダクションの動きは十分に帯域制限されていてエイリアスを出さないが、tanh の非線形ステージだけは倍音が折り返すため。測定(7 kHz, -6 dBFS, 約10 dB GR, FET)では、44.1 kHz で非調波成分が 1x -102 dB → 2x -133 dB と大きく改善し、4x は 2x とほぼ同じ(-133.5 dB)でした。**通常は 2x で十分**です。88.2 kHz 以上ではもともと折り返しが小さく、1x で問題ありません。
- Transparent / Opto / VCA は線形なので Oversampling の効果はありません。ただしモデルを切り替えても**レイテンシーが変わらない**よう、同じ長さだけ遅延させます(OS を上げるとこれらのモデルでは遅延だけ増える点に注意)。
- Lookahead 0〜10 ms。ゲイン変化を先読みして、立ち上がりのオーバーシュートを防ぎます(測定: -50→0 dBFS の急なステップで 0 ms は 12〜13 dB オーバーシュート → 5 ms で 0.00 dB)。
- **総レイテンシー = Lookahead + Oversampling の遅延**(2x: 4 サンプル、4x: 6 サンプル)。DAW へ自動で申告します。測定誤差は 0.1 サンプル未満(`docs/TEST_REPORT.md` T7)。

---

## 6. プリセット

12 種(Vocal Natural / Smooth / Pop / Aggressive / Soft / Lead / Background / Whisper / Broadcast / Transparent / Heavy / Leveling)を内蔵。`presets/*.vcpreset` は同じものの書き出しです(XML)。

- **Save**: 名前を付けて保存 → `%APPDATA%\\VocalCompressor\\Presets\\名前.vcpreset`(同名は上書き)。
- **Rename / Delete**: ユーザープリセットのみ(工場出荷のものは変更不可)。
- 配布された `.vcpreset` を上のフォルダーに入れれば一覧に出ます(Settings → Open user preset folder)。
- プロジェクトを保存すると、全パラメーター、A/B 両方の内容、ウィンドウサイズ、プリセット名が一緒に保存されます。

初期値は「実用的な出発点」です。実際の歌声での最終調整は素材に合わせて行ってください。

---

## 7. ファイル構成

| ファイル | 役割 |
|---|---|
| `Source/CompressorParameters.h` | パラメーター定義表(GUI・プリセット・テストで共有) |
| `Source/CompressorDSP.*` | ゲインコンピューター、バリスティクス、ルックアヘッド、非線形、OS、Mix、レイテンシー |
| `Source/CompressorDetector.*` | ピーク/RMS 検出器、ボーカル解析 |
| `Source/SidechainProcessor.*` | サイドチェイン(HPF/LPF/EQ/重み付け) |
| `Source/Metering.*` | メーター値の共有とバリスティクス |
| `Source/PresetManager.*` | 工場出荷プリセット、保存・読み込み・削除・名前変更 |
| `Source/PluginProcessor.*` | VST3 とのつなぎ、サイドチェイン入力、A/B、Undo、状態保存 |
| `Source/PluginEditor.*` | GUI(独自デザイン) |
| `tests/dsp_tests.cpp` | DSP テスト(GUI なし) |
| `docs/DSP_NOTES.md` | DSP の数式と設計根拠 |
| `docs/TEST_REPORT.md` | テスト結果(5 サンプルレート) |
| `docs/KNOWN_ISSUES.md` | 各フェーズの実装状況、既知の制約、未確認項目 |

ライセンス: JUCE は AGPLv3 または商用ライセンスです。このプラグインを**配布・販売**する場合は JUCE のライセンス条件を確認してください。
