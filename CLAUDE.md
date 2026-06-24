# CLAUDE.md — TwistTimeStopper プロジェクト情報

Visual Studio (MSVC) + DxLib による C++ ゲーム。Project2.sln / [Project2/](Project2/) 配下が本体。日本語 Windows 環境 (コードページ 932) でビルドする前提。

---

## ファイルエンコーディング: 全ファイル UTF-8 BOM 統一

**2026-06-22 にプロジェクト全体を UTF-8 BOM (`EF BB BF`) に統一済み。新規ファイルも UTF-8 BOM で作成すること。**

[.editorconfig](.editorconfig) で `charset = utf-8-bom` を強制しているため、Visual Studio / VS Code は自動的に UTF-8 BOM で保存する。**Claude Code の Edit ツールは既存ファイルの BOM を保持するが、Write ツールは BOM を付与しない**ため、新規作成や全体書き換えに Write を使う場合は要注意 (下記「Claude Code Write ツール使用時の注意」参照)。

### 新規ファイル作成時の注意

PowerShell で空ファイルを作る場合は BOM を明示:
```powershell
[IO.File]::WriteAllText($path, $content, [Text.UTF8Encoding]::new($true))
```

`Set-Content` や `Out-File` のデフォルトは UTF-8 (BOM なし) になりがちなので避ける。

### Claude Code Write ツール使用時の注意

**Claude Code の Write ツールは UTF-8 BOM を自動付与しない** (改行も LF になる) ため、新規ファイル作成や既存ファイルの全体書き換えに使うと、Visual Studio が CP932 として誤解釈し、コメント内のダメ文字 (`ソ/表/能/予` など 2 バイト目が `0x5C` の文字) の `\` 混入で構文崩壊が発生する (2026-06-24 GameSceneMain.cpp β-D-1 で実際に発生、C2059/C2143 が連鎖して 30+ 件のビルドエラー)。

**対策**:
- 既存ファイルの一部修正は **Edit ツール優先** (BOM 保持される、本プロジェクトの推奨)
- Write ツールで全体書き換えした後は **必ず PowerShell で BOM + CRLF に再保存**:
  ```powershell
  $path = 'D:\Repositories\TwistTimeStopper\...'
  $bytes = [IO.File]::ReadAllBytes($path)
  $text = [Text.UTF8Encoding]::new($false, $true).GetString($bytes)
  $text = $text -replace "`r?`n", "`r`n"  # CRLF に統一
  [IO.File]::WriteAllText($path, $text, [Text.UTF8Encoding]::new($true))
  ```
- 確認: Git Bash で `file <path>` を実行し、`UTF-8 (with BOM) text, with CRLF line terminators` と表示されることを確認

[.editorconfig](.editorconfig) は Visual Studio / VS Code に効くが、Claude Code の Write には効かない。

### 万一 Shift-JIS や無 BOM が紛れ込んだ場合の復旧

```powershell
$path = '...'
$bytes = [IO.File]::ReadAllBytes($path)
$utf8Strict = [Text.UTF8Encoding]::new($false, $true)
try { $text = $utf8Strict.GetString($bytes) }  # UTF-8 として読めるか試す
catch { $text = [Text.Encoding]::GetEncoding(932).GetString($bytes) }  # ダメなら CP932
if ($text.Contains([char]0xFFFD)) { throw 'decode failed' }
[IO.File]::WriteAllText($path, $text, [Text.UTF8Encoding]::new($true))
```

---

## ビルド環境

- **ソリューション**: [Project2.sln](Project2.sln) (構成は **Debug|Win32 / Release|Win32 のみ**、x64 構成なし)
- **DxLib**: **`C:\DxLib` にインストール前提** ([Project2/Project2.vcxproj](Project2/Project2.vcxproj) で AdditionalIncludeDirectories と AdditionalLibraryDirectories にハードコード)。vendored ではないので新規環境では別途配置が必要。
- **画面**: 800×700 / 24bit カラー / ウィンドウモード固定 ([Project2/GameMain.cpp:43](Project2/GameMain.cpp#L43) `SetGraphMode(800, 700, 24)`、`ChangeWindowMode(true)`)
- **フレームレート**: 60 FPS ビジーウェイト (`while (GetNowCount() - FrameStartTime < 1000/60) {}` のループ、[Project2/GameMain.cpp](Project2/GameMain.cpp) 行 69 付近)
- **エントリーポイント**: `WinMain` ([Project2/GameMain.cpp:19](Project2/GameMain.cpp#L19))

### 毎フレームの定型フロー

```cpp
ClearDrawScreen();       // 1. バックバッファクリア
GetJoypadInputState();   // 2. 入力取得 (EdgeInput 更新含む)
sceneMove();             // 3. シーン更新
sceneRender();           // 4. シーン描画
ScreenFlip();            // 5. 表示反映
// 60FPS 待機
```

ループ脱出は `ProcessMessage() == -1` または `CheckHitKey(KEY_INPUT_ESCAPE)`。終了時に `DxLib_End()`。

---

## シーンアーキテクチャ

### enum とグローバル状態

[Project2/GameSceneMain.h](Project2/GameSceneMain.h) で `SCENE_NO` enum を定義: `SCENE_NONE (-1)`, `SCENE_MENU`, `SCENE_GAME1`, `SCENE_GAME2`, `SCENE_GAME3` (練習モード), `SCENE_GAME4` (空シーン雛形、メニュー非掲載)、`SCENE_MAX`。
[Project2/GameSceneMain.cpp](Project2/GameSceneMain.cpp) で `static SCENE_NO sceneNo, prevScene, nextScene` を管理。

### 4 関数命名規約 (シーンごとに必須)

各シーン名 `XXXScene` (Menu / Game1 / Game2 / Game3) に対し:

| 関数 | 役割 |
|---|---|
| `initXXXScene()` | リソース確保 / 初期化。シーン入場時 1 回呼ばれる。 |
| `moveXXXScene()` | 毎フレームの状態更新。 |
| `renderXXXScene()` | 毎フレームの描画。 |
| `releaseXXXScene()` | リソース解放。シーン退場時 1 回呼ばれる。 |
| `XXXSceneCollideCallback(nSrc, nTarget, nCollideID)` | DxLib 衝突判定コールバック。 |

新規シーン追加時はこの 5 関数を揃え、[GameSceneMain.h](Project2/GameSceneMain.h) の `SCENE_NO` enum に 1 値追加 + [GameSceneMain.cpp](Project2/GameSceneMain.cpp) の `sceneTable` に 1 行追加で登録 (β-D-1 以降、テーブル駆動)。

### 遷移

`changeScene(SCENE_NO no)` で `nextScene` を設定。次フレームに `sceneNo != nextScene` を検出した時点で `releaseXXX → initYYY → sceneNo = nextScene` の順で実行される。

### ⚠️ CollideCallback の制約

[Project2/GameSceneMain.cpp](Project2/GameSceneMain.cpp) の `CollideCallback` 直上に「**ここでは要素を削除しないこと！！**」コメントあり。DxLib の衝突判定列挙の最中に呼ばれるため、コールバック内で対象オブジェクトを `delete` するとイテレータ破壊で UB になる。削除はフラグを立てて `moveXXXScene` 内で実施するパターン。

---

## 命名規約

### インクルードガード

`<NAME>_H_` 形式。GameMain.h のみ `#pragma once` (例外、変えない)。

| ヘッダ | ガードマクロ |
|---|---|
| GameMain.h | `#pragma once` |
| MenuScene.h | `MENUSCENE_H_` |
| Game1Scene.h | `GAMESCENE1_H_` |
| Game2Scene.h | `GAMESCENE2_H_` |
| Game3Scene.h | `GAMESCENE3_H_` |
| Game4Scene.h | `GAMESCENE4_H_` |
| GameStatus.h | `GAMESTATUS_H_` |
| GameSceneMain.h | `GAMESCENEMAIN_H_` |

Game1/2/3/4Scene は **`GAMESCENE<N>_H_`** (数字は GAMESCENE の後)。`GAME<N>SCENE_H_` ではないので注意。

### インデント

[.editorconfig](.editorconfig) で `indent_style = tab`。既存ファイルもタブ。

---

## コメント方針

**コードコメントは必須**。各箇所が「何のために存在し、何を果たすか」の役割メモを残す方針。

- **保持対象**: 関数/変数/分岐の役割説明、定数の意味付け、状態遷移の意図、DxLib 慣用の前提、入力キーの用途など — たとえ自明に見えても残す
- **削除対象**: コメントアウトされた旧コード (`// char xxx;` 形式や `/* … */` の旧実装ブロック)、ファイル名が古いままの残骸、明らかな誤情報のみ
- **新規コード**: Claude/手動を問わず、新たに書く関数や複雑な分岐には必ず役割コメントを付ける

診断レポート観点7 (過剰コメント整理) のうち「番号付き授業コメント (`//７(1) ①…`)」「関数名から自明な訳コメント」「教科書的説明」は**削減対象としない** — 学習由来であっても役割メモとして残す。後述のフェーズ β-B はこの方針により保留。

---

## 入力処理

### 仕組み

[Project2/GameMain.cpp](Project2/GameMain.cpp) の毎フレーム冒頭:
```cpp
int i = GetJoypadInputState(DX_INPUT_KEY_PAD1);  // 現フレームのビットマスク
EdgeInput = i & ~Input;                           // 立ち上がりエッジ (押された瞬間)
Input = i;                                        // 現状保持
```

- **`Input`** (グローバル `int`): 押されている間ずっと真。
- **`EdgeInput`** (グローバル `int`): 押された瞬間の 1 フレームだけ真。

### 使い分け

| 用途 | 推奨 |
|---|---|
| メニュー移動 / 確定 (連打防止したい) | `EdgeInput & PAD_INPUT_UP` 等 |
| 移動 / 押しっぱなしで効くアクション | `Input & PAD_INPUT_*` |
| キーボード直接検査 (パッドにマップされない G/S/X 等) | `CheckHitKey(KEY_INPUT_G) == 1` |

代表例: [Project2/MenuScene.cpp:32](Project2/MenuScene.cpp#L32) で `EdgeInput & PAD_INPUT_UP`、[Project2/Game1Scene.cpp:71](Project2/Game1Scene.cpp#L71) で `CheckHitKey(KEY_INPUT_G)`。

### 入力定義

[入力Inputについて.txt](入力Inputについて.txt) に PAD_INPUT_* 一覧 (現時点 1 プレイヤーのみ)。

---

## ネットワーク現状 (Game2Scene の netBattle)

**γ-2 完成 (2026-06-24)**。動く LAN 対戦の最小要件を実装。現状の構造の要点:

- **プロトコル**: TCP (`SOCK_STREAM`, `IPPROTO_TCP`)、ポート **3500**、接続先 `"localhost"` ハードコード
- **データ**: スコア (float) を `snprintf("%f", ...)` で文字列化して双方向交換 (Challenger → Defender → Challenger の順)
- **役割切替**: `sideSelect` 変数 (`NET_SIDE` enum) を SIDE_SELECT メニュー UI で選択
  - `NET_SIDE_CHALLENGER` (送信側): `socket` → `connect` → `send` → `recv` → `closesocket`
  - `NET_SIDE_DEFENDER` (受信側): `socket` → `bind` → `listen` → `accept` → `recv` → `send` → `closesocket(remS+lisS)`
- **WinSock 初期化**: `initGame2Scene` で `WSAStartup`、`releaseGame2Scene` で `WSACleanup` (シーン寿命にペアリング)
- **勝敗判定**: 双方とも受信完了時に `netStatus = 2` セット、`renderGame2Scene` で `state.Score` vs `rcScore` 比較表示

**γ-2 で修正済み既知バグ** (詳細はコミット履歴 `CC：γ-2 ...`):
- ✅ `send(remS, ...)` 未初期化バグ → `send(s, ...)` 修正 (Challenger case)
- ✅ `while (szBuf != NULL)` 無限ループ → 1 回交換に変更 (Defender case)
- ✅ `netStatus = 2` 遷移経路欠落 → 両 case で受信完了時セット
- ✅ `lisS` リソースリーク (Challenger でも作成されていた) → Defender case 内に socket+bind+listen+accept 集約
- ✅ `WSAStartup`/`WSACleanup` 非対称 → init/release ペアリング
- ✅ SIDE_SELECT UI 未実装 → MenuScene 流選択肢描画追加 (上下キー + Z 決定 + X 戻り)

**γ-2 仕様化された制約 (将来課題)**:
- ⚠️ 同期 `accept`/`recv` によるフリーズ: Defender 側で N キー押下後、Challenger 起動まで**ゲームループ凍結** (ESC 反応不能、Task Manager 強制終了で脱出)。非ブロッキング化 (`ioctlsocket FIONBIO` + `select` または別スレッド) は **フェーズ δ** 候補

**γ-3 完成 (2026-06-24)**:
- ✅ 全 WinSock 関数の戻り値検査 (`socket`/`bind`/`listen`/`connect`/`accept`/`send`/`recv` + `gethostname`/`gethostbyname`) のインライン `if` 検査追加、エラー時 `MyOutputDebugString(_T("...failed (err=%d)"), WSAGetLastError())` ログ + LIFO 順 `closesocket` + `return -1` で統一
- ✅ `netStatus` enum 化 (β-C-4 連動): `NET_STATUS` (INIT/WAITING/RECEIVED)、WAITING (=1) は将来非同期化布石として残置
- ✅ ネット系マジックナンバー定数化 (β-D-4 持ち越し): `NET_PORT 3500`/`NET_BUF_SIZE 256`/`SERVER_NAME_SIZE 128`/`WINSOCK_VERSION_REQ MAKEWORD(1,1)` を Game2Scene.cpp ファイルスコープに集約 (Game2 専用、NET_SIDE/GAME2_STATE と同方針)
- ✅ `for` ループの `rcScore` コピーを `memcpy(rcScore, szBuf, NET_BUF_SIZE)` に置換 (2 箇所)
- ✅ `netBattle` 戻り値統一: 成功 0 / 失敗 -1

進め方は[リファクタリング戦略](#リファクタリング戦略) の **フェーズ γ-2 / γ-3** 参照。

---

## DxLib のお作法 (細かい点)

- **フォント**: `SetFontSize(N)` / `ChangeFontType(DX_FONTTYPE_*)` はグローバル状態なので、各シーンの init で必ず設定し直す ([Project2/Game2Scene.cpp:85-86](Project2/Game2Scene.cpp#L85-L86) が代表例)。
- **色**: `GetColor(R, G, B)` の戻り値 `unsigned int` はキャッシュする (毎フレーム計算しない)。[GameMain.cpp](Project2/GameMain.cpp) で 7 色を共通定義、[GameMain.h](Project2/GameMain.h) で `extern` 公開 (β-D-2a 以降、全シーン共通)。
- **座標系**: 原点 (0, 0) は **画面左上**。`DrawString(x, y, str, color)` で直接指定。
- **乱数**: `GetRand(n)` で 0〜n-1。シードは起動時に `GetNowCount()` で `SRand` 初期化済み ([Project2/GameMain.cpp:16](Project2/GameMain.cpp#L16))。
- **デバッグ出力**: `MyOutputDebugString(...)` マクロ ([Project2/Game2Scene.cpp:19-26](Project2/Game2Scene.cpp#L19-L26) 定義)。Debug 構成でのみ実体化。

---

## 警告対処パターン

### C4244 (numeric narrowing, `int`/`double` → `float`)

明示キャストまたは `*f` サフィックスで抑制。動作は変えない。

```cpp
// 整数 → float
RandomTgt = (float)(GetRand(19) + 1);

// double → float (リテラル側)
Score = CalFrame * 1.25f;

// double → float (関数戻り値) ★ floor → floorf が定石
scJudge = floorf(scJudge);
```

**注意**: `<iostream>` を取り除くと、それが連鎖的に読み込んでいた `<cmath>` の `float floor(float)` オーバーロードが消えて `floor(float)` が `double` を返すようになり C4244 が新規発生する。`floorf` 等 C99 の `*f` 系関数 (`<math.h>` 提供) に置き換えるのが最も安全。

### C4129 (unrecognized character escape)

原因: Shift-JIS 文字列リテラル中の **ダメ文字** (2 バイト目が `0x5C = '\\'` になる「ソ」「表」「能」など)。UTF-8 BOM 化後は理論上発生しないが、もし過去のソースから残っていれば該当文字を別の漢字に置換するか、デバッグ用なら ASCII 化が最小修正。

### C4060 (空 switch)

```cpp
switch (x) {
default: break;  // ← 追加
}
```

### C4819 (CP932 で表示できない文字)

エンコーディング統一後は基本的に発生しないはず。出たら無 BOM のファイルが混入した可能性が高いので [.editorconfig](.editorconfig) の効きを確認 + 上記復旧手順を実施。

---

## 未完成機能の方針

完成させるかどうかをカテゴリ別に明示:

**完成済み (γ-2, 2026-06-24)**:
- `netBattle` のネットワーク同期 (Game2Scene.cpp) — 双方向交換、致命バグ修正、リソース管理整理済。[ネットワーク現状](#ネットワーク現状-game2scene-の-netbattle) 参照
- `SIDE_SELECT` メニュー UI (`renderGame2Scene` 内に MenuScene 流選択肢描画、X キーでメニュー戻り)

**完成済み (γ-1, 2026-06-24)**:
- `Game3Scene` ([Project2/Game3Scene.cpp](Project2/Game3Scene.cpp)) — **練習モード本体** (シングルプレイ、Game1Scene と違って速度倍率とリアルタイムスコアを公開、学習用)。MenuScene の menu[] 第 3 項目として登録、Z キーで決定して入場。リアルタイムスコアは `TIMER_STATE tmp = state; timerCalcScore(&tmp);` のコピー方式で副作用回避

**空シーン雛形 (シーン追加時のコピー元)**:
- `Game4Scene` ([Project2/Game4Scene.cpp](Project2/Game4Scene.cpp)) — **新しい空シーン雛形** (現 Game3 の役目をスライド)。SCENE_NO/sceneTable に登録だが menu[] には含めない (= ユーザーから到達不可)。将来 Game5/6 を増やす時はこれをコピーして識別子置換 + [GameSceneMain.h](Project2/GameSceneMain.h) の `SCENE_NO` enum + [GameSceneMain.cpp](Project2/GameSceneMain.cpp) の sceneTable + [Project2.vcxproj](Project2/Project2.vcxproj) の ClCompile/ClInclude + 必要なら [MenuScene.cpp](Project2/MenuScene.cpp) の `menu` 配列にも追記する方針

---

## Git 慣習

- **コミットメッセージ**: 日本語。Claude Code 経由の作業には先頭に **`CC：`** プレフィックス (全角コロン) を付ける。例: `CC：エンコーディング統一、ルール追加、.md ファイル追加`。複数作業を 1 コミットにまとめる場合はコンマ区切り。
- **ブランチ**: 作業ブランチは `refact-<YYMMDD>-<purpose>` 形式 (例: `refact-260621-base`)。master が安定ブランチ。
- **コミットは原則ユーザーが手動で行う**。Claude は working tree に変更を残すまでで、`git commit` は実行しない (ユーザーから明示の指示があるまで)。

---

## リファクタリング戦略

2026-06-23 から Claude Code (ultracode) を併用して段階的にコード品質を改善中。同日に 10 観点 (重複/命名/未使用コード/長関数/深ネスト/マジック数/コメント/エラー処理/責務/プラットフォーム依存) で網羅診断を実施し、確定 210 件の改善余地を特定。低リスク → 高リスクの順にフェーズ分けして適用。

各項目は **プランモードで対象提示 → ユーザー承認 → Edit → grep 検証** のミニサイクルで実施。コミットはユーザーが手動で行う ([Git 慣習](#git-慣習) 既定)。

### フェーズ α: 未使用コード一掃 (完了, 2026-06-23)

参照が 0 件であることを grep 確認済みの単発シンボル/コメントを削除。完了 5 項目:

1. [MenuScene.cpp](Project2/MenuScene.cpp) — 未使用変数 `int startfont;` 削除
2. [Game2Scene.cpp](Project2/Game2Scene.cpp) — 未使用変数 `int startfont2;` 削除
3. [Game2Scene.cpp](Project2/Game2Scene.cpp) — 未使用配列 `SCENE_NO netMenu[MENU_MAX_G] = {…};` 削除 (`MENU_MAX_G` は case 4 のメニュー境界チェックで使用継続のため維持)
4. [Game2Scene.cpp](Project2/Game2Scene.cpp) — 旧コードコメント `//本来→SCENE_NO menu[MENU_MAX] = {…, SCENE_GAME3};` 削除 (Game2 ファイル内に置かれていた MenuScene 側 `menu[]` の残骸)
5. 上記各削除に伴う隣接の余分な空行整理 (ブロック区切りを 1 行空きに統一)

### フェーズ β-A: 残った dead code 一掃 (完了, 2026-06-23)

コメントアウト旧コード・未使用 include・未使用変数を順次撤去。完了 4 項目:

1. [GameMain.cpp](Project2/GameMain.cpp) — `/* … */` で囲まれた旧 MessageBox ウィンドウモード確認ブロック削除 (現状は `ChangeWindowMode(true)` で強制ウィンドウ起動が確定)
2. [Game1Scene.cpp](Project2/Game1Scene.cpp) — 未使用 `#include <math.h>` / `#include <string.h>` 削除 (両ヘッダの関数が一切呼ばれていないことを grep 確認)
3. [Game2Scene.cpp](Project2/Game2Scene.cpp) — `netBattle` 関数内の旧ローカル宣言コメント `//char rcScore[256];` 削除 (`rcScore` はグローバルへ移行済み)
4. [GameMain.cpp](Project2/GameMain.cpp) — 未使用変数 `int game_status = GAMETITLE;` 削除 (`GameStatus.h` ファイル本体および 6 マクロは「将来使うかもしれない placeholder」としてユーザー判断で保留)

### フェーズ β-B: 過剰コメント整理 — 保留 (実施しない)

[コメント方針](#コメント方針) により、役割メモは保護対象。番号付きコメント・自明な訳・教科書的説明も「学習由来の役割メモ」として残す。観点7 のうち削減してよいのは dead code 系コメントアウトのみ (これは β-A で既に処理済み)。

### フェーズ β-C: 状態値の enum 化 (進行中, 2026-06-23 開始)

`status` (Game1Scene)、`status2` / `sideSelect` / `netStatus` (Game2Scene) のマジックナンバー (0/1/2/3/4) を enum 化。複数ファイルを跨ぐ中リスク作業のため、サブ項目ごとにプランモードで対象を絞って実施。

**完了**:
1. **β-C-1**: [GameSceneMain.h](Project2/GameSceneMain.h) に共通 `TIMER_STATUS` enum (4 値: INIT/READY/MEASURING/DONE) を新設、[Game1Scene.cpp](Project2/Game1Scene.cpp) の `status` を全置換 (14 箇所)
2. **β-C-2**: [Game2Scene.cpp](Project2/Game2Scene.cpp) 内に Game2 専用 `GAME2_STATE` enum (5 値: INIT/READY/MEASURING/DONE + SIDE_SELECT) を新設、`status2` を全置換 (18 箇所)。SIDE_SELECT が Game1 に無い概念のため、TIMER_STATUS を汚さず別型として定義 (値域 0-3 は意図的にミラー)

**未着手**:
- **β-C-3**: `sideSelect` (Game2、0=送信側/1=受信側/2=未選択) の enum 化
- **β-C-4**: `netStatus` (Game2、0=初期/1=待機/2=受信完了) の enum 化 — ただし現状 `netStatus = 2` への遷移経路が無いため、フェーズγ (ネット対戦完成) と連動

### フェーズ β-D: 中〜高リスク群 (構造系、進行中 2026-06-24 開始)

診断で残った構造系テーマを順次対応。

**完了**:
1. **β-D-1**: シーンディスパッチャ共通化 — [GameSceneMain.cpp](Project2/GameSceneMain.cpp) の 5 つの `switch (sceneNo)` を `SCENE_HANDLERS` 関数ポインタ束 + `sceneTable` 配列 + 1 行ディスパッチへ。`prevScene` 削除、`MessageBox` 5 箇所撤去 (プラットフォーム依存解消)、シーン追加コストが 5 箇所 → 2 箇所 (enum + テーブル) に減
2. **β-D-2a**: 色変数共通化 — Game1/2 のサフィックス`2` 並走を解消。7 色 (`ColorWhite`〜`ColorSkyLike`) を [GameMain.cpp](Project2/GameMain.cpp) に集約定義 + [GameMain.h](Project2/GameMain.h) で `extern` 公開。[MenuScene.cpp](Project2/MenuScene.cpp) と [Game3Scene.cpp](Project2/Game3Scene.cpp) の `GetColor` 直呼び 7 箇所も共通変数に置換 (= 観点6 マジックナンバー RGB 直値部分解消)
3. **β-D-2b+2c (統合)**: TIMER_STATE 構造体導入 + targetTimeSet 共通関数化 — [GameSceneMain.h](Project2/GameSceneMain.h) に `TIMER_STATE` 構造体 (7 メンバ: RandomTgt/CalFrame/RandomMtp/CalMulti/FrameTmp/ScMulti/Score) + `timerReset(TIMER_STATE*)` プロトタイプ追加。[GameSceneMain.cpp](Project2/GameSceneMain.cpp) に `timerReset` 関数定義。Game1Scene.cpp と Game2Scene.cpp の計測変数 7 個 ×2 セットを `TIMER_STATE state = {};` 1 個に集約、約 40 箇所の参照を `state.X` 形式に置換、サフィックス`2`完全消滅。`targetTimeSet`/`targetTimeSet2` 関数定義は削除。これにより乱数初期化ロジックの重複が完全解消
4. **β-D-2d**: スコア計算共通化 — [GameSceneMain.h](Project2/GameSceneMain.h) に `timerCalcScore(TIMER_STATE*)` プロトタイプ追加、[GameSceneMain.cpp](Project2/GameSceneMain.cpp) に関数定義追加 (旧 if-else 3 分岐: 早い/遅い/ピッタリの 1.25 倍ボーナス)。Game1Scene.cpp の `case TIMER_STATUS_DONE` および Game2Scene.cpp の `case GAME2_STATE_DONE` 内の同一スコア計算ブロック (18 行 ×2) を `timerCalcScore(&state);` 1 行呼び出しに置換。β-D-2 系完了 — 計測ロジックの重複が完全消滅
5. **β-D-3**: エラーハンドリング強化 (案 B: 中程度、フォールバック動作追加) — [GameMain.h](Project2/GameMain.h) に `MyOutputDebugString` マクロ昇格 (Game2Scene.cpp 専用 → 全シーン共通、`<stdio.h>`/`<Windows.h>`/`<tchar.h>` も同梱)、Game2Scene.cpp の重複定義削除。[GameMain.cpp](Project2/GameMain.cpp) `DxLib_Init` 失敗時にデバッグログ追加。[GameSceneMain.cpp](Project2/GameSceneMain.cpp) `<assert.h>` include 追加、`changeScene` 範囲外時にログ + Debug ビルド assert、`initCurrentScene` を `BOOL` 戻り値化、`FrameMove` に SCENE_MENU フォールバック (init 失敗時) + 諦め (SCENE_NONE) ロジック追加。現状全 init が TRUE 固定のため発火しないが、将来 LoadGraph 等を追加した時の布石
6. **β-D-4**: マジックナンバー定数化 (案 D: A+B+C+D 一括、ネット系除外) — [GameMain.h](Project2/GameMain.h) に画面 (`SCREEN_WIDTH/HEIGHT/BPP`)・FPS (`FPS/MS_PER_SEC`)・フォント (`FONT_SIZE_DEFAULT`)・Game 共通レイアウト (`LAYOUT_X_DEFAULT`/`LAYOUT_Y_STATUS`〜`Y_SCORE`)・メニュー専用レイアウト (`MENU_X_*`/`MENU_Y_*`) を `#define` で集約。[GameSceneMain.cpp](Project2/GameSceneMain.cpp) にファイルスコープで乱数定数 (`RANDOM_TARGET_RANGE/OFFSET`、`RANDOM_SPEED_RANGE/OFFSET`、`SPEED_MULT_DIVISOR`、`PERFECT_BONUS_RATIO`) を追加し timerReset/timerCalcScore 内置換。GameMain.cpp の `SetGraphMode`/FPS 待機、Game1/2/3/Menu の DrawString/SetFontSize、Game1/2 の `FrameTmp / 60` を全て定数経由に。約 30 箇所の数値が意味づけされた識別子に置換。ネット系 (`3500`/`256`/`MAKEWORD(1,1)`) は γ で `netBattle` 完成と一体整理予定のため除外。これで **β-D 系完了 → 次はフェーズ γ**

### フェーズ γ-1: 練習モード追加 (完了, 2026-06-24)

ユーザー提案を受けて γ 本体着手の前段として実施。Workflow で 3 案 (Game2 統合 / 新シーン Game4 / Game1 拡張) を並列探索 → ユーザー判断で「**Game3Scene を練習モード本体に転用、Game4Scene を新雛形に複製**」を採用。

- **Game3Scene** = 練習モード本体 (Game1Scene ロジック + render 拡張: 倍率常時公開、`TIMER_STATE tmp = state; timerCalcScore(&tmp);` でリアルタイムスコア表示)。`status`/`state` 共に `static` でファイルスコープ閉鎖 (Game1 側の同名グローバルとの LNK2005 回避)
- **Game4Scene** = 新規追加、現 Game3Scene の空シーン雛形を Copy-Item で複製 + 識別子置換 (`GAMESCENE3_H_` → `GAMESCENE4_H_`、`Game3Scene` → `Game4Scene`、"ゲーム画面１です" → "ゲーム画面４です")。SCENE_NO/sceneTable に登録するが menu[] 非掲載 (= 到達不可、コピー元としてのみ存在)
- **MenuScene** `MENU_MAX` 2→3、`menu[] = {GAME1, GAME2, GAME3}`、`menuList[3]` 末尾を "" → "練習モード"、旧コメント `//本来→SCENE_NO menu[MENU_MAX] = …SCENE_GAME3` 削除 (現実化したので役目終了)、`gapY` 80→60 (3 項目目とヒントの視認性確保)
- **Project2.vcxproj** に Game4Scene.cpp/.h を 1 行ずつ追加 (ClCompile/ClInclude)
- リアルタイムスコアのブレ対策なし (ユーザー判断「リアルタイム性最優先」、`%3.1f` のまま 60FPS 更新)

### フェーズ γ-2: 動く LAN 対戦の最小完成 (完了, 2026-06-24)

Workflow で 3 案 (段階分割 / 一括完成 / 2 分割) を並列探索 → **案 C: 2 分割 (機能→品質)** を採用。本 γ-2 は第 1 段 (機能完成)。ユーザー判断: WSA 配置案 ii (init/release ペアリング)、lisS 案 ii (Defender case 内に socket 作成移動)。

- **SIDE_SELECT UI** ([renderGame2Scene](Project2/Game2Scene.cpp)): MenuScene 流の上下選択 + Z 決定 + X 戻り、x=195/y=200/gapY=60 で `menuList2[]` を描画
- **netBattle 全面整理** (約 145 行 → 約 120 行):
  - `send(remS,...)` → `send(s,...)` 修正 (Challenger)
  - `while (szBuf != NULL)` 無限ループ削除 → 1 回交換
  - `netStatus = 2` 遷移を両 case の受信完了時に追加
  - `lisS` を Defender case 内に移動 (Challenger でのリソースリーク解消)
  - **双方向交換実装** (両者で勝敗表示の対称性確保)
- **WinSock リソース管理** (案 ii ペアリング):
  - `WSAStartup` → `initGame2Scene` (BOOL FALSE 戻りで β-D-3 フォールバック発動)
  - `WSACleanup` → `releaseGame2Scene` (既存維持)
  - `closesocket(s)` (Challenger)、`closesocket(remS)+closesocket(lisS)` (Defender) を完備
- **SIDE_SELECT case の X キー脱出処理追加** + 条件付き break を無条件 break に修正
- **テスト方法**: ビルド済み `Project2.exe` を Explorer から 2 回起動 (Defender 先 → Challenger 後)、Visual Studio デバッグでは 1 プロセスのみのため不可

### フェーズ γ-3: 堅牢化リファクタ (完了, 2026-06-24)

Workflow で 3 カテゴリ (戻り値検査 / enum+定数 / memcpy+細部) を並列探索 → 全 5 判断ポイント推奨採用で実装。動作不変リファクタ、編集順序 B → C → A (依存関係順)。

採用判断:
1. **マクロ化しない** (インライン `if`、+60〜70 行) — 複数戻り値型・複数 closesocket・スコープ依存ローカル変数でマクロでは綺麗に書けず、コメント方針整合
2. **`netBattle` 戻り値統一**: 成功 0 / 失敗 -1 (型ひずみなし、将来 caller でエラー表示の土台)
3. **`NET_STATUS_WAITING` (=1) 残す** — TIMER_STATUS と同じ 3 値構造、将来非同期化布石
4. **定数配置: Game2Scene.cpp ファイルスコープ #define** — NET_SIDE/GAME2_STATE と同じ Game2 専用方針
5. **範囲外細部 (cnt/デバッグ出力位置/旧コメント等) は全てフェーズ δ 送り** — スコープ膨張防止

実装内容:
- [Game2Scene.cpp](Project2/Game2Scene.cpp) 約 421 → 約 495 行
- (B) 4 定数 + 1 enum 追加、約 12 箇所置換
- (C) Challenger/Defender 各 1 箇所の `for (i=0; i<256; i++) { rcScore[i]=szBuf[i]; }` → `memcpy(rcScore, szBuf, NET_BUF_SIZE);`
- (A) 全 WinSock 関数 (gethostname/gethostbyname/socket×2/bind/listen/connect/accept/send×2/recv×2) に統一書式の戻り値検査追加、エラー時 `MyOutputDebugString` + `WSAGetLastError` ログ + LIFO `closesocket` + `return -1`
- テスト: Debug|Win32 ビルド + 単体動作のみ (2 プロセステスト不要、γ-2 で本番確認済、γ-3 は値ラベル化と正常系不変のフェイルセーフ追加のみ)

### フェーズ δ (予定): スコアシステム見直し + Game2Scene 全体清掃 + ネット運用強化

#### δ-1: スコアシステム見直し (本命、ユーザー要望)

ユーザー要望 (2026-06-24)。現状の `timerCalcScore` ([GameSceneMain.cpp](Project2/GameSceneMain.cpp)) は **目標秒数 `CalFrame = RandomTgt * FPS` に強く依存**するため、目標時間が長いほどスコアの絶対値も比例して大きくなる (例: 19 秒目標なら満点 ≈ 1140、1 秒目標なら満点 ≈ 60、ピッタリ時はそれぞれ `* PERFECT_BONUS_RATIO`)。これにより:

- 短い目標時間を引いたプレイヤーは高得点を出しにくい (運要素が過大)
- 同じ「ピッタリ」でも目標時間で報酬が大きく異なる (公平性低)
- 練習モード ([Game3Scene.cpp](Project2/Game3Scene.cpp)) のリアルタイムスコア表示で目標が変わるたびに値域が大きく変動 (UX 低下)

見直し案の例 (フェーズ δ 着手時に検討):
- 正規化スコア (目標時間で割って 0〜1.0 のレートに統一、× 100 等で表示)
- 誤差ベーススコア (`|FrameTmp - CalFrame| / CalFrame * 100%` のような相対誤差)
- 階級制 (誤差 ±5% で S、±10% で A 等のグレード表示)

#### δ-2: Game2Scene 全体清掃 (γ-3 範囲外として持ち越し)

γ-3 でスコープ膨張防止のため範囲外とした細部:
- `cnt` 変数整理 (Defender 内 `cnt = 1` 宣言 / `cnt++` ログ、使用箇所限定的)
- デバッグ出力位置見直し (`gethostname`/`inet_ntoa` を Defender 専用に移動するか共通維持か)
- γ-2 追加ログの書式統一 ("送信:" "受信:" 等)
- 不要コメント削除 (例: `//終了のキーワードを判別する` 等の旧実装由来 — ただし CLAUDE.md コメント方針で役割メモは保護)

#### δ-3: ネット運用強化 (将来課題)

- **同期通信の非同期化** (Defender accept 中フリーズ問題、`ioctlsocket FIONBIO` + `select` または別スレッド)
- **`'localhost'` リテラルの定数化** (現状 1 箇所固定、過剰設計だが任意 NIC への拡張時に必要)
- **Winsock 2.2 アップグレード** (`MAKEWORD(2, 2)`、現状 1.1 で動作中)
- **ネット系定数の GameMain.h 昇格** (Game4 等でネット対戦追加する時点で再判断)
- **`netStatus` への caller 側エラー処理追加** (`moveGame2Scene` 内で `netBattle` 戻り値 -1 をユーザー通知)

δ-1/δ-2/δ-3 はそれぞれ独立着手可能。優先度は δ-1 > δ-2 > δ-3 (ユーザー要望 > リファクタ仕上げ > 将来拡張)。

**着手タイミング**: フェーズ γ (LAN ネット対戦完成) 完了後を想定。スコア仕様の変更はネット対戦のスコア交換プロトコルにも影響するため、γ 完成後に一括見直しが筋。

---

## 過去の整理作業履歴

完了済みの方針判断 (再議論不要):

- **ビルドエラー解消** (前々セッション): Game1Scene.cpp に UTF-8 BOM 付与、Game2Scene.cpp に `#include <tchar.h>` 追加・`netBattle` の case ブロック化・`return 0;` 追加・SOCKET 変数の `INVALID_SOCKET` 初期化。
- **デッドコード整理** (2026-06-22): `initGame2Scene` 内の到達不能な 2 つ目の `return TRUE;` 削除 / Game2Scene.cpp の未使用 `#include <iostream>` 削除 (副作用で `floor` → `floorf` 置換) / Game2Scene.h のインクルードガードを `GAME2SCENE_H` → `GAMESCENE2_H_` に統一。
- **エンコーディング統一** (2026-06-22): 残り 13 ファイル (Project2 配下の .cpp/.h と入力Inputについて.txt、CountTimeSysExample.txt) を CP932 → UTF-8 BOM に変換。[.editorconfig](.editorconfig) を新設して以後の保存を BOM 強制。
