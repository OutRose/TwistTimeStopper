<!-- 出典: REVIEW_CHECKLIST.md (マージ前レビュー確定修正パターン集) のカテゴリ別分割。
     索引と使い方は INDEX.md 参照。追記時は INDEX.md にも規則を一行追加する。 -->

## 7. セキュリティ (信頼できない入力)

> 基本 7 カテゴリへの追加カテゴリ。APKExplorer のレビュー実績で最大のクラスタ (must-fix の過半) がここに属するため独立させた。解析対象のアーカイブは**悪意あるファイルでありうる**。エントリ名・宣言サイズ・件数・エラーメッセージに乗るパスまで、すべて攻撃者が制御しうる入力として扱う。このセクションは、表示文字列の無害化 (文脈別)、パス封じ込め、サイズ/件数上限、TOCTOU、内部情報 (一時パス等) の漏洩防止、zip bomb / パストラバーサル対策を対象とする。

### 危険文字の deny-list は「文字の列挙」でなく「脅威クラス」単位で洗い出し、全防御層を同時に更新せよ

- **規則**: 表示偽装対策で危険文字を拒否リスト化するときは、既知の悪い文字カテゴリの列挙で止めず、「同じ表示効果を持つ文字すべて」(改行相当なら Unicode Zl/Zp まで) を脅威クラス単位で洗い出す。同じ文字集合を複数の防御層に重複定義している場合は、全層を同時に更新し層ごとに回帰テストを置く — 盲点はリストごと複製される。
- **出典**: APKExplorer M3 レビュー (commit 6f22ba9、apk_explorer/core/archive_structure.py + apk_explorer/core/apk_parser.py — 両層を同一コミットで更新)

U+2028/U+2029 (行・段落区切り) は Cc/Cf のどちらでもないため、下の判定と、同じ文字集合を列挙していた表示サニタイズの正規表現の**両方**をすり抜け、ツリー表示の行偽装が成立した。

❌ **初回実装にありがちなパターン** — 既知カテゴリ 2 つの列挙で満足する

```python
# Cc (制御文字: NUL/改行/ESC 等) と Cf (書式文字: U+202E RLO 等の双方向制御) は
# ツリー表示の視覚偽装 (拡張子偽装・行押し出し) に使えるため疑わしい扱いにする
if any(unicodedata.category(char) in {"Cc", "Cf"} for char in name):
    return True
```

✅ **レビュー後の修正パターン** — 「行押し出し」という脅威クラスに属する隣接カテゴリまで拒否し、両層を同時更新

```python
# Cc (制御文字: NUL/改行/ESC 等)、Cf (書式文字: U+202E RLO 等の双方向制御)、
# Zl/Zp (行/段落区切り: U+2028/U+2029) は、ツリー表示の視覚偽装
# (拡張子偽装・行押し出し) に使えるため疑わしい扱いにする
if any(unicodedata.category(char) in {"Cc", "Cf", "Zl", "Zp"} for char in name):
    return True
```

```python
# もう一方の防御層 = 表示サニタイズの正規表現にも同じ範囲を追加。M3 当時の修正先は
# apk_parser._UNSAFE_DISPLAY_CHARS (M6 で core/sanitize.py へ移動)。以下は現行 sanitize.py より抜粋:
    "\\u202a-\\u202e"  # LRE/RLE/PDF/LRO/RLO (双方向埋め込み・上書き)
    # ...
    "\\u2066-\\u2069"  # LRI/RLI/FSI/PDI (双方向分離)
    "\\u2028-\\u2029"  # LINE/PARAGRAPH SEPARATOR (行表示破壊)
```

### 「パスとして解釈しない」と「表示用に無害化する」は独立の防御 — 目視判断に使われる表示には必ず後者も適用せよ

- **規則**: 非信頼文字列への「パスとして解釈しない」防御と「表示用に無害化する」防御は別レイヤーであり、片方を実装しても他方は満たされない。ユーザーが目視で可否を判断する表示 (警告マーク付き一覧等) には、必ず表示用の無害化 (制御文字・双方向制御の除去) も通す。
- **出典**: APKExplorer M7 レビュー (commit e1b16e5、apk_explorer/gui/views/structure_view.py)

「危険な名前はパス解釈せずフラット表示」という対策で満足し、表示文字列そのものの無害化を漏らすと、U+202E (RLO) 等がそのままレンダリングされて拡張子偽装が成立する。ユーザーが ⚠ 表示を見て保存可否を判断するフローが載った時点で実害になった (元は初期実装からの潜在バグ)。

❌ **初回実装にありがちなパターン** — パス解釈回避だけで生名を表示に流す

```python
# 生の名前をパス解釈せずフラットに表示 (§5 セキュリティ規約 1)
item = QTreeWidgetItem([f"{_SUSPICIOUS_MARK}{entry.name}", str(entry.size)])
```

✅ **レビュー後の修正パターン** — 表示直前に単一行文脈用の無害化を通す

```python
# 表示自体も単一行無害化する (RLO 等の視覚偽装対策、§5 規約 7、M7 レビュー A-5)
safe_name = sanitize_single_line(entry.name)
item = QTreeWidgetItem([f"{_SUSPICIOUS_MARK}{safe_name}", str(entry.size)])
```

なお無害化は文脈 2 種を使い分けること: 複数行文脈 (全文表示等) は `\t\n\r` を許容してよいが、単一行文脈 (ラベル・ステータスバー・ツリーの 1 セル) では `\t\n\r` も除去する。単一行文脈に複数行用を流用すると改行入りの名前で行偽装される。

### 種別フィルタの `continue` に危険検知の集計を巻き添えにさせるな — 比率の分子・分母は同一母集団で計算せよ

- **規則**: 集計ループの先頭に種別フィルタ (ディレクトリ除外等) の `continue` を置くと、フィルタ条件と無関係であるべき集計 — 特に危険検知カウントと比率の分子 — まで巻き添えでスキップされる。危険検知の計上と比率の分子・分母は `continue` より前で同一の母集団から計算し、同じ情報を表示する他画面との件数一致をテストで固定する。
- **出典**: APKExplorer M4 レビュー (commit e764a13、apk_explorer/core/metadata_reporter.py) — must-fix 1 件を含む同一ループの 2 指摘を統合

この 1 つの `continue` 位置が 2 つの攻撃を許した: (1) 悪意ある名前のディレクトリエントリ (例 `../evil/`) が危険件数に計上されず、他画面の ⚠ 件数と矛盾して過小報告になる。(2) 分子 (圧縮サイズ合計) が非 dir のみ・分母が全エントリという非対称になり、名前が `/` で終わる偽装 dir エントリへ大データを持たせると、表示上の圧縮率を実態と無関係に 0.0% まで恣意的に押し下げられる — zip bomb 判断の手掛かりとして表示している指標を、攻撃者が偽装できてしまう。

❌ **初回実装にありがちなパターン** — 「ディレクトリは集計対象外」と種別フィルタを先頭に置く

```python
for entry in result.structure.entries:
    if entry.is_dir:
        directory_entry_count += 1
        continue
    file_entry_count += 1
    total_compressed_size += entry.compressed_size
    if entry.is_suspicious:
        suspicious_entry_count += 1
```

✅ **レビュー後の修正パターン** — フィルタと無関係な集計をフィルタより前へ移し、母集団の意味をモデルに明文化

```python
for entry in result.structure.entries:
    total_compressed_size += entry.compressed_size
    if entry.is_suspicious:
        suspicious_entry_count += 1
    if entry.is_dir:
        directory_entry_count += 1
        continue
    file_entry_count += 1

# models 側のフィールドコメントで母集団を明文化:
    # is_suspicious エントリ数 (ディレクトリ含む。内部構造タブの ⚠ 件数と一致)
    suspicious_entry_count: int
    # 全エントリの compressed_size 合計 (分母の total_uncompressed_size と対象を揃える)
    total_compressed_size: int
```

### 秘匿情報 (一時パス等) を消す「単一の関門」には、派生データへの複写経路と表記ゆれの全てを通せ

- **規則**: 内部情報 (一時ファイルパス等) をユーザー可視文字列から消す関門を設けるときは、(1) 同じ文字列が派生モデルへ複写される全経路を関門の対象に含め、(2) OS・ライブラリによる repr 化/エスケープの表記ゆれ (バックスラッシュ二重化等) や部分露出 (親ディレクトリ単独) もすべて置換対象にする。漏洩検査テストの走査対象 (haystack) にも複写先を含む全出力を入れる — 実装と同じ盲点を共有したテストは偽の緑になる。
- **出典**: APKExplorer M6 レビュー (commit 6b05679、apk_explorer/core/analysis.py) — must-fix 含む 2 指摘を統合

**(1) 複写経路の見落とし**: 関門が失敗メッセージ (failures) だけを置換し、そこからメッセージを複写する派生モデル (metadata.aspect_statuses) を見落とすと、第二の経路で一時ファイル名がそのまま画面へ漏れる。専用の漏洩テストも aspect_statuses を走査していなかったため、テスト自体が偽の緑だった。

❌ **初回実装にありがちなパターン** — 元データだけを関門に通す

```python
def _relabel_result(result: AnalysisResult, dest: Path, label: str) -> AnalysisResult:
    """result.failures の各メッセージから一時ファイル名を relabel する。"""
    if not result.failures:
        return result
    relabeled_failures = tuple(
        replace(failure, message=_relabel_text(failure.message, dest, label))
        for failure in result.failures
    )
    return replace(result, failures=relabeled_failures)
```

✅ **レビュー後の修正パターン** — 複写先の派生モデルも関門の対象にし、漏洩テストの haystack にも追加

```python
    metadata = result.metadata
    if metadata is not None:
        relabeled_statuses = tuple(
            replace(status, message=_relabel_text(status.message, dest, label))
            for status in metadata.aspect_statuses
        )
        metadata = replace(metadata, aspect_statuses=relabeled_statuses)
    return replace(result, failures=relabeled_failures, metadata=metadata)

# 漏洩検査テスト側 (偽の緑だった専用テストの補強):
                for status in target.result.metadata.aspect_statuses:
                    haystacks.append(status.message)
```

**(2) 表記ゆれの見落とし**: `str(OSError)` はメッセージ内のパスを repr 化するため、Windows ではバックスラッシュが二重 (`C:\\Users\\...`) になり、正規化された 1 表記だけの置換をすり抜ける。

❌ **初回実装にありがちなパターン** — 正規化された 1 表記への素朴な単一置換

```python
    return text.replace(str(dest), label).replace(dest.name, label)
```

✅ **レビュー後の修正パターン** — エスケープ表記ゆれと親ディレクトリ単独露出も置換する多段置換

```python
    for token in (str(dest), str(dest).replace("\\", "\\\\")):
        text = text.replace(token, label)
    parent = str(dest.parent)
    for token in (parent, parent.replace("\\", "\\\\")):
        text = text.replace(token, "一時ディレクトリ")
    return text.replace(dest.name, label)
```

### リソース予算の会計は成功時の戻り値でなく実消費量を情報源とし、失敗経路も必ず通る場所に一本化せよ

- **規則**: 合計サイズ・件数などの資源予算 (安全上限) の減算を成功経路の戻り値だけで行うと、途中失敗した処理の実消費 (実際にディスクへ書かれたバイト等) が計上されず、意図的に失敗する入力の繰り返しで予算を迂回できる。会計は実測値を情報源とし、成功・失敗・キャンセルの全経路が必ず通る単一箇所 (`finally` 等) へ一本化する。
- **出典**: APKExplorer M6 レビュー (commit 6b05679、apk_explorer/core/analysis.py)

CRC 破壊されたエントリは全量をディスクへ書き込んだ後に例外を投げるため、成功時の戻り値だけで減算していると、その実書き込みが合計予算 (zip bomb 対策の上限) に一切計上されない。

❌ **初回実装にありがちなパターン** — happy path の戻り値でのみ減算し、finally では削除だけ行う

```python
                    written = extract_inner_file(
                        path, spec, dest, max_size=effective_limit, cancel=cancel
                    )
                    remaining_budget -= written
# ...
                finally:
                    dest.unlink(missing_ok=True)
```

✅ **レビュー後の修正パターン** — 減算を finally へ一本化し、実ファイルサイズを情報源にする

```python
                    extract_inner_file(path, spec, dest, max_size=effective_limit, cancel=cancel)
# ...
                finally:
                    # 予算減算はここへ一本化する (A-3): 成功時は written と同値、
                    # 失敗時 (CRC 破壊等で全量書き込み後に失敗する場合も含む) は
                    # 実際にディスクへ書き込まれたバイト数を計上する。
                    if dest.exists():
                        remaining_budget -= dest.stat().st_size
                        dest.unlink()
```

失敗後の後続処理が予算超過で正しく失敗すること (迂回できないこと) までテストで検証する。

### エントリ名からパスを導出する展開は「白リスト導出 + 封じ込め二重検証 + ソース再検証 + 実バイト上限」で書け — stdlib の extract/extractall を使うな

- **規則**: アーカイブエントリをディスクへ展開する処理では、標準ライブラリの一括展開 API (Python の `ZipFile.extract`/`extractall` は禁止文字の無言置換・末尾ドットの無言除去・無言上書き・サイズ上限なしという挙動を持つ) に頼らず、(1) 拒否ベースの白リストに合格した名前だけパス導出を許し、不合格はリネームも無言スキップもせず理由付きで可視化する、(2) 書き込み直前に解決済みパスが展開ルート配下にあることを再検証する、(3) 展開時にアーカイブを開き直して解析時のエントリと同一かを照合する (TOCTOU 対策)、(4) 宣言サイズを信用せず実書き込みバイト数で上限判定する、(5) 排他的新規作成で既存ファイルの無言上書きを防ぐ。
- **出典**: 規約由来 (レビュー実績なし) — CLAUDE.md §5 セキュリティ規約 8、現行実装 apk_explorer/core/entry_extractor.py

❌ **初回実装にありがちなパターン** (欠落): `zf.extractall(dest)` の一括呼び出し、または `dest / entry.name` の素朴な結合 + 危険文字の無言リネームで済ませる。パストラバーサル・無言上書き・zip bomb・解析後の差し替え (TOCTOU) がすべて素通しになる。「サニタイズした名前で展開する」のも誤り — サニタイズは表示専用であり、抽出可否の判定・パス導出には使わない。

✅ **レビュー後の修正パターン** (現行実装より抜粋)。第一防御 = 白リスト導出 (拒否は理由付きで可視化):

```python
    if entry.is_suspicious:
        return None, "エントリ名に危険な文字列を含むため展開しません"

    segments = entry.name.split("/")
    for segment in segments:
        if segment in ("", ".", ".."):
            return None, "パスの構成が不正なため展開しません"
        if any(char in _FORBIDDEN_CHARS for char in segment):
            return None, "使用できない文字を含むため展開しません"
        if segment[-1] in (".", " "):
            return None, "セグメント末尾がドットまたはスペースのため展開しません"
        # ... (予約デバイス名・セグメント長・フルパス長の上限も同様に拒否)
```

第二防御 = 書き込み直前の封じ込め検証 (第一防御が正しくても重ねる):

```python
        dest = plan.root / relative_path
        try:
            dest.resolve().relative_to(plan.root.resolve())
        except ValueError:
            # 封じ込め二重検証 (規約 8-b) の第二防御。plan_extraction が正しく動作していれば
            # 常に真になるはずであり、通常のテストでは到達しない内部エラーガード
            # ... (対象別失敗として記録)
```

展開時のソース再検証 (差し替え検出) + 実バイト上限 + 排他的新規作成 (`"xb"`):

```python
    mode = "wb" if overwrite else "xb"
# ...
            infos = zf.infolist()
            if entry_index >= len(infos) or infos[entry_index].orig_filename != expected_name:
                raise CorruptArchiveError(
                    "エントリの位置が一致しません (差し替えの疑い): "
                    f"{sanitize_single_line(expected_name)}"
                )
# ...
                        written += len(chunk)
                        if written > max_size:
                            raise SafetyLimitExceededError(
                                f"抽出サイズが上限 ({max_size:,} バイト) を超えています: "
                                f"{sanitize_single_line(expected_name)}"
                            )
                        out.write(chunk)
```

---
