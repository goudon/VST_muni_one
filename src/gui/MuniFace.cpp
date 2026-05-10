// =============================================================================
// MuniFace.cpp
//
// MuniFace の全機能を実装する。BinaryData からの画像リソース読み込み、
// ピクセル単位の水平反転、各パーツ配置のレイアウト計算 (computeLayout)、
// ヒットテスト、ドラッグハンドラ (mouseDown/Drag/Up)、ノートオン用パルスの
// 30 Hz Timer、スケール変換付きの paint を担う。
// 参照: docs/gui/face.md, docs/parameters.md。
// =============================================================================

#include "MuniFace.h"
#include "ParameterIds.h"

#include <BinaryData.h>

#include <cstring>
#include <limits>

namespace muni
{

namespace
{
    // 顔ベース画像 (208 x 196) 内における各パーツの中心ピクセル座標。
    // 命名規約 (耳・眉): kBaseLeft* = キャラクター視点の左 (= 画面右)。
    // 目は左右対称な同一画像を使うので、ラベルは画面側に従う。
    constexpr juce::Point<int> kBaseLeftEar   { 185, 151 };  // 画面右
    constexpr juce::Point<int> kBaseRightEar  {  23, 151 };  // 画面左
    constexpr juce::Point<int> kBaseLeftEye   {  74, 105 };  // 画面左
    constexpr juce::Point<int> kBaseRightEye  { 134, 105 };  // 画面右
    constexpr juce::Point<int> kBaseLeftBrow  { 135,  74 };  // 画面右 (キャラ左)
    constexpr juce::Point<int> kBaseRightBrow {  73,  74 };  // 画面左 (キャラ右)
    constexpr juce::Point<int> kBaseNose      { 104, 130 };

    // 顔ベース内の半振幅視覚レンジ (px)。視覚的な振幅の総量はこれの 2 倍。
    constexpr float kNoseYRangePx  = 12.0f;
    constexpr float kEyeYRangePx   = 10.0f;
    constexpr float kEyeXRangePx   = 16.0f; // ユーザー要望に応じて広めの目の間隔
    constexpr float kBrowYRangePx  = 10.0f;
    constexpr float kBrowXRangePx  = 10.0f;
    constexpr float kEarRotRangeRad = 0.5f; // 約 28 度

    // パルスアニメーション (note-on → 一時的に拡大)。30 Hz Timer・幾何級数減衰。
    constexpr int   kPulseTimerHz       = 30;
    constexpr float kPulsePeakScaleAdd  = 0.08f; // ピーク時 +8%
    constexpr float kPulseDecayPerTick  = 0.70f; // 1 tick ごとに pulse *= 0.70 (≈5 ticks ≒ 165 ms)
    constexpr float kPulseDeadband      = 0.005f;

    // 眉と目の最低ギャップ (px)。眉の縦位置をクランプする計算で使用する。
    constexpr float kBrowEyeGapPx = 1.0f;

    // ドラッグ何 px で正規化レンジ全体 (0..1) を移動するかの感度。小さいほど鋭敏。
    constexpr float kNoseDragPx = 2.0f * kNoseYRangePx;
    constexpr float kEyeYDragPx = 2.0f * kEyeYRangePx;
    constexpr float kEyeXDragPx = 2.0f * kEyeXRangePx;
    constexpr float kBrowYDragPx = 2.0f * kBrowYRangePx;
    constexpr float kBrowXDragPx = 2.0f * kBrowXRangePx;
    constexpr float kEarDragPx  = 80.0f;

    juce::Image loadImage (const char* data, int size)
    {
        return juce::ImageCache::getFromMemory (data, size);
    }

    // BitmapData を介したピクセル単位の水平反転。コンストラクタで 1 度だけ呼ばれ、
    // paint() 側は drawImageAt のみで済むようキャッシュした反転画像を保持する。
    juce::Image flipHorizontally (const juce::Image& src)
    {
        if (src.isNull())
            return {};

        const int w = src.getWidth();
        const int h = src.getHeight();
        juce::Image dst (src.getFormat(), w, h, true);

        const juce::Image::BitmapData srcBmp (src, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData       dstBmp (dst, juce::Image::BitmapData::writeOnly);
        const int pixelStride = srcBmp.pixelStride;

        for (int y = 0; y < h; ++y)
        {
            const auto* srcLine = srcBmp.getLinePointer (y);
            auto*       dstLine = dstBmp.getLinePointer (y);
            for (int x = 0; x < w; ++x)
            {
                std::memcpy (dstLine + x * pixelStride,
                             srcLine + (w - 1 - x) * pixelStride,
                             static_cast<size_t> (pixelStride));
            }
        }
        return dst;
    }

    // 正規化済みパラメータの直接アクセス用ヘルパ。コンストラクタで取得した
    // RangedAudioParameter* に対して操作するため、文字列 ID 検索が走らない。
    float getNorm (const juce::RangedAudioParameter* p) noexcept
    {
        return p != nullptr ? p->getValue() : 0.5f;
    }

    void setNorm (juce::RangedAudioParameter* p, float v)
    {
        if (p != nullptr)
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
    }

    void beginGesture (juce::RangedAudioParameter* p) { if (p != nullptr) p->beginChangeGesture(); }
    void endGesture   (juce::RangedAudioParameter* p) { if (p != nullptr) p->endChangeGesture();   }

    // 正規化値 0..1 を [-range, +range] の中心オフセットに変換。
    float normToOffset (float norm, float range)
    {
        return (norm - 0.5f) * 2.0f * range;
    }

    juce::Rectangle<int> bbox (juce::Point<int> center, const juce::Image& img)
    {
        return { center.x - img.getWidth()  / 2,
                 center.y - img.getHeight() / 2,
                 img.getWidth(), img.getHeight() };
    }
} // namespace

MuniFace::MuniFace (juce::AudioProcessorValueTreeState& apvtsRef,
                    const std::atomic<int>& noteTriggerCountRef)
    : apvts (apvtsRef),
      noteTriggerCount (noteTriggerCountRef)
{
    faceBase = loadImage (BinaryData::muni_fbase_png,    BinaryData::muni_fbase_pngSize);
    ear      = loadImage (BinaryData::muni_ear_png,      BinaryData::muni_ear_pngSize);
    eye      = loadImage (BinaryData::muni_eye_png,      BinaryData::muni_eye_pngSize);
    eyebrow  = loadImage (BinaryData::muni_eye_brow_png, BinaryData::muni_eye_brow_pngSize);
    nose     = loadImage (BinaryData::muni_nose_png,     BinaryData::muni_nose_pngSize);
    earFlipped     = flipHorizontally (ear);
    eyebrowFlipped = flipHorizontally (eyebrow);

    // パラメータポインタを 1 度だけ引いてキャッシュ
    // (毎フレームの HashMap ID 検索を排除する)。
    gainParamPtr         = apvts.getParameter (params::kGain);
    attackParamPtr       = apvts.getParameter (params::kAttack);
    releaseParamPtr      = apvts.getParameter (params::kRelease);
    pitchParamPtr        = apvts.getParameter (params::kPitch);
    speedParamPtr        = apvts.getParameter (params::kSpeed);
    earLeftTiltParamPtr  = apvts.getParameter (params::kEarLeftTilt);
    earRightTiltParamPtr = apvts.getParameter (params::kEarRightTilt);

    listenedIds = {
        params::kGain, params::kAttack, params::kRelease, params::kPitch,
        params::kSpeed, params::kEarLeftTilt, params::kEarRightTilt
    };
    for (auto& id : listenedIds)
        apvts.addParameterListener (id, this);

    lastSeenTriggerCount = noteTriggerCount.load (std::memory_order_relaxed);
    startTimerHz (kPulseTimerHz);
}

MuniFace::~MuniFace()
{
    stopTimer();
    cancelPendingUpdate();
    for (auto& id : listenedIds)
        apvts.removeParameterListener (id, this);
}

void MuniFace::timerCallback()
{
    bool needsRepaint = false;

    // 前回 tick 以降に発生した note-on を検出。
    const int currentCount = noteTriggerCount.load (std::memory_order_relaxed);
    if (currentCount != lastSeenTriggerCount)
    {
        lastSeenTriggerCount = currentCount;
        pulseLevel    = 1.0f; // ピークにスナップ。次の note-on でも常にここから減衰再開。
        needsRepaint  = true;
    }

    // 進行中のパルスを 0 へ向けて減衰。
    if (pulseLevel > 0.0f)
    {
        pulseLevel *= kPulseDecayPerTick;
        if (pulseLevel < kPulseDeadband)
            pulseLevel = 0.0f;
        needsRepaint = true;
    }

    if (needsRepaint)
        repaint();
}

void MuniFace::parameterChanged (const juce::String&, float)
{
    // parameterChanged はオーディオスレッドからも呼ばれ得る (DAW オートメーション等)。
    // MessageManager::callAsync は内部で new するためヒープ確保が起き、CLAUDE.md §3
    // の RT 制約に違反する。AsyncUpdater は内部の atomic フラグだけで通知できるため
    // ヒープ確保せず安全。同一フレーム内に複数回 fired されても message thread での
    // handleAsyncUpdate は 1 度に統合され、過剰な repaint も自動でデバウンスされる。
    triggerAsyncUpdate();
}

void MuniFace::handleAsyncUpdate()
{
    repaint();
}

MuniFace::PartParams MuniFace::partParams (Part p) const noexcept
{
    switch (p)
    {
        case Part::Nose:                            return { gainParamPtr,         nullptr };
        case Part::EyeLeft: case Part::EyeRight:    return { attackParamPtr,       pitchParamPtr };
        case Part::BrowLeft: case Part::BrowRight:  return { releaseParamPtr,      speedParamPtr };
        case Part::EarLeft:                         return { earLeftTiltParamPtr,  nullptr };
        case Part::EarRight:                        return { earRightTiltParamPtr, nullptr };
        case Part::None: default:                   return { nullptr, nullptr };
    }
}

float MuniFace::computeBrowYCap (float currentEyeY) const noexcept
{
    // 眉の底が目の上端を超えないよう、眉 Y オフセットの最大下方値を返す。
    // 目が下に動けばキャップは緩くなり、上に動けば締まる。eye/eyebrow 画像が
    // 未読込の場合は無限大 (= キャップ無効) を返す。
    if (eye.isNull() || eyebrow.isNull())
        return std::numeric_limits<float>::infinity();

    const float browToEyeBaseDistY = static_cast<float> (kBaseLeftEye.y - kBaseLeftBrow.y);
    const float halfHeightsSum     = (eye.getHeight() + eyebrow.getHeight()) * 0.5f;
    return browToEyeBaseDistY - halfHeightsSum - kBrowEyeGapPx + currentEyeY;
}

MuniFace::LayoutCache MuniFace::computeLayout() const
{
    LayoutCache L;
    L.faceW = faceBase.getWidth();
    L.faceH = faceBase.getHeight();
    const auto b = getLocalBounds();
    L.faceX = b.getX() + (b.getWidth()  - L.faceW) / 2;
    L.faceY = b.getY() + (b.getHeight() - L.faceH) / 2;

    // 符号規約は視覚が自然になるよう便宜的に選択 (適当なマッピング):
    //  - 鼻: 下にドラッグで gain 減
    //  - 眉: 上にドラッグで release 増
    //  - 目 X: 中心に寄せると pitch 上昇 (= 反転)
    const float noseY    = -normToOffset (getNorm (gainParamPtr),    kNoseYRangePx);
    const float eyeY     =  normToOffset (getNorm (attackParamPtr),  kEyeYRangePx);
    const float eyeX     = -normToOffset (getNorm (pitchParamPtr),   kEyeXRangePx);
    const float browYRaw = -normToOffset (getNorm (releaseParamPtr), kBrowYRangePx);
    const float browX    = -normToOffset (getNorm (speedParamPtr),   kBrowXRangePx);
    L.earLRotRad =  normToOffset (getNorm (earLeftTiltParamPtr),  kEarRotRangeRad);
    L.earRRotRad =  normToOffset (getNorm (earRightTiltParamPtr), kEarRotRangeRad);

    // 眉 Y を目とのオーバーラップ防止のためキャップ。下方向の制限のみで、上方向は自由。
    const float browY = juce::jmin (browYRaw, computeBrowYCap (eyeY));

    auto offsetPoint = [&] (juce::Point<int> base, float dx, float dy)
    {
        return juce::Point<int> (L.faceX + base.x + juce::roundToInt (dx),
                                 L.faceY + base.y + juce::roundToInt (dy));
    };

    L.nose  = offsetPoint (kBaseNose, 0.0f, noseY);

    // ミラーペア: X オフセットは両側を ±eyeX/browX で外向きに動かす。
    L.eyeL  = offsetPoint (kBaseLeftEye,  -eyeX, eyeY);
    L.eyeR  = offsetPoint (kBaseRightEye,  eyeX, eyeY);
    L.browL = offsetPoint (kBaseLeftBrow,   browX, browY); // 画面右
    L.browR = offsetPoint (kBaseRightBrow, -browX, browY); // 画面左

    L.earL  = offsetPoint (kBaseLeftEar,  0.0f, 0.0f); // 耳は回転のみ
    L.earR  = offsetPoint (kBaseRightEar, 0.0f, 0.0f);

    return L;
}

MuniFace::Part MuniFace::hitTest (juce::Point<int> p, const LayoutCache& L) const
{
    // 手前のパーツから順にチェック。耳の回転後の正確な当たり判定は省略
    // (小角度なら誤差は小さい)。
    if (! nose   .isNull() && bbox (L.nose,  nose   ).contains (p)) return Part::Nose;
    if (! eyebrow.isNull() && bbox (L.browL, eyebrow).contains (p)) return Part::BrowLeft;
    if (! eyebrow.isNull() && bbox (L.browR, eyebrow).contains (p)) return Part::BrowRight;
    if (! eye    .isNull() && bbox (L.eyeL,  eye    ).contains (p)) return Part::EyeLeft;
    if (! eye    .isNull() && bbox (L.eyeR,  eye    ).contains (p)) return Part::EyeRight;
    if (! ear    .isNull() && bbox (L.earL,  ear    ).contains (p)) return Part::EarLeft;
    if (! ear    .isNull() && bbox (L.earR,  ear    ).contains (p)) return Part::EarRight;
    return Part::None;
}

void MuniFace::paint (juce::Graphics& g)
{
    if (faceBase.isNull())
        return;

    const auto L = computeLayout();

    // ノートオン パルスを顔ベース中央を中心に拡大。以後の drawImage 呼び出し
    // (耳の回転含む) は Graphics の現在座標系を引き継ぐため、顔全体が一体で拡大する。
    juce::Graphics::ScopedSaveState pulseState (g);
    if (pulseLevel > 0.0f)
    {
        const float scale  = 1.0f + pulseLevel * kPulsePeakScaleAdd;
        const float pivotX = static_cast<float> (L.faceX) + L.faceW * 0.5f;
        const float pivotY = static_cast<float> (L.faceY) + L.faceH * 0.5f;
        g.addTransform (juce::AffineTransform::scale (scale, scale, pivotX, pivotY));
    }

    auto drawAt = [&] (const juce::Image& img, juce::Point<int> center)
    {
        if (img.isNull()) return;
        const int dx = center.x - img.getWidth()  / 2;
        const int dy = center.y - img.getHeight() / 2;
        g.drawImageAt (img, dx, dy);
    };

    auto drawRotated = [&] (const juce::Image& img, juce::Point<int> center, float rotRad)
    {
        if (img.isNull()) return;
        if (juce::approximatelyEqual (rotRad, 0.0f))
        {
            drawAt (img, center);
            return;
        }
        // ピボットは耳の上端付近 (頭部とつながる位置)。
        const int   w        = img.getWidth();
        const int   h        = img.getHeight();
        const float dx       = static_cast<float> (center.x - w / 2);
        const float dy       = static_cast<float> (center.y - h / 2);
        const float pivotX   = static_cast<float> (center.x);
        const float pivotY   = static_cast<float> (center.y) - h * 0.4f;
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (rotRad, pivotX, pivotY));
        g.drawImageAt (img, juce::roundToInt (dx), juce::roundToInt (dy));
    };

    // 奥から手前 (docs/gui/face.md 参照)。
    drawRotated (earFlipped, L.earR, L.earRRotRad);
    drawRotated (ear,        L.earL, L.earLRotRad);
    g.drawImageAt (faceBase, L.faceX, L.faceY);
    drawAt (eye,            L.eyeL);
    drawAt (eye,            L.eyeR);
    drawAt (eyebrow,        L.browL);
    drawAt (eyebrowFlipped, L.browR);
    drawAt (nose,           L.nose);
}

void MuniFace::mouseDown (const juce::MouseEvent& e)
{
    if (faceBase.isNull())
        return;

    const auto L = computeLayout();
    draggedPart = hitTest (e.getPosition(), L);
    if (draggedPart == Part::None)
        return;

    dragMouseStart = e.getPosition();

    // 対象パラメータの起点正規化値を保存し、ホストへ change gesture を通知。
    const auto pp = partParams (draggedPart);
    dragStartNormA = getNorm (pp.primary);
    dragStartNormB = getNorm (pp.secondary);
    beginGesture (pp.primary);
    beginGesture (pp.secondary);
}

void MuniFace::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedPart == Part::None)
        return;

    const float dx = static_cast<float> (e.x - dragMouseStart.x);
    const float dy = static_cast<float> (e.y - dragMouseStart.y);

    switch (draggedPart)
    {
        case Part::Nose:
        {
            // 下ドラッグ (dy > 0) で gain 減。
            setNorm (gainParamPtr, dragStartNormA - dy / kNoseDragPx);
            break;
        }
        case Part::EyeLeft:
        case Part::EyeRight:
        {
            // Y: 下ドラッグで attack 長く。
            setNorm (attackParamPtr, dragStartNormA + dy / kEyeYDragPx);
            // X: 中心方向に寄せると pitch 上昇 (= 中心寄り = 高ピッチ)。
            // 左目は画面左に位置するため内向き = 右ドラッグ (dx > 0)。
            // 右目は画面右に位置するため内向き = 左ドラッグ (dx < 0)。
            const float inwardSign = (draggedPart == Part::EyeLeft) ? 1.0f : -1.0f;
            setNorm (pitchParamPtr, dragStartNormB + inwardSign * dx / kEyeXDragPx);
            break;
        }
        case Part::BrowLeft:
        case Part::BrowRight:
        {
            // Y: 上ドラッグで release 長く。眉 Y は目とオーバーラップしないよう
            // 現在の目 Y からキャップを再計算してパラメータ自体をクランプする。
            const float candidateNormY = juce::jlimit (0.0f, 1.0f,
                                                       dragStartNormA - dy / kBrowYDragPx);
            const float candidateBrowY = -normToOffset (candidateNormY, kBrowYRangePx);
            const float currentEyeY    = normToOffset (getNorm (attackParamPtr), kEyeYRangePx);
            const float clampedBrowY   = juce::jmin (candidateBrowY, computeBrowYCap (currentEyeY));
            const float clampedNormY   = juce::jlimit (0.0f, 1.0f,
                                                       0.5f - clampedBrowY / (2.0f * kBrowYRangePx));
            setNorm (releaseParamPtr, clampedNormY);

            // X: 中心方向に寄せると speed 上昇。
            // BrowLeft (kBaseLeftBrow) は画面右なので内向き = 左ドラッグ (dx < 0)。
            // BrowRight (kBaseRightBrow) は画面左なので内向き = 右ドラッグ (dx > 0)。
            const float inwardSign = (draggedPart == Part::BrowLeft) ? -1.0f : 1.0f;
            setNorm (speedParamPtr, dragStartNormB + inwardSign * dx / kBrowXDragPx);
            break;
        }
        case Part::EarLeft:
        {
            // 横ドラッグで回転。左右の耳は独立。
            setNorm (earLeftTiltParamPtr, dragStartNormA + dx / kEarDragPx);
            break;
        }
        case Part::EarRight:
        {
            setNorm (earRightTiltParamPtr, dragStartNormA + dx / kEarDragPx);
            break;
        }
        case Part::None: default: break;
    }
}

void MuniFace::mouseUp (const juce::MouseEvent&)
{
    if (draggedPart == Part::None)
        return;

    const auto pp = partParams (draggedPart);
    endGesture (pp.primary);
    endGesture (pp.secondary);
    draggedPart = Part::None;
}

} // namespace muni 終端
