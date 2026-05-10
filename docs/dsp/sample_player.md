# Sample Player (SampleBuffer)

`dog.wav` をプラグイン起動時に 1 度だけデコードして保持する読み取り専用バッファ。

## 同梱方法
- `src/sample/dog.wav` を `juce_add_binary_data` でバイナリ同梱する。
- 実行時に外部ファイルパスからは読み込まない (CLAUDE.md セキュリティ方針 §4 準拠)。
- 生成される `BinaryData` 名前空間からポインタとサイズを取得し、`juce::WavAudioFormat` でデコード。

## インタフェース (`src/dsp/SampleBuffer.h`)

```cpp
struct SampleBuffer
{
    juce::AudioBuffer<float> data;   // numChannels × numFrames
    double sampleRate = 0.0;         // ソースのサンプルレート
    int    numFrames  = 0;
    int    numChannels = 0;

    // 1 度だけ呼ぶ。失敗時は data を空にして false を返す。
    bool loadFromBinary (const void* binaryData, int binarySize);
};
```

- 失敗時 (デコード不能、空データ) は `numFrames = 0` を保持。`MuniVoice::renderNextBlock` は `numFrames == 0` を見て即 Idle 化する。

## デコード手順
```
juce::MemoryInputStream stream (binaryData, binarySize, false);
juce::WavAudioFormat fmt;
auto* reader = fmt.createReaderFor (&stream, false /* deleteStreamWhenDestroyed */);
if (! reader) return false;

numChannels = reader->numChannels;
numFrames   = static_cast<int> (reader->lengthInSamples);
sampleRate  = reader->sampleRate;
data.setSize (numChannels, numFrames);
reader->read (&data, 0, numFrames, 0, true, true);
delete reader;
```

## スレッドモデル
- 構築は Message thread (Processor コンストラクタ)。
- 以後は **不変** (immutable)。Audio thread からは `std::shared_ptr<const SampleBuffer>` 経由で読み取りのみ。
- `processBlock` 内ではポインタの `numFrames` / `data.getReadPointer()` を呼ぶだけ。アロケーション無し。

## 将来拡張
- 任意 wav の差し替え (UI からドラッグ&ドロップ)。リアルタイム差し替え時はダブルバッファリング + atomic ポインタ swap で対応する。
