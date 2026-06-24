#include "GameMain.h"
#include "GameSceneMain.h"
#include "Game2Scene.h"
#include <math.h>
#include <string.h>
#include <winsock.h>
#pragma comment(lib, "wsock32.lib")
#define _CRT_SECURE_NO_WARNINGS

//MyOutputDebugString マクロは GameMain.h に昇格済み (Game2Scene 専用ではなくなったため)

#define MENU_MAX_G 2
char* menuList2[3] = { "挑む側","挑まれる側","" };
//選択されたゲームを表すメニュー番号の初期化（menuの添え字）
static int selectedGame2 = 0;

//外部定義(GameMain.cppにて宣言)+変数シードに使ったミリ秒データ
extern int Input, EdgeInput, rdSeed;

//計測状態をまとめた構造体 (詳細は GameSceneMain.h の TIMER_STATE)
//static でファイルスコープに限定 (Game1/Game2 の state は別物として独立させる)
static TIMER_STATE state = {};
//ネットワーク対戦用の関数プロトタイプ
int netBattle(float score);

//受信したスコアを格納する変数
char rcScore[256];

//状態遷移マネージメント変数 (詳細は下記 GAME2_STATE enum 参照)
//0-3 は Game1Scene.cpp の TIMER_STATUS とミラー、4 は Game2 専用 SIDE_SELECT
//TIMER_STATUS を共有 enum として汚さないため別型として定義している
typedef enum _GAME2_STATE {
	GAME2_STATE_INIT = 0,			// 目標時間と倍速値セット前 (TIMER_STATUS_INIT 相当)
	GAME2_STATE_READY,				// セット完了、スタート待ち (TIMER_STATUS_READY 相当)
	GAME2_STATE_MEASURING,			// 計測中 (TIMER_STATUS_MEASURING 相当)
	GAME2_STATE_DONE,				// 計測完了、スコア加算OK (TIMER_STATUS_DONE 相当)
	GAME2_STATE_SIDE_SELECT			// Game2 専用: ネット対戦の送受信側を選ぶメニュー
} GAME2_STATE;

GAME2_STATE status2 = GAME2_STATE_SIDE_SELECT;
//ネットワーク対戦用のステータス変数
//0=初期状態、1=接続待機モード、2=受信側モード？
int netStatus = 0;
//ネットワーク対戦の役割 (Game2 専用、詳細は下記 NET_SIDE enum 参照)
//現状は Game2Scene 内のみで使用、将来別シーンで再利用する場合は GameSceneMain.h へ昇格を検討
typedef enum _NET_SIDE {
	NET_SIDE_CHALLENGER = 0,		// 挑む側 (先に送信するクライアント、socket→connect→send)
	NET_SIDE_DEFENDER = 1,			// 挑まれる側 (受信するサーバー、socket→bind→listen→accept→recv)
	NET_SIDE_UNSELECTED = 2			// 未選択 (シーン入場直後の初期状態)
} NET_SIDE;

NET_SIDE sideSelect = NET_SIDE_UNSELECTED;

// シーン開始前の初期化を行う
BOOL initGame2Scene(void)
{
	//入るときは必ずリセット
	status2 = GAME2_STATE_SIDE_SELECT;
	sideSelect = NET_SIDE_UNSELECTED;
	netStatus = 0;

	//WinSock 初期化 (γ-2-c: シーン寿命に合わせて init/release ペアリング、netBattle 内呼び出しを移管)
	//失敗時は BOOL FALSE を返し、initCurrentScene のフォールバック (β-D-3) で SCENE_MENU に戻る
	WORD wVerReq = MAKEWORD(1, 1);
	WSADATA wsaData;
	int nRet = WSAStartup(wVerReq, &wsaData);
	if (nRet != 0 || wsaData.wVersion != wVerReq) {
		MyOutputDebugString(_T("WSAStartup failed (nRet=%d, version=0x%x)\n"), nRet, wsaData.wVersion);
		return FALSE;
	}
	MyOutputDebugString("\n簡易通信プログラム 起動準備：第1段階完了");

	//メニュー関係の初期化
	SetFontSize(FONT_SIZE_DEFAULT);
	ChangeFontType(DX_FONTTYPE_ANTIALIASING_EDGE_8X8);

	selectedGame2 = 0;

	return TRUE;
}

//ネットワーク対戦部分：TCPを応用した通信を行う (γ-2-b で全面整理、双方向交換実装)
//設定するポート番号は「3500」
//WSAStartup/WSACleanup は initGame2Scene/releaseGame2Scene に移管済み (γ-2-c)
int netBattle(float score)
{
	int     nRet;
	int     cnt = 1;
	char    szBuf[256];
	SOCKET  remS = INVALID_SOCKET;	//受信用ソケット (両 case で使用)

	//共通: 自ホスト名取得 (デバッグ出力用、Challenger/Defender 両方で実行)
	nRet = gethostname(szBuf, sizeof(szBuf));
	if (nRet == SOCKET_ERROR)
	{
		return 0;
	}
	LPHOSTENT lpInAddr = gethostbyname(szBuf);
	MyOutputDebugString(_T("\n PC info: %s or %s\n"),
		szBuf, inet_ntoa(*(LPIN_ADDR)*lpInAddr->h_addr_list));

	//これより、送受信どちらかを選んだかにより処理が変化する
	switch (sideSelect)
	{
		//こちらが送信側 (Challenger): socket → connect → send → recv → netStatus=2 → closesocket
		case NET_SIDE_CHALLENGER: {
			LPHOSTENT	lpHostEntry;
			SOCKET		s;
			SOCKADDR_IN saCl;
			short		nPort = 3500;	//通信に使用するポート番号
			//接続先をローカルホストに限定する
			char		szServer[128] = "localhost";

			MyOutputDebugString(_T("\n %sの確認中です…"), szServer);
			lpHostEntry = gethostbyname(szServer);
			if (lpHostEntry == NULL) return FALSE;	//接続先の指定エラーを返す
			MyOutputDebugString("OK!\n");

			//ソケットの作成 + 相手サーバーに接続
			s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			saCl.sin_family = AF_INET;
			saCl.sin_addr = *((LPIN_ADDR)*lpHostEntry->h_addr_list);
			saCl.sin_port = htons(nPort);
			nRet = connect(s, (LPSOCKADDR)&saCl, sizeof(struct sockaddr));

			//自スコアを文字列化して送信 (γ-2-b バグ #1 修正: remS → s)
			snprintf(szBuf, 256, "%f", state.Score);
			nRet = send(s, szBuf, strlen(szBuf), 0);
			MyOutputDebugString(_T("送信: %s\n"), szBuf);

			//相手スコア受信 (γ-2-b バグ #4: 双方向交換のため Challenger に recv 追加)
			memset(szBuf, 0, sizeof(szBuf));
			nRet = recv(s, szBuf, sizeof(szBuf), 0);
			if (nRet != SOCKET_ERROR)
			{
				//受信した相手のスコアを格納する
				for (int i = 0; i < 256; i++) { rcScore[i] = szBuf[i]; }
				//γ-2-b バグ #3: netStatus 遷移追加 (受信完了 = 勝敗判定可能)
				netStatus = 2;
				MyOutputDebugString(_T("受信: %s\n"), szBuf);
			}

			//γ-2-c: closesocket でリソース解放 (旧コードは漏れていた)
			closesocket(s);
			break;
		}
		//こちらが受信側 (Defender): socket → bind → listen → accept → recv → send → netStatus=2 → closesocket
		case NET_SIDE_DEFENDER: {
			SOCKET		lisS = INVALID_SOCKET;	//listen 用ソケット (Defender 専用)
			SOCKADDR_IN saSv;
			short		nPort = 3500;

			//γ-2-b 構造整理 #5: socket+bind+listen+accept を Defender case 内に集約
			//(Challenger では使わないリソースなので関数頭から移動)
			lisS = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			saSv.sin_family = AF_INET;
			saSv.sin_addr.s_addr = INADDR_ANY;	//Let Winsock supply address
			saSv.sin_port = htons(nPort);
			nRet = bind(lisS, (LPSOCKADDR)&saSv, sizeof(struct sockaddr));

			//Connect 要求が来るまで待機 (同期 listen/accept、フリーズ仕様)
			nRet = listen(lisS, SOMAXCONN);
			MyOutputDebugString("接続待機状態に入ります…");

			//受信を承諾する (相手の connect まで accept ブロック)
			remS = accept(lisS, NULL, NULL);
			MyOutputDebugString("接続完了しました\n");

			//相手スコア受信 (γ-2-b バグ #2 修正: while (szBuf != NULL) 無限ループ削除、1 回交換)
			memset(szBuf, 0, sizeof(szBuf));
			nRet = recv(remS, szBuf, sizeof(szBuf), 0);
			if (nRet == SOCKET_ERROR)
			{
				closesocket(lisS);
				closesocket(remS);
				return 0;
			}
			MyOutputDebugString(_T("%ld>%s\n"), cnt++, szBuf);

			//受信した相手のスコアを格納する
			for (int i = 0; i < 256; i++) { rcScore[i] = szBuf[i]; }

			//自スコアを文字列化して送信 (γ-2-b バグ #4: 双方向交換のため Defender に send 追加)
			snprintf(szBuf, 256, "%f", state.Score);
			nRet = send(remS, szBuf, strlen(szBuf), 0);
			if (nRet != SOCKET_ERROR)
			{
				//γ-2-b バグ #3: netStatus 遷移追加 (送受信完了 = 勝敗判定可能)
				netStatus = 2;
				MyOutputDebugString(_T("送信: %s\n"), szBuf);
			}

			//γ-2-c: closesocket でリソース解放 (lisS は Defender 専用、両方解放)
			closesocket(remS);
			closesocket(lisS);
			break;
		}
	}

	return 0;
}

//	フレーム処理
void moveGame2Scene()
{
	switch (status2)
	{
	case GAME2_STATE_INIT://ここではプレイヤーの入力を待つ
		//タイマーを初期化
		state.FrameTmp = 0.0;
		state.Score = 0.0;
		
		//目標秒数、倍速値の設定関数を呼び出す。
		//その後、ステータス番号を変更する
		timerReset(&state);
		status2 = GAME2_STATE_READY;
		break;

	case GAME2_STATE_READY:

		//Gキー（GO）で計測開始
		if (CheckHitKey(KEY_INPUT_G) == 1)
		{
			if (status2 == GAME2_STATE_READY)
			{
				status2 = GAME2_STATE_MEASURING;
			}
		}

		//Xキーでタイトルに戻す
		if (CheckHitKey(KEY_INPUT_X) == 1)
		{
			changeScene(SCENE_MENU);
		}

		break;

	case GAME2_STATE_MEASURING://ここでは計測処理を行う
		//フレーム加算処理
		state.FrameTmp += state.CalMulti;

		//Sキー（STOP）で計測終了
		if (CheckHitKey(KEY_INPUT_S) == 1)
		{
			if (status2 == GAME2_STATE_MEASURING)
			{
				status2 = GAME2_STATE_DONE;
			}
		}

		//Xキーでタイトルに戻る
		if (CheckHitKey(KEY_INPUT_X) == 1)
		{
			changeScene(SCENE_MENU);
		}

		break;

	case GAME2_STATE_DONE://計測が終了したあとの処理を行う
		//スコア計算 (共通関数、目標との差分から算出)
		timerCalcScore(&state);

		//Rキーで状態リセット
		if (CheckHitKey(KEY_INPUT_R) == 1)
		{
			status2 = GAME2_STATE_INIT;
		}

		//Xキーでタイトルに戻す
		if (CheckHitKey(KEY_INPUT_X) == 1)
		{
			changeScene(SCENE_MENU);
		}

		//Nキーでネットワーク対戦をする
		if (CheckHitKey(KEY_INPUT_N) == 1)
		{
			//自分のスコアを送信する
			netBattle(state.Score);
		}

		break;

	case GAME2_STATE_SIDE_SELECT:		//まず送信者か、受信者かを選択してもらう
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
			if (selectedGame2 == 0)sideSelect = NET_SIDE_CHALLENGER;
			else if (selectedGame2 == 1)sideSelect = NET_SIDE_DEFENDER;
			status2 = GAME2_STATE_INIT;
		}

		//Xキーでタイトルに戻る (γ-2-a: SIDE_SELECT 中の脱出手段、ESC 以外を提供)
		if (CheckHitKey(KEY_INPUT_X) == 1)
		{
			changeScene(SCENE_MENU);
		}

		break;
	}
}

//	レンダリング処理
void renderGame2Scene(void)
{
	//SIDE_SELECT 状態: 役割選択メニュー描画 (γ-2-a、MenuScene 流の上下選択、PAD_INPUT_1 で決定)
	//ここで描画を完結させ、以降の計測関連描画はスキップ (return)
	if (status2 == GAME2_STATE_SIDE_SELECT)
	{
		DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "ネット対戦の役割を選んでください", ColorWhite);
		DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_BACK_TO_TITLE, "Zキーで決定、Xキーでタイトルに戻る", ColorWhite);

		//選択肢を MenuScene 流で描画 (selectedGame2 に応じた色分け、ColorRed=選択中)
		int x = 195, y = 200, gapY = 60;
		for (int i = 0; i < MENU_MAX_G; i++, y += gapY) {
			if (i == selectedGame2) {
				DrawString(x, y, menuList2[i], ColorRed);
			}
			else {
				DrawString(x, y, menuList2[i], ColorWhite);
			}
		}
		return;	//SIDE_SELECT 中は計測関連描画スキップ
	}

	//Rリセット可能であることを通知
	if (status2 == GAME2_STATE_DONE)
	{
		DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "Rキーでリセット、Nキーでネット対戦", ColorWhite);
	}
	else
	{
		DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "Gキーで開始、Sキーで停止", ColorWhite);
	}

	//ネット対戦を終えた場合：勝敗を判断し、表示する
	if (netStatus == 2)
	{
		//受信したスコアを浮動小数点数に戻す
		float scJudge = strtof(rcScore, NULL);

		scJudge = floorf(scJudge);
		float Score2J = floorf(state.Score);

		//自分のスコアより少ない場合
		if (scJudge < Score2J)
		{
			DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "あなたの勝利です！", ColorSkyLike);
		}
		//同じ場合
		else if (scJudge == Score2J)
		{
			DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "なんと…引き分け！", ColorYellow);
		}
		//自分のスコアより多い場合
		else if (scJudge > Score2J)
		{
			DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "あなたの敗北です…", ColorBlue);
		}
	}

	DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_BACK_TO_TITLE, "Xボタンでタイトルに戻る", ColorWhite);

	//表示形式についてメモ：INTは整数型%dで問題なし。FLOATは実数型%fを使用
	//なお、%3.1f＝合計3桁、小数第1位以内で実数表示という意味
	DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_TARGET, ColorWhite, "%3.1f秒でストップ！", state.RandomTgt, state.CalFrame);
	//計測終了までは倍速非公開
	if (status2 != GAME2_STATE_DONE)
	{
		DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_SPEED, "ただいま：？.？倍速", ColorYellow);
	}
	else if (status2 == GAME2_STATE_DONE)
	{
		DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_SPEED, ColorYellow, "ただいま：%3.1f倍速", state.CalMulti);
		//ネット上でのスコア交換が終わったならば、相手のスコアに表示を変える
		if (netStatus == 2)
		{
			DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_SPEED, ColorRed, "相手のスコアは%s", rcScore);
		}
	}

	//フレーム値は÷FPS して表示する (FrameTmp は CalMulti 加算済みの累積フレーム)
	DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_CURRENT_TIME, ColorGreen, "現在の時間：%3.2f秒", state.FrameTmp / FPS);
	DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_SCORE, ColorSkyLike, "スコア：%3.1f点", state.Score);
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