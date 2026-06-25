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
#define NET_MSG_END_LEN      3               //strlen("END")
#define NET_MSG_BYEBYE_LEN   6               //strlen("BYEBYE")

//δ-3b: タイムアウト (30 秒 = 30 × FPS = 1800 フレーム)。
//進展なし (= state 遷移なし) のフレームをカウントし、上限到達でエラー状態へ。
//state が進む度に netTimeoutFrames は 0 にリセットされる方針 (絶対時刻ではなく「停滞時間」)。
#define NET_TIMEOUT_FRAMES (30 * FPS)

//δ-3b: 受信メッセージの種別 (SPEC_NETWORK.md §5.2 パース規約)
typedef enum _NET_MSG_TYPE {
	NET_MSG_TYPE_NONE = 0,	//パース不能 / 受信ゼロバイト
	NET_MSG_TYPE_SCORE,		//"SCORE:" プレフィックス付き
	NET_MSG_TYPE_END,		//通常終了通知
	NET_MSG_TYPE_BYEBYE,	//異常終了通知 (相手側エラー伝播)
	NET_MSG_TYPE_UNKNOWN	//プロトコルエラー
} NET_MSG_TYPE;

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

//δ-3b: ネット対戦用ソケット (フレームをまたいで保持、非同期 state machine の中核)
//通信用ソケット (Client: connect 後、Server: accept 結果)。INVALID_SOCKET = 未確立
static SOCKET netSocket = INVALID_SOCKET;
//listen 用ソケット (Server 専用)。accept 完了後も close は GAME2_STATE_ERROR / END 経由
static SOCKET netListenSocket = INVALID_SOCKET;

//δ-3b: state machine 用フラグ (NET_STATUS のサブ状態管理)
static BOOL netScoreSent     = FALSE;	//自スコア送信完了 (SCORE: メッセージ)
static BOOL netScoreReceived = FALSE;	//相手スコア受信完了 (SCORE: メッセージ)
static BOOL netEndSent       = FALSE;	//終了通知 (END) 送信完了
static BOOL netEndReceived   = FALSE;	//終了通知 (END/BYEBYE) 受信完了

//δ-3b: タイムアウト用フレームカウンタ (進展なしフレーム数、進展時 0 リセット)
static int netStallFrames = 0;

//δ-3b: エラーメッセージ表示用バッファ (GAME2_STATE_ERROR で render に表示)
static char netErrorMessage[NET_BUF_SIZE] = "";

//状態遷移マネージメント変数 (詳細は下記 GAME2_STATE enum 参照)
//0-3 は Game1Scene.cpp の TIMER_STATUS とミラー、4 は Game2 専用 SIDE_SELECT
//TIMER_STATUS を共有 enum として汚さないため別型として定義している
typedef enum _GAME2_STATE {
	GAME2_STATE_INIT = 0,			// 目標時間と倍速値セット前 (TIMER_STATUS_INIT 相当)
	GAME2_STATE_READY,				// セット完了、スタート待ち (TIMER_STATUS_READY 相当)
	GAME2_STATE_MEASURING,			// 計測中 (TIMER_STATUS_MEASURING 相当)
	GAME2_STATE_DONE,				// 計測完了、スコア加算OK (TIMER_STATUS_DONE 相当)
	GAME2_STATE_SIDE_SELECT,		// Game2 専用: ネット対戦の送受信側を選ぶメニュー
	GAME2_STATE_ERROR				// δ-3b: ネット対戦エラー画面、Z/X 押下でメニュー復帰
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

	//δ-3b: state machine 用フラグもクリア (前ラウンドの残骸を持ち越さない)
	netSocket          = INVALID_SOCKET;
	netListenSocket    = INVALID_SOCKET;
	netScoreSent       = FALSE;
	netScoreReceived   = FALSE;
	netEndSent         = FALSE;
	netEndReceived     = FALSE;
	netStallFrames     = 0;
	netErrorMessage[0] = '\0';
	rcScore[0]         = '\0';

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

//δ-3b: ソケットを non-blocking モードに切り替える共通ヘルパ (ioctlsocket FIONBIO)
//返り値: 成功 0、失敗 -1 (失敗時は caller がエラー遷移すること)
static int netSetNonBlocking(SOCKET s)
{
	u_long mode = 1;	//1 = non-blocking、0 = blocking (デフォルト)
	if (ioctlsocket(s, FIONBIO, &mode) == SOCKET_ERROR)
	{
		MyOutputDebugString(_T("ioctlsocket FIONBIO failed (err=%d)\n"), WSAGetLastError());
		return -1;
	}
	return 0;
}

//δ-3b: ネット対戦用ソケットを LIFO 順で確実に閉じる (EXCHANGED → END / エラー時の共通後始末)
//再呼び出し可 (INVALID_SOCKET チェック済)、idempotent
static void netCleanupSockets(void)
{
	if (netSocket != INVALID_SOCKET)
	{
		closesocket(netSocket);
		netSocket = INVALID_SOCKET;
	}
	if (netListenSocket != INVALID_SOCKET)
	{
		closesocket(netListenSocket);
		netListenSocket = INVALID_SOCKET;
	}
}

//δ-3b: エラー画面 (GAME2_STATE_ERROR) へ遷移する共通ヘルパ。
//可能なら相手側に BYEBYE を送信してから (ベストエフォート、エラー無視)、ソケットを閉じてメッセージを格納する。
static void netTransitionToError(const char* message)
{
	//ベストエフォート BYEBYE 送信 (相手側に異常終了を通知、戻り値は無視)
	if (netSocket != INVALID_SOCKET)
	{
		send(netSocket, NET_MSG_BYEBYE, NET_MSG_BYEBYE_LEN, 0);
	}
	netCleanupSockets();
	//エラー文言を保持 (render が GAME2_STATE_ERROR で表示する)
	strncpy(netErrorMessage, message, NET_BUF_SIZE - 1);
	netErrorMessage[NET_BUF_SIZE - 1] = '\0';
	status2 = GAME2_STATE_ERROR;
	netStatus = NET_STATUS_INIT;
	MyOutputDebugString(_T("ネット対戦エラー: %s\n"), message);
}

//δ-3b: 受信メッセージのプレフィックスを判別 (SPEC_NETWORK.md §5.2)
//outScore が NULL でなく SCORE: の場合のみ payload を float 化して書き戻す
static NET_MSG_TYPE netParseMessage(const char* buf, float* outScore)
{
	if (strncmp(buf, NET_MSG_PREFIX_SCORE, NET_PREFIX_LEN_SCORE) == 0)
	{
		if (outScore != NULL)
		{
			*outScore = strtof(buf + NET_PREFIX_LEN_SCORE, NULL);
		}
		return NET_MSG_TYPE_SCORE;
	}
	if (strncmp(buf, NET_MSG_END, NET_MSG_END_LEN) == 0)
	{
		return NET_MSG_TYPE_END;
	}
	if (strncmp(buf, NET_MSG_BYEBYE, NET_MSG_BYEBYE_LEN) == 0)
	{
		return NET_MSG_TYPE_BYEBYE;
	}
	if (buf[0] == '\0')
	{
		return NET_MSG_TYPE_NONE;	//受信ゼロバイト (相手切断 or データなし)
	}
	return NET_MSG_TYPE_UNKNOWN;
}

//δ-3b: ネット対戦を開始する (N キー押下で呼ばれる、non-blocking 構成の初期化)
//Client: socket → connect (localhost なので同期で十分高速) → 非同期化 → CONNECTED
//Server: socket → bind → listen → 非同期化 → INIT (次フレーム以降の netStep で accept)
//返り値: 成功 0 / 失敗 -1 (失敗時は netTransitionToError が呼ばれ status2=ERROR になる)
static int netStart(void)
{
	//全フラグ・ソケット・バッファをリセット (前ラウンドの残骸を持ち越さない)
	netCleanupSockets();
	netStatus           = NET_STATUS_INIT;
	netStallFrames      = 0;
	netScoreSent        = FALSE;
	netScoreReceived    = FALSE;
	netEndSent          = FALSE;
	netEndReceived      = FALSE;
	netErrorMessage[0]  = '\0';
	rcScore[0]          = '\0';

	if (sideSelect == SIDE_SELECT_CLIENT)
	{
		//Client: socket → connect (localhost は即応のため同期 OK) → 非同期化
		//接続先 (NET_TARGET_HOST = "localhost", δ-3a で定数化)
		LPHOSTENT lpHostEntry = gethostbyname(NET_TARGET_HOST);
		if (lpHostEntry == NULL)
		{
			MyOutputDebugString(_T("gethostbyname failed for %s (err=%d)\n"), NET_TARGET_HOST, WSAGetLastError());
			netTransitionToError("ホスト名解決に失敗しました");
			return -1;
		}

		netSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (netSocket == INVALID_SOCKET)
		{
			MyOutputDebugString(_T("socket failed for Client (err=%d)\n"), WSAGetLastError());
			netTransitionToError("ソケット作成に失敗しました");
			return -1;
		}

		SOCKADDR_IN saCl;
		saCl.sin_family = AF_INET;
		saCl.sin_addr   = *((LPIN_ADDR)*lpHostEntry->h_addr_list);
		saCl.sin_port   = htons(NET_PORT);
		if (connect(netSocket, (LPSOCKADDR)&saCl, sizeof(struct sockaddr)) == SOCKET_ERROR)
		{
			MyOutputDebugString(_T("connect failed (err=%d)\n"), WSAGetLastError());
			netTransitionToError("接続に失敗しました");
			return -1;
		}

		//connect 完了後に非同期化 (以降の send/recv は WSAEWOULDBLOCK 想定)
		if (netSetNonBlocking(netSocket) != 0)
		{
			netTransitionToError("ソケットの非同期化に失敗しました");
			return -1;
		}

		netStatus      = NET_STATUS_CONNECTED;
		netStallFrames = 0;
		MyOutputDebugString(_T("接続完了 (Client)\n"));
		return 0;
	}
	else if (sideSelect == SIDE_SELECT_SERVER)
	{
		//Server 専用: 自ホスト名/IP をデバッグ出力 (δ-2、どこで listen 待機しているか確認用)
		char szSelf[SERVER_NAME_SIZE];
		if (gethostname(szSelf, sizeof(szSelf)) == SOCKET_ERROR)
		{
			MyOutputDebugString(_T("gethostname failed (err=%d)\n"), WSAGetLastError());
			netTransitionToError("ホスト名取得に失敗しました");
			return -1;
		}
		LPHOSTENT lpInAddr = gethostbyname(szSelf);
		if (lpInAddr == NULL)
		{
			MyOutputDebugString(_T("gethostbyname failed for self host (err=%d)\n"), WSAGetLastError());
			netTransitionToError("ホスト名解決に失敗しました");
			return -1;
		}
		MyOutputDebugString(_T("\n PC info: %s or %s\n"),
			szSelf, inet_ntoa(*(LPIN_ADDR)*lpInAddr->h_addr_list));

		netListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (netListenSocket == INVALID_SOCKET)
		{
			MyOutputDebugString(_T("socket failed for Server (err=%d)\n"), WSAGetLastError());
			netTransitionToError("ソケット作成に失敗しました");
			return -1;
		}

		SOCKADDR_IN saSv;
		saSv.sin_family      = AF_INET;
		saSv.sin_addr.s_addr = INADDR_ANY;
		saSv.sin_port        = htons(NET_PORT);
		if (bind(netListenSocket, (LPSOCKADDR)&saSv, sizeof(struct sockaddr)) == SOCKET_ERROR)
		{
			MyOutputDebugString(_T("bind failed (err=%d)\n"), WSAGetLastError());
			netTransitionToError("bind に失敗しました");
			return -1;
		}

		if (listen(netListenSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			MyOutputDebugString(_T("listen failed (err=%d)\n"), WSAGetLastError());
			netTransitionToError("listen に失敗しました");
			return -1;
		}

		//Listen socket を非同期化 (δ-3b の本命: accept を WSAEWOULDBLOCK で polling 化、フレーム凍結回避)
		if (netSetNonBlocking(netListenSocket) != 0)
		{
			netTransitionToError("リッスン非同期化に失敗しました");
			return -1;
		}

		MyOutputDebugString(_T("接続待機開始 (Server、polling 中)\n"));
		netStatus      = NET_STATUS_INIT;	//accept 完了で CONNECTED に遷移
		netStallFrames = 0;
		return 0;
	}

	//Side 未選択など (本来到達不能)
	netTransitionToError("対戦の役割が選ばれていません");
	return -1;
}

//δ-3b: 1 回の send 試行 (非同期、WSAEWOULDBLOCK は再試行扱い)
//返り値: 1 = 送信成功 / 0 = 未完了 (再試行) / -1 = 異常 (caller がエラー遷移する責任)
static int netTrySend(const char* payload, int payloadLen)
{
	int nRet = send(netSocket, payload, payloadLen, 0);
	if (nRet == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
		{
			return 0;	//まだ送れないだけ、次フレームで再試行
		}
		MyOutputDebugString(_T("send failed (err=%d)\n"), err);
		return -1;
	}
	return 1;
}

//δ-3b: 1 回の recv 試行 (非同期、WSAEWOULDBLOCK は再試行扱い)
//返り値: 1 = 受信成功 (out に格納) / 0 = 未完了 (再試行) / -1 = 異常 (caller がエラー遷移する責任)
//※ buf は NET_BUF_SIZE バイトの領域を渡すこと、関数内で memset 0 クリアしてから recv する
static int netTryRecv(char* buf)
{
	memset(buf, 0, NET_BUF_SIZE);
	int nRet = recv(netSocket, buf, NET_BUF_SIZE - 1, 0);
	if (nRet == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
		{
			return 0;
		}
		MyOutputDebugString(_T("recv failed (err=%d)\n"), err);
		return -1;
	}
	if (nRet == 0)
	{
		//相手側が接続を綺麗にクローズした (FIN)。プロトコル的には BYEBYE 相当扱い
		MyOutputDebugString(_T("recv: peer closed connection\n"));
		return -1;
	}
	return 1;
}

//δ-3b: 毎フレーム呼ばれる state machine の 1 ステップ。
//N キー押下 (netStart 成功) 以降、netStatus に応じて 1 つだけ進める。
//ネット対戦未開始 (netStatus==INIT かつソケット未確保) は早期 return。
static void netStep(void)
{
	//ネット対戦未開始ガード (INIT かつソケット未確保 = N キー前)
	if (netStatus == NET_STATUS_INIT && netSocket == INVALID_SOCKET && netListenSocket == INVALID_SOCKET)
	{
		return;
	}

	//タイムアウト計測 (停滞フレーム数、進展時にゼロリセット)
	netStallFrames++;
	if (netStallFrames >= NET_TIMEOUT_FRAMES)
	{
		netTransitionToError("タイムアウト (30秒進展なし)");
		return;
	}

	switch (netStatus)
	{
	case NET_STATUS_INIT:
	{
		//Server: accept を polling 試行 (Client は netStart で CONNECTED まで進むので来ない)
		if (sideSelect != SIDE_SELECT_SERVER || netListenSocket == INVALID_SOCKET) break;

		SOCKET accepted = accept(netListenSocket, NULL, NULL);
		if (accepted == INVALID_SOCKET)
		{
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK) return;	//まだ来てない、次フレームへ
			MyOutputDebugString(_T("accept failed (err=%d)\n"), err);
			netTransitionToError("接続待ち受けに失敗しました");
			return;
		}

		//accept 成功。Listen ソケットは役目終了、accepted を通信用に昇格
		closesocket(netListenSocket);
		netListenSocket = INVALID_SOCKET;
		netSocket       = accepted;
		if (netSetNonBlocking(netSocket) != 0)
		{
			netTransitionToError("ソケットの非同期化に失敗しました");
			return;
		}
		MyOutputDebugString(_T("接続完了 (Server)\n"));
		netStatus      = NET_STATUS_CONNECTED;
		netStallFrames = 0;
		break;
	}

	case NET_STATUS_CONNECTED:
	case NET_STATUS_SENT:
	case NET_STATUS_RECEIVED:
	{
		//SCORE 送受信フェーズ。送信と受信を並行に進め、両方完了で EXCHANGED へ。
		BOOL progressed = FALSE;

		//自スコア送信 (未送信なら試行)
		if (!netScoreSent)
		{
			char szBuf[NET_BUF_SIZE];
			snprintf(szBuf, NET_BUF_SIZE, "%s%.2f", NET_MSG_PREFIX_SCORE, state.Score);
			int r = netTrySend(szBuf, (int)strlen(szBuf));
			if (r < 0)
			{
				netTransitionToError("スコア送信に失敗しました");
				return;
			}
			if (r == 1)
			{
				netScoreSent = TRUE;
				progressed   = TRUE;
				MyOutputDebugString(_T("送信: %s\n"), szBuf);
			}
		}

		//相手スコア受信 (未受信なら試行)
		if (!netScoreReceived)
		{
			char szBuf[NET_BUF_SIZE];
			int r = netTryRecv(szBuf);
			if (r < 0)
			{
				netTransitionToError("スコア受信に失敗しました");
				return;
			}
			if (r == 1)
			{
				float opp;
				NET_MSG_TYPE t = netParseMessage(szBuf, &opp);
				if (t == NET_MSG_TYPE_SCORE)
				{
					memcpy(rcScore, szBuf, NET_BUF_SIZE);
					netScoreReceived = TRUE;
					progressed       = TRUE;
					MyOutputDebugString(_T("受信: %s\n"), szBuf);
				}
				else if (t == NET_MSG_TYPE_BYEBYE)
				{
					netTransitionToError("相手が切断しました");
					return;
				}
				else
				{
					netTransitionToError("通信エラー (不明なメッセージ)");
					return;
				}
			}
		}

		//状態遷移: 両方完了 → EXCHANGED、片方完了 → SENT or RECEIVED
		if (netScoreSent && netScoreReceived)
		{
			netStatus      = NET_STATUS_EXCHANGED;
			netStallFrames = 0;
		}
		else if (netScoreSent && netStatus != NET_STATUS_SENT)
		{
			netStatus      = NET_STATUS_SENT;
			netStallFrames = 0;
		}
		else if (netScoreReceived && netStatus != NET_STATUS_RECEIVED)
		{
			netStatus      = NET_STATUS_RECEIVED;
			netStallFrames = 0;
		}
		else if (progressed)
		{
			netStallFrames = 0;
		}
		break;
	}

	case NET_STATUS_EXCHANGED:
		//SCORE 交換完了、render が勝敗を表示中。END 送信フェーズに進む
		netStatus      = NET_STATUS_ENDING;
		netStallFrames = 0;
		break;

	case NET_STATUS_ENDING:
	{
		//END 送受信フェーズ。両方完了で END (切断完了) へ
		BOOL progressed = FALSE;

		if (!netEndSent)
		{
			int r = netTrySend(NET_MSG_END, NET_MSG_END_LEN);
			if (r < 0)
			{
				netTransitionToError("終了通知の送信に失敗しました");
				return;
			}
			if (r == 1)
			{
				netEndSent = TRUE;
				progressed = TRUE;
				MyOutputDebugString(_T("送信: END\n"));
			}
		}

		if (!netEndReceived)
		{
			char szBuf[NET_BUF_SIZE];
			int r = netTryRecv(szBuf);
			if (r < 0)
			{
				//相手側 close を BYEBYE 扱いとせず、終了通知として受理 (相手が先に切ったケース)
				netEndReceived = TRUE;
				progressed     = TRUE;
				MyOutputDebugString(_T("受信: 相手が先に切断 (END 扱い)\n"));
			}
			else if (r == 1)
			{
				NET_MSG_TYPE t = netParseMessage(szBuf, NULL);
				if (t == NET_MSG_TYPE_END || t == NET_MSG_TYPE_BYEBYE)
				{
					netEndReceived = TRUE;
					progressed     = TRUE;
					MyOutputDebugString(_T("受信: %s\n"), szBuf);
				}
				//SCORE や UNKNOWN は無視 (END フェーズでは受け付けない)
			}
		}

		if (netEndSent && netEndReceived)
		{
			netStatus = NET_STATUS_END;
			netCleanupSockets();
			MyOutputDebugString(_T("切断完了\n"));
		}
		else if (progressed)
		{
			netStallFrames = 0;
		}
		break;
	}

	case NET_STATUS_END:
		//切断完了、X キーでメニュー復帰待ち。state machine からは何もしない
		break;

	default:
		break;
	}
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

		//δ-3b: ネット対戦の state machine を毎フレーム 1 ステップ進める
		//N キー前 (netStatus==INIT かつソケット未確保) は内部で早期 return するので常時呼んで良い
		netStep();

		//Rキーで状態リセット (ネット対戦中は受け付けない、INIT/END でのみ有効)
		if (CheckHitKey(KEY_INPUT_R) == 1)
		{
			if (netStatus == NET_STATUS_INIT || netStatus == NET_STATUS_END)
			{
				netCleanupSockets();
				netStatus = NET_STATUS_INIT;
				status2   = GAME2_STATE_INIT;
			}
		}

		//Xキーでタイトルに戻す (ネット対戦中なら BYEBYE 送信 + cleanup してから遷移)
		if (CheckHitKey(KEY_INPUT_X) == 1)
		{
			if (netSocket != INVALID_SOCKET)
			{
				//ベストエフォート BYEBYE 通知
				send(netSocket, NET_MSG_BYEBYE, NET_MSG_BYEBYE_LEN, 0);
			}
			netCleanupSockets();
			netStatus = NET_STATUS_INIT;
			changeScene(SCENE_MENU);
		}

		//Nキーでネットワーク対戦を開始 (δ-3b: 非同期版、netStart が socket 確保 + 接続待機 polling 開始)
		//二重起動防止のため netStatus == INIT のみ受け付ける
		if (CheckHitKey(KEY_INPUT_N) == 1 && netStatus == NET_STATUS_INIT && netSocket == INVALID_SOCKET && netListenSocket == INVALID_SOCKET)
		{
			netStart();	//失敗時は内部で netTransitionToError → status2=ERROR 遷移
		}

		break;

	case GAME2_STATE_ERROR://δ-3b: ネット対戦エラー画面、ユーザー手動確認後にメニュー復帰
		//Z/X キー押下でメニューへ (押下感を出すため EdgeInput ではなく CheckHitKey でも可、即時応答優先で CheckHitKey)
		if (CheckHitKey(KEY_INPUT_Z) == 1 || CheckHitKey(KEY_INPUT_X) == 1)
		{
			//念のため (netTransitionToError で既に cleanup 済だが、二重防御として実施)
			netCleanupSockets();
			netStatus = NET_STATUS_INIT;
			changeScene(SCENE_MENU);
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

	//δ-3b: エラー画面 (GAME2_STATE_ERROR)。netTransitionToError でセットされた netErrorMessage を表示し、
	//Z/X キー押下を待つ。計測関連描画はスキップして専用画面に集中
	if (status2 == GAME2_STATE_ERROR)
	{
		DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "通信エラーが発生しました", ColorRed);
		DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_TARGET, ColorWhite, "原因: %s", netErrorMessage);
		DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_SCORE, "Z または X キーでメニューに戻る", ColorYellow);
		return;	//エラー中は計測関連描画スキップ
	}

	//δ-3b: DONE 中の操作案内をネット対戦の進行状況で出し分け (netStatus に応じた誘導)
	if (status2 == GAME2_STATE_DONE)
	{
		switch (netStatus)
		{
		case NET_STATUS_INIT:
			DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "Rキーでリセット、Nキーでネット対戦", ColorWhite);
			break;
		case NET_STATUS_CONNECTED:
		case NET_STATUS_SENT:
		case NET_STATUS_RECEIVED:
			//SCORE 送受信中、まだ勝敗判定までいかない
			DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "対戦相手と通信中…", ColorYellow);
			break;
		case NET_STATUS_EXCHANGED:
		case NET_STATUS_ENDING:
			//render の下段で勝敗結果を表示するため、ここは状態通知のみ
			DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "終了処理中…", ColorYellow);
			break;
		case NET_STATUS_END:
			DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "切断完了、Xキーでメニューへ", ColorWhite);
			break;
		default:
			break;
		}
	}
	else
	{
		DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "Gキーで開始、Sキーで停止", ColorWhite);
	}

	//ネット対戦が EXCHANGED 以降ならば勝敗を判定して表示する (END 状態でも勝敗を残す、δ-3b で表示範囲拡張)
	//相手スコアは下段でも数値表示するため scJudge を外側スコープで保持
	BOOL netExchangeDone = (netStatus == NET_STATUS_EXCHANGED || netStatus == NET_STATUS_ENDING || netStatus == NET_STATUS_END);
	float scJudge = 0.0f;
	if (netExchangeDone)
	{
		//受信したスコアを浮動小数点数に戻す (双方 0〜100 の正規化達成率、δ-1 以降)
		//δ-3a: "SCORE:" プレフィックスを剥がして数値部だけ strtof でパース。プレフィックス無しは旧プロトコル互換扱いで全文字列をパース
		const char* scorePayload = rcScore;
		if (strncmp(rcScore, NET_MSG_PREFIX_SCORE, NET_PREFIX_LEN_SCORE) == 0)
		{
			scorePayload = rcScore + NET_PREFIX_LEN_SCORE;
		}
		scJudge = strtof(scorePayload, NULL);

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
		//δ-3b: スコア交換以降は相手のスコアに表示を変える (SCORE: プレフィックスを剥がして数値表示)
		if (netExchangeDone)
		{
			DrawFormatString(LAYOUT_X_DEFAULT, LAYOUT_Y_SPEED, ColorRed, "相手のスコアは%.2f点", scJudge);
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
	//δ-3b: 中断時のソケット漏れ防止 (シーン途中で X キー押下や強制遷移した場合の保険)
	//ベストエフォート BYEBYE 送信 (相手側に切断通知、戻り値無視)
	if (netSocket != INVALID_SOCKET)
	{
		send(netSocket, NET_MSG_BYEBYE, NET_MSG_BYEBYE_LEN, 0);
	}
	netCleanupSockets();
	WSACleanup();
}

// 当り判定コールバック 　　　ここでは要素を削除しないこと！！
void  Game2SceneCollideCallback(int nSrc, int nTarget, int nCollideID)
{
}