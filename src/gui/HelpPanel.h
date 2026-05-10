// =============================================================================
// HelpPanel.h
//
// 顔のパーツとパラメータの対応を一覧表示する半透明オーバーレイの宣言。
// PluginEditor が `?` ボタンのトグルで visibility を切り替える。
// クリックスルー (setInterceptsMouseClicks(false, false)) なので、表示中も
// 顔のドラッグ操作はそのまま継続できる。
// 関連ドキュメント: docs/gui/layout.md
// =============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace muni
{

class HelpPanel : public juce::Component
{
public:
    HelpPanel();
    void paint (juce::Graphics& g) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpPanel)
};

} // namespace muni
