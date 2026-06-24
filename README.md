# TwistTimeStopper

表示された目標秒数をストップウォッチ感覚で当てる、時間感覚トレーニングゲーム。
見えない倍速で進む内部タイマーを正確に止めて高スコアを狙う。シングルプレイ、LAN 対戦、練習モードを搭載した Windows 向けデスクトップアプリ。

<!-- スクリーンショット (将来追加予定) -->

## 技術スタック

- **言語**: C++
- **ライブラリ**: [DxLib](https://dxlib.xsrv.jp/) (2D 描画・入力・サウンド)
- **ネットワーク**: WinSock (TCP/IP、LAN 対戦用)
- **IDE / ビルド**: Visual Studio 2019 以降 (MSVC、Win32 構成)
- **対象 OS**: Windows 10 / 11

## ビルド方法

### 必要環境

- Windows 10 / 11
- Visual Studio 2019 以降 (「C++ によるデスクトップ開発」ワークロード)
- DxLib (公式サイトからダウンロード)

### 手順

1. **DxLib をインストール**
   [DxLib 公式サイト](https://dxlib.xsrv.jp/) から Visual C++ 用 DxLib をダウンロードし、`C:\DxLib` に展開する。
   ※本プロジェクトはこのパスをハードコード参照しているため、別パスに置く場合は `Project2/Project2.vcxproj` のインクルード/ライブラリパスを修正する必要がある。

2. **リポジトリをクローン**
   ```
   git clone https://github.com/OutRose/TwistTimeStopper.git
   ```

3. **ソリューションを開く**
   `Project2.sln` を Visual Studio で開く。

4. **構成を選択してビルド**
   構成: `Debug|Win32` または `Release|Win32` (x64 構成は未対応)。
   `F5` でビルド & 実行、または `Ctrl+Shift+B` でビルドのみ。

## 操作方法

### メニュー画面

| キー | 動作 |
|---|---|
| ↑ / ↓ | 項目選択 |
| Z | 決定 |
| ESC | アプリ終了 |

### Game1: シングルプレイ

| キー | 動作 |
|---|---|
| G | 計測開始 (GO) |
| S | 計測停止 (STOP) |
| R | リセット (計測完了後のみ) |
| X | タイトルに戻る |

### Game2: LAN 対戦

役割選択画面 (シーン入場直後):

| キー | 動作 |
|---|---|
| ↑ / ↓ | 「挑む側」/「挑まれる側」を選択 |
| Z | 決定 |
| X | タイトルに戻る |

計測画面:

| キー | 動作 |
|---|---|
| G / S / R / X | Game1 と同じ |
| N | ネット対戦実行 (計測完了後、相手とスコア交換し勝敗判定) |

### Game3: 練習モード

操作は Game1 と同じ。違いは:
- 倍速値が計測開始前から公開される
- リアルタイムスコアが計測中も毎フレーム更新される

## プロジェクト構造

```
TwistTimeStopper/
├── Project2/                  ソースコード本体
│   ├── GameMain.cpp / .h      エントリーポイント、メインループ、共通定数
│   ├── GameSceneMain.cpp / .h シーンディスパッチャ、スコア計算、状態管理
│   ├── MenuScene.cpp / .h     メニュー画面
│   ├── Game1Scene.cpp / .h    シングルプレイ
│   ├── Game2Scene.cpp / .h    LAN 対戦
│   ├── Game3Scene.cpp / .h    練習モード
│   └── Game4Scene.cpp / .h    新規シーン用の空雛形
├── Project2.sln               Visual Studio ソリューション
├── CLAUDE.md                  開発者向け詳細ドキュメント
├── SPEC_NETWORK.md            ネットワーク仕様 (将来拡張用)
├── LICENSE                    MIT ライセンス
└── README.md                  本ファイル
```

## 開発状況

**現在のバージョン**: 0.9.1 (フェーズ δ-1 完了)

変更履歴の詳細は [CHANGELOG.md](CHANGELOG.md) を参照。

### 実装済み

- シングルプレイ (Game1)
- 練習モード (Game3) — 倍速とリアルタイムスコアを公開
- LAN 対戦 (Game2) — 双方向スコア交換 + 勝敗判定
- 正規化スコアシステム — 目標時間によらず満点 100 固定、ピッタリ達成時の PERFECT! 演出

### 既知の制約・未完成

- **LAN 対戦の同期通信凍結**: 受信側 (Defender) は接続待ち中ゲームループが停止する。タスクマネージャから強制終了する必要あり。フェーズ δ-3 で非同期化予定
- **エラー画面・タイムアウト未実装**: 通信失敗時はログ出力のみ。ユーザー通知の UI は フェーズ δ-3 で対応予定
- **接続先固定**: 現状 `localhost` ハードコード。任意 IP 指定はフェーズ δ-3 以降

開発履歴・設計判断の詳細は [CLAUDE.md](CLAUDE.md) を、ネットワーク部の将来仕様は [SPEC_NETWORK.md](SPEC_NETWORK.md) を参照。

## ライセンス

本プロジェクト本体は [MIT License](LICENSE) のもとで公開されています。

ただし、本プロジェクトが依存する **DxLib は独自のライセンスに従います**。DxLib の利用条件・再配布条件等については、[DxLib 公式サイト](https://dxlib.xsrv.jp/) のライセンス条項を必ず確認してください。

## 作者

**OutRose**

- GitHub: [@OutRose](https://github.com/OutRose)
