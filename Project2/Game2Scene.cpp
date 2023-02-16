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

#define MENU_MAX_G 2
SCENE_NO menu[MENU_MAX_G] = { SCENE_GAME1, SCENE_GAME2 };
//本来→SCENE_NO menu[MENU_MAX] = { SCENE_GAME1, SCENE_GAME2, SCENE_GAME3 };
char* menuList2[3] = { "挑む側","挑まれる側","" };
//選択されたゲームを表すメニュー番号の初期化（menuの添え字）
static int selectedGame2 = 0;

int startfont2;


//外部定義(GameMain.cppにて宣言)+変数シードに使ったミリ秒データ
extern int Input, EdgeInput, rdSeed;

//乱数用変数を宣言：INT型は目標時間。FLOAT型は倍速値
float RandomTgt2, CalFrame2;
float RandomMtp2, CalMulti2;
void targetTimeSet2();
//ネットワーク対戦用の関数も追加する
int netBattle(float score);

//計測用変数、記録用変数を宣言
float FrameTmp2 = 0.0;
float ScMulti2, Score2 = 0.0;

//受信したスコアを格納する変数
char rcScore[256];

//状態遷移マネージメント変数
//0=目標時間と倍速値セット前、1=目標時間と倍速値セット完了、スタート待ち
//2=計測中、3=計測完了、スコア加算OK（ここまでは仮）
//4=ネット対戦の送受信を選ばせる
int status2 = 4;
//ネットワーク対戦用のステータス変数
//0=初期状態、1=接続待機モード、2=受信側モード？
int netStatus = 0;
//あなたは挑まれるのか、挑むのか（受信か、送信か）
//0=挑む側(先に送信するクライアント)、1=挑まれる側（受信するサーバー）
//2=未選択状態
int sideSelect = 2;

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
	status2 = 4;

	//メニュー関係の初期化
	SetFontSize(32);
	ChangeFontType(DX_FONTTYPE_ANTIALIASING_EDGE_8X8);

	selectedGame2 = 0;

	return TRUE;

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
int netBattle(float score)
{
	//ネットワーク処理中であるかを示す変数
//処理が完了したら0に変更する
	int cFunc = 1;

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
	//char		rcScore[256];	//受け取ったスコアを格納する
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

	//これより、送受信どちらかを選んだかにより処理が変化する
	switch (sideSelect)
	{
		//こちらが送信側の場合
		case 0:
			//相手プログラムとの接続処理を行う
			LPHOSTENT	lpHostEntry;
			SOCKET		s;
			SOCKADDR_IN saSv;
			int			nRet;
			int			cnt = 1;
			short		nPort = 3500;  //通信に使用するポート番号
			char		szBuf[256];

			//接続先をローカルホストに限定する
			char szServer[128] = "localhost";

			MyOutputDebugString(_T("\n %sの確認中です…"), szServer);
			lpHostEntry = gethostbyname(szServer);
			if (lpHostEntry == NULL) return FALSE;  //接続先の指定エラーを返す
			MyOutputDebugString("OK!\n");

			//ソケットの作成処理
			s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

			saSv.sin_family = AF_INET;
			saSv.sin_addr = *((LPIN_ADDR)*lpHostEntry->h_addr_list);
			saSv.sin_port = htons(nPort);
			nRet = connect(s, (LPSOCKADDR)&saSv, sizeof(struct sockaddr));  
			//↑で相手サーバーに接続する

			//メモ：浮動小数点から文字列に変換する関数
			//使い方：(書き込み先文字列, 文字列サイズ, 書式指定文字, 変換元引数)
			//この関数で、ここでは自分のスコアを変換し書き込む
			snprintf(szBuf, 256, "%f", Score2);

			//送信する
			nRet = send(remS, szBuf, strlen(szBuf), 0);

			break;
		//こちらが受信側の場合
		case 1:
			//Connect要求が来るまで待機
			nRet = listen(lisS, SOMAXCONN);
			MyOutputDebugString("接続待機状態に入ります…");

			//受信を承諾する
			remS = accept(lisS, NULL, NULL);
			MyOutputDebugString("接続完了しました\n");
			MyOutputDebugString("相手を待っています…\n");

			//受信の繰り返し
			while (szBuf != NULL)
			{
				memset(szBuf, 0, sizeof(szBuf));
				//受信プロセス
				nRet = recv(remS, szBuf, sizeof(szBuf), 0);
				if (nRet == SOCKET_ERROR)
				{
					closesocket(lisS);
					closesocket(remS);
					return 0;
				}
				//受信内容を表示する
				MyOutputDebugString(_T("%ld>%s\n"), cnt++, szBuf);

				//受信した相手のスコアを格納する
				for (int i = 0; i < 256; i++)
				{
					rcScore[i] = szBuf[i];
				}

				//終了のキーワードを判別する
				//if (strcmp(szBuf, "byebye") == 0)break;

				//送信データを入力させる
				//printf("入力>>");
				//scanf("%s", szBuf);

				//メモ：浮動小数点から文字列に変換する関数
				//使い方：(書き込み先文字列, 文字列サイズ, 書式指定文字, 変換元引数)
				//この関数で、ここでは自分のスコアを変換し書き込む
				snprintf(szBuf, 256, "%f", Score2);

				//送信する
				nRet = send(remS, szBuf, strlen(szBuf), 0);

				//この部分にbyebye切断コードを入れるか？一旦保留で。

				if (nRet == SOCKET_ERROR)break;
				if (strcmp(szBuf, "byebye") == 0)break;
			}
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
			//自分のスコアを送信する
			netBattle(Score2);
		}

		break;

	case 4:		//まず送信者か、受信者かを選択してもらう
		//７メニュー項目の選択
		//７(1) ①新たに↑が押されたら、
		if ((EdgeInput & PAD_INPUT_UP)) 
		{
			//７(1) ②１つ上のメニュー項目が選択されたとする。
			//      　ただし、それより上のメニュー項目がないときは、最下段のメニュー項目が選択されたとする
			if (--selectedGame2 < 0) 
			{
				selectedGame2 = MENU_MAX_G - 1;
			}
		}

		//７(2) ①新たに↓が押されたら、
		if ((EdgeInput & PAD_INPUT_DOWN))
		{
			//７(2) ②１つ下のメニュー項目が選択されたとする。。
			//      　ただし、それより下のメニュー項目がないときは、最上段のメニュー項目が選択されたとする
			if (++selectedGame2 >= MENU_MAX_G) 
			{
				selectedGame2 = 0;
			}
		}

		//７(3) 新たにボタン１が押されたら決定
		if ((EdgeInput & PAD_INPUT_1)) 
		{
			//0が選ばれたら送信者、1が選ばれたら受信者
			//この後、ステータスをゲーム本編に移す
			if (selectedGame2 == 0)sideSelect = 0;
			else if (selectedGame2 == 1)sideSelect = 1;
			status2 = 0;
		}
		if(sideSelect != 2)break;
	}
}

//	レンダリング処理
void renderGame2Scene(void)
{
	//挑むか、挑まれるかを選択させる
	switch (sideSelect)
	{

	}
	
	//Rリセット可能であることを通知
	if (status2 == 3)
	{
		DrawString(30, 50, "Rキーでリセット、Nキーでネット対戦", ColorWhite2);
	}
	else
	{
		DrawString(30, 50, "Gキーで開始、Sキーで停止", ColorWhite2);
	}

	//ネット対戦を終えた場合：勝敗を判断し、表示する
	if (netStatus == 2)
	{
		//受信したスコアを浮動小数点数に戻す
		float scJudge = strtof(rcScore, NULL);

		scJudge = floor(scJudge);
		float Score2J = floor(Score2);

		//自分のスコアより少ない場合
		if (scJudge < Score2J)
		{
			DrawString(30, 50, "あなたの勝利です！", ColorSkyLike2);
		}
		//同じ場合
		else if (scJudge == Score2J)
		{
			DrawString(30, 50, "なんと…引き分け！", ColorYellow2);
		}
		//自分のスコアより多い場合
		else if (scJudge > Score2J)
		{
			DrawString(30, 50, "あなたの敗北です…", ColorBlue2);
		}
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
		//ネット上でのスコア交換が終わったならば、相手のスコアに表示を変える
		if (netStatus == 2)
		{
			DrawFormatString(30, 250, ColorRed2, "相手のスコアは%s", rcScore);
		}
	}

	//フレーム値は÷60して表示すること！
	DrawFormatString(30, 350, ColorGreen2, "現在の時間：%3.2f秒", FrameTmp2 / 60);
	DrawFormatString(30, 400, ColorSkyLike2, "スコア：%3.1f点", Score2);
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