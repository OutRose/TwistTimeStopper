//メイン関数
#include "GameMain.h"
#include "GameSceneMain.h"

//グローバル変数
int game_status = GAMETITLE;

int FrameStartTime;        // ６０ＦＰＳ固定用、時間保存用変数

//入力状態 Input:押しっぱなし
int Input;
//入力状態 EdgeInput:1回のみ
int EdgeInput;

//乱数の初期化：起動ごとに変更される
int rdSeed = GetNowCount();

//WinMain関数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	//実行ログを出力しない
	SetOutApplicationLogValidFlag(false);

	//ウィンドウの設定
	//ウィンドウモードの設定 false:全画面設定
	ChangeWindowMode(true);

	/*
	// ウインドウモードで起動するか確認する
	if (MessageBox(NULL, "ウインドウモードで起動しますか？", "画面モード確認", MB_YESNO) == IDYES)
	{
	// ウインドウモードで起動
	ChangeWindowMode(TRUE);
	}
	*/

	//ウィンドウのリサイズ
	//Check:実行中に画面の大きさが変更可能か
	SetWindowSizeChangeEnableFlag(true);
	//ウィンドウタイトル
	SetMainWindowText("TwistTime-Stopper 0.6");

	//解像度変更：24ビットカラー
	SetGraphMode(800, 700, 24);

	//背景色の設定
	SetBackgroundColor(0, 0, 0);
	//DXライブラリ初期化処理
	//check:-1(例外処理)が来た場合は、セットアップに失敗する
	//パソコン本体の機器の確認をしてもらう(音声デバイスがない等。)
	if (DxLib_Init() == -1)return -1;

	//描画先を一番後ろにする
	SetDrawScreen(DX_SCREEN_BACK);

	//追加処理：乱数シードの設定
	SRand(rdSeed);

	//音楽再生の初期化
	InitSoundMem();	//Dxlib

	// ゲームループ前初期化
	InitGame();

	//メインループ
	while (1) {
		// 画面に描かれているものを一回全部消す
		ClearDrawScreen();

		// １/６０秒立つまで待つ
		while (GetNowCount() - FrameStartTime < 1000 / 60) {}
		// 現在のカウント値を保存
		FrameStartTime = GetNowCount();
		// 入力状態を更新
		{
			int i;
			// パッド１とキーボードから入力を得る
			i = GetJoypadInputState(DX_INPUT_KEY_PAD1);
			// エッジを取った入力をセット
			EdgeInput = i & ~Input;
			// 入力状態の保存
			Input = i;
		}

		//ゲームの更新処理
		FrameMove();
		//ゲームの描画処理
		RenderScene();

		//画面の表示順を一番手前から反映する
		ScreenFlip();

		//windowsシステムから来る情報を処理する(例外処理等)
		if (ProcessMessage() == -1)break;

		//ESCキーが押されたらループから抜ける
		if (CheckHitKey(KEY_INPUT_ESCAPE) == 1)break;
	}

	//Dxlibの開放
	DxLib_End();

	// ゲーム終了時の初期化
	GameRelease();

	//ゲームを終了する
	return 0;
}