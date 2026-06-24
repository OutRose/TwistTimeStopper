#include "GameMain.h"
#include "GameSceneMain.h"
#include "Game1Scene.h"

//外部定義(GameMain.cppにて宣言)+変数シードに使ったミリ秒データ
extern int Input, EdgeInput, rdSeed;

//計測状態をまとめた構造体 (詳細は GameSceneMain.h の TIMER_STATE)
//static でファイルスコープに限定 (Game1/Game2 の state は別物として独立させる)
static TIMER_STATE state = {};

//状態遷移マネージメント変数 (詳細は GameSceneMain.h の TIMER_STATUS を参照)
TIMER_STATUS status = TIMER_STATUS_INIT;

// シーン開始前の初期化を行う
BOOL initGame1Scene(void)
{
	//入るときは必ずリセット
	status = TIMER_STATUS_INIT;
	return TRUE;
}

//	フレーム処理
void moveGame1Scene()
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
		//スコア計算 (共通関数、目標との差分から算出)
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
void renderGame1Scene(void)
{
	//Rリセット可能であることを通知
	if (status == TIMER_STATUS_DONE)
	{
		DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "Rキーでリセット", ColorWhite);
	}
	else
	{
		DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "Gキーで開始、Sキーで停止", ColorWhite);
	}

	DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_BACK_TO_TITLE, "Xボタンでタイトルに戻る", ColorWhite);

	//表示形式についてメモ：INTは整数型%dで問題なし。FLOATは実数型%fを使用
	//なお、%3.1f＝合計3桁、小数第1位以内で実数表示という意味
	DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_TARGET, ColorWhite, "%3.1f秒でストップ！", state.RandomTgt, state.CalFrame);
	//計測終了までは倍速非公開
	if (status != TIMER_STATUS_DONE)
	{
		DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_SPEED, "ただいま：？.？倍速", ColorYellow);
	}
	else if (status == TIMER_STATUS_DONE)
	{
		DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_SPEED, ColorYellow, "ただいま：%3.1f倍速", state.CalMulti);
	}

	//フレーム値は÷FPS して表示する (FrameTmp は CalMulti 加算済みの累積フレーム)
	DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_CURRENT_TIME, ColorGreen, "現在の時間：%3.2f秒", state.FrameTmp / FPS);
	//スコアは 0〜100 の正規化達成率 (δ-1 で再設計、目標時間によらず満点 100 固定)
	DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_SCORE, ColorSkyLike, "スコア：%3.1f", state.Score);

	//ピッタリ達成時のみ "PERFECT!" を演出表示 (δ-1、スコア式は連続関数のまま、不連続段差は描画側に分離)
	if (state.IsPerfect == TRUE && status == TIMER_STATUS_DONE)
	{
		DrawString(LAYOUT_X_DEFAULT + LAYOUT_X_PERFECT_OFFSET, LAYOUT_Y_SCORE, "PERFECT!", ColorRed);
	}
}

//	シーン終了時の後処理
void releaseGame1Scene(void)
{
}

// 当り判定コールバック 　　　ここでは要素を削除しないこと！！
void  Game1SceneCollideCallback(int nSrc, int nTarget, int nCollideID)
{
}