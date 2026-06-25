# Changelog

本プロジェクトの主な変更履歴。書式は [Keep a Changelog](https://keepachangelog.com/ja/1.1.0/) を踏襲。バージョン番号はフェーズ完了でマイナーを 1 上げ、フェーズ内プロセス完了で 3 桁目を 1 上げる独自規則。

開発履歴・設計判断の詳細は [CLAUDE.md](CLAUDE.md) を参照。

---

## [0.10.0] - 2026-06-25 — フェーズ δ 完了

### Changed
- **δ-3b**: ネット運用強化の本体改修パート。同期 `netBattle` 関数を全面解体し、毎フレーム駆動の非同期 state machine へ移行。フェーズ δ 完了 (α/β/γ/δ 全コンプリート)
- ソケット (`netSocket` / `netListenSocket`) を file-scope に昇格し、フレームをまたいで保持。`ioctlsocket FIONBIO` で全 socket を non-blocking 化、`recv` / `accept` の `WSAEWOULDBLOCK` を「次フレームで再試行」として正常扱い。Server の `accept` 待機中フリーズ問題を解消
- `moveGame2Scene` の `GAME2_STATE_DONE` ケースに `netStep()` を組み込み、毎フレーム NET_STATUS に応じた 1 ステップ進行 (CONNECTED → SENT/RECEIVED → EXCHANGED → ENDING → END)
- `NET_STATUS_EXCHANGED` 以降 (ENDING / END 含む) は `renderGame2Scene` で勝敗結果を継続表示
- 相手スコア表示で `SCORE:` プレフィックスを剥がして `%.2f` 数値表示 (γ-3 までは raw 文字列をそのまま表示していた)
- X キーで途中離脱時に `BYEBYE` をベストエフォート送信 + ソケット LIFO 順クローズ + メニュー遷移
- R キーのリセットを `NET_STATUS_INIT` / `END` のみ受け付け (battle 中の誤操作防止)

### Added
- `GAME2_STATE_ERROR` 状態 + `netErrorMessage` バッファ。`netTransitionToError(message)` ヘルパで遷移、`renderGame2Scene` で原因表示と「Z/X キーで戻る」案内
- 進展なし 30 秒 (`NET_TIMEOUT_FRAMES = 30 * FPS = 1800` フレーム) のタイムアウト機構。`netStallFrames` カウンタが state 遷移ごとにリセットされ、ENDING フェーズも含めて停滞 30 秒で `GAME2_STATE_ERROR` 遷移
- `NET_MSG_TYPE` enum + `netParseMessage` パーサ (SPEC_NETWORK.md §5.2 準拠、`SCORE:` / `END` / `BYEBYE` / UNKNOWN を判別)
- `NET_MSG_END_LEN` / `NET_MSG_BYEBYE_LEN` 定数 (毎回 `strlen` を避ける)
- 非同期 state machine 用ヘルパ群: `netSetNonBlocking` / `netCleanupSockets` / `netTryRecv` / `netTrySend` / `netStart` / `netStep`
- `releaseGame2Scene` でソケット未解放時に `BYEBYE` 送信 + cleanup (シーン途中離脱の保険)

### Removed
- 旧 `netBattle(float score)` 関数 (約 150 行の同期実装、機能は分解して上記ヘルパ群に移行)
- Server 側 `accept` の同期ブロック (フリーズ仕様)

### Known Limitations
- TCP は stream protocol のため、Client が SCORE と END を連続送信した時に Server の単一 `recv` で結合されうる。現状は localhost + 自然なフレーム間隔で分離される前提に頼る。長さプレフィックス等の正式なメッセージ framing は将来課題
- ネット系定数の `GameMain.h` 昇格は見送り (Game4 等でネット対戦追加する時点で再判断)

---

## [0.9.3] - 2026-06-25

### Changed
- **δ-3a**: ネット運用強化の機械的整理パート。本体改修 (δ-3b、non-blocking + state machine) の足場を整える
- WinSock を 1.1 → 2.2 へアップグレード (`MAKEWORD(2, 2)` / `<winsock2.h>` / `ws2_32.lib`)。将来 `getaddrinfo` 等の WinSock 2 API を使う場合に備えて土台統一
- `NET_STATUS` enum を 3 値 (INIT/WAITING/RECEIVED) から 7 値 (INIT/CONNECTED/SENT/RECEIVED/EXCHANGED/ENDING/END) に拡張 (SPEC_NETWORK.md §9 準拠、δ-3b で残り 5 値を使用開始)
- 旧 `NET_STATUS_RECEIVED (= 2)` (γ-3 では「双方送受信完了」の意味) を新 `NET_STATUS_EXCHANGED (= 4)` に意味リマップ (SPEC §9.3 マッピング表通り)
- `NET_SIDE` enum を `SIDE_SELECT` enum にリネーム (`NET_SIDE_CHALLENGER` → `SIDE_SELECT_CLIENT` / `NET_SIDE_DEFENDER` → `SIDE_SELECT_SERVER` / `NET_SIDE_UNSELECTED` → `SIDE_SELECT_NONE`、技術用語に統一)
- スコア送信書式を `"%f"` から `"SCORE:%.2f"` (プレフィックス付き、δ-3 のデータフォーマット) に変更。受信側は `strncmp` で `"SCORE:"` プレフィックスを判別しペイロードを `strtof` でパース
- `netBattle` の caller 側 (`moveGame2Scene` の N キー押下処理) で戻り値検査を追加。失敗時に `MyOutputDebugString` でログ出力 (UI 表示は δ-3b で追加予定)
- WinSock デバッグログ内の役割名 ("Challenger" / "Defender") を新名称 ("Client" / "Server") に更新

### Added
- `NET_TARGET_HOST "localhost"` 定数 (将来の任意 IP 拡張時に 1 箇所変更で済む)
- データフォーマット定数 `NET_MSG_PREFIX_SCORE "SCORE:"` / `NET_PREFIX_LEN_SCORE 6` / `NET_MSG_END "END"` / `NET_MSG_BYEBYE "BYEBYE"` (END/BYEBYE は δ-3b の state machine と連動使用)

---

## [0.9.2] - 2026-06-25

### Changed
- **δ-2**: Game2Scene 全体清掃。γ-3 でスコープ膨張防止のため範囲外とした細部を整理
- 自ホスト名取得プロローグ (`gethostname` / `gethostbyname` / `inet_ntoa`) を関数頭の共通位置から Defender case 内に移動 (Challenger は `localhost` 接続のため自ホスト情報は不要、意味整合を強化)
- Defender 受信ログを `%ld>%s` 書式から Challenger と同じ `受信: %s` 書式に統一
- `initGame2Scene` のログ「簡易通信プログラム 起動準備：第1段階完了」を意味の明確な「WSAStartup 完了」に変更 (第 2 段階以降が存在しない曖昧表現を解消)

### Added
- SIDE_SELECT メニュー専用レイアウト定数 (`SIDE_SELECT_X_ITEM` / `SIDE_SELECT_Y_FIRST` / `SIDE_SELECT_GAP_Y`) を [GameMain.h](Project2/GameMain.h) に集約 (β-D-4 のレイアウト集約方針と整合)
- `static char rcScore[NET_BUF_SIZE]` (`static` 修飾子を付与、Game2Scene 内に閉じる)

### Removed
- `cnt` 変数 (Defender 内で 1 回しか出ない `cnt++` ログ用、書式不統一の原因)
- `netBattle` プロトタイプ宣言 (定義が `moveGame2Scene` より上にあるので前方宣言不要)
- `menuList2[3]` の未使用 3 要素目 `""` (配列サイズを `MENU_MAX_G` と一致させた)

---

## [0.9.1] - 2026-06-25

### Changed
- **δ-1**: スコアシステムを正規化スコア (0〜100、目標時間によらず満点 100 固定) に再設計。Workflow で 5 案 × 5 観点 × 3 票の独立判定を経て案 A (誤差率ベース正規化) を採用。`timerCalcScore` は `Score = max(0, 1 - |誤差|/CalFrame) × 100` の連続関数化、不連続段差は撤廃
- ネット対戦の勝敗判定から `floorf` 整数化を除去し float 直接比較に変更 (値域縮小により細かい優劣も拾えるように)

### Added
- ピッタリ達成フラグ `IsPerfect` を `TIMER_STATE` に追加 (Score 表示が "100.0" と見える条件 = `Score >= 99.95` で TRUE)
- Game1/2/3 シーン全てで PERFECT! 演出を描画 (赤文字、Game3 ではリアルタイム表示)
- `LAYOUT_X_PERFECT_OFFSET` をレイアウト定数群に追加 (スコア値との重なり防止)

### Removed
- `TIMER_STATE.ScMulti` (旧計算式の中間値、新式では未使用)
- `PERFECT_BONUS_RATIO 1.25f` (1.25 倍ジャンプの不連続段差ボーナス、IsPerfect フラグ + 描画演出に役割移行)

---

## [0.9.0] - 2026-06-24 — フェーズ γ 完了

### Changed
- **γ-3**: ネットワーク処理を堅牢化。全 WinSock 関数 (`gethostname` / `gethostbyname` / `socket` × 2 / `bind` / `listen` / `connect` / `accept` / `send` × 2 / `recv` × 2) に統一書式の戻り値検査を追加。エラー時は `MyOutputDebugString` + `WSAGetLastError` ログ + LIFO 順 `closesocket` + `return -1` で統一
- `netBattle` 戻り値を `成功 0 / 失敗 -1` に統一
- Challenger/Defender 各 1 箇所の `rcScore` コピーループを `memcpy(rcScore, szBuf, NET_BUF_SIZE)` に置換

### Added
- `NET_STATUS` enum 化 (`INIT` / `WAITING` / `RECEIVED`、β-C-4 連動。`WAITING` は将来の非同期化布石)
- ネット系定数 (`NET_PORT 3500` / `NET_BUF_SIZE 256` / `SERVER_NAME_SIZE 128` / `WINSOCK_VERSION_REQ`) を Game2Scene.cpp ファイルスコープに集約

---

## [0.8.2] - 2026-06-24

### Added
- **γ-2**: LAN 対戦の動く最小実装が完成。双方向スコア交換、SIDE_SELECT メニュー UI (挑む側 / 挑まれる側 + Z 決定 + X 戻り)
- `WSAStartup` / `WSACleanup` をシーン寿命に合わせて `initGame2Scene` / `releaseGame2Scene` にペアリング配置

### Fixed
- `send(remS, ...)` 未初期化バグを `send(s, ...)` に修正 (Challenger ケース)
- `while (szBuf != NULL)` 無限ループを撤去、1 回交換に変更
- `netStatus = 2` への遷移経路欠落を両ケースで補完
- `lisS` リソースリーク (Challenger でも作成されていた) を Defender ケース内に集約

### Known Issues
- Defender 側で N キー押下後、Challenger 起動までゲームループ凍結 (ESC 反応不能、タスクマネージャ強制終了で脱出)。非同期化はフェーズ δ-3 で対応予定

---

## [0.8.1] - 2026-06-24

### Added
- **γ-1**: 練習モード ([Game3Scene](Project2/Game3Scene.cpp)) を新規追加。Game1Scene のロジックをベースに、倍速を計測開始前から公開し、MEASURING 中もリアルタイムスコアを表示。`TIMER_STATE tmp = state; timerCalcScore(&tmp);` のコピー方式で副作用を遮断
- 新規シーン雛形 ([Game4Scene](Project2/Game4Scene.cpp)) を追加 (メニュー非掲載、将来 Game5/6 を増やす時のコピー元)
- メニュー画面に「練習モード」を追加 (`MENU_MAX` 2→3、`menuList[3]` の末尾を埋める)

---

## [0.8.0] - 2026-06-24 — フェーズ β 完了

### Changed
- **β-D-4**: マジックナンバー定数化を一括実施。画面サイズ (`SCREEN_WIDTH` / `HEIGHT` / `BPP`)、FPS (`FPS` / `MS_PER_SEC`)、フォントサイズ (`FONT_SIZE_DEFAULT`)、Game 共通レイアウト (`LAYOUT_X_DEFAULT` / `LAYOUT_Y_STATUS`〜`Y_SCORE`)、メニュー専用レイアウト (`MENU_X_*` / `MENU_Y_*`) を [GameMain.h](Project2/GameMain.h) に集約
- 乱数定数 (`RANDOM_TARGET_RANGE/OFFSET` / `RANDOM_SPEED_RANGE/OFFSET` / `SPEED_MULT_DIVISOR` / `PERFECT_BONUS_RATIO`) を [GameSceneMain.cpp](Project2/GameSceneMain.cpp) ファイルスコープに追加
- 約 30 箇所の数値リテラルを意味づけられた識別子に置換

---

## [0.7.8] - 2026-06-24

### Changed
- **β-D-3**: エラーハンドリング強化 (案 B: 中程度、フォールバック動作追加)
- `MyOutputDebugString` マクロを Game2Scene.cpp 専用から [GameMain.h](Project2/GameMain.h) に昇格 (全シーン共通)
- `initCurrentScene` を `BOOL` 戻り値化、`FrameMove` に SCENE_MENU フォールバック (init 失敗時) + 諦め (SCENE_NONE) ロジック追加
- `changeScene` 範囲外検知時にログ出力 + Debug ビルド assert を追加 ([GameSceneMain.cpp](Project2/GameSceneMain.cpp))

---

## [0.7.7] - 2026-06-24

### Changed
- **β-D-2d**: スコア計算共通化。Game1Scene.cpp の `case TIMER_STATUS_DONE` および Game2Scene.cpp の `case GAME2_STATE_DONE` 内の同一スコア計算ブロック (18 行 × 2) を `timerCalcScore(&state);` 1 行呼び出しに置換
- `timerCalcScore(TIMER_STATE*)` を [GameSceneMain.cpp](Project2/GameSceneMain.cpp) に共通関数として追加

---

## [0.7.6] - 2026-06-24

### Added
- **β-D-2b+c**: `TIMER_STATE` 構造体を [GameSceneMain.h](Project2/GameSceneMain.h) に新規定義 (7 メンバ: RandomTgt / CalFrame / RandomMtp / CalMulti / FrameTmp / ScMulti / Score)
- `timerReset(TIMER_STATE*)` 共通関数を追加 (乱数初期化ロジックの重複解消)

### Changed
- Game1Scene.cpp と Game2Scene.cpp の計測変数 7 個 × 2 セットを `TIMER_STATE state = {};` 1 個に集約 (約 40 箇所の参照を `state.X` 形式に置換)

### Removed
- 重複していた `targetTimeSet` / `targetTimeSet2` 関数定義 (Game1/2 双方から削除、サフィックス`2`完全消滅)

---

## [0.7.5] - 2026-06-24

### Changed
- **β-D-2a**: 色変数共通化。7 色 (`ColorWhite`〜`ColorSkyLike`) を [GameMain.cpp](Project2/GameMain.cpp) に集約定義し [GameMain.h](Project2/GameMain.h) で `extern` 公開
- Game1/2 のサフィックス`2`並走 (`ColorWhite2` 等) を解消、[MenuScene.cpp](Project2/MenuScene.cpp) と Game3Scene.cpp の `GetColor` 直呼び 7 箇所も共通変数に置換

---

## [0.7.4] - 2026-06-24

### Changed
- **β-D-1**: シーンディスパッチャ共通化。[GameSceneMain.cpp](Project2/GameSceneMain.cpp) の 5 つの `switch (sceneNo)` を `SCENE_HANDLERS` 関数ポインタ束 + `sceneTable` 配列 + 1 行ディスパッチへ置換
- シーン追加コストが 5 箇所 → 2 箇所 (enum + テーブル) に削減

### Removed
- `prevScene` 変数 (シーン遷移ロジックで不要に)
- `MessageBox` 呼び出し 5 箇所 (プラットフォーム依存解消)

---

## [0.7.3] - 2026-06-23

### Changed
- **β-C-2**: Game2Scene 専用 `GAME2_STATE` enum (5 値: INIT / READY / MEASURING / DONE + SIDE_SELECT) を新設、`status2` を全置換 (18 箇所)。SIDE_SELECT が Game1 に無い概念のため `TIMER_STATUS` を汚さず別型として定義

---

## [0.7.2] - 2026-06-23

### Changed
- **β-C-1**: 共通 `TIMER_STATUS` enum (4 値: INIT / READY / MEASURING / DONE) を [GameSceneMain.h](Project2/GameSceneMain.h) に新設、Game1Scene.cpp の `status` を全置換 (14 箇所)

---

## [0.7.1] - 2026-06-23

### Removed
- **β-A**: 残った dead code 一掃
- [GameMain.cpp](Project2/GameMain.cpp) の `/* ... */` で囲まれた旧 MessageBox ウィンドウモード確認ブロック
- [Game1Scene.cpp](Project2/Game1Scene.cpp) の未使用 `#include <math.h>` / `#include <string.h>`
- Game2Scene.cpp `netBattle` 関数内の旧ローカル宣言コメント `//char rcScore[256];`
- [GameMain.cpp](Project2/GameMain.cpp) の未使用変数 `int game_status = GAMETITLE;`

---

## [0.7.0] - 2026-06-23 — フェーズ α 完了

### Removed
- **α**: 未使用コード一掃。参照ゼロのシンボルとコメントを削除
- [MenuScene.cpp](Project2/MenuScene.cpp) の未使用変数 `int startfont;`
- [Game2Scene.cpp](Project2/Game2Scene.cpp) の未使用変数 `int startfont2;`
- [Game2Scene.cpp](Project2/Game2Scene.cpp) の未使用配列 `SCENE_NO netMenu[MENU_MAX_G] = {…};`
- 旧コードコメント `//本来→SCENE_NO menu[MENU_MAX] = {…, SCENE_GAME3};`

---

## [0.6.0] - 2026-06-22

リファクタリング開始時点 (基準バージョン)。Claude Code (ultracode) を併用した段階的コード品質改善の起点。10 観点 (重複 / 命名 / 未使用コード / 長関数 / 深ネスト / マジック数 / コメント / エラー処理 / 責務 / プラットフォーム依存) で網羅診断を実施し 210 件の改善余地を特定、フェーズ分けして適用開始。

### Added
- 全プロジェクトファイルを UTF-8 BOM + CRLF に統一 ([.editorconfig](.editorconfig) で強制)
- [CLAUDE.md](CLAUDE.md) を新設しプロジェクトルールを文書化
