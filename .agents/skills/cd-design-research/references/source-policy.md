# 一次資料policy

## 許可source

根拠には次の一次資料だけを使え。

- OpenAIが提供する現行のCodex・API・Help Center文書
- OpenAI公式cookbook
- `github.com/openai/codex`のsource、schema、release
- 対象repository内の実測logと設定。ただし一般仕様ではなく、その環境の観測として扱う

検索結果のsnippetは探索にだけ使い、canonical pageを開いて本文を確認せよ。redirectされた文書は最終到達先を記録せよ。GitHub sourceは40桁commit SHAに固定し、symbolまたは行位置を併記せよ。

## 拒否source

- 第三者blog、Q&A、転載、要約を確定根拠にするな。
- mirror、検索snippet、AI生成要約をsourceに残すな。
- branch、tag、`main`を指すGitHub URLを確定根拠にするな。
- API-key向け仕様や価格をChatGPT認証のCodex runtimeへ無条件に転用するな。

## 取得日とfreshness

`retrieved_at`は取得日の暦日を`YYYY-MM-DD`で記録せよ。文書はcanonical URL、sourceはcommit SHA、runtime観測は`codex --version`と認証surfaceをversion証拠として残せ。

repositoryに失効規則があればそれを優先する。無ければ取得から6暦月、Codex CLIのminor version更新、source redirect、実挙動との不一致のいずれかで再取得対象とせよ。

## 否定表現

一次資料が不存在または非対応を明示した場合だけ「公式に不存在」または「非対応」と断定せよ。許可sourceを探索して該当記述を確認できなかった場合は「今回未発見」、network・権限・検索範囲の制約で取得できなかった場合は「未取得」と書け。
