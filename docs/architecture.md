# アーキテクチャ

VST_muni_one は JUCE 8 ベースのワンショット・サンプル再生 VST3 プラグイン。`dog.wav` をビルド時に同梱し、MIDI ノートでトリガーする。GUI はサンプル/ヘッドフォン姿の muni キャラクターをドラッグして各種パラメータを操作する。

## 信号フロー

```
MIDI In
  │
  ▼
┌──────────────────────────────────────────────────┐
│ processBlock 前処理                              │
│   - midiMessages を走査して note-on 数をカウント  │
│   - noteTriggerCount.fetch_add(n, relaxed)       │
│       → GUI 側 Timer がポーリングして             │
│         パルス拡大アニメーションを起動            │
│   - polyphonic=false の場合、noteOn 前に         │
│     全アクティブボイスを allowTailOff=false で    │
│     即停止 (= シングル再生)                       │
└──────────────────────────────────────────────────┘
  │
  ▼
┌──────────────────────────────────────────────────┐
│ juce::Synthesiser (kNumVoices = 8)               │
│   ┌──────────────────────────────────────────┐   │
│   │ MuniVoice (Sample Player)                │   │
│   │   - SampleBuffer (共有, 読み取り専用)     │   │
│   │   - 線形補間 + 位相 (double 精度)         │   │
│   │   - rate = noteRatio                     │   │
│   │           * 2^(pitch/12) * speed          │   │
│   │           * (sourceSR / hostSR)           │   │
│   │     where noteRatio = 2^((midiNote-60)/12)│   │
│   │   - AR Envelope (線形, クリック防止)      │   │
│   └──────────────────────────────────────────┘   │
└──────────────────────────────────────────────────┘
  │
  ▼
Gain (dB → linear, Smoothed 50ms, range -15..+15 dB)
  │
  ▼
Audio Out (Mono / Stereo)
```

## モジュール構成

| レイヤ | ファイル | 役割 |
|-------|---------|------|
| Processor | `src/PluginProcessor.*` | JUCE の `AudioProcessor`。MIDI/Audio 処理、APVTS 保持、サンプルロード、polyphonic ゲート、note-on 数カウンタ。 |
| Editor    | `src/PluginEditor.*`    | GUI のルートコンポーネント。`MuniFace` を中央配置、左上 Polyphonic トグル、右上 `P` `?` ボタン。レイアウトと配線のみ (内部クラス無し)。|
| GUI       | `src/gui/MuniFace.*`    | キャラクターのレイヤー描画 + ドラッグでパラメータ操作 + ノートオン パルス拡大アニメーション (30 Hz Timer)。 |
| GUI       | `src/gui/HelpPanel.*`   | `?` トグルで開く操作説明オーバーレイ (固定文言のクリックスルー UI)。 |
| GUI       | `src/gui/ParameterOverlay.*` | `P` トグルで開く現在パラメータ値の薄表示オーバーレイ (APVTS リスナ + AsyncUpdater)。 |
| 共通      | `src/ParameterIds.h`    | APVTS パラメータ ID の `namespace muni::params` 集約。GUI 側は `PluginProcessor.h` を引かずこのヘッダだけで足りる。 |
| DSP       | `src/dsp/SampleBuffer.h`   | `dog.wav` バイナリをデコード・保持する読み取り専用バッファ。プロセッサ初期化時に 1 度だけロード。 |
| DSP       | `src/dsp/MuniVoice.h`      | `SynthesiserVoice` 実装 + `MuniSound` (全ノート受理) を同居。MIDI ノートに応じた `noteRatio` でサンプル位相を進めて補間再生。 |
| Resources | `src/sample/dog.wav`       | 再生対象サンプル。`juce_add_binary_data` でバイナリ同梱。 |
| Resources | `src/img/muni_*.png`       | キャラクター画像 (顔ベース・耳・目・眉・鼻)。同上で同梱。詳細: `docs/gui/face.md`。 |
| 開発用     | `resources/source_img/`    | 高解像度ソース画像。実行時には未使用 (再リサイズ用に保管)。 |

`src/dsp/SineOscillator.h` は v0.1 のサイン波シンセ用で、v0.2 で削除済み。

## 状態管理
- パラメータは `juce::AudioProcessorValueTreeState` (APVTS) で一元管理 (8 個: gain / attack / release / pitch / speed / polyphonic / ear_left_tilt / ear_right_tilt)。
- Processor → Editor: `polyphonic` のみ `juce::AudioProcessorValueTreeState::ButtonAttachment` で双方向バインド。それ以外のパラメータは GUI 内部 (`MuniFace`、`ParameterOverlay`) で `apvts.getParameter(id)` を使って `setValueNotifyingHost()` / `getValue()` を直接呼ぶ。`SliderAttachment` は廃止 (スライダー UI 自体を撤去したため)。
- Processor → Voice: `std::atomic<float>*` でパラメータポインタを保持し、`processBlock()` 内でロック無し読み取り。
- Audio → GUI 通知: `std::atomic<int> noteTriggerCount` を Processor が公開。`processBlock` 冒頭で MIDI バッファを走査し note-on 数を `fetch_add(relaxed)`。GUI 側 (`MuniFace`) は 30 Hz Timer でこれをポーリングし、変化を検出するとパルス拡大アニメーションを起動 (詳細: `docs/gui/face.md`)。
- パラメータ変更通知: `MuniFace` と `ParameterOverlay` が `juce::AudioProcessorValueTreeState::Listener` を実装。DAW オートメーションや別コンポーネントからの変更を `parameterChanged` で受け取り、`MessageManager::callAsync` で `repaint()` をスケジュール。
- `SampleBuffer` は `std::shared_ptr<const SampleBuffer>` でボイスに渡す (read-only。ボイス側はポインタのみ保持しデータをコピーしない)。

## スレッド
- **Audio thread**: `processBlock()`。ヒープ割り当て・ロック・例外禁止。サンプル読み取りは事前ロード済みバッファへのインデックスアクセスのみ。MIDI バッファ走査による note-on カウントと `noteTriggerCount.fetch_add(relaxed)` は RT セーフ (整数アトミック加算のみ)。モノモードの `gatedMidi` は `prepareToPlay` で `ensureSize` 済みなので `addEvent` での再確保は起きない。ゲイン適用は `juce::SmoothedValue::applyGain (buffer, numSamples)` 1 行で完結する (チャネルポインタを内部キャッシュした per-sample 補間)。
- **Message thread**: GUI 描画・ユーザー操作・サンプルのデコード (Processor コンストラクタ内)。`MuniFace` の 30 Hz Timer で `noteTriggerCount` を polling、新しい note-on があればパルスレベルを 1.0 にスナップしてアニメーション開始。`paint()` 内で `juce::Graphics::ScopedSaveState + addTransform(scale)` で顔全体を一体スケール描画。
- パラメータ読み取りは atomic ポインタ経由 (`gainParam->load()` 等)。
- polyphonic フラグも atomic 経由で読む (`polyphonicParam->load() >= 0.5f`)。`processBlock()` で MIDI を走査し noteOn 直前にボイスを停止する。
- `parameterChanged` コールバックは AudioProcessorValueTreeState の仕様上どのスレッドからも来るため、GUI 側は `juce::AsyncUpdater` を継承して `triggerAsyncUpdate()` で通知する (内部 atomic フラグのみで動作するため audio thread からのヒープ確保を回避でき、連続 fired は message thread 側で自動デバウンスされる)。`handleAsyncUpdate()` で `repaint()` を呼ぶ。
- GUI 側 (`MuniFace` / `ParameterOverlay`) はコンストラクタで `apvts.getParameter(id)` を 1 度だけ引いて `RangedAudioParameter*` をキャッシュし、毎フレームの ID 文字列ハッシュ検索を排除している。

## 今後の拡張余地
- 複数サンプルスロット / ドラッグ&ドロップ読み込み (パストラバーサル対策が前提)
- ループポイント / one-shot 切替
- リバース再生
- フィルタ (state variable)
- タイムストレッチ (phase vocoder)
- `ear_left_tilt` / `ear_right_tilt` パラメータの DSP 接続 (現状は GUI 表示のみ)
- 高解像度ディスプレイ向けの大画像差し替え (現状はネイティブピクセル描画方針)
