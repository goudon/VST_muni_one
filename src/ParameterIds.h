// =============================================================================
// ParameterIds.h
//
// APVTS パラメータの ID 文字列定数を一元管理するヘッダ。
// `MuniAudioProcessor` ヘッダ (juce::AudioProcessor 等の重い依存を含む) に
// 依存させず、GUI コンポーネント等から ID 文字列だけを参照できるようにする。
// 全 ID は docs/parameters.md と完全に同期させること。
// =============================================================================

#pragma once

namespace muni::params
{
    // DSP パラメータ
    inline constexpr const char* kGain        = "gain";
    inline constexpr const char* kAttack      = "attack";
    inline constexpr const char* kRelease     = "release";
    inline constexpr const char* kPitch       = "pitch";
    inline constexpr const char* kSpeed       = "speed";
    inline constexpr const char* kPolyphonic  = "polyphonic";

    // GUI 専用 (現状 DSP に未接続だが APVTS には保存される)
    inline constexpr const char* kEarLeftTilt  = "ear_left_tilt";
    inline constexpr const char* kEarRightTilt = "ear_right_tilt";
}
