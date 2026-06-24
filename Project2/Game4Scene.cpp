#include "GameMain.h"
#include "GameSceneMain.h"
#include "Game4Scene.h"

//外部定義(GameMain.cppにて宣言)
extern int Input, EdgeInput;

// シーン開始前の初期化を行う
BOOL initGame4Scene(void) {
	return TRUE;
}

//	フレーム処理
void moveGame4Scene() {
	//７(3) 新たにボタン１が押されたら選択されているシーンへ
	if ((EdgeInput & PAD_INPUT_1)) {
		changeScene(SCENE_MENU);
	}
}

//	レンダリング処理
void renderGame4Scene(void) {
	DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_STATUS, "ゲーム画面４です", ColorWhite);
	DrawString(LAYOUT_X_DEFAULT, LAYOUT_Y_BACK_TO_TITLE, "ボタン１でタイトルに戻る", ColorWhite);
}

//	シーン終了時の後処理
void releaseGame4Scene(void) {
}

// 当り判定コールバック 　　　ここでは要素を削除しないこと！！
void  Game4SceneCollideCallback(int nSrc, int nTarget, int nCollideID) {
}