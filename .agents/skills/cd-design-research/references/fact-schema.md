# Portable fact schema

各claimを次のfieldで正規化せよ。

| field | 規則 |
|---|---|
| claim | 単独で検証できる1つの事実。 |
| source | canonical URL、40桁commit固定URL、またはrepository相対path。 |
| retrieved_at | 実際の取得日を`YYYY-MM-DD`で記録。 |
| version | 文書版、commit SHA、または`codex --version`と認証surface。 |
| confidence | `confirmed`、`partial`、`unverified`のいずれか。 |
| verification | 独立再取得、別source照合、単独照合、未実施の別と結果。 |

`confirmed`は独立再取得または別の一次資料が同じ範囲を支持した場合だけ使え。条件が残る場合は`partial`、取得・再現・照合ができない場合は`unverified`とせよ。

repositoryに既存の事実表がある場合は、そのfield名、ID、時系列、更新規約を優先し、このschemaから必要な値だけを写せ。既存IDを推測で採番せず、索引を根拠そのものとして引用するな。

事実資産がない場合は、次のJSON相当のrecordを回答へ出せ。userが新設を明示しない限りfileを作るな。

    {
      "claim": "...",
      "source": "...",
      "retrieved_at": "YYYY-MM-DD",
      "version": "...",
      "confidence": "confirmed|partial|unverified",
      "verification": "..."
    }

patchを作る場合は、対象の正本、追加または更新するrecord、索引などの派生物を区別せよ。read-onlyでは同じ内容を候補として返し、適用済みと表現するな。
