//δ-3a: WinSock 2.2 アップグレード。winsock2.h は必ず Windows.h より前にインクルードする
//(GameMain.h → Windows.h と winsock.h の二重定義衝突を回避するため、最上段で先行 include)
#include <winsock2.h>
#include "GameMain.h"
#include "GameSceneMain.h"
#include "Game2Scene.h"
#include <math.h>
#include <string.h>
#pragma comment(lib, "ws2_32.lib")	//δ-3a: wsock32.lib (1.1) → ws2_32.lib (2.x) に切り替え
#define _CRT_SECURE_NO_WARNINGS

//MyOutputDebugString マクロは GameMain.h に昇格済み (Game2Scene 専用ではなくなったため)

//ネットワーク対戦系定数 (γ-3 で定数化、Game2 専用なのでファイルスコープに閉じる)
#define NET_PORT             3500            //通信用 TCP ポート番号
#define NET_BUF_SIZE         256             //送受信バッファサイズ (szBuf/rcScore)
#define SERVER_NAME_SIZE     128             //接続先ホスト名バッファサイズ (szServer)
#define WINSOCK_VERSION_REQ  MAKEWORD(2, 2)  //δ-3a: 要求する WinSock バージョン (2.2 へアップグレード)

//接続先ホスト (δ-3a: "localhost" リテラルを定数化、将来の任意 IP 拡張時に 1 箇所変更で済む)
#define NET_TARGET_HOST      "localhost"

//データフォーマット定数 (δ-3a、SPEC_NETWORK.md §5 準拠)
//送信: snprintf(szBuf, NET_BUF_SIZE, "%s%.2f", NET_MSG_PREFIX_SCORE, state.Score)
//受信パース: 先頭 NET_PREFIX_LEN_SCORE バイトが "SCORE:" ならスコア、"END"/"BYEBYE" なら終了通知
#define NET_MSG_PREFIX_SCORE "SCORE:"        //スコア送信のプレフィックス
#define NET_PREFIX_LEN_SCORE 6               //strlen("SCORE:")、strncmp 比較長さ用
#define NET_MSG_END          "END"           //通常終了通知 (δ-3b で state machine と連動使用)
#define NET_MSG_BYEBYE       "BYEBYE"        //異常終了通知 (δ-3b で state machine と連動使用)

#define MENU_MAX_G 2
//SIDE_SELECT メニュー項目 (δ-2: 未使用 3 要素目 "" を削除し MENU_MAX_G と要素数を一致)
char* menuList2[MENU_MAX_G] = { "挑む側", "挑まれる側" };
//選択されたゲームを表すメニュー番号の初期化（menuの添え字）
static int selectedGame2 = 0;

//外部定義(GameMain.cppにて宣言)+変数シードに使ったミリ秒データ
extern int Input, EdgeInput, rdSeed;

//計測状態をまとめた構造体 (詳細は GameSceneMain.h の TIMER_STATE)
//static でファイルスコープに限定 (Game1/Game2 の state は別物として独立させる)
static TIMER_STATE state = {};

//受信したスコアを格納する変数 (δ-2: 外部参照ゼロ確認済、static で Game2Scene 内に閉じる)
static char rcScore[NET_BUF_SIZE];

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

//ネットワーク対戦のステータス (γ-3 で enum 化、δ-3a で 7 値に拡張、SPEC_NETWORK.md §9 準拠)
//δ-3a 時点では従来の同期 netBattle 内で INIT → EXCHANGED の 2 値遷移のみ使用。
//残り 5 値 (CONNECTED/SENT/RECEIVED/ENDING/END) は δ-3b の non-blocking + state machine 化で利用予定。
typedef enum _NET_STATUS {
	NET_STATUS_INIT      = 0,	//接続未確立 (γ-3 既存、シーン入場直後の状態)
	NET_STATUS_CONNECTED = 1,	//接続確立済み、両者未送信未受信 (γ-3 WAITING の意味再定義、δ-3b で使用開始)
	NET_STATUS_SENT      = 2,	//自分は送信完了、相手から未受信 (δ-3b で使用開始)
	NET_STATUS_RECEIVED  = 3,	//相手から受信完了、自分は未送信 (δ-3b で使用開始、γ-3 とは値が異なる)
	NET_STATUS_EXCHANGED = 4,	//双方の送受信完了、勝敗判定可能 (γ-3 RECEIVED=2 の意味再割り当て先)
	NET_STATUS_ENDING    = 5,	//終了通知 (END/BYEBYE) の送受信中 (δ-3b で使用開始)
	NET_STATUS_END       = 6	//切断完了、メニュー復帰待ち (δ-3b で使用開始)
} NET_STATUS;

NET_STATUS netStatus = NET_STATUS_INIT;

//ネットワーク対戦の役割 (Game2 専用、詳細は下記 SIDE_SELECT enum 参照、δ-3a で技術用語に統一)
//現状は Game2Scene 内のみで使用、将来別シーンで再利用する場合は GameSceneMain.h へ昇格を検討
typedef enum _SIDE_SELECT {
	SIDE_SELECT_CLIENT = 0,			// 送信側 = クライアント (後から接続、socket→connect→send、γ-3 までの CHALLENGER 相当)
	SIDE_SELECT_SERVER = 1,			// 受信側 = サーバー (先に接続待機、socket→bind→listen→accept→recv、γ-3 までの DEFENDER 相当)
	SIDE_SELECT_NONE   = 2			// 未選択 (シーン入場直後の初期状態)
} SIDE_SELECT;

SIDE_SELECT sideSelect = SIDE_SELECT_NONE;

// シーン開始前の初期化を行う
BOOL initGame2Scene(void)
{
	//入るときは必ずリセット
	status2 = GAME2_STATE_SIDE_SELECT;
	sideSelect = SIDE_SELECT_NONE;
	netStatus = NET_STATUS_INIT;

	//WinSock 初期化 (γ-2-c: シーン寿命に合わせて init/release ペアリング、netBattle 内呼び出しを移管)
	//失敗時は BOOL FALSE を返し、initCurrentScene のフォールバック (β-D-3) で SCENE_MENU に戻る
	WORD wVerReq = WINSOCK_VERSION_REQ;
	WSADATA wsaData;
	int nRet = WSAStartup(wVerReq, &wsaData);
	if (nRet != 0 || wsaData.wVersion != WINSOCK_VERSION_REQ) {
		MyOutputDebugString(_T("WSAStartup failed (nRet=%d, version=0x%x)\n"), nRet, wsaData.wVersion);
		return FALSE;
	}
	MyOutputDebugString("\nWSAStartup 完了");

	//メニュー関係の初期化
	SetFontSize(FONT_SIZE_DEFAULT);
	ChangeFontType(DX_FONTTYPE_ANTIALIASING_EDGE_8X8);

	selectedGame2 = 0;

	return TRUE;
}

//ネットワーク対戦部分：TCPを応用した通信を行う (γ-2 で全面整理、γ-3 で全 WinSock 戻り値検査追加)
//WSAStartup/WSACleanup は initGame2Scene/releaseGame2Scene に移管済み (γ-2-c)
//戻り値: 成功 0 / 失敗 -1 (γ-3 で統一、現状 caller は無視しているが将来エラー表示の土台)
int netBattle(float score)
{
	int     nRet;
	char    szBuf[NET_BUF_SIZE];
	SOCKET  remS = INVALID_SOCKET;	//受信用ソケット (Defender 内 accept 結果格納、関数頭で初期化)

	//これより、送受信どちらかを選んだかにより処理が変化する
	switch (sideSelect)
	{
		//こちらが送信側 (Client = 旧 Challenger): socket → connect → send → recv → netStatus=EXCHANGED → closesocket
		case SIDE_SELECT_CLIENT: {
			LPHOSTENT	lpHostEntry;
			SOCKET		s;
			SOCKADDR_IN saCl;
			short		nPort = NET_PORT;	//通信に使用するポート番号
			//接続先 (δ-3a: ハードコード "localhost" を NET_TARGET_HOST 定数経由に変更)
			char		szServer[SERVER_NAME_SIZE];
			strncpy(szServer, NET_TARGET_HOST, SERVER_NAME_SIZE - 1);
			szServer[SERVER_NAME_SIZE - 1] = '\0';

			MyOutputDebugString(_T("\n %sの確認中です…"), szServer);
			lpHostEntry = gethostbyname(szServer);
			if (lpHostEntry == NULL)
			{
				MyOutputDebugString(_T("gethostbyname failed for %s (err=%d)\n"), szServer, WSAGetLastError());
				return -1;
			}
			MyOutputDebugString("OK!\n");

			//ソケット作成
			s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (s == INVALID_SOCKET)
			{
				MyOutputDebugString(_T("socket failed for Client (err=%d)\n"), WSAGetLastError());
				return -1;
			}

			//相手サーバーに接続
			saCl.sin_family = AF_INET;
			saCl.sin_addr = *((LPIN_ADDR)*lpHostEntry->h_addr_list);
			saCl.sin_port = htons(nPort);
			nRet = connect(s, (LPSOCKADDR)&saCl, sizeof(struct sockaddr));
			if (nRet == SOCKET_ERROR)
			{
				MyOutputDebugString(_T("connect failed (err=%d)\n"), WSAGetLastError());
				closesocket(s);
				return -1;
			}

			//自スコアを文字列化して送信 (δ-3a: SCORE: プレフィックス + %.2f 整形)
			snprintf(szBuf, NET_BUF_SIZE, "%s%.2f", NET_MSG_PREFIX_SCORE, state.Score);
			nRet = send(s, szBuf, strlen(szBuf), 0);
			if (nRet == SOCKET_ERROR)
			{
				MyOutputDebugString(_T("send failed for Client (err=%d)\n"), WSAGetLastError());
				closesocket(s);
				return -1;
			}
			MyOutputDebugString(_T("送信: %s\n"), szBuf);

			//相手スコア受信 (γ-2-b バグ #4: 双方向交換のため Client にも recv 追加)
			memset(szBuf, 0, sizeof(szBuf));
			nRet = recv(s, szBuf, sizeof(szBuf), 0);
			if (nRet == SOCKET_ERROR)
			{
				MyOutputDebugString(_T("recv failed for Client (err=%d)\n"), WSAGetLastError());
				closesocket(s);
				return -1;
			}
			//受信した相手のスコアを格納する (γ-3: for ループから memcpy 化、可読性 + 性能)
			memcpy(rcScore, szBuf, NET_BUF_SIZE);
			//δ-3a: γ-3 では RECEIVED で完了扱いだったが、新 7 値 enum では「双方交換完了」= EXCHANGED に意味再割り当て
			netStatus = NET_STATUS_EXCHANGED;
			MyOutputDebugString(_T("受信: %s\n"), szBuf);

			//γ-2-c: closesocket でリソース解放 (旧コードは漏れていた)
			closesocket(s);
			break;
		}
		//こちらが受信側 (Server = 旧 Defender): socket → bind → listen → accept → recv → send → netStatus=EXCHANGED → closesocket
		case SIDE_SELECT_SERVER: {
			SOCKET		lisS = INVALID_SOCKET;	//listen 用ソケット (Server 専用)
			SOCKADDR_IN saSv;
			short		nPort = NET_PORT;

			//Server 専用: 自ホスト名/IP をデバッグ出力 (δ-2、どこで listen 待機しているか確認用)
			//Client 側は localhost 接続のため自ホスト情報は不要、関数頭の共通プロローグから移動
			nRet = gethostname(szBuf, sizeof(szBuf));
			if (nRet == SOCKET_ERROR)
			{
				MyOutputDebugString(_T("gethostname failed (err=%d)\n"), WSAGetLastError());
				return -1;
			}
			LPHOSTENT lpInAddr = gethostbyname(szBuf);
			if (lpInAddr == NULL)
			{
				MyOutputDebugString(_T("gethostbyname failed for self host (err=%d)\n"), WSAGetLastError());
				return -1;
			}
			MyOutputDebugString(_T("\n PC info: %s or %s\n"),
				szBuf, inet_ntoa(*(LPIN_ADDR)*lpInAddr->h_addr_list));

			//γ-2-b 構造整理 #5: socket+bind+listen+accept を Server case 内に集約
			//(Client では使わないリソースなので関数頭から移動)
			lisS = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (lisS == INVALID_SOCKET)
			{
				MyOutputDebugString(_T("socket failed for Server (err=%d)\n"), WSAGetLastError());
				return -1;
			}
			saSv.sin_family = AF_INET;
			saSv.sin_addr.s_addr = INADDR_ANY;	//Let Winsock supply address
			saSv.sin_port = htons(nPort);
			nRet = bind(lisS, (LPSOCKADDR)&saSv, sizeof(struct sockaddr));
			if (nRet == SOCKET_ERROR)
			{
				MyOutputDebugString(_T("bind failed (err=%d)\n"), WSAGetLastError());
				closesocket(lisS);
				return -1;
			}

			//Connect 要求が来るまで待機 (同期 listen/accept、フリーズ仕様)
			nRet = listen(lisS, SOMAXCONN);
			if (nRet == SOCKET_ERROR)
			{
				MyOutputDebugString(_T("listen failed (err=%d)\n"), WSAGetLastError());
				closesocket(lisS);
				return -1;
			}
			MyOutputDebugString("接続待機状態に入ります…");

			//受信を承諾する (相手の connect まで accept ブロック)
			remS = accept(lisS, NULL, NULL);
			if (remS == INVALID_SOCKET)
			{
				MyOutputDebugString(_T("accept failed (err=%d)\n"), WSAGetLastError());
				closesocket(lisS);
				return -1;
			}
			MyOutputDebugString("接続完了しました\n");

			//相手スコア受信 (γ-2-b バグ #2 修正: while (szBuf != NULL) 無限ループ削除、1 回交換)
			memset(szBuf, 0, sizeof(szBuf));
			nRet = recv(remS, szBuf, sizeof(szBuf), 0);
			if (nRet == SOCKET_ERROR)
			{
				MyOutputDebugString(_T("recv failed for Server (err=%d)\n"), WSAGetLastError());
				closesocket(remS);
				closesocket(lisS);
				return -1;
			}
			//δ-2: cnt 変数 + "%ld>%s" 書式を撤去し Client と同じ "受信: %s" 書式に統一
			MyOutputDebugString(_T("受信: %s\n"), szBuf);

			//受信した相手のスコアを格納する (γ-3: for ループから memcpy 化、可読性 + 性能)
			memcpy(rcScore, szBuf, NET_BUF_SIZE);

			//自スコアを文字列化して送信 (δ-3a: SCORE: プレフィックス + %.2f 整形)
			snprintf(szBuf, NET_BUF_SIZE, "%s%.2f", NET_MSG_PREFIX_SCORE, state.Score);
			nRet = send(remS, szBuf, strlen(szBuf), 0);
			if (nRet == SOCKET_ERROR)
			{
				MyOutputDebugString(_T("send failed for Server (err=%d)\n"), WSAGetLastError());
				closesocket(remS);
				closesocket(lisS);
				return -1;
			}
			//δ-3a: γ-3 では RECEIVED で完了扱いだったが、新 7 値 enum では「双方交換完了」= EXCHANGED に意味再割り当て
			netStatus = NET_STATUS_EXCHANGED;
			MyOutputDebugString(_T("送信: %s\n"), szBuf);

			//γ-2-c: closesocket でリソース解放 (lisS は Server 専用、両方解放、LIFO 順)
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
			//δ-3a: caller でネット対戦の戻り値を検査 (失敗時のログ確認の土台、UI 表示は δ-3b で追加)
			//現状は同期 netBattle なので、ここでブロックして戻ってくるまで全画面凍結する点は δ-3b で解消予定
			if (netBattle(state.Score) != 0)
			{
				MyOutputDebugString(_T("netBattle failed, see preceding log for WSA error code\n"));
			}
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
			//0 が選ばれたら Client (送信側)、1 が選ばれたら Server (受信側) (δ-3a で SIDE_SELECT に統一)
			//この後、ステータスをゲーム本編に移す
			if (selectedGame2 == 0)sideSelect = SIDE_SELECT_CLIENT;
			else if (selectedGame2 == 1)sideSelect = SIDE_SELECT_SERVER;
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
		//δ-2: マジックナンバーを GameMain.h の SIDE_SELECT_* 定数に置換
		int x = SIDE_SELECT_X_ITEM, y = SIDE_SELECT_Y_FIRST, gapY = SIDE_SELECT_GAP_Y;
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
	if (netStatus == NET_STATUS_EXCHANGED)
	{
		//受信したスコアを浮動小数点数に戻す (双方 0〜100 の正規化達成率、δ-1 以降)
		//δ-3a: "SCORE:" プレフィックスを剥がして数値部だけ strtof でパース。プレフィックス無しは旧プロトコル互換扱いで全文字列をパース
		const char* scorePayload = rcScore;
		if (strncmp(rcScore, NET_MSG_PREFIX_SCORE, NET_PREFIX_LEN_SCORE) == 0)
		{
			scorePayload = rcScore + NET_PREFIX_LEN_SCORE;
		}
		float scJudge = strtof(scorePayload, NULL);

		//floorf 整数化は δ-1 で除去 (0〜100 スケールに縮小したため、95.7 vs 95.3 が同点扱いになり粒度が粗すぎる)。
		//float 直接比較に変更。引き分けは事実上ピッタリ同点のみ。
		//自分のスコアより少ない場合
		if (scJudge < state.Score)
		{
			DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "あなたの勝利です！", ColorSkyLike);
		}
		//同じ場合
		else if (scJudge == state.Score)
		{
			DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "なんと…引き分け！", ColorYellow);
		}
		//自分のスコアより多い場合
		else if (scJudge > state.Score)
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
		if (netStatus == NET_STATUS_EXCHANGED)
		{
			DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_SPEED, ColorRed, "相手のスコアは%s", rcScore);
		}
	}

	//フレーム値は÷FPS して表示する (FrameTmp は CalMulti 加算済みの累積フレーム)
	DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_CURRENT_TIME, ColorGreen, "現在の時間：%3.2f秒", state.FrameTmp / FPS);
	//スコアは 0〜100 の正規化達成率 (δ-1 で再設計、ネット交換 "%f" 1 個の契約は維持し意味のみ更新)
	DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_SCORE, ColorSkyLike, "スコア：%3.1f点", state.Score);

	//ピッタリ達成時のみ "PERFECT!" を演出表示 (δ-1、DONE 後のみ、SIDE_SELECT/READY 中は出さない)
	if (state.IsPerfect == TRUE && status2 == GAME2_STATE_DONE)
	{
		DrawString(LAYOUT_X_DEFAULT + LAYOUT_X_PERFECT_OFFSET, LAYOUT_Y_SCORE, "PERFECT!", ColorRed);
	}
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