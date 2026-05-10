# VST_muni_one

JUCE 8 ベースのシンセサイザー VST3 プラグイン。

詳細仕様は [docs/](docs/README.md) を参照。

## 動作環境

- **ホスト DAW**: VST3 対応 (Cubase / Studio One / Reaper / FL Studio / Ableton Live 等)
- **OS (ビルド済みバイナリ)**: Windows 11 (64-bit) / macOS 11+ / Linux (x86_64)
- **プラグインフォーマット**: VST3, Standalone
- **開発・ビルド環境 (主)**: Windows 11 + **PowerShell 7+ (`pwsh`)** + Visual Studio 2022。Git Bash / WSL / macOS / Linux でのビルド手順も後述する。

## インストール方法

### 方法 A: ビルド済みバイナリを使う

ビルド済み `VST_muni_one.vst3` を以下のフォルダに配置するだけ。

#### Windows
```
C:\Program Files\Common Files\VST3\
```
または環境変数で:
```
%CommonProgramFiles%\VST3\
```

管理者権限が無い場合はユーザー領域:
```
%LOCALAPPDATA%\Programs\Common\VST3\
```

#### macOS
```
/Library/Audio/Plug-Ins/VST3/              # 全ユーザー共通
~/Library/Audio/Plug-Ins/VST3/             # 自分だけ
```

#### Linux
```
/usr/lib/vst3/                             # 全ユーザー共通
~/.vst3/                                   # 自分だけ
```

配置後、DAW を再起動し「プラグイン再スキャン」を実行する。

### 方法 B: ソースからビルドする

#### 必要なツール
| ツール | バージョン | 備考 |
|--------|-----------|------|
| CMake  | 3.22 以上 | 4.x 推奨 |
| C++ コンパイラ | C++20 対応 | Windows: Visual Studio 2022 / macOS: Xcode 14+ / Linux: GCC 11+ または Clang 14+ |
| Git    | 2.x       | submodule 取得に必要 |

#### 手順 (Windows / MSVC) — PowerShell

本プロジェクトは Windows 11 上の **PowerShell 7+ (`pwsh`)** での実行を主環境とする。
Git Bash / WSL でも `cmake` コマンド自体は同一だが、ファイル操作・プロセス操作の構文が異なる。

```powershell
# 1. リポジトリ取得 + JUCE submodule 展開
git clone <repo-url> VST_muni_one
Set-Location VST_muni_one
git submodule update --init --recursive

# 2. CMake でプロジェクト生成 (初回のみ)
cmake -B build -G "Visual Studio 17 2022" -A x64

# 3. Release ビルド (再ビルドは毎回これ)
cmake --build build --config Release

# Debug ビルドにする場合
cmake --build build --config Debug

# クリーンビルド (build フォルダごと作り直す)
Remove-Item -Recurse -Force build
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

#### 手順 (Windows) — Git Bash / WSL

```bash
git clone <repo-url> VST_muni_one
cd VST_muni_one
git submodule update --init --recursive

cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# クリーンビルド
rm -rf build
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

ビルド成果物 (Windows 共通):
```
build\VST_muni_one_artefacts\Release\VST3\VST_muni_one.vst3\     ← VST3 バンドル
build\VST_muni_one_artefacts\Release\Standalone\VST_muni_one.exe ← スタンドアロン版
```

Standalone を直接起動して動作確認:

```powershell
# PowerShell
& .\build\VST_muni_one_artefacts\Release\Standalone\VST_muni_one.exe
```

```bash
# bash
./build/VST_muni_one_artefacts/Release/Standalone/VST_muni_one.exe
```

#### 手順 (macOS) — zsh / bash

```bash
git submodule update --init --recursive
cmake -B build -G "Xcode"
cmake --build build --config Release
```

#### 手順 (Linux) — bash

```bash
# 依存 (Ubuntu/Debian 例)
sudo apt install libasound2-dev libjack-jackd2-dev libfreetype-dev \
                 libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \
                 libwebkit2gtk-4.1-dev libgtk-3-dev

git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### インストール (ビルド後)

ビルド済み `VST_muni_one.vst3` バンドル (フォルダ) を上記 OS 別のプラグインパスにコピーする。

```powershell
# PowerShell
Copy-Item -Recurse -Force `
    .\build\VST_muni_one_artefacts\Release\VST3\VST_muni_one.vst3 `
    "$env:CommonProgramFiles\VST3\"
```

```bash
# Git Bash / WSL
cp -r build/VST_muni_one_artefacts/Release/VST3/VST_muni_one.vst3 \
      "/c/Program Files/Common Files/VST3/"
```

```bash
# macOS
cp -R build/VST_muni_one_artefacts/Release/VST3/VST_muni_one.vst3 \
      ~/Library/Audio/Plug-Ins/VST3/
```

```bash
# Linux
cp -r build/VST_muni_one_artefacts/Release/VST3/VST_muni_one.vst3 ~/.vst3/
```

※ VST3 はファイルではなくディレクトリバンドル (Windows でも中身はフォルダ構造)。フォルダごとコピーする。

## DAW 側の確認

1. DAW を起動し、設定からプラグインフォルダに上記パスが含まれていることを確認する。
2. 「VST3 プラグインを再スキャン」を実行する。
3. インストゥルメントトラックを作成し、`VST_muni_one` (Manufacturer: `muni`) を選択する。
4. MIDI キーボードまたは鍵盤入力でサイン波が鳴れば成功。

## アンインストール

インストール時に配置した `VST_muni_one.vst3` フォルダを削除し、DAW を再起動する。

## トラブルシューティング

| 症状 | 対処 |
|------|------|
| DAW に表示されない | VST3 パスが正しいか、DAW のプラグイン再スキャンを実行したか確認。64-bit DAW か確認。 |
| Windows で「このアプリは保護されています」 | 未署名バイナリのため。配布版ではコード署名を行う予定。自前ビルドなら「詳細情報」→「実行」。 |
| 音が鳴らない | MIDI 入力がトラックに届いているか、Gain が -60 dB 以下になっていないか確認。 |
| macOS で「開発元を確認できない」 | `xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/VST_muni_one.vst3` を実行。 |
| ビルド時 `LNK1104: cannot open file ...VST_muni_one.vst3` | 前回ビルドの Standalone か DAW がプラグインを掴んでいるため上書きできない。下記参照。 |

### ビルド時のファイルロック (`LNK1104`) 解消手順

VST3 / Standalone がロックされていると再リンクに失敗する。該当プロセスを終了してから再ビルドする。

```powershell
# PowerShell — 誰が掴んでいるか確認
Get-Process VST_muni_one -ErrorAction SilentlyContinue

# Standalone を強制終了
Stop-Process -Name VST_muni_one -Force -ErrorAction SilentlyContinue
```

```bash
# Git Bash / WSL
tasklist //FI "IMAGENAME eq VST_muni_one.exe"
taskkill //F //IM VST_muni_one.exe
```

DAW がロードしている場合は **DAW 自体を閉じる** (プラグインのアンロードだけではハンドルが残ることがある)。
解消後、再ビルド:

```powershell
cmake --build build --config Release
```

## ライセンス

(未定)
