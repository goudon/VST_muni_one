// =============================================================================
// PluginProcessor.h
//
// VST_muni_one プラグイン本体である MuniAudioProcessor (juce::AudioProcessor 派生)
// を宣言する。APVTS で扱うパラメータ ID 群、note-on カウンタ (GUI ポーリング用)、
// 内部に保持する juce::Synthesiser とサンプルバッファ、ゲイン用 SmoothedValue 等の
// メンバを定義し、ホスト DAW との橋渡しを担う。
// 詳細仕様は docs/architecture.md および docs/parameters.md を参照。
// =============================================================================

#pragma once

#include <atomic>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "ParameterIds.h"
#include "dsp/SampleBuffer.h"

namespace muni
{

class MuniAudioProcessor : public juce::AudioProcessor
{
public:
    MuniAudioProcessor();
    ~MuniAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VST_muni_one"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override           { return 1; }
    int getCurrentProgram() override        { return 0; }
    void setCurrentProgram (int) override   {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }

    // processBlock() 内で MIDI note-on を受信するたびに単調増加する。GUI 側はこの
    // 値をアトミックにポーリングし、muni の顔に短い "パルス" スケールアニメーションを
    // 駆動する。relaxed な atomic 加算のみで RT セーフ。
    const std::atomic<int>& getNoteTriggerCount() const noexcept { return noteTriggerCount; }

    // パラメータ ID は ParameterIds.h (namespace muni::params) で管理。
    // ID 文字列だけが必要な GUI コンポーネントは PluginProcessor.h ではなく
    // ParameterIds.h を直接 include すれば足りる。

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    juce::Synthesiser synth;

    std::shared_ptr<const SampleBuffer> sampleBuffer;

    std::atomic<float>* gainParam       = nullptr;
    std::atomic<float>* attackParam     = nullptr;
    std::atomic<float>* releaseParam    = nullptr;
    std::atomic<float>* pitchParam      = nullptr;
    std::atomic<float>* speedParam      = nullptr;
    std::atomic<float>* polyphonicParam = nullptr;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSmoothed;

    juce::MidiBuffer gatedMidi;

    std::atomic<int> noteTriggerCount { 0 };

    static constexpr int kNumVoices = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuniAudioProcessor)
};

} // namespace muni 終端
