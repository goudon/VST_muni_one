// =============================================================================
// PluginProcessor.cpp
//
// MuniAudioProcessor の実装ファイル。コンストラクタでの APVTS 初期化と
// Synthesiser へのボイス/サウンド登録、createParameterLayout でのパラメータ定義、
// processBlock での MIDI 走査・note-on カウント加算・モノ/ポリ切替時のゲート処理・
// voice 駆動・SmoothedValue を用いたゲイン適用、そして getStateInformation /
// setStateInformation による状態の保存と復元を実装する。
// 詳細仕様は docs/architecture.md を参照。
// =============================================================================

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/MuniVoice.h" // MuniSound も同ヘッダに同居

#include <BinaryData.h>

namespace muni
{

MuniAudioProcessor::MuniAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    gainParam       = apvts.getRawParameterValue (params::kGain);
    attackParam     = apvts.getRawParameterValue (params::kAttack);
    releaseParam    = apvts.getRawParameterValue (params::kRelease);
    pitchParam      = apvts.getRawParameterValue (params::kPitch);
    speedParam      = apvts.getRawParameterValue (params::kSpeed);
    polyphonicParam = apvts.getRawParameterValue (params::kPolyphonic);

    auto loadedBuffer = std::make_shared<SampleBuffer>();
    loadedBuffer->loadFromBinary (BinaryData::dog_wav, BinaryData::dog_wavSize);
    sampleBuffer = std::move (loadedBuffer);

    synth.addSound (new MuniSound());
    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new MuniVoice (sampleBuffer,
                                       attackParam,
                                       releaseParam,
                                       pitchParam,
                                       speedParam));
}

juce::AudioProcessorValueTreeState::ParameterLayout
MuniAudioProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { params::kGain, 1 },
        "Gain",
        Range { -15.0f, 15.0f, 0.01f },
        -12.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    auto attackRange  = Range { 0.001f, 2.0f, 0.0001f };
    attackRange.setSkewForCentre (0.1f);
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { params::kAttack, 1 },
        "Attack",
        attackRange,
        0.01f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    auto releaseRange = Range { 0.001f, 5.0f, 0.0001f };
    releaseRange.setSkewForCentre (0.3f);
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { params::kRelease, 1 },
        "Release",
        releaseRange,
        0.2f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { params::kPitch, 1 },
        "Pitch",
        Range { -24.0f, 24.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("st")));

    auto speedRange = Range { 0.25f, 4.0f, 0.001f };
    speedRange.setSkewForCentre (1.0f);
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { params::kSpeed, 1 },
        "Speed",
        speedRange,
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("x")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { params::kPolyphonic, 1 },
        "Polyphonic",
        true));

    // キャラクター顔 GUI 用の視覚専用パラメータ (耳の回転角、deg)。DAW のオートメ
    // ーションや状態保存を活かすため APVTS に格納するが、現状 DSP では未使用。
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { params::kEarLeftTilt, 1 },
        "Ear Left Tilt",
        juce::NormalisableRange<float> { -45.0f, 45.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("deg")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { params::kEarRightTilt, 1 },
        "Ear Right Tilt",
        juce::NormalisableRange<float> { -45.0f, 45.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("deg")));

    return { params.begin(), params.end() };
}

void MuniAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    gainSmoothed.reset (sampleRate, 0.05);
    // ゲイン範囲は -15..+15 dB。"最小値 == ミュート" という特別扱いは不要
    // (JUCE デフォルトの -100 dB を負の無限大として利用)。
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));

    // RT セーフ性: processBlock のモノフォニックモードで gatedMidi.addEvent が
    // ヒープ再確保しないよう、想定最大サイズを事前に確保する (CLAUDE.md §3)。
    // 1 イベントあたり最大 ~16 バイト。samplesPerBlock 個の MIDI イベントが詰まる
    // 最悪ケースを想定し、十分な余裕を取って確保する。
    const auto reserveBytes = static_cast<size_t> (juce::jmax (256, samplesPerBlock) * 16);
    gatedMidi.ensureSize (reserveBytes);
}

void MuniAudioProcessor::releaseResources() {}

bool MuniAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::stereo();
}

void MuniAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumOutputChannels = getTotalNumOutputChannels();
    for (int ch = getTotalNumInputChannels(); ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    buffer.clear();

    // GUI が顔をパルスアニメーションさせるため、本ブロック内の note-on 数を集計。
    // relaxed atomic を使用 — GUI 側は単にポーリングするだけなので厳密な順序は不要。
    int noteOnsThisBlock = 0;
    for (const auto event : midiMessages)
        if (event.getMessage().isNoteOn())
            ++noteOnsThisBlock;
    if (noteOnsThisBlock > 0)
        noteTriggerCount.fetch_add (noteOnsThisBlock, std::memory_order_relaxed);

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    const bool polyphonic = polyphonicParam != nullptr
                                ? polyphonicParam->load() >= 0.5f
                                : true;

    if (polyphonic)
    {
        synth.renderNextBlock (buffer, midiMessages, 0, numSamples);
    }
    else
    {
        // モノフォニック動作: 新しい noteOn が到着した瞬間に発音中のボイスを止め、
        // 同時には常に 1 音のみが鳴るようにする。レンダリングを noteOn を境に区間
        // 分割し、各 noteOn の直前で allNotesOff (tail-off 付き) を呼び出す。
        gatedMidi.clear();
        int currentSample = 0;

        for (const auto event : midiMessages)
        {
            const auto& msg          = event.getMessage();
            const int   eventSample  = juce::jlimit (0, numSamples, event.samplePosition);

            if (msg.isNoteOn())
            {
                const int segLength = eventSample - currentSample;
                if (segLength > 0)
                    synth.renderNextBlock (buffer, gatedMidi, currentSample, segLength);

                gatedMidi.clear();
                synth.allNotesOff (msg.getChannel(), true);
                currentSample = eventSample;
            }

            gatedMidi.addEvent (msg, eventSample);
        }

        const int remaining = numSamples - currentSample;
        if (remaining > 0)
            synth.renderNextBlock (buffer, gatedMidi, currentSample, remaining);

        gatedMidi.clear();
    }

    const float targetGain = juce::Decibels::decibelsToGain (gainParam->load());
    gainSmoothed.setTargetValue (targetGain);

    // SmoothedValue::applyGain がチャネルポインタを内部で取得し全チャネルに per-sample
    // 補間ゲインを掛ける。手書きの 2 重ループ (setSample/getSample 経由) よりも
    // メモリアクセス順序が良くキャッシュ効率も高い。
    juce::ignoreUnused (numChannels);
    gainSmoothed.applyGain (buffer, numSamples);
}

juce::AudioProcessorEditor* MuniAudioProcessor::createEditor()
{
    return new MuniAudioProcessorEditor (*this);
}

void MuniAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MuniAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace muni 終端

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new muni::MuniAudioProcessor();
}
