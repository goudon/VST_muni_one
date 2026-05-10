// =============================================================================
// PluginEditor.h
//
// MuniAudioProcessorEditor (GUI ルート) を宣言するヘッダ。
// レイアウトは「上部バー (Polyphonic + P + ?) + 中央 MuniFace」で、ヘルプ
// オーバーレイとパラメータ表示オーバーレイは src/gui/ 配下の独立クラス
// (HelpPanel, ParameterOverlay) に切り出している。
// 詳細は docs/gui/layout.md を参照。
// =============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "gui/MuniFace.h"
#include "gui/HelpPanel.h"
#include "gui/ParameterOverlay.h"

namespace muni
{

class MuniAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MuniAudioProcessorEditor (MuniAudioProcessor& p);
    ~MuniAudioProcessorEditor() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    MuniAudioProcessor& processorRef;

    MuniFace           face;
    juce::ToggleButton polyphonicButton;
    juce::TextButton   helpButton;
    juce::TextButton   pButton;
    HelpPanel          helpPanel;
    ParameterOverlay   paramOverlay;

    std::unique_ptr<ButtonAttachment> polyphonicAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuniAudioProcessorEditor)
};

} // namespace muni
