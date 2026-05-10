// =============================================================================
// ParameterOverlay.h
//
// 顔の上に現在の生パラメータ値を半透明の白テキストで薄く表示するオーバーレイ。
// PluginEditor が `P` ボタンのトグルで visibility を切り替える。クリックスルー。
// 監視中のパラメータが変化するたびに repaint する (APVTS リスナー +
// AsyncUpdater で audio thread からのヒープ確保を回避)。
// 関連ドキュメント: docs/gui/layout.md, docs/parameters.md
// =============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace muni
{

class ParameterOverlay : public juce::Component,
                         private juce::AudioProcessorValueTreeState::Listener,
                         private juce::AsyncUpdater
{
public:
    explicit ParameterOverlay (juce::AudioProcessorValueTreeState& apvts);
    ~ParameterOverlay() override;

    void paint (juce::Graphics& g) override;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState& apvts;
    juce::StringArray listenedIds; // ctor/dtor の add/removeParameterListener 用にのみ保持

    // RangedAudioParameter ポインタキャッシュ。paint() の per-frame で
    // apvts.getRawParameterValue(id) (HashMap 検索) を呼ばないため。
    const juce::RangedAudioParameter* gainP     = nullptr;
    const juce::RangedAudioParameter* attackP   = nullptr;
    const juce::RangedAudioParameter* releaseP  = nullptr;
    const juce::RangedAudioParameter* pitchP    = nullptr;
    const juce::RangedAudioParameter* speedP    = nullptr;
    const juce::RangedAudioParameter* earLeftP  = nullptr;
    const juce::RangedAudioParameter* earRightP = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterOverlay)
};

} // namespace muni
