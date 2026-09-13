# Delegation測定

## 比較値

同一課題の直接runと委譲runから次を記録せよ。

- 直接runの親context growth
- 委譲runの親context growth
- 子runのusage 5 fields
- 委譲の起動・待機・要約overhead
- 両runのquality gate

親contextの節約と完了までの親子総usageを区別せよ。品質非劣化と総token減少を採用条件にし、親文脈は同等時の補助指標とせよ。実測なしに数値閾値を作るな。

## Route

| task特性 | route |
|---|---|
| 独立したread-heavy探索、test、triage、要約 | 委譲候補。直接runと同じgate、親子総tokenで比較する。 |
| 確定claimへの敵対的検証 | verifier候補。品質上必要ならtoken削減対象から除外する。 |
| 親が本文を既読の単発判断 | 親で処理する。 |
| 同一fileへ及ぶwrite-heavy作業 | 1 writerだけにする。 |
| security、不可逆操作、最終acceptance | 親が判断する。 |

subagentが利用不能、receiver IDを取得できない、またはusageを取得できない場合は委譲効果をINSUFFICIENTとし、数値を推測しない。
