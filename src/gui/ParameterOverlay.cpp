// =============================================================================
// ParameterOverlay.cpp
//
// ParameterOverlay の実装。コンストラクタで監視対象 ID のリスナーを登録し、
// 各 RangedAudioParameter ポインタをキャッシュ。parameterChanged は AsyncUpdater
// 経由で repaint をトリガする (audio thread からの呼び出しでもヒープ確保なし)。
// paint では生パラメータ値を formatted で 7 行表示する。
// =============================================================================

#include "ParameterOverlay.h"
#include "../ParameterIds.h"

namespace muni
{

ParameterOverlay::ParameterOverlay (juce::AudioProcessorValueTreeState& a)
    : apvts (a)
{
    setInterceptsMouseClicks (false, false);

    listenedIds = {
        params::kGain, params::kAttack, params::kRelease, params::kPitch,
        params::kSpeed, params::kEarLeftTilt, params::kEarRightTilt
    };
    for (auto& id : listenedIds)
        apvts.addParameterListener (id, this);

    // RangedAudioParameter ポインタを 1 度だけ引いてキャッシュ。
    gainP     = apvts.getParameter (params::kGain);
    attackP   = apvts.getParameter (params::kAttack);
    releaseP  = apvts.getParameter (params::kRelease);
    pitchP    = apvts.getParameter (params::kPitch);
    speedP    = apvts.getParameter (params::kSpeed);
    earLeftP  = apvts.getParameter (params::kEarLeftTilt);
    earRightP = apvts.getParameter (params::kEarRightTilt);
}

ParameterOverlay::~ParameterOverlay()
{
    cancelPendingUpdate();
    for (auto& id : listenedIds)
        apvts.removeParameterListener (id, this);
}

void ParameterOverlay::parameterChanged (const juce::String&, float)
{
    // オーディオスレッドからも呼ばれ得る (DAW オートメーション)。
    // MessageManager::callAsync は内部 new でヒープ確保するため RT 不適。
    // AsyncUpdater は atomic フラグのみで通知し、連続通知は自動デバウンスされる。
    triggerAsyncUpdate();
}

void ParameterOverlay::handleAsyncUpdate()
{
    repaint();
}

void ParameterOverlay::paint (juce::Graphics& g)
{
    auto getRaw = [] (const juce::RangedAudioParameter* p) -> float
    {
        return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f;
    };

    const juce::String lines[] =
    {
        juce::String::formatted ("Gain    %+6.2f dB",  getRaw (gainP)),
        juce::String::formatted ("Attack   %5.3f s",   getRaw (attackP)),
        juce::String::formatted ("Release  %5.3f s",   getRaw (releaseP)),
        juce::String::formatted ("Pitch   %+6.2f st",  getRaw (pitchP)),
        juce::String::formatted ("Speed    %5.2f x",   getRaw (speedP)),
        juce::String::formatted ("Ear L   %+6.1f deg", getRaw (earLeftP)),
        juce::String::formatted ("Ear R   %+6.1f deg", getRaw (earRightP)),
    };

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                  10.5f, juce::Font::plain));

    auto area = getLocalBounds().toFloat().reduced (8.0f);
    constexpr float lineHeight = 13.0f;
    float y = area.getY();
    for (const auto& line : lines)
    {
        g.drawText (line,
                    juce::Rectangle<float> (area.getX(), y,
                                            area.getWidth(), lineHeight),
                    juce::Justification::topLeft, false);
        y += lineHeight;
    }
}

} // namespace muni
