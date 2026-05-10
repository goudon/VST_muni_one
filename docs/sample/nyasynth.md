# nyasynth — 参考シンセ仕様

参照: <https://github.com/a2aaron/nyasynth> (GitHub, Rust + nih-plug)
情報源: `src/params.rs`, `src/sound_gen.rs` (master ブランチ, 2026-05-07 取得)
ライセンス・取扱: 本ドキュメントは仕様参照のためのまとめであり、コード等は流用していない。

> nyasynth は Meowsynth (32-bit 限定で現代 DAW では動かない) を 64-bit 対応で
> 再現することを目的とした OSS シンセ。「猫の鳴き声」風サンプル/シンセ音色を
> ADSR で整形し、ビブラートとフィルタで質感を作るのが特徴。

## アーキテクチャ概要

| 項目 | 内容 |
|------|------|
| オシレータ | bandlimited オシレータ + PolyBLEP アンチエイリアス。primary は鋸歯波。サブで sine / saw / triangle。xorshift ベースの白色ノイズをミックス可。 |
| フィルタ | Biquad ローパス (Direct Form 1)。カットオフは ADSR エンベロープと velocity で変調。16 sample ごとに係数更新。 |
| エンベロープ | 複数 ADSR (volume, filter cutoff, vibrato attack)。各 stage で easing 関数を選べる。 |
| モジュレーション | portamento + pitch bend + velocity 由来 vibrato が pitch を変調。filter envelope が cutoff をスイープ。 |
| 特徴 | リトリガー時のクロスフェード、PolyBLEP によるアンチエイリアス、ノイズ混合、リズム同期可能なビブラート。 |

## パラメータ一覧 (操作可能項目)

### Master

| 表示名 | ID | 型 | 範囲 | 既定値 | 単位 |
|-------|----|----|------|--------|------|
| Master Volume | `gain` | float | -36.0 〜 12.0 | -6.0 | dB |

### Volume Envelope (ADSR)

| 表示名 | ID | 型 | 範囲 | 既定値 | 単位 |
|-------|----|----|------|--------|------|
| Meow Attack  | `meow_attack`  | float | 0.001 〜 10.0 | 0.03 | sec |
| Meow Decay   | `meow_decay`   | float | 0.001 〜 5.0  | 1.25 | sec |
| Meow Sustain | `meow_sustain` | float | -24.0 〜 0.0  | -15.0 | dB  |
| Meow Release | `meow_release` | float | 0.001 〜 4.0  | 0.49 | sec |

### Vibrato (LFO 系)

| 表示名 | ID | 型 | 範囲 / 選択肢 | 既定値 | 単位 |
|-------|----|----|----------------|--------|------|
| Vibrato Amount      | `vibrato_amount`      | float | 0.0 〜 1.0 | 0.0 | % |
| Vibrato Attack      | `vibrato_attack`      | float | 0.001 〜 5.0 | 0.0 | sec |
| Vibrato Rate        | `vibrato_rate`        | enum  | 4 bar / 2 bar / 1 bar / 1/2 / 1/4 / 1/8 / 1/12 / 1/16 | 1/8 | テンポ同期 |
| Vibrato Note Shape  | `vibrato_note_shape`  | enum  | (波形) | Triangle | — |

### Pitch / Portamento

| 表示名 | ID | 型 | 範囲 | 既定値 | 単位 |
|-------|----|----|------|--------|------|
| Pitchbend  | `pitch_bend`      | int   | 1 〜 12        | 12   | semitone |
| Portamento | `portamento_time` | float | 0.0001 〜 5.0  | 0.12 | sec |
| Polycat    | `polycat`         | bool  | On / Off       | Off  | — (poly トグル) |

### Filter

| 表示名 | ID | 型 | 範囲 | 既定値 | 単位 |
|-------|----|----|------|--------|------|
| Filter Cutoff   | `filter_cutoff_freq`  | float | 20.0 〜 22100.0 | 350.0  | Hz |
| Filter Q        | `filter_q`            | float | 0.01 〜 10.0    | 2.5    | — |
| Filter Type     | `filter_type`         | enum  | LowPass 他       | LowPass | — |
| Filter Dry/Wet  | `filter_dry_wet`      | float | 0.0 〜 1.0      | 1.0    | % |
| Filter EnvMod   | `filter_envlope_mod`  | float | 0.0 〜 22100.0  | 7000.0 | Hz |

### Chorus

| 表示名 | ID | 型 | 範囲 | 既定値 | 単位 |
|-------|----|----|------|--------|------|
| Chorus Mix       | `chorus_mix`        | float | 0.0 〜 1.0    | 0.0   | % |
| Chorus Depth     | `chorus_depth`      | float | 0.0 〜 100.0  | 44.0  | — |
| Chorus Distance  | `chorus_distance`   | float | 0.0 〜 1000.0 | 450.0 | — |
| Chorus Rate      | `chorus_rate`       | float | 0.1 〜 10.0   | 0.33  | Hz |
| Chorus Note Shape| `chorus_note_shape` | enum  | (波形)        | Sine  | — |

### Noise

| 表示名 | ID | 型 | 範囲 | 既定値 | 単位 |
|-------|----|----|------|--------|------|
| Noise | `noise_mix` | float | 0.0 〜 1.0 | 0.0 | % |

## 集計

- 操作可能パラメータ数: **23 項目** (Master 1 / Envelope 4 / Vibrato 4 / Pitch 系 3 / Filter 5 / Chorus 5 / Noise 1)
- 型の内訳: float 16 / enum 4 / int 1 / bool 1 (本ドキュメントでの分類)

## VST_muni_one への参考としての示唆

| 観点 | nyasynth の取り方 | muni 設計上の検討 |
|------|------------------|-------------------|
| エンベロープ | A/D/S/R の 4 段。Sustain は dB スケール | 現状 muni は A/R のみ。Decay/Sustain 追加で表現力増 |
| フィルタ | Cutoff + Q + Type + Wet + EnvMod の 5 ノブ + EG 連動 | パーツ「眉」にカットオフをマップする案と整合 |
| ビブラート | テンポ同期 enum + Amount + Attack + 波形 | パーツ「目」の上下動と相性が良い |
| Portamento | float (sec) + Polycat (bool) | muni の `Polyphonic` トグルと近い役割 |
| Chorus | Mix/Depth/Distance/Rate/Shape の 5 項目 | パーツ操作に紐付けるなら 1〜2 項目に絞るのが現実的 |
| 全体ボリューム | Master Volume (-36 〜 +12 dB) | muni は -60 〜 0 dB。範囲拡張の余地あり |

実装方針・採用可否は別途 `docs/parameters.md` での検討事項。
