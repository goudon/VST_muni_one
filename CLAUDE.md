# VST_muni_one - VST Synthesizer Plugin

## プロジェクト概要
DAW向けシンセサイザーVSTプラグイン。JUCEフレームワークを使用し、カスタムGUIを備える。

## 技術スタック
- **フレームワーク**: JUCE 8 (CMake統合)
- **言語**: C++20
- **ビルドシステム**: CMake 4.2+
- **コンパイラ**: MSVC (Visual Studio 2022 Community)
- **プラットフォーム**: Windows 11 (主ターゲット)
- **プラグインフォーマット**: VST3 / AU / CLAP

## ディレクトリ構成
```
VST_muni_one/
├── CLAUDE.md
├── CMakeLists.txt
├── README.md
├── .gitignore             # 証明書 (*.pfx 等) と env 系も除外
├── .gitmodules
├── docs/                  # 詳細仕様書 (マークダウン)
│   ├── README.md          # docs 全体のインデックス
│   ├── architecture.md    # アーキテクチャ全体像
│   ├── parameters.md      # パラメータ定義・レンジ・単位
│   ├── dsp/               # DSPモジュール仕様 (sample_player.md / voice.md / envelope.md)
│   ├── gui/               # GUI仕様 (layout.md / face.md / example/muni.png)
│   └── sample/            # 参考シンセ仕様 (外部 OSS リファレンス)
├── libs/
│   └── JUCE/              # JUCE (git submodule, タグ 8.0.12 にピン留め)
├── src/
│   ├── PluginProcessor.h / .cpp     # AudioProcessor 派生 + APVTS + note-on カウンタ
│   ├── PluginEditor.h / .cpp        # GUI ルート (上部バー + MuniFace 配置のみ)
│   ├── ParameterIds.h                # APVTS パラメータ ID の名前空間 muni::params
│   ├── dsp/                          # サンプラー DSP
│   │   ├── SampleBuffer.h            # dog.wav のデコード保持
│   │   └── MuniVoice.h               # SynthesiserVoice + MuniSound (同居)
│   ├── gui/                          # キャラクター GUI
│   │   ├── MuniFace.h / .cpp         # 顔の描画・ドラッグ操作・パルスアニメ
│   │   ├── HelpPanel.h / .cpp        # ? トグルで開く操作説明オーバーレイ
│   │   └── ParameterOverlay.h / .cpp # P トグルで開く現在値表示オーバーレイ
│   ├── img/                          # ランタイム埋め込み用パーツ画像 (バイナリ同梱)
│   └── sample/                       # ランタイム埋め込み用音声 (dog.wav)
└── resources/
    └── source_img/                   # 高解像度ソース画像 (実行時には未使用、再生成元)
```

注: CLAUDE.md 当初案にあった `test/` (ユニットテスト) は v0.2 時点では未着手。
追加時は `test/` 配下に CTest 連携で配置する想定。

## ドキュメント運用ルール (重要)
- **詳細仕様は必ず `docs/` 以下のマークダウンファイルに記述する**。CLAUDE.md には概要と方針のみ残し、個別機能・アルゴリズム・パラメータ定義などは `docs/` を参照する。
- **処理(DSP, GUI, パラメータ, 状態管理など)を変更する前に、必ず `docs/` 以下の該当ファイルを精査する**。仕様と実装の乖離を防ぐため、事前にドキュメントで設計意図を確認する。
- **処理を変更した後は、対応する `docs/` 以下のファイルを必ず更新する**。コード変更とドキュメント更新はセットで行う。
- 新しいモジュール/機能を追加する場合は、まず `docs/` に仕様書を作成してから実装に着手する。
- `docs/README.md` をインデックスとして維持し、ドキュメントの追加・削除時に更新する。

## ビルド手順
```bash
# 初回セットアップ
git init
git submodule add https://github.com/juce-framework/JUCE.git libs/JUCE

# ビルド
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

## コーディング規約
- JUCE標準の命名規則に従う (PascalCase for classes, camelCase for methods/variables)
- DSPコードはリアルタイムセーフに保つ (ヒープ割り当て禁止、ロック禁止、例外禁止)
- `processBlock()` 内でのメモリアロケーションは厳禁
- パラメータは `juce::AudioProcessorValueTreeState` で管理
- GUI描画は `juce::Component` ベースのカスタム実装

## 開発方針
- 最小構成から段階的に機能追加する
- DSPとGUIは疎結合に保つ
- パラメータ変更はスレッドセーフに処理する

## セキュリティ上の注意事項
VST プラグインはホスト DAW のプロセス内にロードされる共有ライブラリ (dll/vst3) である。クラッシュや脆弱性はホスト全体に波及するため、以下を厳守する。

### 1. 状態の復元 (`setStateInformation`) は untrusted input として扱う
- DAW のセッションファイルやプリセット由来のバイナリは破損・改ざんの可能性がある前提でパースする。
- XML / ValueTree のタグ名・型・レンジを必ず検証し、想定外なら **デフォルト状態にフォールバックする** (例外を投げてホストを落とさない)。
- `juce::AudioProcessorValueTreeState::replaceState()` の前に `hasTagName()` でルートタグを確認済み。今後フィールド追加時はレンジ検証を追加する。

### 2. バッファ境界・数値安全性
- `processBlock()` 内で `buffer.getNumChannels()` / `getNumSamples()` を信頼し、独自のインデックス計算で境界外アクセスしない。
- MIDI ノート番号・ベロシティは 0–127 にクランプしてから DSP に渡す (ホストが仕様外の値を送る実例あり)。
- 分母に時間系パラメータを使う箇所 (envelope の attack/release) は `jmax(0.0001f, ...)` で 0 除算を防ぐ。
- denormal 対策として `processBlock` 冒頭で `juce::ScopedNoDenormals` を使用済み。

### 3. リアルタイムスレッドの制約 (安全性=可用性)
- `processBlock()` / `renderNextBlock()` 内で **ヒープ割り当て・ロック・I/O・例外・システムコール禁止**。これに違反するとオーディオドロップアウトやデッドロックでホストが不安定化する。
- 共有状態は `std::atomic` またはロックフリーキューのみ。`std::mutex` 等は使わない。
- ログ出力 (`juce::Logger`, `DBG`) も Release ビルドでは処理スレッドから行わない。

### 4. ファイル I/O / 外部リソース
- プラグイン本体はネットワーク通信・テレメトリを行わない (`JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0` を CMake で指定済み)。
- 将来プリセット読込を追加する場合は、パスをユーザー指定ディレクトリ配下に限定し、シンボリックリンク解決・パストラバーサル (`..`) を拒否する。
- 外部ファイル (画像・フォント等) は `resources/` にバイナリ同梱し、実行時パスから動的ロードしない。

### 5. 依存関係 / サプライチェーン
- JUCE は git submodule として **安定リリースタグにピン留め** する (`master`/`develop` 追従禁止)。現在は **`8.0.12`** 固定。
- JUCE 以外のサードパーティコードを追加する場合は、ライセンス・メンテナンス状況・脆弱性履歴を確認のうえ採用可否を判断する。
- ビルド成果物 (VST3) は配布前にコード署名する (Windows: SignTool / macOS: codesign + notarization)。

### 6. ログ・秘密情報
- リポジトリに API キー・署名証明書・個人情報をコミットしない。`.gitignore` で `build/`, IDE 設定, 証明書ファイルを除外済み。
- クラッシュダンプ・ログに MIDI 内容や音声バッファ等ユーザー制作物を含めない。

### 7. 新機能追加時のチェックリスト
- [ ] 外部入力 (MIDI / プリセット / ファイル) の境界・型・レンジを検証するか
- [ ] `processBlock` 系の RT 制約 (割り当て・ロック・例外禁止) を満たすか
- [ ] スレッド間共有が atomic / lock-free か
- [ ] 追加依存のライセンス・サプライチェーンを確認したか
- [ ] ドキュメント (`docs/`) を更新したか
