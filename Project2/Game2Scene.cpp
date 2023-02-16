#include "GameMain.h"
#include "GameSceneMain.h"
#include "Game2Scene.h"
#include <math.h>
#include <string.h>

#include <stdio.h>
#include <Windows.h>
#include <iostream>
#include <winsock.h>
#pragma comment(lib, "wsock32.lib")
#define _CRT_SECURE_NO_WARNINGS

//デバッグ文字列出力マクロ
//使用方法：
//MyOutputDebugString(_T"出力したい文字 %s", 指定する対応変数);
//※_Tは変数を混ぜたい場合にのみ使用する
#ifdef _DEBUG
#   define MyOutputDebugString( str, ... ) \
      { \
        TCHAR c[256]; \
        _stprintf( c, str, __VA_ARGS__ ); \
        OutputDebugString( c ); \
      }
#else
#    define MyOutputDebugString( str, ... ) // 空実装
#endif

//外部定義(GameMain.cppにて宣言)+変数シードに使ったミリ秒データ
extern int Input, EdgeInput, rdSeed;

//乱数用変数を宣言：INT型は目標時間。FLOAT型は倍速値
float RandomTgt2, CalFrame2;
float RandomMtp2, CalMulti2;
void targetTimeSet2();
//ネットワーク対戦用の関数も追加する
int netBattle(int score);

//計測用変数、記録用変数を宣言
float FrameTmp2 = 0.0;
float ScMulti2, Score2 = 0.0;

//状態遷移マネージメント変数
//0=目標時間と倍速値セット前、1=目標時間と倍速値セット完了、スタート待ち
//2=計測中、3=計測完了、スコア加算OK（ここまでは仮）
int status2 = 0;
//ネットワーク対戦用のステータス変数
//0=初期状態、1=接続待機モード、2=受信側モード？
int netStatus = 0;

//色変数セット
unsigned int ColorWhite2 = GetColor(255, 255, 255);
unsigned int ColorRed2 = GetColor(255, 0, 0);
unsigned int ColorGreen2 = GetColor(0, 255, 0);
unsigned int ColorBlue2 = GetColor(0, 0, 255);
unsigned int ColorYellow2 = GetColor(255, 255, 0);
unsigned int ColorPurple2 = GetColor(255, 0, 255);
unsigned int ColorSkyLike2 = GetColor(0, 255, 255);

// シーン開始前の初期化を行う
BOOL initGame2Scene(void)
{
	//入るときは必ずリセット
	status2 = 0;
	return TRUE;
}

void targetTimeSet2()
{
	//目標時間をセットする（乱数で取得、フレーム換算して計算の準備）
	RandomTgt2 = GetRand(19) + 1;
	CalFrame2 = RandomTgt2 * 60;

	//倍速値をセットする（乱数で取得、フレーム換算して計算の準備）
	RandomMtp2 = GetRand(39) + 10;
	CalMulti2 = RandomMtp2 / 10;
}

//ネットワーク対戦部分：TCPを応用した通信を行う
//設定するポート番号は「3500」
int netBattle(int score)
{
	//ココから初期化処理
	WORD    wVerReq = MAKEWORD(1, 1);
	WSADATA wsaData;
	int     nRet;
	nRet = WSAStartup(wVerReq, &wsaData);

	if (wsaData.wVersion != wVerReq)
	{
		fprintf(stderr, "\n Wrong Version\n");
		return -1;
	}
	MyOutputDebugString("\n簡易通信プログラム 起動準備：第1段階完了");

	nRet = 1;
	int nLen, cnt = 1;
	char        szBuf[256];
	SOCKET      lisS, remS;
	SOCKADDR_IN saSv;
	//ソケット定義
	lisS = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	saSv.sin_family = AF_INET;
	//Let Winsock supply address
	saSv.sin_addr.s_addr = INADDR_ANY;
	//Use Port from command line
	saSv.sin_port = htons(3500);
	nRet = bind(lisS, (LPSOCKADDR)&saSv, sizeof(struct sockaddr));

	nLen = sizeof(SOCKADDR);
	nRet = gethostname(szBuf, sizeof(szBuf));
	if (nRet == SOCKET_ERROR)
	{
		closesocket(lisS);
		return 0;
	}

	LPHOSTENT lpInAddr = gethostbyname(szBuf);

	//デバッグ出力
	MyOutputDebugString(_T("\nこのパソコンは\n、%s または %s です\n"),
		szBuf, inet_ntoa(*(LPIN_ADDR)*lpInAddr->h_addr_list));

	//Connect要求が来るまで待機
	nRet = listen(lisS, SOMAXCONN);
	MyOutputDebugString("接続待機状態に入ります…");
	//ココまで初期化処理

	switch (netStatus)
	{
		default:
			return TRUE;
			break;
	}
}

//	フレーム処理
void moveGame2Scene()
{
	switch (status2)
	{
	case 0://ここではプレイヤーの入力を待つ
		//タイマーを初期化
		FrameTmp2 = 0.0;
		Score2 = 0.0;
		
		//目標秒数、倍速値の設定関数を呼び出す。
		//その後、ステータス番号を変更する
		targetTimeSet2();
		status2 = 1;
		break;

	case 1:

		//Gキー（GO）で計測開始
		if (CheckHitKey(KEY_INPUT_G) == 1)
		{
			if (status2 == 1)
			{
				status2 = 2;
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
		FrameTmp2 += CalMulti2;

		//Sキー（STOP）で計測終了
		if (CheckHitKey(KEY_INPUT_S) == 1)
		{
			if (status2 == 2)
			{
				status2 = 3;
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
		if (CalFrame2 > FrameTmp2)//目標より早いパターン
		{
			//誤差が増すほどスコアから多く引かれる
			ScMulti2 = FrameTmp2 - CalFrame2;
			Score2 = CalFrame2 - (-ScMulti2);
		}
		else if (CalFrame2 < FrameTmp2)//目標より遅いパターン
		{
			//誤差が増すほどスコアから多く引かれる
			ScMulti2 = CalFrame2 - FrameTmp2;
			Score2 = CalFrame2 - (-ScMulti2);
		}
		else if (CalFrame2 == FrameTmp2)//目標ピッタリ！？
		{
			//フレーム単位で合わせるとは油断ならぬ。ボーナス！
			Score2 = CalFrame2 * 1.25;
		}

		//Rキーで状態リセット
		if (CheckHitKey(KEY_INPUT_R) == 1)
		{
			status2 = 0;
		}

		//Xキーでタイトルに戻す
		if ((EdgeInput & KEY_INPUT_X))
		{
			changeScene(SCENE_MENU);
		}

		//Nキーでネットワーク対戦をする
		if (CheckHitKey(KEY_INPUT_N) == 1)
		{
			//ポート番号を3500に指定し、スコアも一緒に送信する
			netBattle(Score2);
		}

		break;
	}
}

//	レンダリング処理
void renderGame2Scene(void)
{
	//Rリセット可能であることを通知
	if (status2 == 3)
	{
		DrawString(30, 50, "Rキーでリセット、Nキーでネット対戦", ColorWhite2);
	}
	else
	{
		DrawString(30, 50, "Gキーで開始、Sキーで停止", ColorWhite2);
	}

	DrawString(30, 100, "Xボタンでタイトルに戻る", ColorWhite2);

	//表示形式についてメモ：INTは整数型%dで問題なし。FLOATは実数型%fを使用
	//なお、%3.1f＝合計3桁、小数第1位以内で実数表示という意味
	DrawFormatString(30, 200, ColorWhite2, "%3.1f秒でストップ！", RandomTgt2, CalFrame2);
	//計測終了までは倍速非公開
	if (status2 != 3)
	{
		DrawString(30, 250, "ただいま：？.？倍速", ColorYellow2);
	}
	else if (status2 == 3)
	{
		DrawFormatString(30, 250, ColorYellow2, "ただいま：%3.1f倍速", CalMulti2);
	}

	//フレーム値は÷60して表示すること！
	DrawFormatString(30, 350, ColorGreen2, "現在の時間：%3.2f秒", FrameTmp2 / 60);
	DrawFormatString(30, 400, ColorSkyLike2, "スコア：%3.1f", Score2);
}

//	シーン終了時の後処理
void releaseGame2Scene(void)
{
	WSACleanup();
}

// 当り判定コールバック 　　　ここでは要素を削除しないこと！！
void  Game2SceneCollideCallback(int nSrc, int nTarget, int nCollideID)
{
}