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

- `netBattle` のネットワーク同期 (Game2Scene.cpp)
- `case 0` 内で `s` ではなく `remS` を使うべきという既知ロジックバグ
- Game3Scene の中身全般 ([Project2/Game3Scene.cpp](Project2/Game3Scene.cpp))
- メニュー第 3 項目 (`SCENE_GAME3`)

---

## 過去の整理作業履歴

完了済みの方針判断 (再議論不要):

- **ビルドエラー解消** (前々セッション): Game1Scene.cpp に UTF-8 BOM 付与、Game2Scene.cpp に `#include <tchar.h>` 追加・`netBattle` の case ブロック化・`return 0;` 追加・SOCKET 変数の `INVALID_SOCKET` 初期化。
- **デッドコード整理** (2026-06-22):
  - `initGame2Scene` 内の到達不能な 2 つ目の `return TRUE;` 削除
  - Game2Scene.cpp の未使用 `#include <iostream>` 削除 (副作用で `floor` → `floorf` 置換)
  - Game2Scene.h のインクルードガードを `GAME2SCENE_H` → `GAMESCENE2_H_` に統一
- **エンコーディング統一** (2026-06-22): 残り 13 ファイル (Project2 配下の .cpp/.h と入力Inputについて.txt、CountTimeSysExample.txt) を CP932 → UTF-8 BOM に変換。[.editorconfig](.editorconfig) を新設して以後の保存を BOM 強制。
