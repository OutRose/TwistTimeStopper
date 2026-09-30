<!-- 出典: REVIEW_CHECKLIST.md (マージ前レビュー確定修正パターン集) のカテゴリ別分割。
     索引と使い方は ../INDEX.md 参照。追記時は ../3-error-handling.md と ../INDEX.md にも反映する。 -->

## 3. エラー処理

対象範囲: 例外階層設計、外部ライブラリ例外の境界翻訳、エラーメッセージの構造化、リカバリ可能性の明示、観点別ベストエフォート、境界チェック。共通する根本原因は「ハッピーパスの成立だけを見て、チェック通過後・失敗経路・状態変化後の世界を検討しない」こと。

### ファイルシステムの「チェック」と「使用」は 1 回の stat 系呼び出しに統合し、その呼び出し自体の失敗も翻訳する

- **規則**: 存在確認と属性取得を別々のファイルシステム呼び出しに分けると、その間の削除・権限変更 (TOCTOU) で 2 回目の呼び出しが生の OS 例外を投げ、境界翻訳をすり抜ける。1 回の stat 系呼び出しに統合して結果から全判定を導き、その呼び出し自体の失敗もドメイン例外へ翻訳する (チェック通過を後続呼び出しの成功保証と見なさない)。
- **出典**: APKExplorer M4 レビュー (commit e764a13、apk_explorer/core/container.py)

❌ **初回実装にありがちなパターン** — チェックと使用が別呼び出しで、2 回目は「失敗しない前提」:

```python
    if not path.is_file():
        raise FileNotFoundError(f"ファイルが見つかりません: {path}")
    # ...
        file_size=path.stat().st_size,
```

✅ **レビュー後の修正パターン** — stat() は try 付きの 1 回、判定はその結果から:

```python
    try:
        stat_result = path.stat()
    except OSError as exc:
        raise FileNotFoundError(f"ファイルが見つかりません: {path}") from exc
    if not stat.S_ISREG(stat_result.st_mode):  # 通常ファイル以外 (ディレクトリ等) の拒否
        raise FileNotFoundError(f"ファイルが見つかりません: {path}")
    # ...
        file_size=stat_result.st_size,
```

あわせて「拡張子だけ正しいディレクトリ」と「stat 時 PermissionError」の両テストを追加すること。

### データモデルの不変条件は docstring で終わらせず、構築時検証で機械的に強制する

- **規則**: 「先頭要素は必ず X」「この種別ならフィールド Y は必須」のような不変条件をコメントや docstring に書いただけでは、違反データが静かに成立して下流が壊れた前提で動く。コンストラクタ検証 (Python なら `__post_init__`) で違反を即座に拒否し、違反パターンごとの拒否テストを書く。**後からフィールドを追加したときも同様に強制を追加する** (既存フィールドの排他検証があるのに新フィールドの必須検証を漏らす、という同型の欠陥が 1 マイルストーン後に再発した)。
- **出典**: APKExplorer M6 レビュー (commit 6b05679、apk_explorer/models/apk_data.py) / 同型の再発は M7 レビュー B-4 (commit e1b16e5、同ファイル)

❌ **初回実装にありがちなパターン** — 不変条件が docstring の記述のみ:

```python
    単体 .apk / .obb では targets は PACKAGE の 1 件のみ。
    """

    container: ContainerInfo
    targets: tuple[AnalysisTarget, ...]
```

✅ **レビュー後の修正パターン** — `__post_init__` で違反 3 パターンを拒否:

```python
    def __post_init__(self) -> None:
        if not self.targets:
            raise ValueError("AnalysisBundle.targets は空にできません")
        first = self.targets[0]
        if first.kind is not TargetKind.PACKAGE:
            raise ValueError("AnalysisBundle.targets[0] は TargetKind.PACKAGE である必要があります")
        if first.result is None:
            raise ValueError("AnalysisBundle.targets[0].result は None にできません")
```

✅ **後から追加したフィールドにも同じ強制を** (M7 での再発修正):

```python
            raise ValueError(
                "AnalysisTarget は result と error のちょうど一方が非 None である必要があります"
            )
        if self.kind is not TargetKind.PACKAGE and self.entry_index is None:
            raise ValueError(
                "PACKAGE 以外の AnalysisTarget は entry_index が必須です (M7 レビュー B-4)"
            )
```

### ネストイベントループやモーダルを挟んで届くインデックス・参照は、受信時点の状態に対して再検証してから使う

- **規則**: コンテキストメニューやモーダルダイアログ (ネストイベントループ) の表示中にも背後の状態は変わりうる。シグナル/イベント経由で受け取ったインデックスや参照は「送信時点の状態」を指しており、受信・再開時点の状態と整合する保証がない。添字アクセス等の使用直前に境界チェックで再検証し、不整合なら安全に無視する。
- **出典**: APKExplorer M7 レビュー A-1・must-fix (commit e1b16e5、apk_explorer/gui/main_window.py)

❌ **初回実装にありがちなパターン** — ペイロードのインデックスをそのまま添字に:

```python
        entry = target.result.structure.entries[entry_index]
```

✅ **レビュー後の修正パターン** — 使用直前の境界チェック + 到達経路のコメント:

```python
        entries = target.result.structure.entries
        if not (0 <= entry_index < len(entries)):
            # 通常到達しない防御: QMenu.exec() 中 (ネストイベントループ) に別ファイルの
            # 解析が完了して対象が差し替わると、古い entry_index が新しい対象へ渡りうる
            return
        entry = entries[entry_index]
```

範囲外インデックスを emit しても例外にならないことの回帰テストも併設する。同じ脅威クラス (イベントループ再入を跨ぐ状態) への世代番号照合・モーダル前後の同一性再検証は「その他」セクションの該当項を参照。

### 既存ファイルへの上書き保存は一時ファイル+アトミック置換で行い、失敗時は元データを無傷で保全する

- **規則**: 宛先へ直接書き込むと、書き込み開始の瞬間に既存内容が失われ、途中失敗・キャンセルで元ファイルが書きかけの残骸になる (失敗時に削除処理を足しても元の内容は復元できない)。同一ディレクトリ内の一時ファイルへ排他的新規作成で書き込み、成功時のみアトミックな置き換え (`os.replace` 相当) で差し替える。ユーザーの上書き確認は「正常完了時の上書き」への同意であり、「失敗時の元データ破壊」への同意ではない。
- **出典**: APKExplorer M7 レビュー A-3・must-fix (commit e1b16e5、apk_explorer/gui/app_state.py)

❌ **初回実装にありがちなパターン** — 上書き確認済みを根拠に宛先へ直接 overwrite (テストも `overwrite is True` をそのまま固定していた):

```python
        written = extract_single(
            archive_path,
            entry_index,
            entry_name,
            dest,
            max_size=declared_size,
            overwrite=True,
            cancel=cancel,
        )
    except AnalysisCancelledError:
        raise
```

✅ **レビュー後の修正パターン** — 一時ファイルに排他作成で書き、成功時のみアトミック置換:

```python
    temp_path = _make_temp_path(dest)
    try:
        written = extract_single(
            # ...
            temp_path,
            max_size=declared_size,
            overwrite=False,
            cancel=cancel,
        )
    except AnalysisCancelledError:
        temp_path.unlink(missing_ok=True)
        raise
    # ...
        temp_path.replace(dest)
```

失敗・キャンセル経路では一時ファイルのみ削除し、宛先 (上書き対象だった既存ファイル) には一切触れないこと。

### 外部ライブラリの例外は境界層でドメイン例外へ翻訳し、捕捉順は継承関係を確認して決める

- **規則**: 外部ライブラリ・標準ライブラリ由来の例外 (OS ロケール依存メッセージや内部パスを含みうる) は境界層で自前のドメイン例外階層へ翻訳し、上位層 (UI) は基底例外の捕捉で足りるようにする。複数の例外型を翻訳し分けるときは継承関係を確認し、サブクラスの except を必ず先に書く (逆順は握りつぶしになるが実行時エラーにならず、テストがなければ気づけない)。
- **出典**: 規約由来 (レビュー実績なし — 本カテゴリの抽出 finding には含まれない。CLAUDE.md §5-11・§5 セキュリティ規約 6 に基づく。スニペットは現行コードからの実コード)

❌ **初回実装にありがちなパターン** (欠落の説明): `except RuntimeError` を先に書いてサブクラスの `NotImplementedError` を意図と違う分類で握りつぶす。あるいはライブラリが投げる例外型 (この例では zipfile の `BadZipFile` / `ValueError` / `struct.error` / `zlib.error` や、`open()` の `RuntimeError` / `NotImplementedError`) を調べ切らず、想定外の型が生のまま UI 層へ伝播する。

✅ **レビュー後の到達形** (現行実コード、apk_explorer/core/entry_extractor.py):

```python
            try:
                with zf.open(info) as source, dest.open(mode) as out:
                    # ... 上限付きチャンク読み出し ...
            except NotImplementedError as exc:
                # 非対応の圧縮方式。NotImplementedError は RuntimeError のサブクラスのため、
                # 下の except RuntimeError より必ず先に書く (でないと握りつぶされる)
                raise UnsupportedFileTypeError(
                    "対応していない圧縮方式のため取り出せません"
                ) from exc
            except RuntimeError as exc:
                # zf.open() はパスワード保護エントリで RuntimeError を投げる
                raise UnsupportedFileTypeError(
                    "パスワード保護されたエントリのため取り出せません"
                ) from exc
    except (zipfile.BadZipFile, ValueError, struct.error, zlib.error) as exc:
        raise CorruptArchiveError(
            f"ZIP アーカイブとして読み取れません: {archive_path.name}"
        ) from exc
```

ドメイン例外のメッセージはそのままユーザー表示に使われる前提で書き (この方式ではリカバリ可能性の判別は例外型が担う)、一時ファイルのフルパス等の内部情報を埋め込まないこと。

---

### 「パス or データ本体」の二形態引数は内容ベースで判別する (存在チェックで分岐しない)

- **規則**: 1 つの引数がファイルパスとデータ本体 (b64 文字列等) の両方を受けるとき、`isfile()` の真偽で分岐すると「パスのつもりだがファイル不在」の入力がデータ解釈の経路へ落ち、欠落したパスがどのエラーメッセージにも現れなくなる (診断性の退行)。判別はデータ側の構造的特徴 (マジックバイトの接頭辞、スキーム等) で行い、パス側は素直に open してパス明示の例外を出させる。長さ等のヒューリスティックも避ける (境界値のテストデータで壊れる)。
- **出典**: LLLM_Kari simplify レビュー (2026-07-16、g3_inpaint_fix.py) — isfile 分岐が不在パスを b64 デコードへ流し FileNotFoundError が binascii.Error に化けた。長さ閾値 (>1000) への修正案も等価性ハーネスの 1×1 PNG スタブ (b64 92 文字) で誤判定し、マジックバイト判別 (`s.startswith(("iVBORw0KGgo", "/9j/"))`) が確定形

❌ **初回実装にありがちなパターン**:

```python
def _load_image(src):
    if os.path.isfile(src):          # 不在パスは「b64」扱いに落ちる
        return Image.open(src)
    return Image.open(BytesIO(base64.b64decode(src)))  # エラーに欠落パスが出ない
```

✅ **レビュー後の修正パターン** — データ側の構造で判別、パスは open に任せる:

```python
def _is_b64(s):
    # b64 画像は必ずマジックバイトの base64 で始まる (PNG='iVBORw0KGgo', JPEG='/9j/')
    return s.startswith(("iVBORw0KGgo", "/9j/"))

def _load_image(src):
    if _is_b64(src):
        return Image.open(BytesIO(base64.b64decode(src)))
    return Image.open(src)  # 不在ならパスを明示した FileNotFoundError になる
```

### 共通ヘルパーへの置換は「契約の一致」を確認する — 特に旧コードの失敗様式を無音成功に変えない

- **規則**: 重複コードを共通ヘルパーへ寄せるとき、シグネチャが合うことは契約が合うことを意味しない。旧コードとヘルパーで (1) 出力の個数・命名規則、(2) 想定外入力での失敗様式 (例外で停止するか・黙って何もしないか)、(3) 戻り値の意味、を突き合わせる。とくに「旧: `xs[0]` で KeyError 停止 → 新: ループ 0 回で正常続行 + 無条件の成功ログ」は失敗の無音化であり、送信データの等価性テストでは検出できない (応答側・保存側の差だから)。
- **出典**: LLLM_Kari simplify レビュー (2026-07-16、g3_variants.py) — 単発保存 `save_one` (常に images[0] を固定名で保存) を複数枚対応 `save_images` (枚数依存の連番命名・全枚保存) に置換した結果、ControlNet 応答の detectmap 混入で出力ファイル名が変わり、かつ images 欠落時に成功ログを出して続行する退行。等価性ハーネス (リクエスト列比較) は全 PASS のまま検出できず、レビューで発覚

❌ **初回実装にありがちなパターン**:

```python
res = post(endpoint, p)
save_images(res, name, out_dir)      # 旧 save_one と枚数・命名契約が違う。戻り値 (保存数 0) も無視
print(f"OK {name}")                  # 1 枚も保存していなくても成功ログ
```

✅ **レビュー後の修正パターン** — 旧契約を維持し、失敗はそのまま失敗させる:

```python
res = post(endpoint, p)
# 生成画像は images[0] のみ (CN 有効時は detectmap が末尾に付加され複数枚になる)。
# 全枚保存の save_images は使わない (旧 save_one と同じ保存契約を維持)
with open(os.path.join(out_dir, name + ".png"), "wb") as f:
    f.write(base64.b64decode(res["images"][0]))   # images 欠落は KeyError で停止 (無音化しない)
print(f"OK {name}")
```
