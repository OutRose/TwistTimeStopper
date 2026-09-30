<!-- 出典: REVIEW_CHECKLIST.md (マージ前レビュー確定修正パターン集) のカテゴリ別分割。
     索引と使い方は INDEX.md 参照。追記時は INDEX.md にも規則を一行追加する。 -->

## 6. 依存方向

対象範囲: 層間依存の一方向規約 (表示層 → ロジック層 → データモデル層)、循環参照の予防、宣言なき依存 (推移的依存の直接 import) の禁止、依存規約の機械的強制。

### 下位層 (ロジック/モデル) から GUI フレームワークを import せず、境界はコールバックと標準型で設計する

- **規則**: 層間依存は「表示層 → ロジック層 → データモデル層」の一方向のみとし、データモデルはどの層も知らず、ロジック層は GUI フレームワークを import しない。進捗通知・キャンセルのような「ロジックから表示への逆向き情報」はフレームワーク型ではなく素のコールバックと標準ライブラリのプリミティブ (`threading.Event` 等) で受け、表示層側のワーカーがフレームワークのイベント (Signal 等) へ変換する。
- **出典**: 規約由来 (レビュー実績なし)。APKExplorer CLAUDE.md §4 (依存方向ルール) / §5-10 (並行処理モデル)

❌ **初回実装にありがちなパターン** (例示):

```python
# (例示) core が進捗通知のために Qt へ依存してしまう — 逆方向依存で
# 「ロジックは GUI なしでテスト・CLI 利用できる」性質が壊れる
from PySide6.QtCore import QObject, Signal

class AnalysisProgress(QObject):
    message = Signal(str)

def run_analysis(path: Path, progress: AnalysisProgress) -> AnalysisResult:
    # ...
    progress.message.emit("解析中...")
```

✅ **規約準拠パターン** (現行実装):

```python
# apk_explorer/core/analysis.py — core は Qt 非依存のコールバック + threading.Event
ProgressCallback = Callable[[str], None]

def run_analysis(
    path: Path,
    *,
    progress: ProgressCallback | None = None,
    cancel: threading.Event | None = None,
) -> AnalysisResult:
```

```python
# apk_explorer/gui/app_state.py (_AnalysisTask.run) — gui 側ワーカーが Signal へ変換
result = self._analyze(
    self._path,
    progress=lambda message: self._emit(self._signals.progress, generation, message),
    cancel=self._cancel,
)
```

補足: 表示層がデータモデル層を直接参照するのは正当な方向 (gui → models)。「必ずロジック層を経由」と過剰に狭めない。禁止は逆方向 (models が core/gui を知る、core が gui を知る) のみ。

### 依存方向ルールを lint + サブプロセステストの二段構えで機械的に強制する

- **規則**: 層間依存規約は文書に書くだけでは守られない。(1) lint の import 禁止設定でエディタ上で即検出し、(2) 「下位層のモジュールを import しても GUI フレームワークがロードされない」ことを検証する自動テストを併設する二段構えにする。テストランナー自身 (pytest-qt 等) がフレームワークを先に import する環境では同一プロセスの `sys.modules` 検査は成立しないため、サブプロセスへ分離して検査する。
- **出典**: APKExplorer 初回セッションレビュー (commit f93ce50、tests/test_sanity.py — 確定指摘「依存方向ルールの自動検証テスト追加」) + 改善レビュー A 群 (commit 65eebe7、pyproject.toml — ruff TID253 による lint 層強制の追加)

❌ **初回実装にありがちなパターン** (例示):

```python
# (例示) 同一プロセスでの検査 — pytest-qt が PySide6 を先に import 済みのため
# core が規約を守っていても常に失敗し、検査を諦めて docstring 記述だけに後退しがち
def test_core_does_not_import_qt() -> None:
    import apk_explorer.core.analysis
    assert "PySide6" not in sys.modules
```

✅ **レビュー後の修正パターン** (現行実装):

```toml
# pyproject.toml — lint 層での強制 (エディタ上で即検出)
[tool.ruff.lint.flake8-tidy-imports]
banned-module-level-imports = ["PySide6"]

[tool.ruff.lint.per-file-ignores]
"apk_explorer/gui/**" = ["N802", "TID253"]  # GUI 層のみ import を許可
"tests/gui/**" = ["TID253"]
```

```python
# tests/test_sanity.py — テスト層での強制 (サブプロセスで sys.modules を検査)
def test_core_and_models_do_not_import_qt() -> None:
    core_and_models = [m for m in ALL_MODULES if ".core" in m or ".models" in m]
    code = (
        f"import sys, importlib; [importlib.import_module(m) for m in {core_and_models!r}]; "
        "sys.exit(1 if 'PySide6' in sys.modules else 0)"
    )
    result = subprocess.run([sys.executable, "-c", code], timeout=30, check=False)
    assert result.returncode == 0
```

補足: このテストの検査対象は明示リスト (`ALL_MODULES`) から導出される。**新規モジュールをリストへ登録し忘れると依存検査の対象外のまま素通りする**ため、モジュール追加とリスト登録をセットで行うこと (CLAUDE.md §8)。

### 推移的依存 (依存の依存) を自コードから直接 import しない

- **規則**: 依存宣言 (pyproject / packages.config 等) に直接依存として書いていないライブラリは、依存ツリー経由でインストール済みでも自コードから import しない — 宣言なき依存は上流ライブラリの依存整理で予告なく消える。サードパーティの内部機構 (ログ設定等) に触れる必要がある場合も、そのライブラリが公式に公開している API を経由する。
- **出典**: 規約由来 (レビュー実績なし)。APKExplorer CLAUDE.md §5-9 (ログ規約、M2 で実装)

❌ **初回実装にありがちなパターン** (例示):

```python
# (例示) loguru は androguard の推移的依存 — 直接 import すると宣言なき依存になり、
# androguard がログ実装を替えた時点で自コードが壊れる
from loguru import logger

logger.remove()  # androguard のログを直接止めにいく
```

✅ **規約準拠パターン** (現行実装、apk_explorer/log_config.py):

```python
"""ログ設定の一元化 (CLAUDE.md §5-9)。

自コードは stdlib logging で統一する (loguru は androguard の推移的依存であり、
直接 import すると宣言なき依存になるため使わない)。androguard のログ抑制は
同ライブラリ公式の androguard.util.set_log を経由して行う。
"""
def configure_logging() -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )
    # androguard は import が重いため、必要になるこの時点で読み込む
    from androguard.util import set_log

    # ValueError = 既に抑制済み (loguru の既定ハンドラは 1 度しか削除できない)
    with contextlib.suppress(ValueError):
        set_log("ERROR")
```

### 型なし外部ライブラリは境界モジュール 1 箇所に封じ込め、Any を上位層へ漏らさない

- **規則**: 型情報を持たない外部ライブラリの import と生オブジェクト操作は**境界モジュール 1 箇所に限定**する。Any の生オブジェクトが上位層へ漏れると、上位層がライブラリの API 仕様へ暗黙に依存し (宣言なき依存の変種)、静的型検査の網からも外れ、ライブラリ差し替え時の影響が全層へ拡散する。本項の焦点は「import と操作をどこに置くか」であり、変換ヘルパーの書き方 (Any → 型付き不変モデル) の実例は「型ヒント」セクションの同名の項を参照。
- **出典**: 規約由来 (レビュー実績なし)。APKExplorer CLAUDE.md §5-6 (androguard 境界規約)

✅ **規約準拠パターン** — 境界モジュールの docstring で封じ込めを宣言する (現行実装、apk_explorer/core/apk_parser.py):

```python
"""androguard をラップする .apk 解析の境界層。

androguard の API は型情報を持たない (Any) ため、この層で models.apk_data の
不変モデル (ParsedApk) へ変換してから外へ返す (CLAUDE.md §5-6)。
"""
```

---
