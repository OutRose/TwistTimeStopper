# SPEC_NETWORK.md — LAN 対戦システム仕様書

**プロジェクト**: TwistTimeStopper
**作成日**: 2026-06-24
**対象フェーズ**: δ-3 (ネット運用強化) — 将来実装方針
**現状コード**: [Project2/Game2Scene.cpp](Project2/Game2Scene.cpp) (γ-3 時点の同期版を前提に再設計予定)

本ファイルは [CLAUDE.md](CLAUDE.md) の「フェーズ δ-3」セクションから参照される。本仕様で示す挙動は **δ-3 着手時に実装する目標仕様** であり、γ-3 時点のコードはこの仕様の一部 (双方向交換・基本エラーログ等) のみを満たす。

---

## 1. 概要

`Game2Scene` の LAN 対戦機能を、現状の同期 (blocking) ベースの最小実装から、**non-blocking socket + 状態遷移ベース** の堅牢な実装へ刷新する。

刷新の動機:
- 現状は `accept` / `recv` 待機中にゲームループ全体がフリーズ ([CLAUDE.md「γ-2 仕様化された制約」](CLAUDE.md#L194) 既述)
- ESC キー応答不能、Task Manager 強制終了が脱出経路になっている
- δ-3 で `ioctlsocket FIONBIO` + `select` ベースに移行し、待機中も画面更新と入力受付を継続できるようにする

---

## 2. 接続モデル

### 2.1 トポロジ
- **1 対 1 の対戦** (peer-to-peer の最小構成)
- 役割は固定的に **サーバー側 (受信側) / クライアント側 (送信側)** の 2 種類

### 2.2 役割定義
| 役割 | 動作 | WinSock 関数の典型呼び出し |
|---|---|---|
| **サーバー側 (受信側)** | 先に起動して `bind` → `listen` → `accept` で接続待ち | `socket` → `bind` → `listen` → `accept` |
| **クライアント側 (送信側)** | 後から起動してサーバーに `connect` | `socket` → `connect` |

接続確立後は両者とも `send` / `recv` を双方向で実行する (役割名は接続フェーズの初動だけを表す)。

### 2.3 接続順序
**固定**。サーバー側を先に起動して `accept` 待機状態にしておき、クライアント側はその後で `connect` する。順序が逆だと `connect` が即座に失敗 (`WSAECONNREFUSED` 等)。

### 2.4 接続終了
終了通知 (`END` / `BYEBYE`) の送受信完了後に **自動切断**。`closesocket` は LIFO 順 (Defender なら `remS` → `lisS`、Challenger なら `s`)。

---

## 3. ゲームフロー

```
1. 両者が SIDE_SELECT で役割を選択
2. サーバー側が listen → accept 待機
3. クライアント側が connect
4. 接続確立 (netStatus = CONNECTED)
5. 両者が同じタイマー測定ゲームをプレイ
6. 各自で timerCalcScore() を実行してスコア算出
7. 各自のスコアを相手へ送信 (SCORE:1234.56 形式)
8. 相手からスコアを受信、両者のスコアを比較して勝敗判定
9. 勝敗判定後、終了通知 (END / BYEBYE) を送受信
10. 自動切断、メニューへ戻る
```

---

## 4. データ送信タイミング

**1 セッション中に 2 回の送信**:

| 回 | 送信タイミング | 送信内容 | 形式 |
|---|---|---|---|
| **1 回目** | 測定完了直後 (`timerCalcScore` 実行直後) | 自分のスコア | `SCORE:1234.56` |
| **2 回目** | 勝敗判定後 | 終了通知 | `END` (通常終了) または `BYEBYE` (異常終了通知) |

---

## 5. データフォーマット

### 5.1 プレフィックス付き文字列

| プレフィックス | 用途 | 例 | 値域 |
|---|---|---|---|
| `SCORE:` | スコア送信 | `SCORE:1234.56` | float、`%.2f` 整形 |
| `END` | 通常終了通知 | `END` | プレフィックスなし、固定文字列 |
| `BYEBYE` | 異常終了通知 | `BYEBYE` | プレフィックスなし、固定文字列 |

### 5.2 パース規約
- 受信文字列の先頭 6 文字が `"SCORE:"` ならスコア (残りを `atof` で float 化)
- 受信文字列が `"END"` または `"BYEBYE"` なら終了通知
- それ以外はプロトコルエラー → ログ出力 + `netStatus = END` で切断

### 5.3 バッファサイズ
γ-3 で既に定数化済 ([Game2Scene.cpp](Project2/Game2Scene.cpp)):
```cpp
#define NET_BUF_SIZE 256  // 送受信バッファサイズ
```

---

## 6. 通信方向と送信順序

### 6.1 通信方向
**双方向**。サーバー側・クライアント側ともに `send` と `recv` を実行する (γ-2 で確立済の方針)。

### 6.2 送信順序
**非同期**。両者が独立に送信し、受信タイミングは状況依存 (相手の進行スピードや測定時間の長さに左右される)。

これにより:
- 一方が先に測定終了 → 先にスコア送信 → 相手側はまだ測定中で `recv` 未実行
- 後から測定終了した側が `recv` した時点で初めてスコア交換が完成

実装上の制約: **non-blocking socket + `select` で「データが届いているか」を毎フレーム検査** し、届いていれば `recv`、まだなら次フレームへ進む構造とする。

---

## 7. エラー処理方針

### 7.1 基本方針
**リトライしない**。エラー発生時は即座に:

1. 画面にエラーメッセージを表示 (例: `"接続に失敗しました"`)
2. `MyOutputDebugString` でログ出力 (`WSAGetLastError` の値を含む)
3. メニュー (`SCENE_MENU`) に戻る

### 7.2 エラー種別と対応

| 種別 | 発生例 | 画面表示 | 復帰先 |
|---|---|---|---|
| 接続失敗 | `connect` / `accept` 失敗 | 「接続に失敗しました」 | メニュー |
| 送信失敗 | `send` 失敗 | 「送信に失敗しました」 | メニュー |
| 受信失敗 | `recv` 失敗 | 「受信に失敗しました」 | メニュー |
| プロトコルエラー | 不明なプレフィックス受信 | 「通信エラー」 | メニュー |
| タイムアウト | 一定時間 (例 30 秒) 進展なし | 「タイムアウト」 | メニュー |

### 7.3 異常終了の相手側通知
自分側でエラー検知 → 可能なら `BYEBYE` を送信してから切断 → 相手側にも異常終了を伝える。

---

## 8. 実装方針

### 8.1 Non-blocking socket
全 socket を `ioctlsocket(s, FIONBIO, &nonblocking)` で non-blocking 化する。

`recv` / `accept` 等は即座に `SOCKET_ERROR` + `WSAEWOULDBLOCK` を返すケースを正常扱いとして処理:
```cpp
int n = recv(s, buf, sizeof(buf), 0);
if (n == SOCKET_ERROR) {
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK) {
        // まだデータが来ていないだけ、次フレームで再試行
        return;
    }
    // 本物のエラー
    MyOutputDebugString(_T("recv failed (err=%d)\n"), err);
    netStatus = NET_STATUS_END;
    return;
}
```

### 8.2 `select` による多重化
複数の socket を同時に監視する場合は `select(...)` で「読み取り可能なもの」を毎フレーム検査。タイムアウトは 0 秒 (= 即時返却) で「polling」スタイル。

---

## 9. netStatus 状態遷移 (7 状態)

γ-3 時点の `NET_STATUS` enum (3 値) を δ-3 で 7 値に拡張する。

```cpp
typedef enum _NET_STATUS {
    NET_STATUS_INIT      = 0,    // 接続未確立 (γ-3 既存)
    NET_STATUS_CONNECTED = 1,    // 接続確立済み、両者未送信未受信
    NET_STATUS_SENT      = 2,    // 自分は送信完了、相手から未受信
    NET_STATUS_RECEIVED  = 3,    // 相手から受信完了、自分は未送信
    NET_STATUS_EXCHANGED = 4,    // 双方の送受信完了、勝敗判定可能
    NET_STATUS_ENDING    = 5,    // 終了通知の送受信中
    NET_STATUS_END       = 6     // 切断完了
} NET_STATUS;
```

### 9.1 状態遷移図

```
        [INIT]
           |
           | (connect/accept 成功)
           v
       [CONNECTED]
        /        \
       /          \
   (自send完了)  (相手からrecv完了)
       |             |
       v             v
     [SENT]      [RECEIVED]
       \             /
        \           /
     (相手からrecv完了) (自send完了)
            \    /
             v  v
         [EXCHANGED]
              |
              | (END/BYEBYE 送受信開始)
              v
          [ENDING]
              |
              | (送受信完了)
              v
            [END]
              |
              | (closesocket、メニュー復帰)
```

### 9.2 各状態での主な処理

| 状態 | move 処理 | render 処理 |
|---|---|---|
| `INIT` | `socket` / `bind` / `listen` / `connect` を試行 | 「接続中…」 |
| `CONNECTED` | 測定ゲーム本体 (`Game1Scene` 相当) を実行 | ゲーム画面 |
| `SENT` | 相手からの `recv` を polling | 「相手のスコア待ち…」 |
| `RECEIVED` | 自分の `send` を試行 (まだ未送信なら) | 「スコア送信中…」 |
| `EXCHANGED` | 勝敗判定、`END` 送信開始 | 勝敗結果 |
| `ENDING` | `END` の send/recv 完了を待つ | 「終了処理中…」 |
| `END` | `closesocket`、`changeScene(SCENE_MENU)` | (即遷移) |

### 9.3 γ-3 既存値とのマッピング

| γ-3 NET_STATUS | δ-3 NET_STATUS |
|---|---|
| `INIT (0)` | `INIT (0)` (継続) |
| `WAITING (1)` | `CONNECTED (1)` (意味を昇格、γ-3 では未使用の布石) |
| `RECEIVED (2)` | `EXCHANGED (4)` (γ-3 の「受信完了 = 勝敗判定可」を δ-3 では送受信両方完了に細分化) |

---

## 10. sideSelect 定義

γ-3 時点で `NET_SIDE` enum として既存 ([Game2Scene.cpp](Project2/Game2Scene.cpp))。δ-3 で意味を明確化:

```cpp
typedef enum _SIDE_SELECT {
    SIDE_SELECT_CLIENT = 0,    // 送信側 = クライアント (後接続、connect)
    SIDE_SELECT_SERVER = 1,    // 受信側 = サーバー (先接続待機、bind/listen/accept)
    SIDE_SELECT_NONE   = 2     // 未選択 (SIDE_SELECT メニュー UI の初期状態)
} SIDE_SELECT;
```

### 10.1 γ-3 既存名とのマッピング

| γ-3 NET_SIDE | δ-3 SIDE_SELECT | 理由 |
|---|---|---|
| `NET_SIDE_CHALLENGER (0)` | `SIDE_SELECT_CLIENT (0)` | 「挑む側 = 後から接続する側 = クライアント」を技術用語に統一 |
| `NET_SIDE_DEFENDER (1)` | `SIDE_SELECT_SERVER (1)` | 「待ち受ける側 = サーバー」と一致 |
| `NET_SIDE_NONE (2)` | `SIDE_SELECT_NONE (2)` | 同義、名前のみ変更 |

UI 表示文字列 (`menuList2[]`) は「挑む側 / 守る側」を維持してもよい (ユーザー向け表現と内部識別子を分離)。

---

## 11. δ-3 着手時の作業ステップ (見積)

着手前に CLAUDE.md フェーズ δ-3 と本仕様を再読し、最新化された情報で再計画すること。

1. **non-blocking 化**: `ioctlsocket FIONBIO` 全 socket、`recv` の `WSAEWOULDBLOCK` 正常扱い
2. **`select` 導入**: 複数 socket 監視、毎フレーム polling
3. **NET_STATUS 7 値拡張**: enum 定義変更、`moveGame2Scene` / `renderGame2Scene` の状態分岐再構築
4. **`SIDE_SELECT` リネーム**: `NET_SIDE_*` → `SIDE_SELECT_*` 全置換 (要 grep 確認)
5. **データフォーマット導入**: `"SCORE:%.2f"` / `"END"` / `"BYEBYE"` のパーサ追加
6. **エラー画面実装**: `renderGame2Scene` にエラーメッセージ表示状態を追加、`changeScene(SCENE_MENU)` 遷移
7. **タイムアウト**: タイマー (例 30 秒) で `netStatus = END` 強制遷移
8. **`localhost` リテラル定数化**: `#define NET_TARGET_HOST "localhost"` で集約
9. **動作検証**: 2 プロセス起動で正常系 + サーバー未起動・途中切断・タイムアウトの異常系

---

## 12. 制約・既知の限界

- **IPv4 のみ** (`gethostbyname` 使用、IPv6 対応は `getaddrinfo` への置換が必要)
- **WinSock 1.1** で動作中 (γ-3 時点)、δ-3 で 2.2 (`MAKEWORD(2, 2)`) へアップグレード予定 ([CLAUDE.md フェーズ δ-3](CLAUDE.md) 既述)
- **同一マシン内ループバック前提** (`"localhost"` ハードコード、リモート IP 入力 UI は本仕様外)
- **暗号化なし** (LAN 内対戦想定、TLS 等は不要)
- **NAT 越え非対応** (UPnP 等は本仕様外)

---

## 13. 関連ドキュメント

- [CLAUDE.md](CLAUDE.md) — プロジェクト全体方針、フェーズ δ-3 概要
- [Project2/Game2Scene.cpp](Project2/Game2Scene.cpp) — γ-3 時点の実装
- [Project2/GameSceneMain.h](Project2/GameSceneMain.h) — TIMER_STATE 構造体 (スコア計算共有)