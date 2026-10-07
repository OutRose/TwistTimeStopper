<!-- 出典: REVIEW_CHECKLIST.md (マージ前レビュー確定修正パターン集) のカテゴリ別分割。
     索引と使い方は INDEX.md 参照。追記時は INDEX.md にも規則を一行追加する。 -->

## 8. その他

対象範囲: パフォーマンス配慮、副作用管理 (アトミック書き込み・世代番号による競合防御等)、可読性、コメント量の適正化、UI 表示の正確性。

### 拡張子・ファイル名の判定は標準 API の慣習 (basename 単位・先頭ドットは拡張子でない) に揃えよ

- **規則**: 拡張子判定は各言語の標準 API (Python の `os.path.splitext` 等) と同じ慣習に揃える — basename を先に切り出してから判定し、先頭ドットの隠しファイル (stem が空) は「拡張子なし」扱いにする。テストは「たまたま対応表に無い名前だから通る」ケースではなく、既知拡張子を持つ隠しファイル (`.png` 等) のように誤分類が実際に起きる反例で検証する (偶然通過する偽の緑を作らない)。
- **出典**: APKExplorer M3 レビュー (commit 6f22ba9、apk_explorer/core/asset_extractor.py)

❌ **初回実装にありがちなパターン** — フルパス全体を rpartition し、stem 空 (隠しファイル) の検査を落とす:

```python
def categorize_asset(name: str) -> AssetCategory:
    """エントリ名の拡張子からカテゴリを決める純関数。大文字小文字は区別しない。"""
    _, separator, extension = name.rpartition(".")
    if not separator or not extension:
        # ドットなし (rpartition は末尾に名前全体を返す) と "file." は拡張子なし扱い
        return AssetCategory.OTHER
    return _CATEGORY_BY_EXTENSION.get(extension.lower(), AssetCategory.OTHER)
```

✅ **レビュー後の修正パターン** — basename → stem/拡張子の順に分解し、反例をテストに固定:

```python
    basename = name.rpartition("/")[2]
    stem, separator, extension = basename.rpartition(".")
    if not stem or not separator or not extension:
        # stem が空 (隠しファイル)、ドットなし (rpartition は末尾に名前全体を返す)、
        # "file." (末尾ドット) はいずれも拡張子なし扱い
        return AssetCategory.OTHER
    return _CATEGORY_BY_EXTENSION.get(extension.lower(), AssetCategory.OTHER)

# テスト側 (抜粋):
        (".png", AssetCategory.OTHER),  # 先頭ドットの隠しファイル (拡張子ではなく stem 扱い)
        ("dir/sub/photo.png", AssetCategory.IMAGE),  # basename ベースの判定
```

### フィルタで除外したデータは無言で捨てず、除外件数を結果モデルに保持して UI で明示せよ

- **規則**: セキュリティ等の理由でデータを一覧から除外するときは、除外の事実を結果モデル (件数フィールド等) に記録し、UI で件数を明示して詳細を確認できる場所へ誘導する。無言の除外は「0 件 = 存在しない」という誤誘導を生む — 除外がユーザー可視の集計値 (件数・合計サイズ) を静かに変えることを見落とさない。
- **出典**: APKExplorer M3 レビュー (commit 6f22ba9、apk_explorer/core/asset_extractor.py)

❌ **初回実装にありがちなパターン** — 除外の事実をどこにも記録せず捨てる:

```python
    for entry in structure.entries:
        if entry.is_dir or entry.is_suspicious:
            continue
        # ...
    return AssetInventory(entries=tuple(entries), total_size=total_size)

# asset_view.py:
        self.summary_label.setText(
            f"アセット: {len(result.assets.entries)} 件 "
            f"(合計 {_format_size(result.assets.total_size)})"
        )
```

✅ **レビュー後の修正パターン** — 除外件数をモデルに保持し、サマリで明示:

```python
        if entry.is_suspicious:
            suspicious_count += 1
            continue
        # ...
    return AssetInventory(
        entries=tuple(entries), total_size=total_size, suspicious_count=suspicious_count
    )

# asset_view.py:
        if result.assets.suspicious_count > 0:
            # suspicious 除外は無言で消さず、件数を明示する (誤誘導防止。詳細は内部構造タブ)
            summary += (
                f"、⚠ 危険な名前 {result.assets.suspicious_count} 件は除外 (内部構造タブ参照)"
            )
```

### 表示する数値に嘘をつかせるな — 丸め境界は繰り上げ、最終単位の飽和領域まで境界を張り、欠落値は欠落マーカーで示せ

- **規則**: 単位換算と小数丸めを組み合わせた表示では、丸め後に単位上限値へ達する帯域 (閾値 − 丸め幅/2 の領域) を次の単位へ繰り上げ、境界の両側を境界値テストで固定する。境界テストは中間単位で打ち切らず、**最終単位とその飽和領域 (繰り上げ先がない超過値) まで張り、実データが最初に踏むレンジ (GB 級ファイル等) を必ず含める**。また Optional な値の表示フォールバックに `or 既定値` を使わず、欠落 (None/null) は明示判定して欠落マーカーを出す — 「データが無い」と「実値がゼロ」を混同させない。
- **出典**: APKExplorer M3 レビュー (commit 6f22ba9、apk_explorer/gui/views/asset_view.py) / M4 レビュー (commit e764a13、tests/gui/test_formatting.py — GB 帯・飽和領域の境界テスト欠落、apk_explorer/gui/views/metadata_view.py — `or 0` の欠落値すり替え)

❌ **初回実装にありがちなパターン (丸め境界)** — 閾値判定と表示丸めを別々に書き、丸めが閾値を跨ぐ帯域で "1024.0 KB" が出る:

```python
    for next_unit in ("KB", "MB", "GB"):
        value /= 1024
        unit = next_unit
        if value < 1024:
            break
    return f"{value:.1f} {unit}"  # GB 超は GB 表記のまま
```

✅ **レビュー後の修正パターン** — 丸め幅を考慮した閾値に変更し、両側 + 最終単位の飽和領域を境界値テストで固定:

```python
        # 1023.95 以上は小数第 1 位への丸めで "1024.0" と表示されてしまうため、
        # その帯域は繰り上げて次の単位に回す
        if value < 1023.95:
            break

# テスト側:
        (1048575, "1.0 MB"),  # 丸め境界: "1024.0 KB" にならず MB へ繰り上げ
        (1048502, "1023.9 KB"),  # 繰り上げ境界のすぐ手前は従来どおり KB のまま
        (3 * 1024**3, "3.0 GB"),  # 実データ (GB 級ファイル) が最初に踏むレンジ (M4 で追加)
        (2 * 1024**4, "2048.0 GB"),  # GB 超は繰り上げ先がなく GB 表記のまま (飽和領域)
```

❌ **初回実装にありがちなパターン (欠落値)** — `or 0` が None を 0 にすり替え「合計 0 B」と表示:

```python
        section.addChild(
            QTreeWidgetItem(
                [
                    "アセット",
                    f"{_int_or_missing(metadata.asset_count)} 件 "
                    f"(合計 {format_size(metadata.asset_total_size or 0)})",
                ]
            )
        )
```

✅ **レビュー後の修正パターン** — None を明示判定して欠落マーカー「—」を表示:

```python
        total_size = (
            _MISSING
            if metadata.asset_total_size is None
            else format_size(metadata.asset_total_size)
        )
        section.addChild(
            QTreeWidgetItem(
                [
                    "件数と合計",
                    f"{_int_or_missing(metadata.asset_count)} 件 (合計 {total_size})",
                ]
            )
        )
```

### 状態切替時は前状態の表示残留を掃除し、その掃除が正当な表示を壊さないよう条件を絞れ

- **規則**: 状態切替 UI では、主要な表示要素 (メインビュー・タブ活性等) だけでなく、ステータスバー等の副次的な表示に残る前状態のメッセージも掃除する。ただし無条件の上書きは正当な別メッセージを壊しうるため、掃除対象の条件を絞り、「残留が消える」「正当な表示は残る」の両方向をテストする。
- **出典**: APKExplorer M6 レビュー (commit 6b05679、apk_explorer/gui/main_window.py)

❌ **初回実装にありがちなパターン** — 掃除処理そのものが欠落。対象切替ハンドラがビューとタブ活性のみ更新し、失敗対象から健全な対象へ切り替えても「対象の解析に失敗」がステータスバーに残留する。

✅ **レビュー後の修正パターン** — 失敗文言のときのみ差し替える条件付き更新:

```python
            # 失敗対象から健全な対象へ切り替えたときに「対象の解析に失敗」の残留を消す
            # (A-4)。無条件更新は解析直後の自動選択が上書きしてしまう「解析完了: ...」
            # (_on_analysis_finished 発火) を壊すため、失敗文言のときのみ差し替える
            if self.statusBar().currentMessage().startswith("対象の解析に失敗"):
                self.statusBar().showMessage(f"表示中: {target.label}")
```

### 表示 API の解釈モード (リッチ/プレーン) をヒューリスティック任せにせず、エスケープと明示指定をセットで行え

- **規則**: 表示 API が内容ヒューリスティックで解釈モードを切り替える場合 (Qt の `mightBeRichText` 等)、「エスケープすれば正しく表示される」とは仮定できない。エスケープと解釈モードの明示指定をセットで行い、特殊文字 (`&` 等) がエスケープ → 表示で正しく復元される往復をテストする。
- **出典**: APKExplorer M6 レビュー (commit 6b05679、apk_explorer/gui/widgets/target_sidebar.py)

❌ **初回実装にありがちなパターン** — エスケープ結果をそのまま渡し、プレーンテキスト判定で `&amp;` が生表示される:

```python
                if tooltip:
                    item.setToolTip(html.escape(tooltip))
```

✅ **レビュー後の修正パターン** — タグで包んで解釈モードを固定:

```python
                if tooltip:
                    # <p> で包んで常にリッチテキスト解釈させる (A-5)。タグを含まない
                    # 文字列だと Qt の mightBeRichText がプレーンテキスト扱いし、
                    # html.escape 済みのエンティティ (&amp; 等) がそのまま生文字列として
                    # 表示されてしまう。注入は escape 済みのため安全
                    item.setToolTip(f"<p>{html.escape(tooltip)}</p>")
```

### イベントループの再入 (非同期完了・モーダルダイアログ) を跨ぐ状態は、世代照合と同一性再検証で守れ

- **規則**: 協調的キャンセルは完了阻止を保証しない (完了間際のジョブはフラグを無視して正常完了する) ため、キャンセル要求とは別に世代番号 (トークン) 照合による遅延結果の破棄を必ず併設する。同様に、モーダルダイアログを挟む処理はダイアログ表示中に別の非同期処理が状態を差し替えうるため、依存状態をダイアログ表示前にすべて確定させ、戻った時点で状態の同一性を再検証してから実行する。(同じ脅威クラスのインデックス境界チェックは「エラー処理」セクションの該当項を参照)
- **出典**: APKExplorer M7 レビュー (commit e1b16e5、apk_explorer/gui/app_state.py / apk_explorer/gui/main_window.py — A-2 must-fix / A-6)

❌ **初回実装にありがちなパターン (世代照合の欠落)** — 「キャンセルすれば結果は届かない」と仮定し、片方の世代番号だけ進める:

```python
        if self._extraction_cancel is not None:
            self._extraction_cancel.set()
        if self._cancel is not None:
            self._cancel.set()
        self._generation += 1
```

✅ **レビュー後の修正パターン** — 系統ごとの世代番号を必ず進め、遅れて届く旧世代の結果をすべて破棄:

```python
        if self._extraction_cancel is not None:
            self._extraction_cancel.set()
        self._extraction_generation += 1
        if self._cancel is not None:
            self._cancel.set()
        self._generation += 1
```

❌ **初回実装にありがちなパターン (モーダル前後)** — 状態取得がダイアログの前後に分散し、表示中の差し替えで不整合な組み合わせのまま実行:

```python
        target = self._current_target
        if target is None or target.result is None:
            return
        directory = QFileDialog.getExistingDirectory(self, "展開先フォルダーを選択")
        if not directory:
            return
        bundle = self.app_state.current_bundle
        if bundle is None:
            return
```

✅ **レビュー後の修正パターン** — 依存状態を表示前に確定し、戻った直後に同一性を再検証:

```python
        bundle = self.app_state.current_bundle
        if bundle is None:
            return  # 通常到達しない防御
        container = bundle.container
        archive_display_name = container.display_name or container.path.stem

        directory = QFileDialog.getExistingDirectory(self, "展開先フォルダーを選択")
        if not directory:
            return
        if self._current_target is not target:
            # ダイアログ表示中に対象が差し替わった。無言で古いデータのまま進めない
            self.statusBar().showMessage("対象が変更されたため中止しました")
            return
```

### 同名のシグナル/イベントでも発生源のスコープが異なるならハンドラを共有せず、対象範囲を明示的に渡せ

- **規則**: 複数のビューが同名のシグナル/イベントを発行していても、各ビューがユーザーに提示している対象範囲が異なるなら、ハンドラを安易に共有しない。発生源ごとに専用ハンドラを設け、そのビューが表示している対象のスコープ (索引一覧等) を明示的に引き渡す — 「アセット一覧の『すべて』」がアーカイブ全体を意味してはならない。
- **出典**: APKExplorer M7 レビュー (commit e1b16e5、apk_explorer/gui/main_window.py — A-7)

❌ **初回実装にありがちなパターン** — 同名シグナルを見て全体展開ハンドラへ接続:

```python
        self.asset_view.extract_all_requested.connect(self._on_extract_all_requested)
```

✅ **レビュー後の修正パターン** — 専用ハンドラで表示中の対象一覧を明示的に渡す:

```python
        self.asset_view.extract_all_requested.connect(self._on_asset_extract_all_requested)
# ...
    def _on_asset_extract_all_requested(self) -> None:
        target = self._current_target
        if target is None or target.result is None or target.result.assets is None:
            return
        indices = [entry.entry_index for entry in target.result.assets.entries]
        self._start_batch_extraction(indices)
```

### 提出前に作業ツリーの衛生を確認せよ — ツール生成物の VCS 無視設定と、フォーマッタ整形後の可読性

- **規則**: キャッシュや生成物を作るツールを導入するときは、依存追加と同じコミットでその生成物を VCS 無視設定に追加するところまでを導入作業の完了条件にする。また、フォーマッタの自動整形結果を無批判に受け入れない — 長い説明は行末コメントではなく定義の前行に置き、フォーマッタが生成した不自然な折返しはコメント位置の側を直す。
- **出典**: APKExplorer M3 レビュー (commit 6f22ba9、.gitignore) / M4 レビュー (commit e764a13、apk_explorer/models/apk_data.py)

❌ **初回実装にありがちなパターン (無視設定の欠落)** — テストツール導入時に依存追加とテスト作成で完了とみなし、実行時キャッシュ (`.hypothesis/` 等) の gitignore 追加が漏れて未追跡ファイルが作業ツリーを汚す。

✅ **レビュー後の修正パターン** — 他ツールキャッシュの並びに追加:

```gitignore
.pytest_cache/
.mypy_cache/
.ruff_cache/
.hypothesis/
```

❌ **初回実装にありがちなパターン (整形結果の無批判な受け入れ)** — 行末コメントが括弧折返しされ、一見「= (0)」という無意味な括弧に見える:

```python
    file_size: int = (
        0  # ディスク上のファイルサイズ (バイト)。既定 0 はテスト用ファクトリの互換のため
    )
```

✅ **レビュー後の修正パターン** — コメントを前行へ移して素直な 1 行に:

```python
    # ディスク上のファイルサイズ (バイト)。既定 0 はテスト用ファクトリの互換のため
    file_size: int = 0
```

### PowerShell ラッパで任意コマンドを素通しするなら「非 advanced スクリプト + $args」— CmdletBinding + ValueFromRemainingArguments は使わない

- **規則**: 「ラッパ引数 + 残り全部を被実行コマンドとして受ける」PowerShell スクリプトを CmdletBinding + `ValueFromRemainingArguments` で書くと、(1) 被コマンドのハイフン付きトークン (`-v`, `-port` 等) が共通パラメータや宣言済みパラメータへ横取りされて無言で消える、(2) 裸のトークンが位置束縛で数値パラメータ等に当たって型エラーになる、(3) bash 慣習の `--` 区切りは pwsh -File では「空のパラメータ名」としてバインドエラーになり回避策にならない。非 advanced (CmdletBinding も [Parameter] もなし) にすれば未宣言トークンは `$args` へ文字どおり素通しされる (引用文字列・ハイフン引数とも保持)。必須検証は手動 if で行い、宣言パラメータ名と衝突し得る被コマンド引数だけ注意書きする。
- **出典**: LLLM_Kari simplify レビュー (2026-07-16、run_with_forge.ps1 / run_with_comfy.ps1) — 委譲実装の CmdletBinding 版は文書化した呼び出し例そのものが起動不能 (bindtest 4 ケース実測で全呼び出し形態が破綻)。$args 受けへの書き直し後、実エンジンスモーク 3 系統 (成功系 / exit code 伝播 / 再利用系) で確認

❌ **初回実装にありがちなパターン** (起動すらできない):

```powershell
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$LogDir,
    [Parameter(Mandatory = $true, ValueFromRemainingArguments = $true)][string[]]$Command
)
# pwsh -File wrapper.ps1 -LogDir x -- python foo.py -v
#   → '--' がバインドエラー。'--' を外すと -v が -Verbose に横取りされる
```

✅ **レビュー後の修正パターン** — 非 advanced + $args 素通し:

```powershell
param(
    [string]$LogDir
)
if (-not $LogDir) { Write-Host 'エラー: -LogDir を指定してください'; exit 1 }
$Command = @($args)   # ハイフン引数・引用文字列とも文字どおり届く
if ($Command.Count -eq 0) { Write-Host 'エラー: コマンドを指定してください'; exit 1 }
& $Command[0] @($Command | Select-Object -Skip 1)
```

### 表示・出力用のパス表記を内部の索引キーに流用するな — 異なる実体が同じ表記へ潰れる

- **規則**: 監査出力や互換のために短縮したパス表記 (親ディレクトリ名 + ファイル名、basename 等) は、別の実体と衝突し得る。それを dict のキーや照合キーに流用すると、後から登録した実体が前の実体を黙って上書きし、前者に対する検査 (欠損・期限・ハッシュ) が後者の値で通ってしまう。内部の索引キーには解決済み絶対パスや内容ハッシュなど一意な識別子を使い、表示用の表記は出力欄にだけ残す。テストは「表記が同じで中身が違う 2 実体」を並べ、索引の件数と、前者だけが持つ欠陥が検出されることを確認する。
- **出典**: Racing-Predictor2 PRE-04 レビュー (commit 8b048c1、racing_predictor/jvlink.py)。過去走の基底 `.day-sources/<id>/option1/manifest.json` と差分 `shared/option1/manifest.json` が同じ `option1/manifest.json` になり、基底の提供元時刻不明が差分の証拠で隠れた

❌ **初回実装にありがちなパターン** — 出力用の短縮表記を索引キーにする:

```python
def _canonical_provenance_path(path: Path) -> str:
    return path.relative_to(path.parent.parent).as_posix()  # 監査出力用の短縮表記

evidence_by_path = {
    _canonical_provenance_path(path): evidence  # 基底と差分が同じキーになり後勝ち
    for path, evidence in zip(manifests, evidences)
}
```

✅ **レビュー後の修正パターン** — 索引キーは一意な識別子にし、表示用表記は出力欄に残す:

```python
def _source_evidence_key(manifest: str | Path) -> str:
    """時刻証拠の内部索引key。表示用の相対表記は基底と差分で衝突するため使わない。"""
    return str(Path(manifest).resolve())

# テスト側 (抜粋): 表記が同じ 2 実体で、索引件数と前者の欠陥検出を確認する
base = write_manifest(root / "source" / "option1", source_update_id=None)
delta = write_manifest(root / "batch" / "option1", source_update_id="20260915085900")
evidence_map = _source_evidence_map((base, delta), evidence)
self.assertEqual(len(evidence_map), 2)
self.assertIn("SOURCE_TIME_UNKNOWN", _strict_time_blockers((base, delta), cutoff, source_evidence=evidence_map))
```
