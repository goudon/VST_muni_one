# パラメータ定義

APVTS (`juce::AudioProcessorValueTreeState`) で管理する。パラメータ ID は文字列定数化して `PluginProcessor` 内で公開する。

## パラメータ一覧 (v0.2 — Sample Player)

| ID            | 表示名      | 型     | 範囲              | 単位       | デフォルト | スキュー   | 備考 |
|---------------|------------|--------|-------------------|-----------|-----------|-----------|------|
| `gain`        | Gain       | float  | -15.0 〜 +15.0    | dB         | -12.0     | 線形      | 出力段の音量。`SmoothedValue` で 50ms リニア補間。最大 +15dB はおよそ +5.6 倍の増幅。「音量」はこのパラメータが担う。 |
| `attack`      | Attack     | float  | 0.001 〜 2.0      | s          | 0.01      | 対数 (0.3) | ノートオン時の線形アタック時間。クリック防止用。 |
| `release`     | Release    | float  | 0.001 〜 5.0      | s          | 0.2       | 対数 (0.3) | ノートオフ時の線形リリース時間。 |
| `pitch`       | Pitch      | float  | -24.0 〜 +24.0    | semitones  | 0.0       | 線形      | サンプル再生レートの倍率に変換 (`2^(pitch/12)`)。タイムストレッチは行わないため、ピッチ変更は再生時間も変える。 |
| `speed`       | Speed      | float  | 0.25 〜 4.0       | ×          | 1.0       | 対数 (1.0) | 再生速度倍率。`pitch` の倍率と乗算した結果が最終的なサンプル送り速度。 |
| `polyphonic`  | Polyphonic | bool   | {false, true}     | -          | true      | -         | true: 重複再生可 (最大 `kNumVoices` 個同時)。false: モノフォニック。新規ノートで既存ボイスを即停止 (allowTailOff=false) して鳴らし直す。 |
| `ear_left_tilt`  | Ear Left Tilt  | float | -45.0 〜 45.0 | deg | 0.0 | 線形 | キャラクター GUI の左耳回転 (画面右側)。現状 DSP に未接続だが APVTS に保存され DAW のオートメーションには載る。`MuniFace` がドラッグで操作。 |
| `ear_right_tilt` | Ear Right Tilt | float | -45.0 〜 45.0 | deg | 0.0 | 線形 | 右耳回転 (画面左側)。同上。 |

## キャラクター GUI ↔ パラメータ マッピング (暫定)

`MuniFace` (`src/gui/MuniFace.cpp`) のドラッグ操作と APVTS の対応。視覚的フィードバックは
パーツ自体が動くこと。数値は表示しない。マッピングは試作段階の便宜的なもので、後で再検討する。

| パーツ | ドラッグ軸 | パラメータ | 視覚範囲 (px / rad) | ドラッグ感度 (px = 全レンジ) |
|-------|-----------|------------|----------------------|------------------------------|
| 鼻       | 縦のみ           | `gain`            | ±12 px       | 24 px |
| 目 (左右ミラー) | 縦              | `attack`          | ±10 px       | 20 px |
|   | 横 (間隔・反転)  | `pitch`           | ±16 px (中心寄り = pitch 高) | 32 px |
| 眉 (左右ミラー) | 縦              | `release`         | ±10 px (目とのオーバーラップは動的にキャップ) | 20 px |
|   | 横 (間隔・反転)  | `speed`           | ±10 px (中心寄り = speed 高) | 20 px |
| 左耳     | 横 → 回転        | `ear_left_tilt`   | ±0.5 rad (~28°) | 80 px |
| 右耳     | 横 → 回転        | `ear_right_tilt`  | ±0.5 rad (~28°) | 80 px |

**ミラー挙動**: 目・眉は左右どちらをつかんでも同じパラメータ (`attack`/`pitch`/`release`/`speed`) を更新し、
反対側のパーツは APVTS の値変化を受けて自動で対称位置に追従する。耳は左右独立。

**眉↔目 オーバーラップ防止**: 眉の縦位置 (`release` 駆動) は、現在の目の縦位置から逆算した
キャップ値より下には行かない。式は `browYCap = (kBaseLeftEye.y - kBaseLeftBrow.y) - (eye_h + brow_h)/2 - margin + eyeY`
(現在のサイズで `7 + eyeY - 1 = 6 + eyeY`)。眉ドラッグ中は param 自体を、目移動時は描画値を、それぞれクランプする。

**回転ピボット**: 耳の回転中心は耳画像の上端中央付近 (`MuniFace::paint` の `drawRotated` 内、
`pivotY = center.y - h * 0.4` で算出)。

## 「現在の操作設定」と新規追加の対応

ユーザー要求 "現在の操作設定に加えて、ピッチ、再生速度、音量、重複可否（シングルで音を出せるか）" との対応:

| 要求 | 実装パラメータ | 備考 |
|------|---------------|------|
| ピッチ | `pitch` | 半音単位 |
| 再生速度 | `speed` | ピッチと独立だが同じレートに乗算される |
| 音量 | `gain` (既存) | 既存の出力段ゲインで担う |
| 重複可否 | `polyphonic` | true=重複可、false=シングル |
| 既存 Attack | `attack` (既存) | クリック防止用にそのまま残す |
| 既存 Release | `release` (既存) | tail-off 用にそのまま残す |

## レート計算 (note × pitch × speed)

```
noteRatio = 2^((midiNote - kRootMidiNote) / 12)   // kRootMidiNote = 60 (C4)
pitchMult = 2^(pitch / 12)
rate      = noteRatio * pitchMult * speed
positionInc (samples) = rate * (sourceSampleRate / hostSampleRate)
```

- `midiNote` は `startNote` で受け取った MIDI ノート番号 (`MuniVoice::currentNoteNumber`)。
- ルートノート C4 (60) を弾いたとき `noteRatio = 1.0` でサンプルが元の高さで鳴る。1 オクターブ上で 2 倍速、1 オクターブ下で 1/2 倍速。
- `pitch` パラメータはグローバルな追加シフト。`noteRatio` と `pitchMult` は乗算 (= 半音単位で加算と等価) なので、例えば C4 + pitch=+12 と C5 + pitch=0 は同じ再生レートになる。
- `sourceSampleRate` は `dog.wav` 自体のサンプルレート (デコード時に取得)。
- `rate` は voice 内で毎ブロック先頭で読み込み、サンプル間で固定 (ブロック境界でしか変化しない)。
- `rate` の下限は実装で `1e-4` にクランプ (停止防止)。タイムストレッチはしないため、ピッチが上がると再生時間は短くなる。

## 命名規約
- ID は小文字 + アンダースコア (snake_case)。
- 表示名は英語・先頭大文字。
- 単位は `juce::AudioParameterFloatAttributes().withLabel()` で付与。

## 追加時の手順
1. 本ドキュメントに行を追加する。
2. `PluginProcessor.cpp` の `createParameterLayout()` に `AudioParameterFloat` / `AudioParameterBool` を追加する。
3. 参照側 (Voice / Editor) に atomic ポインタ取得 / SliderAttachment / ButtonAttachment を追加する。

## バリデーション (setStateInformation)
- 全 float パラメータは `NormalisableRange` がクランプを行うため、不正値はレンジ内に丸まる。
- bool パラメータも APVTS が型強制を行う。
- ルートタグは `apvts.state.getType()` と `hasTagName()` で確認済み。タグが一致しない場合はデフォルト状態を保持してフォールバック。
