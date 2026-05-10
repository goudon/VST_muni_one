# docs - 詳細仕様書インデックス

このディレクトリには VST_muni_one プロジェクトの詳細仕様を記述する。
CLAUDE.md の「ドキュメント運用ルール」に従い、処理の変更前後で必ず精査・更新すること。

## ドキュメント一覧

| ファイル | 内容 |
|---------|------|
| [architecture.md](architecture.md) | プラグイン全体のアーキテクチャ・信号フロー・モジュール関係 |
| [parameters.md](parameters.md) | パラメータ定義 (ID, レンジ, 単位, デフォルト値, スキュー) |
| [dsp/README.md](dsp/README.md) | DSPモジュール仕様インデックス |
| [dsp/sample_player.md](dsp/sample_player.md) | `SampleBuffer` (`dog.wav` のロード・保持) |
| [dsp/voice.md](dsp/voice.md) | `MuniVoice` (サンプル再生 + AR エンベロープ) / `MuniSound` |
| [gui/README.md](gui/README.md) | GUI仕様インデックス |
| [gui/layout.md](gui/layout.md) | GUI レイアウト |
| [gui/face.md](gui/face.md) | `MuniFace` コンポーネント (パーツ画像のレイヤー合成) |
| [sample/README.md](sample/README.md) | 参考シンセ仕様 (外部 OSS) のインデックス |
| [sample/nyasynth.md](sample/nyasynth.md) | nyasynth (Meowsynth 系) のパラメータ全項目とアーキテクチャ |

## 運用ルール

1. **変更前**: 該当ドキュメントを読み、設計意図・現状仕様を把握する。
2. **実装**: 仕様に沿ってコードを変更する。仕様変更が必要ならドキュメントを先に更新する。
3. **変更後**: 実装と整合するようドキュメントを更新し、このインデックスにも反映する。

## ファイル追加時

新しい仕様書を追加したら、上記テーブルに1行追加する。
