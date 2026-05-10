# MuniVoice (Sample Player)

`juce::SynthesiserVoice` 実装。1 ノートあたり 1 サンプル再生インスタンス。

## 構成
- `std::shared_ptr<const SampleBuffer>` (共有・読み取り専用)
- 線形補間付き位相 (`double position`)
- AR Envelope ×1 (voice.h に内包、クリック防止用)
- atomic パラメータポインタへの参照 (`attackParam`, `releaseParam`, `pitchParam`, `speedParam`)
- ルートノート定数 `kRootMidiNote = 60` (C4) — サンプルの「自然なピッチ」扱い
- `currentNoteNumber` メンバ — 直近の `startNote` で受けた MIDI ノート番号

## ライフサイクル
| コールバック | 動作 |
|-------------|------|
| `startNote(note, velocity, ...)` | MIDI ノート番号を `currentNoteNumber` に格納 (`[0, 127]` でクランプ)。`position = 0.0`, `velocity` を保持、envelope = Attack。 |
| `stopNote(velocity, allowTailOff)` | tailOff 時は envelope = Release。非 tailOff (mono ボイススチール時) は `position` リセット + 即 Idle + `clearCurrentNote()`。 |
| `renderNextBlock(buffer, start, numSamples)` | ループで sample ×envelope ×velocity を全チャンネルに加算。`position` がサンプル末尾を超えたら envelope を強制 Release に遷移し、Idle 到達で `clearCurrentNote()`。 |
| `pitchWheelMoved`, `controllerMoved` | v0.3 では無視 (将来 pitch wheel ベンドを足すならここに実装) |

## 再生レート計算
ブロック先頭で 1 度だけ計算し、ブロック内では固定:

```
noteRatio   = 2^((currentNoteNumber - kRootMidiNote) / 12)
pitchSemis  = clamp(pitchParam, -24, +24)
speed       = clamp(speedParam, 0.25, 4.0)
pitchMult   = 2^(pitchSemis / 12)
sourceSR    = sampleBuffer->sampleRate
positionInc = max(1e-4, noteRatio * pitchMult * speed * sourceSR / hostSR)
```

- **`noteRatio`** が MIDI ノートによる音階再生を担う。C4 (60) で 1.0、半音上で `2^(1/12)` ≈ 1.0595、1 オクターブ上で 2.0。
- `pitch` パラメータ (`pitchSemis`) はグローバルな追加シフトとして **加算的** に効く (両者の倍率が乗算される = 半音単位で加算と等価)。
- ルートノートを変えたい場合は `MuniVoice::kRootMidiNote` を変更する (現状ハードコード)。
- `positionInc` は `double` で保持し、累積誤差を抑える。
- タイムストレッチは行わないため、ピッチが上がると再生時間は短くなる (サンプル末尾到達で envelope は Release に遷移)。

## 補間
線形補間 (linear interpolation):

```
i0    = floor(position)
i1    = i0 + 1
frac  = position - i0
sample = (1 - frac) * src[i0] + frac * src[i1]
position += positionInc
```

- `i1` がサンプル末尾 (`numSourceSamples - 1`) を超えたら 0 を返し、Release に強制遷移。
- ステレオソースの場合、出力チャンネル数に合わせて mix down / 拡張する。
  - mono out: ソース全チャンネル平均
  - stereo out, mono source: 全チャンネルに同サンプル
  - stereo out, stereo source: そのまま

## Polyphonic / Mono の切替
ボイス自身は polyphonic フラグを見ない。`PluginProcessor::processBlock()` が MIDI ストリームを前処理し、`polyphonic=false` のときは noteOn 直前に `synth.allNotesOff(midiChannel, allowTailOff=false)` を呼ぶ。

## エンベロープ (AR)

`MuniVoice` 内部に同居する最小エンベロープ。クリック・ポップ防止のみを目的とした線形 AR を実装し、Sustain レベルは常に 1.0 固定 (将来 ADSR 化する場合に Decay/Sustain レベルを追加する余地を残している)。

### 状態遷移

```
Idle ──noteOn──▶ Attack ──level==1──▶ Sustain ──noteOff──▶ Release ──level==0──▶ Idle
```

- Attack / Release は線形ランプ。
- Sustain は固定レベル (1.0) を維持するだけで時間進行しない。

### 計算

- `attackInc  = 1 / (attackSec  * sampleRate)`
- `releaseDec = 1 / (releaseSec * sampleRate)`
- サンプル毎に `level` を増減させ、閾値到達で次状態へ遷移する。
- `attackSec` / `releaseSec` は 0 除算回避のため下限クランプを行う (`jmax(0.0001f, ...)`)。

### 終了判定

Release 状態で `level <= 0` に達したら Idle に遷移し、`MuniVoice::clearCurrentNote()` を呼んで JUCE 側に voice を解放させる。サンプル末尾到達時 (`renderNextBlock` 側で強制 Release 遷移) も同じ経路で Idle に到達する。

## MuniSound
`juce::SynthesiserSound` を継承し `appliesToNote` / `appliesToChannel` は常に true を返す。全 MIDI ノート・全チャンネルを受理。クラス本体が 5 行と小さく `MuniVoice` とペアでしか使わないため、独立ヘッダにせず `src/dsp/MuniVoice.h` に同居させている。

## リアルタイム制約
- `renderNextBlock()` 内で割り当て・ロック・例外禁止。
- サンプルバッファは事前ロード済み・読み取り専用なので、ボイスは `const float*` を通じてインデックスアクセスのみ行う。
- atomic 読み取りはブロック先頭の 1 回のみ (毎サンプル読みは不要)。
