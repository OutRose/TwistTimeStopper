#include "GameMain.h"
#include "GameSceneMain.h"
#include "Game1Scene.h"
#include <math.h>
#include <string.h>

//外部定義(GameMain.cppにて宣言)+変数シードに使ったミリ秒データ
extern int Input, EdgeInput, rdSeed;

//乱数用変数を宣言：INT型は目標時間。FLOAT型は倍速値
float RandomTgt, CalFrame;
float RandomMtp, CalMulti;
void targetTimeSet();

//計測用変数、記録用変数を宣言
float FrameTmp = 0.0;
float ScMulti, Score = 0.0;

//状態遷移マネージメント変数
//0=目標時間と倍速値セット前、1=目標時間と倍速値セット完了、スタート待ち
//2=計測中、3=計測完了、スコア加算OK（ここまでは仮）
int status = 0;

//色変数セット
unsigned int ColorWhite = GetColor(255, 255, 255);
unsigned int ColorRed = GetColor(255, 0, 0);
unsigned int ColorGreen = GetColor(0, 255, 0);
unsigned int ColorBlue = GetColor(0, 0, 255);
unsigned int ColorYellow = GetColor(255, 255, 0);
unsigned int ColorPurple = GetColor(255, 0, 255);
unsigned int ColorSkyLike = GetColor(0, 255, 255);

// シーン開始前の初期化を行う
BOOL initGame1Scene(void)
{
	//入るときは必ずリセット
	status = 0;
	return TRUE;
}

void targetTimeSet()
{
	//目標時間をセットする（乱数で取得、フレーム換算して計算の準備）
	RandomTgt = GetRand(19) + 1;
	CalFrame = RandomTgt * 60;

	//倍速値をセットする（乱数で取得、フレーム換算して計算の準備）
	RandomMtp = GetRand(39) + 10;
	CalMulti = RandomMtp / 10;
}

//	フレーム処理
void moveGame1Scene()
{
	switch (status)
	{
	case 0://ここではプレイヤーの入力を待つ
		//タイマーを初期化
		FrameTmp = 0.0;
		Score = 0.0;

		//目標秒数、倍速値の設定関数を呼び出す。
		//その後、ステータス番号を変更する
		targetTimeSet();
		status = 1;
		break;

	case 1:

		//Gキー（GO）で計測開始
		if (CheckHitKey(KEY_INPUT_G) == 1)
		{
			if (status == 1)
			{
				status = 2;
			}
		}

		//Xキーでタイトルに戻す
		if ((EdgeInput & KEY_INPUT_X))
		{
			changeScene(SCENE_MENU);
		}

		break;

	case 2://ここでは計測処理を行う
		//フレーム加算処理
		FrameTmp += CalMulti;

		//Sキー（STOP）で計測終了
		if (CheckHitKey(KEY_INPUT_S) == 1)
		{
			if (status == 2)
			{
				status = 3;
			}
		}

		//Xキーでタイトルに戻る
		if ((EdgeInput & KEY_INPUT_X))
		{
			changeScene(SCENE_MENU);
		}

		break;

	case 3://計測が終了したあとの処理を行う
		//スコアリング処理：目標秒フレームとスコア秒フレームを比較
		if (CalFrame > FrameTmp)//目標より早いパターン
		{
			//誤差が増すほどスコアから多く引かれる
			ScMulti = FrameTmp - CalFrame;
			Score = CalFrame - (-ScMulti);
		}
		else if (CalFrame < FrameTmp)//目標より遅いパターン
		{
			//誤差が増すほどスコアから多く引かれる
			ScMulti = CalFrame - FrameTmp;
			Score = CalFrame - (-ScMulti);
		}
		else if (CalFrame == FrameTmp)//目標ピッタリ！？
		{
			//フレーム単位で合わせるとは油断ならぬ。ボーナス！
			Score = CalFrame * 1.25;
		}

		//Rキーで状態リセット
		if (CheckHitKey(KEY_INPUT_R) == 1)
		{
			status = 0;
		}

		//Xキーでタイトルに戻す
		if ((EdgeInput & KEY_INPUT_X))
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
	if (status == 3)
	{
		DrawString(30, 50, "Rキーでリセット", ColorWhite);
	}
	else
	{
		DrawString(30, 50, "Gキーで開始、Sキーで停止", ColorWhite);
	}

	DrawString(30, 100, "Xボタンでタイトルに戻る", ColorWhite);

	//表示形式についてメモ：INTは整数型%dで問題なし。FLOATは実数型%fを使用
	//なお、%3.1f＝合計3桁、小数第1位以内で実数表示という意味
	DrawFormatString(30, 200, ColorWhite, "%3.1f秒でストップ！", RandomTgt, CalFrame);
	//計測終了までは倍速非公開
	if (status != 3)
	{
		DrawString(30, 250, "ただいま：？.？倍速", ColorYellow);
	}
	else if (status == 3)
	{
		DrawFormatString(30, 250, ColorYellow, "ただいま：%3.1f倍速", CalMulti);
	}

	//フレーム値は÷60して表示すること！
	DrawFormatString(30, 350, ColorGreen, "現在の時間：%3.2f秒", FrameTmp / 60);
	DrawFormatString(30, 400, ColorSkyLike, "スコア：%3.1f", Score);
}

//	シーン終了時の後処理
void releaseGame1Scene(void)
{
}

// 当り判定コールバック 　　　ここでは要素を削除しないこと！！
void  Game1SceneCollideCallback(int nSrc, int nTarget, int nCollideID)
{
}