// =============================================================================
// PluginEditor.cpp
//
// MuniAudioProcessorEditor の実装。レイアウト定数の定義、上部 Polyphonic トグル /
// P / ? ボタンの配置とトグル動作の配線、APVTS への ButtonAttachment バインドを担う。
// HelpPanel と ParameterOverlay の paint 実装は src/gui/ 配下に分離済み。
// 詳細は docs/gui/layout.md を参照。
// =============================================================================

#include "PluginEditor.h"
#include "ParameterIds.h"

namespace muni
{

namespace
{
    // muni 顔 (208 x 196 px) を中央に置き、耳が ±0.5 rad 最大回転 + ノートオン
    // パルス (+8%) で広がっても見切れない余白を確保したサイズ。
    // ローカル座標で顔ベース端〜耳 bbox 端: 標準時 +32 / 回転時 +52 / パルス時 +60 px。
    // 左右各 60 px ずつの余白を取りたいので 顔ベース 208 + 60*2 = 328、ボタン
    // 配置や上下マージンも含めて 360 x 320 に設定。
    constexpr int kEditorWidth     = 360;
    constexpr int kEditorHeight    = 320;
    constexpr int kMargin          = 10;
    constexpr int kTopButtonHeight = 28;
    constexpr int kHelpButtonSize  = 32;
    constexpr int kPolyphonicWidth = 110;
    constexpr int kTopButtonGap    = 4;   // ? と P の間
    constexpr int kFaceTopSpacing  = 4;   // 上部バーと顔エリアの間
}

MuniAudioProcessorEditor::MuniAudioProcessorEditor (MuniAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      face (p.getApvts(), p.getNoteTriggerCount()),
      paramOverlay (p.getApvts())
{
    setSize (kEditorWidth, kEditorHeight);

    addAndMakeVisible (face);

    polyphonicButton.setButtonText ("Polyphonic");
    addAndMakeVisible (polyphonicButton);

    auto styleTopButton = [] (juce::TextButton& b, const juce::String& text, const juce::String& tip)
    {
        b.setButtonText (text);
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId,   juce::Colours::yellow.withAlpha (0.85f));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colours::yellow);
        b.setColour (juce::TextButton::textColourOffId,  juce::Colours::black);
        b.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
        b.setTooltip (tip);
    };

    styleTopButton (helpButton, "?", "Show / hide controls help");
    helpButton.onClick = [this]
    {
        helpPanel.setVisible (helpButton.getToggleState());
    };
    addAndMakeVisible (helpButton);

    styleTopButton (pButton, "P", "Show / hide current parameter values");
    pButton.onClick = [this]
    {
        paramOverlay.setVisible (pButton.getToggleState());
    };
    addAndMakeVisible (pButton);

    // 両オーバーレイは初期非表示。トグルボタンで表示を切り替える。
    // 子追加順 = Z 順なので、両方表示時は HelpPanel が前面になる。
    addChildComponent (paramOverlay);
    addChildComponent (helpPanel);

    polyphonicAttachment = std::make_unique<ButtonAttachment> (
        processorRef.getApvts(),
        params::kPolyphonic,
        polyphonicButton);
}

void MuniAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
}

void MuniAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    polyphonicButton.setBounds (kMargin, kMargin,
                                kPolyphonicWidth, kTopButtonHeight);

    // 右上: P (パラメータ) は ? (ヘルプ) の左隣。
    helpButton.setBounds (area.getRight() - kHelpButtonSize - kMargin, kMargin,
                          kHelpButtonSize, kHelpButtonSize);
    pButton.setBounds (helpButton.getX() - kHelpButtonSize - kTopButtonGap, kMargin,
                       kHelpButtonSize, kHelpButtonSize);

    // 顔は上部バーの下に配置されるエディタ全体を占有する。
    auto faceArea = area.reduced (kMargin);
    faceArea.removeFromTop (kHelpButtonSize + kFaceTopSpacing);
    face.setBounds (faceArea);

    // 両オーバーレイは顔エリア全体を覆い、上部ボタンはクリック可能なまま残す。
    helpPanel    .setBounds (faceArea);
    paramOverlay .setBounds (faceArea);
}

} // namespace muni
