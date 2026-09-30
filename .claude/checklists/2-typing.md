<!-- 出典: REVIEW_CHECKLIST.md (マージ前レビュー確定修正パターン集) のカテゴリ別分割。
     索引と使い方は INDEX.md 参照。追記時は INDEX.md にも規則を一行追加する。 -->

## 2. 型ヒント

対象範囲: mypy strict を通す最低ライン、外部ライブラリ境界での Any 排除、Optional/Union の扱い、`Signal(object)` 等の「型検査に不可視なペイロード」への防御。本カテゴリのレビュー実績 finding は 1 件 (M7 A-4) で、残りはプロジェクト規約 (CLAUDE.md §5-6 / §5-10 / §7) と現行コードから導出した規準である (各項目の出典に明記)。

### 操作の由来など意味的な区別は、データ形状から推測せず判別フィールドとして型に刻印する

- **規則**: 「この結果はどの操作に由来するか」のような意味情報を、結果データの形状 (件数・null か否か・コレクションの空/非空) から推測で復元しない。生成元が明示的に刻印する判別フィールド (enum 等) を結果モデルに持たせ、消費側はそれを唯一の情報源として分岐する。
- **出典**: APKExplorer M7 レビュー A-4 (commit e1b16e5、apk_explorer/models/apk_data.py / apk_explorer/gui/main_window.py)

❌ **初回実装にありがちなパターン** — 件数 1 = 単体保存と推測する (非ディレクトリエントリが 1 件しかないアーカイブの一括展開でも同形になり誤分類):

```python
        if len(report.items) == 1:
            self._show_single_extraction_feedback(report.items[0])
        else:
            self._show_batch_extraction_summary(report)
```

✅ **レビュー後の修正パターン** — 判別 enum を結果モデルに新設し、生成元 (単体保存/一括展開の各実行関数) が刻印する:

```python
@unique
class ExtractionOrigin(Enum):
    SINGLE = "single"
    BATCH = "batch"


@dataclass(frozen=True, slots=True)
class ExtractionReport:
    root: Path
    origin: ExtractionOrigin
    items: tuple[ExtractionItem, ...]
    # ...
```

```python
        # 単体保存/一括展開の判別は report.origin を唯一の情報源とする (M7 レビュー A-4)。
        # items の件数からの推測は、対象アーカイブの非ディレクトリエントリが 1 件しかない
        # 一括展開でも同形になり誤分類する
        if report.origin is ExtractionOrigin.SINGLE:
            self._show_single_extraction_feedback(report.items[0])
        else:
            self._show_batch_extraction_summary(report)
```

あわせて「1 件のみのバッチでも概要表示が出る」ことを固定するテストを追加する (推測ロジックへの退行防止)。

### 型情報を持たない外部ライブラリの戻り値は、境界層で自前の型付き不変モデルへ変換してから返す

- **規則**: 型スタブのない外部ライブラリ (戻り値が Any/object/dynamic になるもの) は、専用の境界モジュール 1 箇所でラップし、自前の型付き不変モデルへ変換してから上位層へ返す。動的型の値やライブラリの生オブジェクトを境界より外 (UI 層・他モジュール) へ漏らさない。(依存関係の観点からの同じ規則は「依存方向」セクションの封じ込めの項を参照)
- **出典**: 規約由来 (レビュー実績なし) — CLAUDE.md §5-6 / §7、現行 apk_explorer/core/apk_parser.py

❌ **初回実装にありがちなパターン** (例示) — 戻り値注釈を省略してライブラリの生オブジェクトを上位層へ運ぶ:

```python
def parse_apk(path):  # 戻り値注釈なし → 呼び出し側すべてが Any になる
    apk = APK(str(path))
    return apk  # ライブラリの生オブジェクトを GUI 層まで運ぶ (例示)
```

✅ **規約に沿ったパターン** — Any を受けるのは境界の内側だけにし、外へは frozen dataclass を返す:

```python
def parse_apk(
    path: Path,
    *,
    max_file_size: int = DEFAULT_MAX_FILE_SIZE,
    max_manifest_size: int = DEFAULT_MAX_MANIFEST_SIZE,
) -> ParsedApk:
    """.apk を androguard で解析し、型付き不変モデルへ変換して返す。"""
    # ...

def _to_parsed_apk(apk: Any, axml: Any) -> ParsedApk:
    """androguard の Any な API 群から型付きの生事実を写し取る。"""
    # ...
    return ParsedApk(
        package=_display_str(apk.get_package()),
        app_name=_display_str(apk.get_app_name()),
        # ...
        requested_permissions=_str_tuple(apk.get_permissions()),
        manifest_xml=_sanitize_text(_decode_xml(axml.get_xml())),
    )
```

### 欠落 (None) の扱いはフィールドごとに型で確定し、`str(value)` や暗黙変換で潰さない

- **規則**: 外部由来の「欠落しうる値」は、境界でフィールドごとに「欠落を既定値へ正規化してよい (非 Optional 型)」か「欠落自体が情報 (Optional 型のまま保持)」かを決め、明示的な None 判定で変換する。None をそのまま文字列化すると "None" という偽の実値が表示に漏れる。
- **出典**: 規約由来 (レビュー実績なし) — CLAUDE.md §5-6 (M2 レビューの「欠落属性の "None" 表示」修正で確立)、現行 apk_explorer/core/apk_parser.py

❌ **初回実装にありがちなパターン** (例示):

```python
min_sdk = str(apk.get_min_sdk_version())  # None → 文字列 "None" が GUI に出る (例示)
```

✅ **規約に沿ったパターン** — 正規化ヘルパーを 2 系統に分け、モデルの型 (`str` / `str | None`) と対応させる:

```python
def _display_str(value: Any) -> str:
    """欠落 (None) は空文字へ正規化する ("None" という文字列を GUI に出さない)。"""
    return "" if value is None else _sanitize_text(str(value))


def _optional_display_str(value: Any) -> str | None:
    return None if value is None else _sanitize_text(str(value))
```

```python
        package=_display_str(apk.get_package()),
        # ...
        min_sdk=_optional_display_str(apk.get_min_sdk_version()),
```

### 型検査に不可視なペイロード (`Signal(object)` 等) は、受信時の isinstance ガード + 型付きアクセサで防御する

- **規則**: イベント/シグナル機構のペイロードが型消去される場合 (Qt の `Signal(object)`、object 型のイベント引数等)、静的検査はペイロード型の変更を検出できない。受信側は (1) 引数を object として受けて isinstance で検証し、不一致は黙って破棄する、(2) 状態の読み出しはシグナル引数ではなく型注釈付きのアクセサ (プロパティ) を情報源とする、の二重防御を敷く。
- **出典**: 規約由来 (レビュー実績なし) — CLAUDE.md §5-10 (M6 で `analysis_finished` のペイロードが AnalysisResult → AnalysisBundle へ変更された際に確立)、現行 apk_explorer/gui/app_state.py / apk_explorer/gui/main_window.py

❌ **初回実装にありがちなパターン** (例示) — スロット引数に「そうであるはず」の型を注釈して信じる:

```python
    def _on_task_finished(self, generation: int, result: AnalysisBundle) -> None:
        self._bundle = result  # 実ペイロードが何であれ mypy は素通しする (例示)
```

✅ **規約に沿ったパターン** — object で受けて isinstance ガード、読み出しは型付きプロパティ経由:

```python
    analysis_finished = Signal(object)  # AnalysisBundle

    @property
    def current_bundle(self) -> AnalysisBundle | None:
        """直近に完了した解析の束 (未解析または失敗中は None)。"""
        return self._bundle

    def _on_task_finished(self, generation: int, result: object) -> None:
        if generation != self._generation or not isinstance(result, AnalysisBundle):
            return
        self._bundle = result
        # ...
```

```python
    def _on_extraction_finished(self, report: object) -> None:
        if not isinstance(report, ExtractionReport):
            return
```

### テスト注入可能な呼び出し依存は `Callable[..., X]` ではなく Protocol で型付けする

- **規則**: 差し替え可能な関数依存 (DI) のシグネチャにキーワード専用引数や省略可能引数が含まれる場合、`Callable[..., 戻り値]` の `...` は引数検査を全放棄する。`__call__` を持つ Protocol (C# なら delegate、C++ ならコンセプト相当) で実物と同じシグネチャを宣言し、注入されるテストダブルにも同じ検査を効かせる。
- **出典**: 規約由来 (レビュー実績なし) — 現行 apk_explorer/gui/app_state.py (Analyzer / SingleExtractor / BatchExtractor)

❌ **初回実装にありがちなパターン** (例示):

```python
analyze: Callable[..., AnalysisBundle]  # "..." がキーワード引数の検査をすべて消す (例示)
```

✅ **規約に沿ったパターン** — キーワード専用引数まで含めて Protocol で宣言:

```python
class Analyzer(Protocol):
    """解析入口の型 (テストで差し替え可能にするための Protocol)。"""

    def __call__(
        self,
        path: Path,
        *,
        progress: ProgressCallback | None = None,
        cancel: threading.Event | None = None,
    ) -> AnalysisBundle: ...
```

### 型検査の抑止は「エラーコード付き・局所・最小限」に限定する

- **規則**: strict 検査を通すための抑止 (`# type: ignore` 等) は、必ず対象エラーコードを明示して 1 行単位で行う。コードなしの一括抑止はエラーコードの取り違え・別エラーの巻き添え隠蔽を招く。テストコード・スクリプトも検査対象に含め、「テストだから型を書かない」逃げ道を作らない。
- **出典**: 規約由来 (レビュー実績なし) — CLAUDE.md §7 (`ignore-without-code` 有効化)、現行 pyproject.toml / tests/models/test_apk_data.py

❌ **初回実装にありがちなパターン** (例示):

```python
        ArchiveEntry(name="a.txt", size=1)  # type: ignore  ← コードなし抑止 (例示)
```

✅ **規約に沿ったパターン** — 設定でコードなし抑止をエラー化し、抑止は意図したエラーコードだけを対象にする:

```toml
[tool.mypy]
strict = true
files = ["apk_explorer", "tests", "scripts"]
# ...
enable_error_code = [
    "ignore-without-code",
    # ...
]
```

```python
    with pytest.raises(TypeError):
        ArchiveEntry(  # type: ignore[call-arg]
            name="a.txt", size=1, compressed_size=1, is_dir=False, is_suspicious=False
        )
```

---
