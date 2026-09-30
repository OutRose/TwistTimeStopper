<!-- 出典: REVIEW_CHECKLIST.md (マージ前レビュー確定修正パターン集) のカテゴリ別分割。
     索引と使い方は INDEX.md 参照。追記時は INDEX.md にも規則を一行追加する。 -->

## 1. ネーミング

> このカテゴリに分類されたレビュー finding は存在しない (M1〜M7 のレビューで命名起因の確定指摘は 0 件)。以下はすべて APKExplorer の規約 (CLAUDE.md §5-3 / §6 / §7) と現行コードから導出した規準であり、レビュー実績由来ではない。✅ スニペットは現行コードの実物、❌ スニペットは (例示) である。

### 定数は SCREAMING_SNAKE_CASE + `Final` で宣言し、公開範囲を先頭 `_` で区別する

- **規則**: モジュールレベルの定数は大文字スネークケース + `Final` 注釈で「再代入しない値」であることを型検査器にも読者にも宣言する。モジュール外へ見せない定数は先頭 `_` を付け、単位や由来は名前かコメントで明示する (マジックナンバーをロジック中に直置きしない)。
- **出典**: 規約由来 (レビュー実績なし) — CLAUDE.md §6 (定数規約)、現行コード `apk_explorer/core/archive_structure.py` / `apk_explorer/core/entry_extractor.py` / `apk_explorer/core/container.py`

❌ **初回実装にありがちなパターン** (例示):

```python
# 小文字の「定数のつもり」変数 + ロジック中のマジックナンバー
max_entries = 100000          # 再代入可能に見える。Final なしは mypy も守ってくれない
chunk = 1048576               # 単位が読めない

while data := stream.read(1048576):  # 上と同じ値の重複直書き
    ...
```

✅ **規約準拠の現行パターン** (現行コード実物):

```python
from typing import Final

# archive_structure.py — 公開定数 (呼び出し側が既定値として参照する)
DEFAULT_MAX_ENTRIES: Final = 100_000
DEFAULT_MAX_TOTAL_SIZE: Final = 10 * 1024**3  # 10 GiB

# entry_extractor.py — モジュール内専用は先頭 _
_CHUNK_SIZE: Final = 1024 * 1024  # 1 MiB

# container.py — コレクション定数は Final[...] で要素型も固定する
_TYPE_BY_SUFFIX: Final[dict[str, ContainerType]] = {
    # ...
}
```

### PySide6 の API 名 (`Signal` / `Slot` / `Property`) を使い、PyQt 系の名前を持ち込まない

- **規則**: 同一機能に複数の呼称を持つバインディング系ライブラリでは、採用したバインディングの正式 API 名に統一する。Web 上のサンプルコードは別バインディング (PyQt) 由来のことが多く、コピペすると `pyqtSignal` 等の別名が混入して動かない・依存が混ざる。
- **出典**: 規約由来 (レビュー実績なし) — CLAUDE.md §5-3 (「PySide6 の API 名は `Signal` / `Slot` / `Property`。`pyqtSignal` 等の PyQt 系 API 名は使わない。Web 上のサンプルコードからのコピペに注意」)、現行コード `apk_explorer/gui/app_state.py`

❌ **初回実装にありがちなパターン** (例示):

```python
# Web 検索結果 (PyQt サンプル) のコピペ — 本プロジェクトでは import からして失敗する
from PyQt6.QtCore import pyqtSignal, pyqtSlot

class TaskSignals(QObject):
    finished = pyqtSignal(int, object)
```

✅ **規約準拠の現行パターン** (現行コード実物):

```python
from PySide6.QtCore import QObject, QRunnable, QThreadPool, Signal, SignalInstance

class _TaskSignals(QObject):
    # ...
    finished = Signal(int, object)  # (世代番号, AnalysisBundle)
    failed = Signal(int, str)  # (世代番号, ユーザー向け日本語メッセージ)
    progress = Signal(int, str)  # (世代番号, 進捗メッセージ)
```

### camelCase は Qt オーバーライドに限定し、GUI 層の外へ持ち出さない (逆に snake_case へ「直さない」)

- **規則**: フレームワークが名前で結合する仮想メソッドのオーバーライド (Qt の `closeEvent` 等) は、命名規約に反していてもフレームワーク側の綴りを維持する — snake_case に「修正」すると別メソッドになり、呼ばれないまま無言で無効化される。逆にフレームワーク境界の外 (コアロジック層) では camelCase を一切書かず、lint の除外設定も境界のディレクトリ単位に限定する。
- **出典**: 規約由来 (レビュー実績なし) — CLAUDE.md §6 例外 (「gui 以外では camelCase を書かないこと」)、現行コード `apk_explorer/gui/main_window.py` / `pyproject.toml`

❌ **初回実装にありがちなパターン** (例示):

```python
# PEP 8 に「準拠」させようとして Qt オーバーライドを改名 — オーバーライドが成立せず、
# 終了時のクリーンアップが一度も呼ばれない (例外も lint エラーも出ない)
def close_event(self, event: QCloseEvent) -> None:
    self.app_state.shutdown()
```

✅ **規約準拠の現行パターン** (現行コード実物):

```python
# main_window.py — Qt の綴りを維持する (gui 配下のみ N802 除外済み)
def closeEvent(self, event: QCloseEvent) -> None:
    # 実行中の解析を止め、破棄済みオブジェクトへのシグナル配送を防ぐ
    self.app_state.shutdown()
```

```toml
# pyproject.toml — 除外は gui ディレクトリに限定し、core/models には及ぼさない
[tool.ruff.lint.per-file-ignores]
# gui 層: Qt の camelCase オーバーライド (N802) と PySide6 import (TID253) を許容
"apk_explorer/gui/**" = ["N802", "TID253"]
```

### シグナル名は snake_case の事象形、ハンドラは `_on_<シグナル名>` で統一する

- **規則**: イベント (シグナル) は「何が起きたか」を表す snake_case の事象形 (`*_started` / `*_finished` / `*_requested` 等) で命名し、その購読ハンドラは `_on_<シグナル名>` の機械的な対応で命名する。発火側と購読側の名前が一対一で辿れると、配線ミス (別スコープのシグナルへの誤接続) がレビューで名前だけから検出できる。
- **出典**: 規約由来 (レビュー実績なし) — CLAUDE.md §5-3 (イベントドリブン設計) / §6 (関数 snake_case・プライベート先頭 `_`)、現行コード `apk_explorer/gui/app_state.py` / `apk_explorer/gui/main_window.py`

❌ **初回実装にありがちなパターン** (例示):

```python
# 命名スタイルが混在し、シグナルとハンドラの対応が名前から辿れない
analysisDone = Signal(object)      # camelCase シグナル (Qt の C++ 風に引きずられる)
sig_fail = Signal(str)             # 省略形 + 事象形でない

def handleResult(self, bundle): ...  # どのシグナル購読か名前から不明
def on_error(self, msg): ...         # 接頭 _ なし (公開 API に見える)
```

✅ **規約準拠の現行パターン** (現行コード実物):

```python
# app_state.py — シグナルは snake_case の事象形
analysis_started = Signal(str)  # 解析対象のパス文字列
analysis_finished = Signal(object)  # AnalysisBundle
analysis_failed = Signal(str)  # ユーザー向けエラーメッセージ (日本語)
extraction_progress = Signal(int, int, str)  # (完了バイト数, 計画合計バイト数, 表示名)

# main_window.py — 購読ハンドラは _on_<シグナル名> の一対一対応
def _on_analysis_started(self, path_text: str) -> None: ...
def _on_analysis_finished(self, bundle: object) -> None: ...
def _on_analysis_failed(self, message: str) -> None: ...
```

---
