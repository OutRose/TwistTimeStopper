# AGENTS.md — TwistTimeStopper入口

詳細規約のcanonical sourceは`PROJECT_GUIDE.md`です。全体を常時読み込まず、作業に該当する節だけ参照してください。

## 常時適用

- Windows/MSVC/DxLibを対象とし、全テキストをUTF-8 BOM・CRLFで保持する。
- 既存の利用者変更、未所有資産、生成物を尊重し、秘密情報やbuild出力を追加しない。
- 作業branchを使い、破壊的なGit操作を行わない。

## 条件付き参照

- encoding・改行: `PROJECT_GUIDE.md`「ファイルエンコーディング」
- build・frame処理: 「ビルド環境」、scene変更: 「アーキテクチャ」
- 命名・comment・入力・DxLib・warning: 対応する各節
- network・未完了事項・Git・refactor: 対応する各節
- code変更時はguide記載のbuildを実行する。文書変更時はlink、BOM、CRLF、`git diff --check`を検証する。

設計文書と実装が衝突する、未所有資産への破壊的変更が必要、または新しい設計判断が必要な場合は、変更を止めて根拠pathと論点を報告してください。
