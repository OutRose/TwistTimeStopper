#include "GameSceneMain.h"

//全てのシーンのヘッダファイルをインクルードする
#include "Game1Scene.h"
#include "Game2Scene.h"
#include "Game3Scene.h"
#include "MenuScene.h"

//このファイル内だけで使用する関数のプロトタイプ宣言
//現在のシーンの初期化処理
void initCurrentScene(void);
//現在のシーンのフレーム処理
void moveCurrentScene();
//現在のシーンのレンダリング処理
void renderCurrentScene(void);
//現在のシーンの削除処理
void releaseCurrentScene(void);

//このファイル内だけで使用する変数の宣言（staticをつけて宣言する）
static SCENE_NO sceneNo = SCENE_NONE;	// 現在のシーン番号（必ず初期値はSCENE_NONE）
static SCENE_NO nextScene = SCENE_NONE;	// 次のシーン番号（必ず初期値はSCENE_NONE）

//シーンディスパッチャ用の関数ポインタ束 (全シーン共通インターフェース)
//新規シーン追加時は (1) GameSceneMain.h の SCENE_NO に 1 値追加、(2) 下の sceneTable に 1 行追加で完了
typedef struct _SCENE_HANDLERS {
	BOOL (*init)(void);						// initXXXScene
	void (*move)(void);						// moveXXXScene
	void (*render)(void);					// renderXXXScene
	void (*release)(void);					// releaseXXXScene
	void (*collide)(int, int, int);			// XXXSceneCollideCallback
} SCENE_HANDLERS;

//シーン番号順に並べたディスパッチテーブル (SCENE_NO の値がそのまま添字)
static const SCENE_HANDLERS sceneTable[SCENE_MAX] = {
	/* [SCENE_MENU]  */ { initMenuScene,  moveMenuScene,  renderMenuScene,  releaseMenuScene,  MenuSceneCollideCallback  },
	/* [SCENE_GAME1] */ { initGame1Scene, moveGame1Scene, renderGame1Scene, releaseGame1Scene, Game1SceneCollideCallback },
	/* [SCENE_GAME2] */ { initGame2Scene, moveGame2Scene, renderGame2Scene, releaseGame2Scene, Game2SceneCollideCallback },
	/* [SCENE_GAME3] */ { initGame3Scene, moveGame3Scene, renderGame3Scene, releaseGame3Scene, Game3SceneCollideCallback },
};

//sceneNo が sceneTable の有効インデックス範囲内か判定するヘルパ
static BOOL isValidSceneIndex(SCENE_NO no) {
	return (no > SCENE_NONE && no < SCENE_MAX);
}

//タイマー状態を乱数初期化 (Game1Scene/Game2Scene 共通)
void timerReset(TIMER_STATE* state) {
	//目標時間をセット (1〜20秒の乱数)、フレーム換算
	state->RandomTgt = (float)(GetRand(19) + 1);
	state->CalFrame = state->RandomTgt * 60;

	//倍速値をセット (1.0〜4.9倍の乱数)、10で除して倍率に変換
	state->RandomMtp = (float)(GetRand(39) + 10);
	state->CalMulti = state->RandomMtp / 10;
}

//３ゲーム開始前の初期化を行う
BOOL InitGame(void) {
	// 全てのシーンで共有するモノを初期化する

	//３(1) 初めのシーン番号の設定
	changeScene(SCENE_MENU);
	return TRUE;
}

//フレーム処理
void FrameMove() {
	// 次のシーンに変更するかどうか判断する
	if (sceneNo != nextScene) {
		//現在のシーンの削除処理
		releaseCurrentScene();
		//現在のシーンを新規シーンに変更する
		sceneNo = nextScene;
		//新しいシーンの初期化処理
		initCurrentScene();
	}

	//現在のシーンのフレーム処理
	moveCurrentScene();
}

//レンダリング処理
void RenderScene() {
	//現在のシーンのレンダリング処理
	renderCurrentScene();
}

//ゲーム終了時の後処理
void GameRelease(void) {
	//現在のシーンの削除処理
	releaseCurrentScene();
	// 全てのシーンで共有するモノの削除処理をする
}

//３(2) 当り判定コールバック 　　　ここでは要素を削除しないこと！！
void  CollideCallback(int nSrc, int nTarget, int nCollideID) {
	if (isValidSceneIndex(sceneNo)) sceneTable[sceneNo].collide(nSrc, nTarget, nCollideID);
}

//シーンを変更する関数
void changeScene(SCENE_NO no) {
	// 現在のシーンと同じときは何もしない
	if (sceneNo == no)return;
	// 正しくないシーン番号の時は無視する
	if (no >= SCENE_MAX) return;
	if (no <= SCENE_NONE) return;
	// シーンを変更する
	nextScene = no;
}

//３(3) 現在のシーンの初期化処理
void initCurrentScene(void) {
	if (isValidSceneIndex(sceneNo)) sceneTable[sceneNo].init();
}
//３(4) 現在のシーンのフレーム処理
void moveCurrentScene() {
	if (isValidSceneIndex(sceneNo)) sceneTable[sceneNo].move();
}
//３(5) 現在のシーンのレンダリング処理
void renderCurrentScene(void) {
	if (isValidSceneIndex(sceneNo)) sceneTable[sceneNo].render();
}
//３(6) 現在のシーンの削除処理
void releaseCurrentScene(void) {
	if (isValidSceneIndex(sceneNo)) sceneTable[sceneNo].release();
}
