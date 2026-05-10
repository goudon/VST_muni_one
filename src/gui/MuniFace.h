// =============================================================================
// MuniFace.h
//
// muni キャラクターの顔 GUI コンポーネント (juce::Component 派生) を宣言する。
// 顔ベース・耳・目・眉・鼻の各画像をネイティブピクセルサイズで重ね描きし、
// 各パーツのドラッグ操作でバインドされた APVTS パラメータを更新する。
// MIDI ノートオン受信時には +8% のパルス拡大アニメーションを実行する。
// juce::AudioProcessorValueTreeState::Listener と juce::Timer を実装。
// 詳細仕様は docs/gui/face.md を参照。
// =============================================================================

#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace muni
{

// muni キャラクターの顔 (ネイティブピクセルサイズの画像レイヤ群) を描画し、
// 各パーツをドラッグするとバインドされた APVTS パラメータが更新される。仕様: docs/gui/face.md。
//
// AsyncUpdater を継承することで、parameterChanged (任意スレッドから呼ばれうる) の
// 通知を MessageManager::callAsync ではなくロックレスフラグ経由で message thread
// に渡し、オーディオスレッドからのヒープ確保を回避する。
class MuniFace : public juce::Component,
                 private juce::AudioProcessorValueTreeState::Listener,
                 private juce::Timer,
                 private juce::AsyncUpdater
{
public:
    MuniFace (juce::AudioProcessorValueTreeState& apvts,
              const std::atomic<int>& noteTriggerCount);
    ~MuniFace() override;

    void paint (juce::Graphics& g) override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;

private:
    enum class Part { None, Nose, EyeLeft, EyeRight, BrowLeft, BrowRight, EarLeft, EarRight };

    // 各 Part がドラッグ中に書き換える RangedAudioParameter のキャッシュ済みポインタ。
    // secondary が nullptr の場合は 1 軸操作 (鼻/耳)、両方非 null の場合は X+Y の
    // 2 軸操作 (目/眉)。mouseDown / mouseDrag / mouseUp の 3 つの switch を 1 か所に
    // 集約する目的。
    struct PartParams { juce::RangedAudioParameter* primary; juce::RangedAudioParameter* secondary; };

    struct LayoutCache
    {
        int faceX, faceY, faceW, faceH;
        juce::Point<int> nose;
        juce::Point<int> eyeL,  eyeR;
        juce::Point<int> browL, browR;
        juce::Point<int> earL,  earR;
        float earLRotRad = 0.0f, earRRotRad = 0.0f;
    };

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;
    void handleAsyncUpdate() override;

    PartParams partParams (Part p) const noexcept;
    float computeBrowYCap (float currentEyeY) const noexcept;

    LayoutCache computeLayout() const;
    Part hitTest (juce::Point<int> p, const LayoutCache& L) const;

    juce::AudioProcessorValueTreeState& apvts;
    const std::atomic<int>& noteTriggerCount;
    int   lastSeenTriggerCount = 0;
    float pulseLevel = 0.0f; // 0..1 — ノートオン時のスケールアニメーションを駆動する

    // パラメータポインタのキャッシュ (ID → RangedAudioParameter*)。コンストラクタで
    // 1 度引いて以後直接使うことで、毎フレームの HashMap ルックアップを排除する。
    juce::RangedAudioParameter* gainParamPtr        = nullptr;
    juce::RangedAudioParameter* attackParamPtr      = nullptr;
    juce::RangedAudioParameter* releaseParamPtr     = nullptr;
    juce::RangedAudioParameter* pitchParamPtr       = nullptr;
    juce::RangedAudioParameter* speedParamPtr       = nullptr;
    juce::RangedAudioParameter* earLeftTiltParamPtr  = nullptr;
    juce::RangedAudioParameter* earRightTiltParamPtr = nullptr;

    juce::Image faceBase;
    juce::Image ear,     earFlipped;
    juce::Image eye;
    juce::Image eyebrow, eyebrowFlipped;
    juce::Image nose;

    juce::StringArray listenedIds; // ctor/dtor の add/removeParameterListener 用にのみ保持

    Part draggedPart = Part::None;
    juce::Point<int> dragMouseStart;
    float dragStartNormA = 0.0f; // ドラッグ開始時のプライマリパラメータの正規化値
    float dragStartNormB = 0.0f; // セカンダリ (目/眉の X+Y コンボでのみ使用)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuniFace)
};

} // namespace muni 終端
