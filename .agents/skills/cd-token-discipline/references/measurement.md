# Usage測定

## Class

| class | 対象 | 主な測定 |
|---|---|---|
| a | 常時文脈: AGENTS、Skill索引、tool・MCP定義 | UTF-8 bytes、description code points、tool数、input usage |
| b | Skill発火時: SKILL本文、選択したreferences | 本文bytes、読んだreference、input usage |
| c | runtime: tool output、reasoning、delegation | usage 5 fields、quality gate、親文脈へ戻る要約 |

同じ入力を複数classへ重複計上するな。Skill metadataはa、発火後の本文はb、実行中の出力はcへ割り当てよ。

## Usage fields

各runから次の5 fieldを整数またはnullで記録せよ。

- `input_tokens`
- `cached_input_tokens`
- `cache_write_input_tokens`
- `output_tokens`
- `reasoning_output_tokens`

欠落fieldを0とみなすな。取得不能ならnull、理由、取得surfaceを記録し、判定をINSUFFICIENTにせよ。

## 完了までの総token

親と全ての子について、同じ計測windowの各responseを一度だけ集計せよ。累積usageと個別responseを重複加算するな。`cached_input_tokens`と`cache_write_input_tokens`は`input_tokens`の内数、`reasoning_output_tokens`は`output_tokens`の内数である。総tokenは親子の`input_tokens + output_tokens`の合計とし、再試行や引継ぎも完了までに使った分を含めよ。子のusageが1件でも不明なら総量を推定せずINSUFFICIENTとせよ。ChatGPT利用枠は別観測とし、tokenに換算するな。

## Before and after

同じcase、成功条件、入力artifact、認証surface、usage window状態を使え。変更対象以外のmodel、effort、profile、tool順、Skill集合を固定せよ。各条件を独立runとして保存し、fieldごとにbefore、after、deltaを出せ。cacheのwarm/cold、compact limit、delegation fan-outも固定条件へ含めよ。

## Quality gate

token減少だけでPASSにするな。課題固有test、schema検証、出典確認、rubric scoreなど同じgateを両条件へ適用せよ。

- PASS: 両runが親子全体で測定可能でquality gateが非劣化し、完了までの総token減少を観測した。
- FAIL: 測定可能だがquality gateが劣化した、または総tokenが増えた。
- INSUFFICIENT: usage欠落、run error、比較条件不一致、または有効run不足。

結果には`case_id`、`class`、`auth_surface`、`changed_variable`、`fixed_conditions`、`before`、`after`、`delta`、`quality_gate`、`verdict`、`evidence`を含めよ。
