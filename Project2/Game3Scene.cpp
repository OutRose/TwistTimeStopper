#include "GameMain.h"
#include "GameSceneMain.h"
#include "Game3Scene.h"

//外部定義(GameMain.cppにて宣言)
extern int Input, EdgeInput;

//計測状態をまとめた構造体 (詳細は GameSceneMain.h の TIMER_STATE)
//static でファイルスコープに限定 (Game1/Game3 の state は別物として独立、β-D-2b+c の教訓)
static TIMER_STATE state = {};

//状態遷移マネージメント変数 (詳細は GameSceneMain.h の TIMER_STATUS を参照)
//static でファイルスコープに限定 (Game1 側の同名グローバルとの LNK2005 回避)
static TIMER_STATUS status = TIMER_STATUS_INIT;

// シーン開始前の初期化を行う
BOOL initGame3Scene(void)
{
	//入るときは必ずリセット
	status = TIMER_STATUS_INIT;
	return TRUE;
}

//	フレーム処理 (Game1Scene と同じロジック: G/S/R/X 入力 + status 遷移)
void moveGame3Scene()
{
	switch (status)
	{
	case TIMER_STATUS_INIT://ここではプレイヤーの入力を待つ
		//タイマーを初期化
		state.FrameTmp = 0.0;
		state.Score = 0.0;

		//目標秒数、倍速値の設定関数を呼び出す。
		//その後、ステータス番号を変更する
		timerReset(&state);
		status = TIMER_STATUS_READY;
		break;

	case TIMER_STATUS_READY:

		//Gキー（GO）で計測開始
		if (CheckHitKey(KEY_INPUT_G) == 1)
		{
			if (status == TIMER_STATUS_READY)
			{
				status = TIMER_STATUS_MEASURING;
			}
		}

		//Xキーでタイトルに戻す
		if (CheckHitKey(KEY_INPUT_X) == 1)
		{
			changeScene(SCENE_MENU);
		}

		break;

	case TIMER_STATUS_MEASURING://ここでは計測処理を行う
		//フレーム加算処理
		state.FrameTmp += state.CalMulti;

		//Sキー（STOP）で計測終了
		if (CheckHitKey(KEY_INPUT_S) == 1)
		{
			if (status == TIMER_STATUS_MEASURING)
			{
				status = TIMER_STATUS_DONE;
			}
		}

		//Xキーでタイトルに戻る
		if (CheckHitKey(KEY_INPUT_X) == 1)
		{
			changeScene(SCENE_MENU);
		}

		break;

	case TIMER_STATUS_DONE://計測が終了したあとの処理を行う
		//スコア計算 (共通関数、Game1 と同じ確定スコア算出。R リセット前のスコアを安定化させるため必須)
		timerCalcScore(&state);

		//Rキーで状態リセット
		if (CheckHitKey(KEY_INPUT_R) == 1)
		{
			status = TIMER_STATUS_INIT;
		}

		//Xキーでタイトルに戻す
		if (CheckHitKey(KEY_INPUT_X) == 1)
		{
			changeScene(SCENE_MENU);
		}

		break;
	}
}

//	レンダリング処理
//Game1Scene との違いは以下 3 点 (練習モードの学習用 UX):
// (a) 操作説明文の冒頭に "練習: " を追加 (UX 差別化)
// (b) "ただいま：？.？倍速" 隠蔽を解除 (倍率を常時公開)
// (c) MEASURING 中もリアルタイムスコアを表示 (コピー方式で副作用回避)
void renderGame3Scene(void)
{
	//Rリセット可能であることを通知 (練習プレフィックスでモード視認性を上げる)
	if (status == TIMER_STATUS_DONE)
	{
		DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "練習: Rキーでリセット", ColorWhite);
	}
	else
	{
		DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "練習: Gキーで開始、Sキーで停止", ColorWhite);
	}

	DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_BACK_TO_TITLE, "Xボタンでタイトルに戻る", ColorWhite);

	//目標時間表示 (Game1 と同じ)
	DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_TARGET, ColorWhite, "%3.1f秒でストップ！", state.RandomTgt, state.CalFrame);

	//倍率は常時公開 (練習モードのため、Game1 のような ?.?倍速 隠蔽は無し)
	DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_SPEED, ColorYellow, "ただいま：%3.1f倍速", state.CalMulti);

	//現在の経過時間 (Game1 と同じ、FrameTmp は CalMulti 加算済みの累積フレーム)
	DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_CURRENT_TIME, ColorGreen, "現在の時間：%3.2f秒", state.FrameTmp / FPS);

	//リアルタイムスコア (Game1 では計測終了後のみ表示だが、練習モードでは MEASURING 中も毎フレーム表示)
	//コピー方式: timerCalcScore は state.Score/IsPerfect を書き換えるため、tmp に複製してから呼び副作用を遮断
	//(δ-1 で ScMulti は削除済、書き換え対象は Score と IsPerfect の 2 メンバ)
	TIMER_STATE tmp = state;
	timerCalcScore(&tmp);
	//スコアは 0〜100 の正規化達成率 (δ-1 で再設計、目標が変わっても値域固定で UX 安定)
	DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_SCORE, ColorSkyLike, "スコア：%3.1f", tmp.Score);

	//練習モード: PERFECT! をリアルタイムでも表示 (ユーザー選択、δ-1)。
	//目標近傍でフラグが瞬間的に ON/OFF 切替わる可能性はあるが、学習用の即時フィードバック優先。
	if (tmp.IsPerfect == TRUE)
	{
		DrawString(LAYOUT_X_DEFAULT + LAYOUT_X_PERFECT_OFFSET, LAYOUT_Y_SCORE, "PERFECT!", ColorRed);
	}
}

//	シーン終了時の後処理
void releaseGame3Scene(void)
{
}

// 当り判定コールバック 　　　ここでは要素を削除しないこと！！
void  Game3SceneCollideCallback(int nSrc, int nTarget, int nCollideID)
{
}