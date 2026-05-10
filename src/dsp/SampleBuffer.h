// =============================================================================
// SampleBuffer.h
//
// `SampleBuffer` 構造体を定義する。dog.wav バイナリを juce::WavAudioFormat で
// デコードし、numChannels x numFrames の juce::AudioBuffer<float> として保持する
// 読み取り専用バッファ。プロセッサ初期化時に 1 度だけロードされ、以後不変として
// 各ボイスから共有参照される。
// 関連ドキュメント: docs/dsp/sample_player.md
// =============================================================================
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace muni
{

struct SampleBuffer
{
    juce::AudioBuffer<float> data;
    double sampleRate  = 0.0;
    int    numFrames   = 0;
    int    numChannels = 0;

    bool loadFromBinary (const void* binaryData, int binarySize)
    {
        if (binaryData == nullptr || binarySize <= 0)
            return false;

        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        auto* stream = new juce::MemoryInputStream (binaryData,
                                                    static_cast<std::size_t> (binarySize),
                                                    false);

        std::unique_ptr<juce::AudioFormatReader> reader (
            formatManager.createReaderFor (std::unique_ptr<juce::InputStream> (stream)));

        if (reader == nullptr)
            return false;

        const auto frames   = static_cast<int> (reader->lengthInSamples);
        const auto channels = static_cast<int> (reader->numChannels);
        if (frames <= 0 || channels <= 0)
            return false;

        data.setSize (channels, frames, false, true, false);
        if (! reader->read (&data, 0, frames, 0, true, true))
        {
            data.setSize (0, 0);
            return false;
        }

        sampleRate  = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
        numFrames   = frames;
        numChannels = channels;
        return true;
    }
};

} // namespace muni
