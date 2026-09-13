# Cache・model policy

## Instruction削減

削除候補は、同一ruleの再掲、挙動を変えないstyle説明、modelが確実に行う一般手順、課題に無関係なtool説明に限れ。userに見える成果、成功基準、停止条件、安全・業務・証拠・権限制約、routing、出力形式を保持せよ。

文章全体へ新たな「簡潔にせよ」を足さず、不要な指示そのものを削れ。

## Static prefixとcache

静的で反復するinstructionとtool定義を先頭、動的なuser値を末尾へ置け。冒頭へtimestamp、session ID、乱数を置くな。tool定義の内容と順序、profile、AGENTS、Skill集合をrun間で固定せよ。

cache hit率だけでなく`cached_input_tokens`と`cache_write_input_tokens`を記録せよ。cached inputはinputの内数であり、高cache率だけで総token削減を主張するな。

## 出力とcompact

`max_output_tokens`を削減施策にするな。compactは同じ課題の新規sessionと比較し、quality gateとusage 5 fieldsで採否を決めよ。自動compact limitとscopeを結果へ明示せよ。

## Effortとmodel

現行effortを基線にし、1段下だけを同じevalで比較せよ。model変更では利用権限とsmoke testを確認し、extraction、classification、conversion、structured summaryだけを小型model候補にせよ。

`gpt-5.6-sol` / `medium`と`gpt-6-astra` / `medium`を同一case、同一品質gateで明示比較するまでAstraの採用効果を未検証とせよ。Astraの`low`や高cache率だけを節約根拠にするな。起動前に現行Codex runtime catalogでmodelとeffortの対応を確認し、未対応または不明なら停止せよ。

ChatGPT認証ではCodex runtime catalogのmodel、context、effort、usageを使え。API-key runだけは取得日付きのAPI仕様とpricingで換算してよい。

## Skill index縮退

repo、user、bundledの全enabled scopeでSkill数とdescription合計を測れ。予算超過時は次の順を守れ。

1. 不要なbundled、user、repo Skillを設定でdisableする。
2. positiveとnegativeの発火条件を保ってdescriptionを短縮する。
3. 重複する責務のSkillを統合する。

repo scopeへ移すだけの施策を削減として数えるな。Desktop runtimeが自動提供するtoolはrepository所有分と分離し、所有外設定を勝手に変更するな。
