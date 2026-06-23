# CLAUDE.md — TwistTimeStopper プロジェクト情報

Visual Studio (MSVC) + DxLib による C++ ゲーム。Project2.sln / [Project2/](Project2/) 配下が本体。日本語 Windows 環境 (コードページ 932) でビルドする前提。

---

## ファイルエンコーディング: 全ファイル UTF-8 BOM 統一

**2026-06-22 にプロジェクト全体を UTF-8 BOM (`EF BB BF`) に統一済み。新規ファイルも UTF-8 BOM で作成すること。**

[.editorconfig](.editorconfig) で `charset = utf-8-bom` を強制しているため、Visual Studio / VS Code は自動的に UTF-8 BOM で保存する。手動編集する場合も Edit/Write ツールで素直に書ける (BOM 付き UTF-8 はツールが正しく扱える)。

### 新規ファイル作成時の注意

PowerShell で空ファイルを作る場合は BOM を明示:
```powershell
[IO.File]::WriteAllText($path, $content, [Text.UTF8Encoding]::new($true))
```

`Set-Content` や `Out-File` のデフォルトは UTF-8 (BOM なし) になりがちなので避ける。

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

[Project2/GameSceneMain.h](Project2/GameSceneMain.h) で `SCENE_NO` enum を定義: `SCENE_NONE (-1)`, `SCENE_MENU`, `SCENE_GAME1`, `SCENE_GAME2`, `SCENE_GAME3`, `SCENE_MAX`。
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

新規シーン追加時はこの 5 関数を揃え、[Project2/GameSceneMain.cpp](Project2/GameSceneMain.cpp) のディスパッチに登録する。

### 遷移

`changeScene(SCENE_NO no)` で `nextScene` を設定。次フレームに `sceneNo != nextScene` を検出した時点で `releaseXXX → initYYY → sceneNo = nextScene` の順で実行される。

### ⚠️ CollideCallback の制約

[Project2/GameSceneMain.cpp:62](Project2/GameSceneMain.cpp#L62) 付近に「**ここでは要素を削除しないこと！！**」コメントあり。DxLib の衝突判定列挙の最中に呼ばれるため、コールバック内で対象オブジェクトを `delete` するとイテレータ破壊で UB になる。削除はフラグを立てて `moveXXXScene` 内で実施するパターン。

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
| GameStatus.h | `GAMESTATUS_H_` |
| GameSceneMain.h | `GAMESCENEMAIN_H_` |

Game1/2/3Scene は **`GAMESCENE<N>_H_`** (数字は GAMESCENE の後)。`GAME<N>SCENE_H_` ではないので注意。

### インデント

[.editorconfig](.editorconfig) で `indent_style = tab`。既存ファイルもタブ。

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

**未完成。完成させない方針** (下記「未完成機能」参照)。ただし現状の構造を把握するために要点を記載:

- **プロトコル**: TCP (`SOCK_STREAM`, `IPPROTO_TCP`)、ポート **3500**
- **接続先**: `"localhost"` ハードコード ([Project2/Game2Scene.cpp:169](Project2/Game2Scene.cpp#L169))
- **データ**: スコア (float) を `snprintf("%f", ...)` で文字列化して送受信
- **役割切替**: `sideSelect` 変数 (UI 上で N キー押下時に切り替え予定だが、その UI 自体未実装)
  - `sideSelect = 0`: クライアント (`socket` → `connect` → `send`)
  - `sideSelect = 1`: サーバー (`socket` → `bind` → `listen` → `accept` → `recv`)
- **既知バグ**: クライアント側の `case 0` で `remS` (未初期化) を `send` 引数に渡している。本来は送信用 socket `s` を渡すべき。実行すると失敗するが、警告は SOCKET 変数の `INVALID_SOCKET` 初期化で抑止済み。

---

## DxLib のお作法 (細かい点)

- **フォント**: `SetFontSize(N)` / `ChangeFontType(DX_FONTTYPE_*)` はグローバル状態なので、各シーンの init で必ず設定し直す ([Project2/Game2Scene.cpp:85-86](Project2/Game2Scene.cpp#L85-L86) が代表例)。
- **色**: `GetColor(R, G, B)` の戻り値 `unsigned int` はキャッシュする (毎フレーム計算しない)。[Project2/Game1Scene.cpp:25-31](Project2/Game1Scene.cpp#L25-L31) 参照。
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

## 未完成機能 — **完成させなくて良い**

過去の方針: 「未完成機能の完成は不要、プレースホルダ/コメントで OK」。以下は触らない:

- `netBattle` のネットワーク同期 (Game2Scene.cpp) — 上述の `remS` バグ含めて触らない
- Game3Scene の中身全般 ([Project2/Game3Scene.cpp](Project2/Game3Scene.cpp))
- メニュー第 3 項目 (`SCENE_GAME3`)

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

### フェーズ β 以降: 未着手

診断で残ったテーマ (Game1/2Scene のサフィックス `2` 重複解消・共通基盤化、`status`/`sideSelect`/`netStatus` の enum 化、`netBattle` の分離とバグ修正、エラーハンドリング強化、シーンディスパッチャの共通化、マジックナンバー定数化、過剰コメント整理 等) を順次対応予定。

---

## 過去の整理作業履歴

完了済みの方針判断 (再議論不要):

- **ビルドエラー解消** (前々セッション): Game1Scene.cpp に UTF-8 BOM 付与、Game2Scene.cpp に `#include <tchar.h>` 追加・`netBattle` の case ブロック化・`return 0;` 追加・SOCKET 変数の `INVALID_SOCKET` 初期化。
- **デッドコード整理** (2026-06-22): `initGame2Scene` 内の到達不能な 2 つ目の `return TRUE;` 削除 / Game2Scene.cpp の未使用 `#include <iostream>` 削除 (副作用で `floor` → `floorf` 置換) / Game2Scene.h のインクルードガードを `GAME2SCENE_H` → `GAMESCENE2_H_` に統一。
- **エンコーディング統一** (2026-06-22): 残り 13 ファイル (Project2 配下の .cpp/.h と入力Inputについて.txt、CountTimeSysExample.txt) を CP932 → UTF-8 BOM に変換。[.editorconfig](.editorconfig) を新設して以後の保存を BOM 強制。
