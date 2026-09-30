<!-- 出典: REVIEW_CHECKLIST.md (マージ前レビュー確定修正パターン集) のカテゴリ別分割。
     索引と使い方は INDEX.md 参照。追記時は INDEX.md にも規則を一行追加する。 -->

## 5. docstring

このカテゴリに直接分類されたレビュー finding はない。以下は主としてプロジェクト規約 (CLAUDE.md §4〜§5) と現行コードの確立済みパターンから導出した規準である (各項目の出典に明記)。なお本プロジェクトの docstring は Google スタイルの Args/Returns/Raises セクションを**使わない** (リポジトリ全体で該当セクションは 0 件)。型ヒント + mypy strict がシグネチャ情報を担保するため、docstring は「型に現れない契約」を散文で書くスタイルで統一されている。

### モジュール docstring に層の依存禁止事項を明記せよ

- **規則**: 層アーキテクチャを持つプロジェクトでは、各モジュールの docstring 末尾に「この層が import してはならないもの」を宣言する。機械的強制 (リント規則・依存検査テスト) があっても、docstring の宣言は新規モジュール作成時のコピー元・レビュー時の照合点として必須とする。
- **出典**: 規約由来 (CLAUDE.md §4「各モジュールの docstring にも同ルールを明記済み。新規モジュール追加時も踏襲すること」、レビュー実績なし)

❌ **初回実装にありがちなパターン**: モジュールの目的だけを書き、依存境界の宣言を省く (機械的強制があるからと docstring 側を空にする)。新規モジュール追加時に宣言の踏襲が漏れ、レビュー時に「このモジュールは Qt を import してよい層か」を docstring から判定できなくなる。

✅ **レビュー後の修正パターン** (現行コード `apk_explorer/core/container.py`):

```python
"""ファイル種別の判定と capability 集合の決定 (CLAUDE.md §5-12)。

apk_parser が「.apk 固有の解析」を担うのに対し、本モジュールは
「どの種別か・どの解析観点が有効か」だけを決める小さな入口。
この層は Qt (PySide6) を import しない。
"""
```

禁止対象が複数ある層では理由も添える (現行コード `apk_explorer/core/xapk_extractor.py`):

```python
この層は Qt (PySide6) を import しない。androguard も import しない
(選定・抽出は ZIP 構造のみを扱う純粋な境界処理のため)。
```

### 関数 docstring には型シグネチャの反復ではなく「型に現れない契約」を書け

- **規則**: 引数名と型を言い換えただけの Args/Returns 羅列は書かず、シグネチャから読み取れない契約 — 例外の伝播/翻訳方針、失敗時の後始末の責務分担、キャンセル・再検証のセマンティクス — を散文で記述する。特に「この関数がやらないこと (呼び出し側の責務)」は、省くと呼び出し側の実装者が誤った前提を置く。
- **出典**: 規約由来 (現行コードの確立パターン、レビュー実績なし)

❌ **初回実装にありがちなパターン**: `"""エントリを抽出する。 Args: archive_path: アーカイブのパス。 Returns: 書き込みバイト数。"""` のような型ヒントの複製。例外がそのまま伝播するのか翻訳されるのか、失敗時に書きかけファイルを誰が消すのかが読み取れない。

✅ **レビュー後の修正パターン** (現行コード `apk_explorer/core/entry_extractor.py`):

```python
def extract_entry(
    archive_path: Path,
    entry_index: int,
    expected_name: str,
    dest: Path,
    *,
    max_size: int,
    overwrite: bool = False,
    cancel: threading.Event | None = None,
) -> int:
    """1 エントリを指定パスへ上限付きストリーミングで抽出し、書き込みバイト数を返す。

    単体保存・一括展開の両方から呼ばれる下位関数。TOCTOU 対策 (規約 8-e) として、
    抽出時に再読込した infolist() の該当位置が expected_name と一致しない場合は
    CorruptArchiveError とする。宣言サイズは信用せず、実書き込みバイト数のみで
    max_size を判定する (規約 8-d)。

    overwrite=False (既定) では "xb" (排他的新規作成) で書き込み、既存ファイルがあれば
    FileExistsError をそのまま呼び出し元へ伝播させる (execute_plan が COLLIDED として捕捉する)。
    失敗時の書きかけファイルの削除はこの関数の責務ではない (呼び出し側が行う。
    xapk_extractor.extract_inner_file と同じ契約)。
    """
```

### docstring に不変条件を書いたら、強制箇所を docstring 側にも明記せよ

- **規則**: データモデルの不変条件を docstring やコメントに記述するだけでは強制力がなく、レビューで「記述のみ・未強制」と指摘される (強制の実装方法はエラー処理のカテゴリを参照)。docstring 側の規準としては、不変条件の記述に「どこで機械的に強制されるか」への参照を必ず添え、記述と強制が対になっていることを読み手が確認できるようにする。
- **出典**: APKExplorer M6 レビュー (レビュー修正 commit 6b05679、`apk_explorer/models/apk_data.py` — AnalysisBundle の不変条件が docstring 記述のみで未強制。`__post_init__` 追加 + docstring への「__post_init__ で強制する (A-6)」追記で修正) / M7 レビュー B-4 (レビュー修正 commit e1b16e5、同ファイル — AnalysisTarget.entry_index の「PACKAGE 以外は必ず設定する」規約がフィールド直前コメントの記述のみで未強制。`__post_init__` へ「(M7 レビュー B-4)」付きの強制を追加)

❌ **初回実装にありがちなパターン**: `"""targets[0] は必ず PACKAGE でなければならない。"""` と docstring に書いて終わり。違反インスタンスは構築できてしまい、docstring を読まない呼び出し側で静かに壊れる。

✅ **レビュー後の修正パターン** (現行コード `apk_explorer/models/apk_data.py` — docstring が強制箇所と指摘番号を名指しする):

```python
@dataclass(frozen=True, slots=True)
class AnalysisBundle:
    """1 回の「開く」操作から得られる解析対象の束 (M6: .xapk 対応)。

    不変条件: targets[0] は必ず kind=PACKAGE かつ result 非 None (外側コンテナ自体の
    解析は全体失敗として run_bundle_analysis から伝播するため、束の要素にはならない)。
    単体 .apk / .obb では targets は PACKAGE の 1 件のみ。__post_init__ で強制する (A-6)。
    """

    container: ContainerInfo
    targets: tuple[AnalysisTarget, ...]

    def __post_init__(self) -> None:
        if not self.targets:
            raise ValueError("AnalysisBundle.targets は空にできません")
        # ...
```

### センチネル既定値・危険なフィールドの意味は定義位置で宣言せよ

- **規則**: 「空文字列 = 別の値を使う規約」「0 = 互換のための既定」のようなセンチネル既定値は、意味を dataclass の docstring またはフィールド直前コメントで宣言する。攻撃者が制御しうるフィールドには、安全に使うための前提条件 (検証フラグの確認義務等) を定義位置に書く — 利用側のコードコメントに分散させない。
- **出典**: 規約由来 (現行コードの確立パターン、レビュー実績なし。関連: M4 レビューの「`or 0` による欠落値と実値ゼロの混同」は別カテゴリで扱う)

❌ **初回実装にありがちなパターン**: `display_name: str = ""` とだけ書き、空文字列が「未設定」なのか「表示名なし」なのか「別ソースへのフォールバック指示」なのかを利用側の実装者が推測する。

✅ **レビュー後の修正パターン** (現行コード `apk_explorer/models/apk_data.py`):

```python
@dataclass(frozen=True, slots=True)
class ContainerInfo:
    """開いたファイルの種別と、その種別で有効な解析観点の集合。

    display_name: 表示用の名前。既定 "" は「path.name を表示名とする」規約を表す。
    # ...
    """

    path: Path
    container_type: ContainerType
    capabilities: frozenset[AnalysisAspect]
    # ディスク上のファイルサイズ (バイト)。既定 0 はテスト用ファクトリの互換のため
    file_size: int = 0
    display_name: str = ""
```

危険フィールドの前提条件宣言の例 (同ファイル `ArchiveEntry`):

```python
    name は ZIP 内の生のエントリ名。悪意ある名前でありうるため、
    パスとして解釈・抽出する前に is_suspicious を必ず確認すること (CLAUDE.md §5 セキュリティ規約)。
```

### 意図的なコード重複・設計逸脱には理由を docstring に残せ

- **規則**: 類似モジュールとの意図的な独立実装 (DRY の意図的違反) や、一般的なベストプラクティスからの意図的な逸脱は、理由と参照先をモジュール docstring に明記する。書かなければ、後続の実装エージェントやリファクタリングが「重複の統合」として善意で破壊する。
- **出典**: 規約由来 (CLAUDE.md §10 M7「xapk_extractor とは意図的に独立実装、コード重複許容」、レビュー実績なし)

❌ **初回実装にありがちなパターン**: 既存モジュールと骨格が類似した新モジュールを無言で追加する。次の改修者には「統合し忘れた重複」に見え、安定コード側を巻き込むリファクタリングを誘発する。

✅ **レビュー後の修正パターン** (現行コード `apk_explorer/core/entry_extractor.py` モジュール docstring):

```python
`core/xapk_extractor.py` (M6、.xapk 内包対象の抽出) とは意図的に**独立実装**である
(xapk_extractor は M6 の安定コードで広範なテストに守られており、本モジュールの追加で
変更するリスクを避けるため。ストリーミング抽出・TOCTOU 再検証・例外翻訳の骨格は類似するが
コード共有はしない)。
```

---
