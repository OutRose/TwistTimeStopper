<!-- 出典: REVIEW_CHECKLIST.md (マージ前レビュー確定修正パターン集) のカテゴリ別分割。
     索引と使い方は ../INDEX.md 参照。追記時は ../4-testing.md と ../INDEX.md にも反映する。 -->

## 4. テスト

対象範囲: Given-When-Then 構造、境界値網羅、モックの適切性、Red-Green-Refactor 遵守、偽の緑の防止、網羅性テストの情報源結合、property-based testing。

### enum をキーとする辞書には集合比較の網羅性テストを置き、期待値は真の情報源と結合させよ

- **規則**: enum をキーに直引き・列挙する辞書 (表示ラベル、ソート順など) には「キー集合 == enum 全メンバー (除外分は明示)」を集合比較で直接検証するテストを必ず付ける。辞書側から列挙する実装では追加漏れが例外にならず「エントリの無言消失」になるため、実行時エラーによる検出は期待できない。さらに、2 モジュール間で同期すべき集合の期待値はテスト内にハードコードで複製せず、片方の定義を直接参照して比較する (テストを真の情報源と結合させる)。
- **出典**: APKExplorer M3 レビュー (commit 6f22ba9、tests/gui/test_asset_view.py) / M4 レビュー (commit e764a13、tests/gui/test_metadata_view.py) / M6 レビュー (commit 6b05679、tests/core/test_xapk_extractor.py、tests/gui/test_target_sidebar.py)

❌ **初回実装にありがちなパターン** — 網羅性テスト自体を書かない (M3: 「今は揃っている」ため将来の追加漏れ検出まで考えない)、または書いても期待値を「現時点の正解の複製」としてベタ書きする (M4: reporter 側に観点が追加されてもテスト内の複製リストは追随しない):

```python
    assert set(_ASPECT_LABELS) == {
        AnalysisAspect.STRUCTURE,
        AnalysisAspect.MANIFEST,
        AnalysisAspect.PERMISSIONS,
        AnalysisAspect.ASSETS,
    }
```

✅ **レビュー後の修正パターン** — 集合比較の網羅性テストを置き、期待値は同期相手の定義を直接 import する:

```python
def test_category_labels_cover_all_asset_categories() -> None:
    """_CATEGORY_LABELS.items() で列挙するため、AssetCategory 追加時の更新漏れは

    エントリが無言で消える形で表れる (KeyError 等では検出できない)。網羅性を直接検証する。
    """
    assert set(_CATEGORY_LABELS) == set(AssetCategory)
```

```python
from apk_explorer.core.metadata_reporter import _REPORTED_ASPECTS
# ...
    assert set(_ASPECT_LABELS) == set(_REPORTED_ASPECTS)
```

```python
# 除外があるなら除外分を明示して比較する (M6):
    assert set(_SORT_ORDER) == set(TargetKind) - {TargetKind.PACKAGE}
```

### 数値・長さ上限は「ちょうど = 合格」「+1 = 不合格」の対で検証し、超過時の伝播先仕様も固定せよ

- **規則**: サイズ・件数・長さの上限チェックには「上限とちょうど等しい入力で成功」「1 単位超過で失敗」の両側を対でテストし、比較演算子の off-by-one を機械的に検出できるようにする。あわせて、超過時のエラーが個別失敗として扱われるのか処理全体の失敗として伝播するのか、伝播先の仕様もテストで固定する。
- **出典**: APKExplorer M6 レビュー (commit 6b05679、tests/core/test_xapk_extractor.py、tests/core/test_analysis.py) / M7 レビュー (commit e1b16e5、tests/core/test_entry_extractor.py)

❌ **初回実装にありがちなパターン** — 上限チェックの実装だけ書き、テストは「大きく超えるケース」1 点のみ (M6 のサイズ上限・件数上限、M7 のフルパス長上限 3 件とも境界両側のテストが欠落していた)。

✅ **レビュー後の修正パターン** — 境界の両側を対で検証する:

```python
def test_extract_succeeds_when_written_exactly_equals_max_size(
    make_zip: ZipFactory, tmp_path: Path
) -> None:
    payload = b"A" * 777
    path = make_zip("app.xapk", {"base.apk": payload})
    spec = _spec_for(path, "base.apk")
    written = extract_inner_file(path, spec, tmp_path / "out.apk", max_size=len(payload))
    assert written == len(payload)

def test_extract_fails_when_written_is_one_byte_over_max_size(
    make_zip: ZipFactory, tmp_path: Path
) -> None:
    payload = b"A" * 777
    path = make_zip("app.xapk", {"base.apk": payload})
    spec = _spec_for(path, "base.apk")
    with pytest.raises(SafetyLimitExceededError):
        extract_inner_file(path, spec, tmp_path / "out.apk", max_size=len(payload) - 1)
```

```python
# 超過時の伝播先 (個別失敗ではなく全体失敗) も仕様としてテストで固定する:
def test_max_inner_targets_limit_propagates_as_bundle_level_failure(
    xapk_factory: Callable[..., Path],
) -> None:
    """件数上限超過は対象別失敗ではなく束全体の解析失敗として伝播する (B-2)。"""
    xapk_path = xapk_factory(extra_entries={"split_a.apk": b"x", "split_b.apk": b"y"})
    with pytest.raises(SafetyLimitExceededError):
        run_bundle_analysis(xapk_path, max_inner_targets=1)
```

### 多重防御の各層は、その層だけが反応する専用フィクスチャで独立に検証せよ (偽の緑の防止)

- **規則**: 複数の防御 (フィルタ・判定) が重なる箇所で 1 つのフィクスチャを使い回すと、手前の層で先に弾かれて奥の層が一度も実行されないまま「偶然通過する偽の緑」になる。層ごとに、検証したい層だけが反応する入力を用意して独立にテストする。複数の防御が同じ結果 (拒否等) に合流する場合は、結果だけでなく理由・メッセージまで検証してどの防御が効いたかを特定する。
- **出典**: APKExplorer M6 レビュー (commit 6b05679、tests/core/test_xapk_extractor.py) / M7 レビュー (commit e1b16e5、tests/core/test_entry_extractor.py)

❌ **初回実装にありがちなパターン** — スラッシュ終端名は suffix フィルタ (`.endswith(".apk")` が False) だけで先に除外されるため、防御本体である `is_dir` 分岐が一度も実行されない:

```python
def test_directories_are_never_selected(make_zip: ZipFactory) -> None:
    path = make_zip("app.xapk", {"base.apk": b"x"})
    with zipfile.ZipFile(path, "a") as zf:
        zf.writestr("split.apk/", b"")  # ディレクトリ扱いの偽装エントリ
```

✅ **レビュー後の修正パターン** — 手前の層では弾けない形の入力で、検証したい分岐そのものを通す:

```python
    # is_dir 分岐自体の検証: "/" 終端ではないが is_dir=True の偽装エントリ
    # (suffix フィルタだけでは弾けない形) が選定されないことを確かめる
    disguised_dir = ArchiveEntry(
        name="fake.apk", size=0, compressed_size=0, is_dir=True, is_suspicious=False
    )
    disguised_structure = ArchiveStructure(entries=(disguised_dir,), total_uncompressed_size=0)
    assert select_inner_targets(disguised_structure) == ()
```

```python
    # 別の防御 (末尾ドット判定) でも偶然同じ結果になるため、理由文言まで検証して
    # "." 専用分岐の作動を特定する (上流の suspicious 判定に頼らないことも明示):
    structure = _structure(_entry("a/./b.txt", is_suspicious=False, entry_index=0))
    plan = plan_extraction(structure, root=tmp_path / "out")
    assert plan.items[0].outcome is ExtractionOutcome.REJECTED
    assert "パスの構成が不正" in plan.items[0].reason
```

### 統合テスト・表示メッセージは存在確認ではなく、フィクスチャから導出できる正確な期待値で検証せよ

- **規則**: フィクスチャの内容が既知なら、「> 0」等の存在確認ではなく、そこから導出できる正確な件数・ラベル・名前で検証する — ゆるいアサーションは誤配線・誤分類でも通過してしまう。ユーザー向け文言に埋め込まれた件数・集計ロジック (除外条件・集合演算) も通常のロジックと同様にテスト対象とし、境界を固定する。
- **出典**: APKExplorer M3 レビュー (commit 6f22ba9、tests/gui/test_main_window.py) / M7 レビュー (commit e1b16e5、tests/gui/test_app_state.py)

❌ **初回実装にありがちなパターン** — 「何かが表示された」レベルの存在確認 (カテゴリ名・件数・エントリ名がすべて間違っていても通過する):

```python
    path = make_zip("app.apk")  # 既定エントリに assets/data.txt を含む
    # ...
    assert window.asset_view.tree.topLevelItemCount() > 0
```

✅ **レビュー後の修正パターン** — フィクスチャから導ける正確な期待値で検証する:

```python
    assert window.asset_view.tree.topLevelItemCount() == 1
    text_group = window.asset_view.tree.topLevelItem(0)
    assert text_group is not None
    assert text_group.text(0) == "テキスト (1)"
    assert text_group.childCount() == 1
    assert text_group.child(0).text(0) == "assets/data.txt"
```

```python
    # 文言中の件数計算 (ディレクトリ除外・指定 index との交差・存在しない index の無視) も
    # 正確な期待値で固定する (M7):
    with qtbot.waitSignal(state.extraction_finished, timeout=5000):
        state.start_batch_extraction(
            Path("app.apk"), _mixed_structure(), root=Path("out"), entry_indices=[0, 1, 3, 99]
        )

    assert started == ["2 件をフォルダーへ展開中"]
```

### property-based テストは、生成器が出力の全分類・実装の主要分岐に到達できる入力空間を張っているか確認せよ

- **規則**: property テストは「緑になった」ことでは性質の証明にならない。生成器の設計時に、実装の主要分岐と出力の全分類が実際に生成されうる入力空間を張っているかを確認する — 一部の入力を常に空・固定で渡す狭い生成器では、一部の分岐だけを通った全緑になる。
- **出典**: APKExplorer M3 レビュー (commit 6f22ba9、tests/core/test_permission_classifier.py)

❌ **初回実装にありがちなパターン** — third_party 集合しか draw せず、もう一方の入力 (AOSP 権限詳細) を常に空で渡すため、出力 5 分類のうち DANGEROUS/SIGNATURE/NORMAL が一度も生成されない:

```python
    third_party = (
        frozenset(data.draw(st.sets(st.sampled_from(requested)))) if requested else frozenset()
    )
    parsed = make_parsed_apk(
        requested_permissions=requested,
        third_party_permissions=tuple(sorted(third_party)),
    )
```

✅ **レビュー後の修正パターン** — 各要素を排他的な種別へ割り当てて draw し、全分類が生成されうる入力空間に広げる:

```python
    unique_names = tuple(dict.fromkeys(requested))  # 出現順を保った重複排除
    assignments = {
        name: data.draw(st.sampled_from(("detail", "third_party", "none")), label=f"kind:{name}")
        for name in unique_names
    }
    aosp_permission_details = tuple(
        AospPermissionDetail(
            name=name,
            protection_level=data.draw(
                st.sampled_from(_SAMPLE_PROTECTION_LEVELS), label=f"level:{name}"
            ),
            # ...
        )
        for name, kind in assignments.items()
        if kind == "detail"
    )
```

### サニタイズ・無害化は、悪意入力をパイプライン全体に通して可視出力の全チャネルを走査するテストで検証せよ

- **規則**: 無害化・サニタイズ処理は「呼び出しコードを書いた」時点では未完成とみなす。悪意入力 (双方向制御文字・改行入りの名前等) を入口から通し、ユーザー可視出力の全チャネル (ラベル・進捗・エラー文言) に危険文字が残らないことを端から端まで走査する検証テストを必ず付ける。
- **出典**: APKExplorer M6 レビュー (commit 6b05679、tests/core/test_analysis.py) — must-fix 指摘

❌ **初回実装にありがちなパターン** — 無害化関数を適用する実装コードは書いたが、悪意入力を実際に通して出力を検査するテストが 1 本もない (欠落)。

✅ **レビュー後の修正パターン** — 悪意入力を合成してパイプラインへ流し、全出力チャネルを走査する (危険文字はエスケープ表記で書き、テストコード自体に生の制御文字を埋め込まない):

```python
    evil_name = "evil" + "\u202e" + "RLO\nnewline.apk"  # RLO (双方向制御) + 改行
    xapk_path = xapk_factory(extra_entries={evil_name: b"x"})
    messages: list[str] = []
    bundle = run_bundle_analysis(xapk_path, progress=messages.append)
# ...
    haystacks = [t.label for t in bundle.targets] + messages
    for text in haystacks:
        assert UNSAFE_DISPLAY_CHARS.search(text) is None
        assert "\t" not in text
        assert "\n" not in text
        assert "\r" not in text
```

### 代表 1 ケースで網羅を打ち切るな — 空コレクション・復路遷移・enum 全値をそれぞれ検証せよ

- **規則**: 代表的な正常系 1 ケースの通過をもって「他も同じだろう」と網羅を打ち切らない。(1) コレクションが 0 件になる境界ケースはコア層の戻り値と UI 層の表示の両方で、(2) UI の状態トグル (表示/非表示、活性/非活性) は往路に加えて別種の入力へ切り替えたとき元へ戻る復路も同一インスタンス上で、(3) 値ごとに分岐しうる enum ゲートは代表 1 値ではなく該当する全値で (かつフィクスチャが意図した分岐を通ることを確認して)、それぞれ検証する。
- **出典**: APKExplorer M6 レビュー (commit 6b05679、tests/core/test_analysis.py、tests/gui/test_main_window.py) / M7 レビュー (commit e1b16e5、tests/gui/test_main_window.py)

❌ **初回実装にありがちなパターン** — 内包物ありの代表正常系のみ・サイドバーが「表示される」方向のみ・ゲート検証は enum 代表 1 値 (BASE_APK) のみで打ち切る (いずれもテスト欠落)。

✅ **レビュー後の修正パターン** — 0 件境界・復路遷移・enum 全値をそれぞれ明示的に検証する:

```python
def test_xapk_with_zero_inner_targets_has_only_package_target(make_zip: ZipFactory) -> None:
    xapk_path = make_zip("app.xapk", {"manifest.json": b"{}", "icon.png": b"x"})
    bundle = run_bundle_analysis(xapk_path)
    assert len(bundle.targets) == 1
    assert bundle.targets[0].kind is TargetKind.PACKAGE
```

```python
def test_xapk_then_single_apk_hides_sidebar_again(
    qtbot: QtBot, window: MainWindow, xapk_with_broken_inner: Path, make_zip: ZipFactory
) -> None:
    with qtbot.waitSignal(window.app_state.analysis_finished, timeout=5000):
        window.open_path(xapk_with_broken_inner)
    assert window.target_sidebar.isVisibleTo(window)

    with qtbot.waitSignal(window.app_state.analysis_finished, timeout=5000):
        window.open_path(make_zip("app.apk"))
    assert not window.target_sidebar.isVisibleTo(window)
```

```python
    # 同じゲートを通る enum の全値 (SPLIT_APK / OBB) も検証する (M7):
    with qtbot.waitSignal(window.app_state.target_selected, timeout=5000):
        window.target_sidebar.setCurrentRow(split_row)
    assert _extraction_enabled_states(window) == (False, False)

    with qtbot.waitSignal(window.app_state.target_selected, timeout=5000):
        window.target_sidebar.setCurrentRow(obb_row)
    assert _extraction_enabled_states(window) == (False, False)
```

### カバレッジ等の品質閾値は実測値の直下でラチェットし、以後下げるな

- **規則**: 品質閾値 (カバレッジの fail_under 等) を実測値より低い「安全そうなキリのいい数字」に置くと、その余裕マージン幅の退行が無検出で通る緩衝帯になる。閾値は実測値の直下に設定してラチェットし、以後は下げない運用にする。
- **出典**: APKExplorer M3 レビュー (commit 6f22ba9、pyproject.toml)

❌ **初回実装にありがちなパターン** — 実測 99.75% に対して約 5 ポイントの緩衝帯を作る:

```toml
fail_under = 95
```

✅ **レビュー後の修正パターン** — 実測値の直下に引き上げてラチェット開始 (以後下げない):

```toml
fail_under = 99
```

### 品質ゲートの合否比較は表示丸めに依存し得る — 閾値未満で実際に落ちることを一度実証せよ

- **規則**: 閾値ゲートはツール側が「丸め後の値」で合否比較することがある。coverage.py は total を `precision` (既定 0 桁) で丸めてから `fail_under` と比較するため、precision 未指定の `fail_under = 99.9` は実測 99.5 でも 100 に丸められて通過する。小数の閾値を設定するときは丸め精度を閾値の小数桁以上に明示し、ゲート導入時に「閾値未満の状態を意図的に作って exit code が非 0 になる」ことを一度実証する。一度も落ちたことのないゲートは動作未検証と見なす。
- **出典**: Racing-Predictor T5-03 レビュー (commit 47b5b73、pyproject.toml — T3-04 のゲート導入以来、実測 99.56% が fail_under=99.9 を整数丸めで常時すり抜けていた。委譲実装の「カバレッジ 100%」誤報告をメイン側が実測で検証したことから発覚)

❌ **初回実装にありがちなパターン** — 丸め精度を意識せず小数閾値だけ置く:

```toml
[tool.coverage.report]
fail_under = 99.9   # precision 既定 0 桁 → 実測 99.56 が 100 に丸められ通過
```

✅ **レビュー後の修正パターン** — 精度を明示し、落ちることを実証してから運用に入れる:

```toml
[tool.coverage.report]
precision = 2       # fail_under の小数桁以上を明示 → 99.56 < 99.9 が exit 2 で検出される
fail_under = 99.9
```

---
