// =============================================================================
// HelpPanel.cpp
//
// HelpPanel の実装。半透明黒背景 + 黄色枠 + 等幅フォントのテキストで、
// パーツ → パラメータの対応を英語表示する。テキストは固定文字列で、APVTS
// の値変化には反応しない (現在値表示は ParameterOverlay の責務)。
// =============================================================================

#include "HelpPanel.h"

namespace muni
{

HelpPanel::HelpPanel()
{
    // クリックスルー: ヘルプ表示中も背後の顔をドラッグ操作できる。
    setInterceptsMouseClicks (false, false);
}

void HelpPanel::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    g.setColour (juce::Colours::black.withAlpha (0.80f));
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (juce::Colours::yellow);
    g.drawRoundedRectangle (area.reduced (1.0f), 8.0f, 1.5f);

    g.setColour (juce::Colours::yellow);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    auto headerArea = area.reduced (12.0f, 10.0f).removeFromTop (18.0f);
    g.drawText ("Drag a face part:",
                headerArea, juce::Justification::centredLeft, false);

    // 小さなエディタウィンドウに収まるように圧縮した ASCII テーブル。
    static constexpr const char* lines[] = {
        "",
        "  Nose   Y         ->  Gain",
        "  Eye    Y         ->  Attack",
        "         X (in)    ->  Pitch",
        "  Brow   Y         ->  Release",
        "         X (in)    ->  Speed",
        "  Ear L  X         ->  Ear Left Tilt",
        "  Ear R  X         ->  Ear Right Tilt",
        "",
        "  Eyes/Brows mirror L/R",
        "  Closer eyes = higher pitch",
        "  Closer brows = faster speed",
    };

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                  11.0f, juce::Font::plain));

    auto textArea = area.reduced (12.0f, 32.0f);
    constexpr float lineHeight = 14.0f;
    float y = textArea.getY();
    for (const char* line : lines)
    {
        g.drawText (juce::String (line),
                    juce::Rectangle<float> (textArea.getX(), y,
                                            textArea.getWidth(), lineHeight),
                    juce::Justification::centredLeft, false);
        y += lineHeight;
    }
}

} // namespace muni
