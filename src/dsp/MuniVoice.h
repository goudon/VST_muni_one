// =============================================================================
// MuniVoice.h
//
// `MuniVoice` (juce::SynthesiserVoice 派生) を宣言する。MIDI ノート番号から
// noteRatio を算出し、pitch/speed パラメータと乗算して再生レートを決定して
// SampleBuffer から線形補間で読み出す。AR エンベロープ (Attack/Sustain/Release)
// により発音/消音時のクリックを防止する。リアルタイムスレッド上で動作するため、
// メモリ割り当て・ロック・例外の使用は禁止。
// 関連ドキュメント: docs/dsp/voice.md, docs/dsp/envelope.md
// =============================================================================
#pragma once

#include <atomic>
#include <cmath>
#include <memory>
#include <juce_audio_basics/juce_audio_basics.h>

#include "SampleBuffer.h"

namespace muni
{

// MuniVoice とペアで使う最小限の SynthesiserSound。全 MIDI ノート / 全チャンネルを
// 受理する。単独ファイルにするほどの責務がないため MuniVoice.h に同居させている。
class MuniSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote    (int /*midiNoteNumber*/) override { return true; }
    bool appliesToChannel (int /*midiChannel*/)    override { return true; }
};

class MuniVoice : public juce::SynthesiserVoice
{
public:
    MuniVoice (std::shared_ptr<const SampleBuffer> sample,
               std::atomic<float>* attackSecParam,
               std::atomic<float>* releaseSecParam,
               std::atomic<float>* pitchSemisParam,
               std::atomic<float>* speedParam) noexcept
        : sampleBuffer (std::move (sample)),
          attackParam  (attackSecParam),
          releaseParam (releaseSecParam),
          pitchParam   (pitchSemisParam),
          speedParam   (speedParam)
    {
    }

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<MuniSound*> (sound) != nullptr;
    }

    // サンプルの「基準」ピッチ。MIDI ノートはここからの半音オフセットとして
    // 解釈される。kRootMidiNote より上のノートは速く (高く)、下のノートは
    // 遅く (低く) 再生される。C4 == 60 == ミドル C。
    static constexpr int kRootMidiNote = 60;

    void startNote (int midiNoteNumber,
                    float velocity,
                    juce::SynthesiserSound* /*sound*/,
                    int /*currentPitchWheelPosition*/) override
    {
        currentNoteNumber = juce::jlimit (0, 127, midiNoteNumber);
        currentVelocity   = juce::jlimit (0.0f, 1.0f, velocity);
        position          = 0.0;
        envLevel          = 0.0f;
        envState          = EnvState::Attack;
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            envState = EnvState::Release;
        }
        else
        {
            envLevel = 0.0f;
            envState = EnvState::Idle;
            position = 0.0;
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample,
                          int numSamples) override
    {
        if (envState == EnvState::Idle)
            return;

        if (sampleBuffer == nullptr || sampleBuffer->numFrames <= 0)
        {
            envState = EnvState::Idle;
            clearCurrentNote();
            return;
        }

        const double hostSR = getSampleRate();
        if (hostSR <= 0.0)
            return;

        const float attackSec  = juce::jmax (0.0001f, attackParam  != nullptr ? attackParam->load()  : 0.01f);
        const float releaseSec = juce::jmax (0.0001f, releaseParam != nullptr ? releaseParam->load() : 0.2f);
        const float pitchSemis = juce::jlimit (-24.0f, 24.0f, pitchParam != nullptr ? pitchParam->load() : 0.0f);
        const float speed      = juce::jlimit (0.25f, 4.0f,    speedParam != nullptr ? speedParam->load() : 1.0f);

        const float attackInc  = static_cast<float> (1.0 / (attackSec  * hostSR));
        const float releaseDec = static_cast<float> (1.0 / (releaseSec * hostSR));

        const double noteRatio   = std::pow (2.0,
                                              static_cast<double> (currentNoteNumber - kRootMidiNote) / 12.0);
        const double pitchMult   = std::pow (2.0, pitchSemis / 12.0);
        const double positionInc = juce::jmax (1.0e-4,
                                               noteRatio * pitchMult * static_cast<double> (speed)
                                                   * (sampleBuffer->sampleRate / hostSR));

        const int   srcFrames   = sampleBuffer->numFrames;
        const int   srcChannels = sampleBuffer->numChannels;
        const int   outChannels = outputBuffer.getNumChannels();

        // 入力/出力チャネルポインタをループ外で取得して、per-sample の getReadPointer /
        // addSample による重複オーバーヘッドと境界チェックを排除する。
        const float* const src0 = sampleBuffer->data.getReadPointer (0);
        const float* const src1 = (srcChannels > 1) ? sampleBuffer->data.getReadPointer (1) : src0;
        float* const       out0 = outputBuffer.getWritePointer (0) + startSample;
        float* const       out1 = (outChannels > 1) ? outputBuffer.getWritePointer (1) + startSample : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            switch (envState)
            {
                case EnvState::Attack:
                    envLevel += attackInc;
                    if (envLevel >= 1.0f) { envLevel = 1.0f; envState = EnvState::Sustain; }
                    break;
                case EnvState::Sustain:
                    envLevel = 1.0f;
                    break;
                case EnvState::Release:
                    envLevel -= releaseDec;
                    if (envLevel <= 0.0f) { envLevel = 0.0f; envState = EnvState::Idle; }
                    break;
                case EnvState::Idle:
                    break;
            }

            const auto i0 = static_cast<int> (position);
            const auto i1 = i0 + 1;

            // 長時間再生で `position` が `int` を超える / 負に折り返す事象を防ぐため
            // 両端ガード。サンプル末尾到達と同じ扱いで envelope を Release 経由でなく
            // 即時 Idle に落とす。
            if (i0 < 0 || i1 >= srcFrames)
            {
                envLevel = 0.0f;
                envState = EnvState::Idle;
            }
            else
            {
                const auto  frac = static_cast<float> (position - static_cast<double> (i0));
                const float gain = envLevel * currentVelocity;
                const float invFrac = 1.0f - frac;

                if (outChannels == 1)
                {
                    // モノ出力: ソース全チャンネルを平均してから加算。
                    float acc = invFrac * src0[i0] + frac * src0[i1];
                    if (srcChannels > 1)
                    {
                        acc += invFrac * src1[i0] + frac * src1[i1];
                        acc *= 0.5f;
                    }
                    out0[i] += acc * gain;
                }
                else
                {
                    // ステレオ出力。srcChannels >= 2 なら src1 は 2ch 目、=1 なら src1 == src0。
                    const float s0 = (invFrac * src0[i0] + frac * src0[i1]) * gain;
                    const float s1 = (invFrac * src1[i0] + frac * src1[i1]) * gain;
                    out0[i] += s0;
                    out1[i] += s1;
                }

                position += positionInc;
            }

            if (envState == EnvState::Idle)
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    enum class EnvState { Idle, Attack, Sustain, Release };

    std::shared_ptr<const SampleBuffer> sampleBuffer;
    std::atomic<float>* attackParam  = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* pitchParam   = nullptr;
    std::atomic<float>* speedParam   = nullptr;

    EnvState envState          = EnvState::Idle;
    float    envLevel          = 0.0f;
    float    currentVelocity   = 1.0f;
    double   position          = 0.0;
    int      currentNoteNumber = kRootMidiNote;
};

} // namespace muni
