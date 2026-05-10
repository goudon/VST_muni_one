# GUI レイアウト

## ウィンドウ
- 初期サイズ: **300 × 300 px** (顔ベース 208×196 + 耳回転の余白 + 上部ボタン分でぴったり収まるサイズ)
- リサイズ: 不可

## 描画領域の根拠
- 顔ベース: 208 × 196 px (ネイティブ)
- 耳の回転 (±0.5 rad): 標準位置から横 +30 px / 縦 +10 px ほど BBox がはみ出す
- 上部バー: margin 10 + ボタン高 32 + spacing 4 = 46 px
- → 上下左右に最低 30 px 強の余白を残しつつ収まる最小サイズが 300 × 300 px

## 配置

```
┌─────────────────────────────────────────────────────────┐
│ [✓] Polyphonic                            [ P ] [ ? ]   │  ← 上部バー
├─────────────────────────────────────────────────────────┤
│                                                         │
│                                                         │
│                                                         │
│                       (muni)                            │  ← MuniFace
│                                                         │     (中央配置)
│                                                         │
│                                                         │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

ヘルプ ON 時 (右上 `?` 切替):

```
┌─────────────────────────────────────────────────────────┐
│ [✓] Polyphonic                            [ P ] [ ? ]   │
├─────────────────────────────────────────────────────────┤
│ ╔═════════════════════════════════════════════════════╗ │
│ ║ Drag a face part:                                   ║ │
│ ║                                                     ║ │
│ ║   Nose   Y         ->  Gain                         ║ │
│ ║   Eye    Y         ->  Attack                       ║ │
│ ║          X (in)    ->  Pitch                        ║ │
│ ║   Brow   Y         ->  Release                      ║ │
│ ║          X (in)    ->  Speed                        ║ │
│ ║   Ear L  X         ->  Ear Left Tilt                ║ │
│ ║   Ear R  X         ->  Ear Right Tilt               ║ │
│ ║                                                     ║ │
│ ║   Eyes/Brows mirror L/R                             ║ │
│ ║   Closer eyes  = higher pitch                       ║ │
│ ║   Closer brows = faster speed                       ║ │
│ ╚═════════════════════════════════════════════════════╝ │
└─────────────────────────────────────────────────────────┘
```

ヘルプパネルは半透明オーバーレイで、`setInterceptsMouseClicks(false, false)` によりクリックスルー。
ヘルプを表示したまま顔のドラッグ操作も継続できる。
パラメータ現在値オーバーレイ (`P` トグル) も同様にクリックスルーで、両方を同時に表示することも可能。

## コンポーネント

| コンポーネント | クラス | 役割 |
|----------------|--------|------|
| `MuniFace`        | `muni::MuniFace` (`src/gui/`)             | キャラクターのレイヤー描画 + ドラッグでパラメータ操作 |
| Polyphonic トグル | `juce::ToggleButton`                      | 同時発音可否 (左上 110 × 28 px) |
| Help (`?`)        | `juce::TextButton` (toggle, 黄色背景・黒文字) | クリックでヘルプオーバーレイの表示切替 (右上 32 × 32 px) |
| Params (`P`)      | `juce::TextButton` (toggle, 黄色背景・黒文字) | クリックで現在パラメータ値オーバーレイの表示切替 (`?` の左隣) |
| `HelpPanel`       | 内部クラス (`MuniAudioProcessorEditor::HelpPanel`) | パーツ ↔ パラメータ対応を英語表示。半透明黒背景 + 黄枠。 |
| `ParameterOverlay`| 内部クラス (`MuniAudioProcessorEditor::ParameterOverlay`) | 全 7 float パラメータの現在値を半透明白テキストで顔上に薄く表示。背景なし。APVTS リスナで自動更新。 |

## 設計上のポイント

- **タイトル表示は廃止**。プラグイン名は DAW のヘッダ表示で十分という判断。
- **数値表示・スライダーは廃止**。パラメータ操作はキャラクター GUI のみで行う。各パーツの動きが視覚フィードバックを兼ねる。
  パラメータの値そのものを確認したい場合は DAW 側の APVTS 表示 (Generic Editor / Automation Lane) を使う。
- **Polyphonic は左上、Help は右上** に配置し、上部バーが両端を担う。
- ヘルプの英語化は、海外ユーザにも配布する想定 + 短いラベル英語化で混乱を避けるため。
- パーツ ↔ パラメータの対応詳細は [face.md](face.md) と [../parameters.md](../parameters.md) を参照。

## APVTS 接続

- `juce::AudioProcessorValueTreeState::ButtonAttachment` で `polyphonic` をバインド。
- 他の全パラメータは `MuniFace` 内部で APVTS 参照を直接取得 (`getParameter()->setValueNotifyingHost`) して操作する。
