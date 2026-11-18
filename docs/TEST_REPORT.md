# Vocal Compressor DSP テスト結果(自動生成)

テストプログラム: `tests/dsp_tests.cpp`。実際の JUCE 8.0.4 の juce_core / juce_audio_basics / juce_dsp にリンクして実行。

### T1. ゲインリダクションの精度(静的カーブ)

4 kHz 正弦波の定常状態のGR平均と、`computeGainReduction()` の理論値の差。Threshold -20 dB / Ratio 4 / Knee 0・6・12 dB / RMS検出。判定: 全レベル・全方式で |誤差| < 0.25 dB。

| fs | 方式 | 最大誤差 [dB] | 判定 |
|---|---|---|---|
| 44.1 kHz | Transparent | 0.007 | PASS |
| 44.1 kHz | Opto | 0.001 | PASS |
| 44.1 kHz | FET | 0.067 | PASS |
| 44.1 kHz | VCA | 0.009 | PASS |
| 44.1 kHz | Vari-Mu | 0.002 | PASS |
| 48.0 kHz | Transparent | 0.007 | PASS |
| 48.0 kHz | Opto | 0.001 | PASS |
| 48.0 kHz | FET | 0.067 | PASS |
| 48.0 kHz | VCA | 0.009 | PASS |
| 48.0 kHz | Vari-Mu | 0.002 | PASS |
| 88.2 kHz | Transparent | 0.006 | PASS |
| 88.2 kHz | Opto | 0.001 | PASS |
| 88.2 kHz | FET | 0.064 | PASS |
| 88.2 kHz | VCA | 0.009 | PASS |
| 88.2 kHz | Vari-Mu | 0.002 | PASS |
| 96.0 kHz | Transparent | 0.006 | PASS |
| 96.0 kHz | Opto | 0.001 | PASS |
| 96.0 kHz | FET | 0.063 | PASS |
| 96.0 kHz | VCA | 0.009 | PASS |
| 96.0 kHz | Vari-Mu | 0.002 | PASS |
| 192.0 kHz | Transparent | 0.006 | PASS |
| 192.0 kHz | Opto | 0.001 | PASS |
| 192.0 kHz | FET | 0.064 | PASS |
| 192.0 kHz | VCA | 0.009 | PASS |
| 192.0 kHz | Vari-Mu | 0.002 | PASS |

### T2. 透過性(ヌルテスト)

判定: 入力との差が完全に 0(float で一致)。

| 項目 | 結果 | 判定 |
|---|---|---|
| Ratio 1:1 / Transparent | 最大差 = 0 | PASS |
| Ratio 1:1 / Opto | 最大差 = 0 | PASS |
| Ratio 1:1 / VCA | 最大差 = 0 | PASS |
| Mix 0 % | 最大差 = 0 | PASS |
| Bypass(レイテンシー分遅延、10 ms 平滑後) | 最大差 = 0(レイテンシー 150 サンプル) | PASS |

### T3. アタック / リリース時間

-60 → -10 dBFS(4 kHz)のステップで、GRが最終値の63%に達する時間(アタック)と、-10 → -60 で37%まで戻る時間(リリース)を測定。Threshold -30 dB / Ratio 4 / ピーク検出 / Auto Release OFF / Attack 20 ms / Release 200 ms。Transparent の判定: アタック 20 ms ±15%、リリース 200〜260 ms(2段のスムージングのため設定値より少し長い)。

| fs | 方式 | アタック63% [ms] | リリース37% [ms] | 最終GR [dB] | 判定 |
|---|---|---|---|---|---|
| 44.1 kHz | Transparent | 20.0 | 220.7 | 14.98 | PASS |
| 48.0 kHz | Transparent | 20.0 | 221.0 | 15.00 | PASS |
| 48.0 kHz | Opto | 20.9 | 533.6 | 15.00 | PASS |
| 48.0 kHz | FET | 5.0 | 238.0 | 15.00 | PASS |
| 48.0 kHz | VCA | 20.0 | 456.0 | 15.00 | PASS |
| 48.0 kHz | Vari-Mu | 22.4 | 542.5 | 14.53 | PASS |
| 88.2 kHz | Transparent | 20.0 | 220.7 | 14.98 | PASS |
| 96.0 kHz | Transparent | 20.0 | 221.0 | 15.00 | PASS |
| 192.0 kHz | Transparent | 20.0 | 221.0 | 15.00 | PASS |

方式別の違い(48 kHz, Attack 10 ms / Release 100 ms):

| 方式 | アタック63% [ms] | リリース37% [ms] |
|---|---|---|
| Transparent | 10.0 | 110.5 |
| Opto | 11.7 | 264.0 |
| FET | 2.5 | 119.0 |
| VCA | 10.0 | 228.0 |
| Vari-Mu | 14.2 | 271.1 |

| 項目 | 結果 | 判定 |
|---|---|---|
| 5方式の時間特性が互いに異なる | アタック・リリースの組が全組み合わせで区別できる | PASS |
| FETのアタックが最速 | FET 2.5 ms < Transparent 10.0 / Opto 11.7 / Vari-Mu 14.2 | PASS |
| Auto Release: 短いバースト < 長いバーストの戻り時間 | 30 ms バースト後 160 ms / 1.5 s 後 404 ms | PASS |

### T4. THD / THD+N

1 kHz 正弦波。(a) 圧縮なし(-30 dBFS、Threshold -20 dB)と (b) 約10 dBのGR(-6 dBFS、Ratio 4)。THD = 2〜20 kHz の高調波 / 基音。THD+N = 基音以外すべて(全帯域)/ 基音。Attack 20 ms / Release 300 ms / RMS検出。判定: 線形方式(Transparent/Opto/VCA)の圧縮なしは THD+N < -110 dB、圧縮中は THD+N < -60 dB。FET/Vari-Mu は意図的な倍音付加なので判定せず、数値のみ示す。

| fs | 方式 | (a) THD [dB] | (a) THD+N [dB] | (b) THD [dB] | (b) THD+N [dB] | 判定 |
|---|---|---|---|---|---|---|
| 44.1 kHz | Transparent | -162.9 | -152.0 | -158.7 | -123.2 | PASS |
| 44.1 kHz | Opto | -162.9 | -152.0 | -164.9 | -136.6 | PASS |
| 44.1 kHz | FET | -76.2 | -76.2 | -48.7 | -48.7 | (参考値) |
| 44.1 kHz | VCA | -162.9 | -152.0 | -123.8 | -117.5 | PASS |
| 44.1 kHz | Vari-Mu | -75.3 | -75.3 | -49.1 | -49.1 | (参考値) |
| 48.0 kHz | Transparent | -150.4 | -149.8 | -148.1 | -147.5 | PASS |
| 48.0 kHz | Opto | -150.4 | -149.8 | -153.3 | -147.1 | PASS |
| 48.0 kHz | FET | -76.2 | -76.2 | -48.7 | -48.7 | (参考値) |
| 48.0 kHz | VCA | -150.4 | -149.8 | -123.6 | -123.6 | PASS |
| 48.0 kHz | Vari-Mu | -75.3 | -75.3 | -49.1 | -49.1 | (参考値) |
| 88.2 kHz | Transparent | -162.9 | -152.0 | -157.9 | -140.7 | PASS |
| 88.2 kHz | Opto | -162.9 | -152.0 | -165.3 | -146.2 | PASS |
| 88.2 kHz | FET | -76.2 | -76.2 | -48.7 | -48.7 | (参考値) |
| 88.2 kHz | VCA | -162.9 | -152.0 | -124.1 | -123.9 | PASS |
| 88.2 kHz | Vari-Mu | -75.3 | -75.3 | -49.1 | -49.1 | (参考値) |
| 96.0 kHz | Transparent | -155.1 | -150.3 | -153.8 | -151.7 | PASS |
| 96.0 kHz | Opto | -155.1 | -150.3 | -157.5 | -146.8 | PASS |
| 96.0 kHz | FET | -76.2 | -76.2 | -48.7 | -48.7 | (参考値) |
| 96.0 kHz | VCA | -155.1 | -150.3 | -124.0 | -124.0 | PASS |
| 96.0 kHz | Vari-Mu | -75.3 | -75.3 | -49.1 | -49.1 | (参考値) |
| 192.0 kHz | Transparent | -158.9 | -151.5 | -154.0 | -148.3 | PASS |
| 192.0 kHz | Opto | -158.9 | -151.5 | -157.4 | -146.4 | PASS |
| 192.0 kHz | FET | -76.2 | -76.2 | -48.7 | -48.7 | (参考値) |
| 192.0 kHz | VCA | -158.9 | -151.5 | -124.2 | -124.2 | PASS |
| 192.0 kHz | Vari-Mu | -75.3 | -75.3 | -49.1 | -49.1 | (参考値) |

### T4b. オーバーサンプリングの効果(非線形方式のエイリアシング)

7 kHz 正弦波 -6 dBFS・FET・約10 dBのGRで、**非調波成分**(基音と全高調波を除いた残り = エイリアシング+ノイズ)を測定。判定: 44.1 / 48 kHz で 4x の非調波成分が 1x より 10 dB 以上小さいこと。

| fs | 方式 | 1x [dB] | 2x [dB] | 4x [dB] | 判定 |
|---|---|---|---|---|---|
| 44.1 kHz | FET | -102.1 | -133.1 | -133.5 | PASS |
| 44.1 kHz | Vari-Mu | -105.7 | -137.2 | -137.2 | PASS |
| 48.0 kHz | FET | -102.1 | -139.2 | -139.8 | PASS |
| 48.0 kHz | Vari-Mu | -105.7 | -135.5 | -136.5 | PASS |
| 96.0 kHz | FET | -148.4 | -140.0 | -137.6 | PASS |
| 96.0 kHz | Vari-Mu | -148.9 | -138.1 | -137.7 | PASS |

### T5. ノイズフロア・無音・デノーマル

判定: 無音入力で出力が厳密に 0(= 自己雑音なし)、極小信号(-130 dBFS)で NaN/Inf/発振が出ないこと。

| 項目 | 結果 | 判定 |
|---|---|---|
| 無音入力 / Transparent / 44 kHz | 出力ピーク = 0 | PASS |
| 無音入力 / Opto / 44 kHz | 出力ピーク = 0 | PASS |
| 無音入力 / FET / 44 kHz | 出力ピーク = 0 | PASS |
| 無音入力 / VCA / 44 kHz | 出力ピーク = 0 | PASS |
| 無音入力 / Vari-Mu / 44 kHz | 出力ピーク = 0 | PASS |
| 無音入力 / Transparent / 192 kHz | 出力ピーク = 0 | PASS |
| 無音入力 / Opto / 192 kHz | 出力ピーク = 0 | PASS |
| 無音入力 / FET / 192 kHz | 出力ピーク = 0 | PASS |
| 無音入力 / VCA / 192 kHz | 出力ピーク = 0 | PASS |
| 無音入力 / Vari-Mu / 192 kHz | 出力ピーク = 0 | PASS |
| 極小信号(-130 dBFS)の通過 / FET 4x | 入力 -133.0 dB → 出力 -133.0 dB | PASS |
| デノーマル(無音区間の処理時間 / 通常区間) | 通常 5.70 ms/秒、無音 5.28 ms/秒(比 0.93)。実プラグインでは ScopedNoDenormals も併用 | PASS |

### T6. 周波数特性・位相(GRなし、-30 dBFS)

Ratio 1:1(GRなし)での振幅・位相。位相はレイテンシー(整数サンプル遅延)分を除いた値。判定: Transparent(線形方式)は 20 Hz〜0.45 fs で 振幅偏差 < 0.001 dB、位相偏差 < 0.01°。FET + OS(IIRハーフバンド)は 20 Hz〜0.4 fs で振幅偏差 < 0.5 dB を確認し、位相は線形位相ではない(最小位相IIR)ことを数値で示す。

| fs | 方式 | 周波数 | 振幅 [dB] | 位相 [deg] |
|---|---|---|---|---|
| 48 kHz | Transparent(OS4+LA2ms) | 20 Hz | 0.0000 | 0.000 |
| 48 kHz | Transparent(OS4+LA2ms) | 50 Hz | -0.0000 | -0.000 |
| 48 kHz | Transparent(OS4+LA2ms) | 100 Hz | -0.0000 | -0.000 |
| 48 kHz | Transparent(OS4+LA2ms) | 500 Hz | 0.0000 | 0.000 |
| 48 kHz | Transparent(OS4+LA2ms) | 1000 Hz | 0.0000 | -0.000 |
| 48 kHz | Transparent(OS4+LA2ms) | 5000 Hz | 0.0000 | 0.000 |
| 48 kHz | Transparent(OS4+LA2ms) | 10000 Hz | 0.0000 | 0.000 |
| 48 kHz | Transparent(OS4+LA2ms) | 9600 Hz | 0.0000 | -0.000 |
| 48 kHz | Transparent(OS4+LA2ms) | 19200 Hz | 0.0000 | 0.000 |
| 48 kHz | Transparent(OS4+LA2ms) | 21600 Hz | 0.0000 | 0.000 |
| T6 Transparent 振幅/位相 48 kHz | 最大振幅偏差 0.00000 dB / 最大位相偏差 0.0000° | PASS |
| 48 kHz | FET OS4x | 20 Hz | -0.0952 | 8.531 |
| 48 kHz | FET OS4x | 50 Hz | -0.0142 | 3.434 |
| 48 kHz | FET OS4x | 100 Hz | -0.0025 | 1.718 |
| 48 kHz | FET OS4x | 500 Hz | 0.0013 | 0.345 |
| 48 kHz | FET OS4x | 1000 Hz | 0.0014 | 0.179 |
| 48 kHz | FET OS4x | 5000 Hz | 0.0014 | 0.644 |
| 48 kHz | FET OS4x | 10000 Hz | 0.0014 | -1.318 |
| 48 kHz | FET OS4x | 9600 Hz | 0.0014 | -0.682 |
| 48 kHz | FET OS4x | 19200 Hz | 0.0014 | -116.840 |
| 48 kHz | FET OS4x | 21600 Hz | 0.0014 | 118.393 |
| T6 FET OS4x 振幅 48 kHz | 最大振幅偏差 0.095 dB(0.4 fs まで) | PASS |
| 192 kHz | Transparent(OS4+LA2ms) | 20 Hz | 0.0000 | 0.000 |
| 192 kHz | Transparent(OS4+LA2ms) | 50 Hz | 0.0000 | 0.000 |
| 192 kHz | Transparent(OS4+LA2ms) | 100 Hz | 0.0000 | 0.000 |
| 192 kHz | Transparent(OS4+LA2ms) | 500 Hz | 0.0000 | -0.000 |
| 192 kHz | Transparent(OS4+LA2ms) | 1000 Hz | 0.0000 | -0.000 |
| 192 kHz | Transparent(OS4+LA2ms) | 5000 Hz | 0.0000 | -0.000 |
| 192 kHz | Transparent(OS4+LA2ms) | 10000 Hz | 0.0000 | 0.000 |
| 192 kHz | Transparent(OS4+LA2ms) | 38400 Hz | 0.0000 | 0.000 |
| 192 kHz | Transparent(OS4+LA2ms) | 76800 Hz | 0.0000 | 0.000 |
| 192 kHz | Transparent(OS4+LA2ms) | 86400 Hz | 0.0000 | 0.000 |
| T6 Transparent 振幅/位相 192 kHz | 最大振幅偏差 0.00000 dB / 最大位相偏差 0.0000° | PASS |
| 192 kHz | FET OS4x | 20 Hz | -0.0965 | 8.531 |
| 192 kHz | FET OS4x | 50 Hz | -0.0154 | 3.434 |
| 192 kHz | FET OS4x | 100 Hz | -0.0037 | 1.718 |
| 192 kHz | FET OS4x | 500 Hz | 0.0000 | 0.344 |
| 192 kHz | FET OS4x | 1000 Hz | 0.0001 | 0.172 |
| 192 kHz | FET OS4x | 5000 Hz | 0.0002 | 0.049 |
| 192 kHz | FET OS4x | 10000 Hz | 0.0002 | 0.126 |
| 192 kHz | FET OS4x | 38400 Hz | 0.0002 | -0.694 |
| 192 kHz | FET OS4x | 76800 Hz | 0.0002 | -116.842 |
| 192 kHz | FET OS4x | 86400 Hz | 0.0002 | 118.392 |
| T6 FET OS4x 振幅 192 kHz | 最大振幅偏差 0.096 dB(0.4 fs まで) | PASS |

### T7. レイテンシー申告の正確さ

`getLatencySamples()` の申告値と、実測遅延(ノイズ相互相関で粗く、1 kHz の位相で細かく)の比較。判定: 誤差 < 1 サンプル(Transparent/Opto/VCA は整数サンプルなので 0.05 以内)。オーバーサンプリングの IIR ハーフバンドは周波数により遅延が変わるため、1 kHz での値との差が出る。

| fs | 方式 | Lookahead | OS | 申告 [samples] | 実測 [samples] | 差 | 判定 |
|---|---|---|---|---|---|---|---|
| 44.1 kHz | Transparent | 2 ms | 4x | 94 | 94.00 | -0.00 | PASS |
| 44.1 kHz | FET | 2 ms | 4x | 94 | 93.98 | -0.02 | PASS |
| 48.0 kHz | Transparent | 0 ms | 1x | 0 | 0.00 | +0.00 | PASS |
| 48.0 kHz | Transparent | 0 ms | 2x | 4 | 4.00 | +0.00 | PASS |
| 48.0 kHz | Transparent | 0 ms | 4x | 6 | 6.00 | +0.00 | PASS |
| 48.0 kHz | Transparent | 2 ms | 1x | 96 | 96.00 | +0.00 | PASS |
| 48.0 kHz | Transparent | 2 ms | 2x | 100 | 100.00 | +0.00 | PASS |
| 48.0 kHz | Transparent | 2 ms | 4x | 102 | 102.00 | +0.00 | PASS |
| 48.0 kHz | Transparent | 10 ms | 1x | 480 | 480.00 | +0.00 | PASS |
| 48.0 kHz | Transparent | 10 ms | 2x | 484 | 484.00 | +0.00 | PASS |
| 48.0 kHz | Transparent | 10 ms | 4x | 486 | 486.00 | +0.00 | PASS |
| 48.0 kHz | FET | 0 ms | 1x | 0 | -0.02 | -0.02 | PASS |
| 48.0 kHz | FET | 0 ms | 2x | 4 | 3.98 | -0.02 | PASS |
| 48.0 kHz | FET | 0 ms | 4x | 6 | 5.98 | -0.02 | PASS |
| 48.0 kHz | FET | 2 ms | 1x | 96 | 95.98 | -0.02 | PASS |
| 48.0 kHz | FET | 2 ms | 2x | 100 | 99.98 | -0.02 | PASS |
| 48.0 kHz | FET | 2 ms | 4x | 102 | 101.98 | -0.02 | PASS |
| 48.0 kHz | FET | 10 ms | 1x | 480 | 479.98 | -0.02 | PASS |
| 48.0 kHz | FET | 10 ms | 2x | 484 | 483.98 | -0.02 | PASS |
| 48.0 kHz | FET | 10 ms | 4x | 486 | 485.98 | -0.02 | PASS |
| 88.2 kHz | Transparent | 2 ms | 4x | 182 | 182.00 | +0.00 | PASS |
| 88.2 kHz | FET | 2 ms | 4x | 182 | 181.96 | -0.04 | PASS |
| 96.0 kHz | Transparent | 2 ms | 4x | 198 | 198.00 | -0.00 | PASS |
| 96.0 kHz | FET | 2 ms | 4x | 198 | 197.95 | -0.05 | PASS |
| 192.0 kHz | Transparent | 0 ms | 1x | 0 | 0.00 | +0.00 | PASS |
| 192.0 kHz | Transparent | 0 ms | 2x | 4 | 4.00 | +0.00 | PASS |
| 192.0 kHz | Transparent | 0 ms | 4x | 6 | 6.00 | -0.00 | PASS |
| 192.0 kHz | Transparent | 2 ms | 1x | 384 | 384.00 | +0.00 | PASS |
| 192.0 kHz | Transparent | 2 ms | 2x | 388 | 388.00 | +0.00 | PASS |
| 192.0 kHz | Transparent | 2 ms | 4x | 390 | 390.00 | -0.00 | PASS |
| 192.0 kHz | Transparent | 10 ms | 1x | 1920 | 1920.00 | +0.00 | PASS |
| 192.0 kHz | Transparent | 10 ms | 2x | 1924 | 1924.00 | +0.00 | PASS |
| 192.0 kHz | Transparent | 10 ms | 4x | 1926 | 1926.00 | -0.00 | PASS |
| 192.0 kHz | FET | 0 ms | 1x | 0 | -0.09 | -0.09 | PASS |
| 192.0 kHz | FET | 0 ms | 2x | 4 | 3.91 | -0.09 | PASS |
| 192.0 kHz | FET | 0 ms | 4x | 6 | 5.91 | -0.09 | PASS |
| 192.0 kHz | FET | 2 ms | 1x | 384 | 383.91 | -0.09 | PASS |
| 192.0 kHz | FET | 2 ms | 2x | 388 | 387.91 | -0.09 | PASS |
| 192.0 kHz | FET | 2 ms | 4x | 390 | 389.91 | -0.09 | PASS |
| 192.0 kHz | FET | 10 ms | 1x | 1920 | 1919.91 | -0.09 | PASS |
| 192.0 kHz | FET | 10 ms | 2x | 1924 | 1923.91 | -0.09 | PASS |
| 192.0 kHz | FET | 10 ms | 4x | 1926 | 1925.91 | -0.09 | PASS |
| 項目 | 結果 | 判定 |
|---|---|---|
| 方式を切り替えてもレイテンシーが一定 | 全方式 150 サンプル(Lookahead 3 ms + OS 4x) | PASS |

### T8. 過渡応答とルックアヘッド

-50 → 0 dBFS のステップ(1 kHz)。Threshold -30 dB / Ratio 20 / Attack 0.1 ms(Opto・Vari-Mu は物理的にアタックが遅い)。オーバーシュート = ステップ直後のピーク - 定常出力ピーク。判定: Transparent で Lookahead 5 ms のオーバーシュートが Lookahead 0 より 6 dB 以上小さい。

| fs | 方式 | Lookahead | オーバーシュート [dB] | 定常ピーク [dBFS] |
|---|---|---|---|---|
| 44.1 kHz | Transparent | 0 ms | 12.01 | -28.48 |
| 44.1 kHz | Transparent | 5 ms | 0.00 | -28.50 |
| 44.1 kHz | FET | 0 ms | 4.10 | -28.47 |
| 44.1 kHz | FET | 5 ms | 0.05 | -28.48 |
| 44.1 kHz | VCA | 0 ms | 12.01 | -28.48 |
| 44.1 kHz | VCA | 5 ms | 0.00 | -28.50 |

| 項目 | 結果 | 判定 |
|---|---|---|
| T8 ルックアヘッドの効果 44 kHz | オーバーシュート 12.01 dB(0 ms)→ 0.00 dB(5 ms) | PASS |

| fs | 方式 | Lookahead | オーバーシュート [dB] | 定常ピーク [dBFS] |
|---|---|---|---|---|
| 96.0 kHz | Transparent | 0 ms | 12.80 | -28.48 |
| 96.0 kHz | Transparent | 5 ms | 0.00 | -28.50 |
| 96.0 kHz | FET | 0 ms | 5.60 | -28.47 |
| 96.0 kHz | FET | 5 ms | 0.05 | -28.48 |
| 96.0 kHz | VCA | 0 ms | 12.81 | -28.48 |
| 96.0 kHz | VCA | 5 ms | 0.00 | -28.50 |

| 項目 | 結果 | 判定 |
|---|---|---|
| T8 ルックアヘッドの効果 96 kHz | オーバーシュート 12.80 dB(0 ms)→ 0.00 dB(5 ms) | PASS |

| fs | 方式 | Lookahead | オーバーシュート [dB] | 定常ピーク [dBFS] |
|---|---|---|---|---|
| 192.0 kHz | Transparent | 0 ms | 13.21 | -28.48 |
| 192.0 kHz | Transparent | 5 ms | 0.00 | -28.50 |
| 192.0 kHz | FET | 0 ms | 6.56 | -28.47 |
| 192.0 kHz | FET | 5 ms | 0.05 | -28.48 |
| 192.0 kHz | VCA | 0 ms | 13.22 | -28.48 |
| 192.0 kHz | VCA | 5 ms | 0.00 | -28.50 |

| 項目 | 結果 | 判定 |
|---|---|---|
| T8 ルックアヘッドの効果 192 kHz | オーバーシュート 13.21 dB(0 ms)→ 0.00 dB(5 ms) | PASS |

| fs | 方式 | Lookahead | オーバーシュート [dB] | 定常ピーク [dBFS] |
|---|---|---|---|---|

### T9. ステレオリンク

L = -10 dBFS、R = -40 dBFS(Threshold -30 dB / Ratio 4)。判定: Link 100% で両chのGR一致、0% で独立(Rはほぼ 0)、50% で中間。

| 項目 | 結果 | 判定 |
|---|---|---|
| Link 100 % | L 15.03 dB / R 15.03 dB | PASS |
| Link 0 % | L 15.03 dB / R 0.00 dB | PASS |
| Link 50 % | R 7.51 dB(0%: 0.00 / 100%: 15.03) | PASS |

### T10. Mix / Makeup / Auto Makeup

判定: Mix 50 % の振幅は dry と wet の平均、Makeup +6 dB は +6.00 dB、Auto Makeup は基準 -12 dBFS の信号で出力が入力と ±0.3 dB。

| 項目 | 結果 | 判定 |
|---|---|---|
| Mix 50 % | 振幅 0.15206(期待 0.15206) | PASS |
| Makeup +6 dB | 6.000 dB | PASS |
| Auto Makeup / Transparent | 出力 - 入力 = -0.03 dB | PASS |
| Auto Makeup / Opto | 出力 - 入力 = -0.01 dB | PASS |
| Auto Makeup / FET | 出力 - 入力 = -0.25 dB | PASS |
| Auto Makeup / VCA | 出力 - 入力 = -0.04 dB | PASS |
| Auto Makeup / Vari-Mu | 出力 - 入力 = -0.01 dB | PASS |

### T11. サイドチェイン



| 項目 | 結果 | 判定 |
|---|---|---|
| SC HPF 1 kHz: 100 Hz のGRが減る | HPFなし 15.24 dB → HPF 1 kHz 0.00 dB | PASS |
| SC HPF 1 kHz: 5 kHz のGRは変わらない | 15.01 dB → 15.00 dB | PASS |
| SC LPF 1 kHz: 5 kHz のGRが減る | LPFなし 15.01 dB → LPF 1 kHz 0.00 dB | PASS |
| SC Gain +12 dB ≙ 入力 +12 dB | 12.776 dB vs 12.776 dB | PASS |
| SC EQ Mid +12 dB @2 kHz | 0.01 dB → 9.01 dB | PASS |
| SC 重み付け Voice: 50 Hz のGRが減る | 15.45 dB → 3.46 dB | PASS |
| A特性の近似(1 kHz=0 dB / 100 Hz ≈ -19.1 / 10 kHz ≈ -2.5) | 1 kHz 0.00 / 100 Hz -19.2 / 10 kHz -1.9 dB | PASS |
| 外部サイドチェイン | 外部が大きいときGR 15.08 dB / 無音のとき 0.000 dB | PASS |
| SC モニター(外部、フィルターなし) | モニター出力と外部入力の最大差 = 0 | PASS |
| 外部選択でも外部入力が無い場合に安全 | 出力は有限値 | PASS |
| Feed Back は Feed Forward よりGRが小さい(同じ設定) | FF 15.03 dB / FB 8.59 dB | PASS |

### T12. ボーカル専用機能(合成信号での方向性の確認)

実際のボーカル録音ではなく合成信号(倍音を持つ有声音、80 Hz のバースト、高域ノイズ)で、各機能が設計どおりの方向に効くことを確認する。**実際の歌唱・朗読での効果と音質は 未確認**(§README 参照)。

| 項目 | 結果 | 判定 |
|---|---|---|
| Plosive Guard: 80 Hz バースト時の最大GRが減る | OFF 15.00 dB → 100% 12.97 dB | PASS |
| Sibilance Guard: 高域ノイズ区間の最大GRが減る | OFF 12.30 dB → 100% 9.16 dB | PASS |
| Vocal Leveling: 大小のレベル差が縮まる | OFF 18.00 dB → 100% 5.40 dB(入力差 18 dB) | PASS |
| Syllable Aware: 音節の谷でGRが速く戻る | 谷の終わりのGR OFF 8.93 dB → 100% 6.07 dB | PASS |
| Phrase Aware: 休符でGRが速く戻る | 休符 400 ms 後のGR OFF 3.25 dB → 100% 1.85 dB | PASS |
| Adaptive: 大きいフレーズでより圧縮する | 大きいフレーズのGR OFF 10.66 dB → 100% 11.11 dB | PASS |

### T13. パラメーター急変時のクリック / ブロックサイズ独立性

1 kHz 正弦波(-12 dBFS)の途中、ブロック境界でパラメーターを急に変える。出力の最大サンプル間差分 / 入力の最大サンプル間差分(正弦波そのものの傾き)を測る。1 に近いほど段差なし。判定: Threshold/Makeup/Mix/Output/Input/Bypass/Ratio の急変で比が 2.0 未満。方式切替は参考値のみ。

| 急変させる項目 | 比 (出力の最大差分 / 入力の最大差分) | 判定 |
|---|---|---|
| Threshold -50 → 0 dB | 0.98 | PASS |
| Makeup 0 → +18 dB | 0.11 | PASS |
| Mix 100 → 0 % | 1.00 | PASS |
| Output 0 → -24 dB | 0.11 | PASS |
| Input 0 → +12 dB | 0.08 | PASS |
| Bypass ON | 1.00 | PASS |
| Ratio 2 → 20 | 0.11 | PASS |
| Model Transparent → FET | 0.11 | (参考値) |

| 項目 | 結果 | 判定 |
|---|---|---|
| ブロックサイズ 1 と 512 の出力差 | 最大差 1.49e-08(ピーク比 -144.7 dB) | PASS |
| ブロックサイズ 7 と 512 の出力差 | 最大差 7.45e-09(ピーク比 -150.8 dB) | PASS |
| ブロックサイズ 64 と 512 の出力差 | 最大差 0(ピーク比 -600.0 dB) | PASS |
| ブロックサイズ 333 と 512 の出力差 | 最大差 0(ピーク比 -600.0 dB) | PASS |

### T14. 頑健性(ファズ)・オーディオスレッドでのメモリ確保

全パラメーターをブロックごとにランダムに変え、ブロックサイズ 1〜512、ノイズ・インパルス・DC・NaN・Inf・±1e30・デノーマルを混ぜて処理。判定: 出力がすべて有限値かつ |x| ≤ 1e4、`process()`/`setParams()` 内の `operator new` 呼び出しが 0 回。

| 項目 | 結果 | 判定 |
|---|---|---|
| ランダム入力・ランダムパラメーター(全5サンプルレート) | 13500 ブロック処理、非有限値 0 個、|x|>1e4 が 0 個 | PASS |
| オーディオスレッドでのメモリ確保 | process()/setParams() 内の new 呼び出し 0 回 | PASS |
| モノラル(1ch)処理 | 出力は有限値 | PASS |

### T16. プリセット(工場出荷 12 種 + 保存 / 読み込み / 削除 / 名前変更)

判定: 仕様の12種が揃い全値が範囲内、全プリセットで実際に処理して出力が有限、XML往復で値が完全一致、保存・読み込み・名前変更・削除が動作、不正入力を拒否。

| 項目 | 結果 | 判定 |
|---|---|---|
| 工場出荷プリセットが仕様の12種と一致 | 12 種 | PASS |
| 全プリセットの全値がパラメーター範囲内 |  | PASS |
| 全12プリセットで処理でき、出力が有限値 | 最大レイテンシー 2.00 ms | PASS |
| XML の書き出し→読み込みで全値が一致 |  | PASS |
| 保存 |  | PASS |
| 保存した名前が一覧に出る |  | PASS |
| 読み込みで値が一致 |  | PASS |
| 名前変更 |  | PASS |
| 名前変更後も値が保持される |  | PASS |
| 工場出荷と同名の保存を拒否 |  | PASS |
| 工場出荷プリセットの削除を拒否 |  | PASS |
| 工場出荷プリセットの名前変更を拒否 |  | PASS |
| 空の名前・禁止文字のみの名前を拒否 |  | PASS |
| 禁止文字の除去(パス区切りなど) |  | PASS |
| 削除 |  | PASS |
| 壊れたファイルは読み込みに失敗(クラッシュしない) |  | PASS |
| 範囲外・NaN の値は安全な値に丸められる |  | PASS |
| 工場出荷プリセットをファイルへ書き出せる |  | PASS |

### T15. CPU負荷(このテスト環境での参考値)

ステレオ、ブロック 512、ホワイトノイズ(-12 dBFS)、全ボーカル機能 ON、10 秒分を処理した時間。**このコンテナの CPU(gcc -O2)での値**で、ユーザー環境(MSVC Release)の値ではない。目安としてのみ使うこと。

| fs | 方式 | OS | 実時間比(大きいほど軽い) | 1コア使用率 |
|---|---|---|---|---|
| 48.0 kHz | Transparent | 1x | 139× | 0.72 % |
| 48.0 kHz | Opto | 1x | 115× | 0.87 % |
| 48.0 kHz | FET | 1x | 114× | 0.87 % |
| 48.0 kHz | FET | 4x | 62× | 1.61 % |
| 48.0 kHz | VCA | 1x | 139× | 0.72 % |
| 48.0 kHz | Vari-Mu | 1x | 90× | 1.11 % |
| 48.0 kHz | Vari-Mu | 4x | 54× | 1.84 % |
| 192.0 kHz | Transparent | 1x | 37× | 2.67 % |
| 192.0 kHz | FET | 1x | 31× | 3.21 % |
| 192.0 kHz | FET | 4x | 16× | 6.10 % |

## 集計

判定項目: PASS 200 / FAIL 0
