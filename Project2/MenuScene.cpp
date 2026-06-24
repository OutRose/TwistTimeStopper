#include "GameMain.h"
#include "GameSceneMain.h"
#include "MenuScene.h"

//メニュー項目のシーン番号の配列 (γ-1 で Game3 = 練習モード本体に転用、menuList[3] の予約枠を活用)
//Game4 は空シーン雛形のため menu[] には含めない (= ユーザーから到達不可、コピー元としてのみ存在)
#define MENU_MAX 3
SCENE_NO menu[MENU_MAX] = { SCENE_GAME1, SCENE_GAME2, SCENE_GAME3 };
char* menuList[3] = { "ゲームスタート","ネットワークバトル","練習モード" };
//選択されたゲームを表すメニュー番号の初期化（menuの添え字）
static int selectedGame = 0;

//外部定義(GameMain.cppにて宣言)
extern int Input, EdgeInput;

//シーン開始前の初期化を行う
BOOL initMenuScene(void) {
	SetFontSize(FONT_SIZE_DEFAULT);
	ChangeFontType(DX_FONTTYPE_ANTIALIASING_EDGE_8X8);

	selectedGame = 0;

	return TRUE;
}

//フレーム処理
void moveMenuScene() {
	//７メニュー項目の選択
	//７(1) ①新たに↑が押されたら、
	if ((EdgeInput & PAD_INPUT_UP)) {
		//７(1) ②１つ上のメニュー項目が選択されたとする。
		//      　ただし、それより上のメニュー項目がないときは、最下段のメニュー項目が選択されたとする
		if (--selectedGame < 0) {
			selectedGame = MENU_MAX - 1;
		}
	}

	//７(2) ①新たに↓が押されたら、
	if ((EdgeInput & PAD_INPUT_DOWN)) {
		//７(2) ②１つ下のメニュー項目が選択されたとする。。
		//      　ただし、それより下のメニュー項目がないときは、最上段のメニュー項目が選択されたとする
		if (++selectedGame >= MENU_MAX) {
			selectedGame = 0;
		}
	}

	//７(3) 新たにボタン１が押されたら選択されているシーンへ
	if ((EdgeInput & PAD_INPUT_1)) {
		changeScene(menu[selectedGame]);
	}
}

//レンダリング処理
void renderMenuScene(void) {
	DrawString(MENU_X_TITLE, MENU_Y_TITLE, "ねじれストップウォッチ（仮）", ColorWhite);
	DrawString(MENU_X_VERSION, MENU_Y_VERSION, "Version 0.9.2", ColorWhite);
	DrawString(MENU_X_HINT, MENU_Y_HINT, "Zキーで決定、ESCキーで終了", ColorWhite);

	//６(2) メニュー項目の表示
	int x = 195, y = 200, gapY = 60;	//（x,y)：表示開始座標　gapY：行の高さ (3 項目目 y=320、MENU_Y_HINT=400 と 80px 差で視認性確保)
	for (int i = 0; i < MENU_MAX; i++, y += gapY) {
		//６(2) ①選択された項目の表示
		if (i == selectedGame) {
			DrawString(x, y, menuList[i], ColorRed);
			//６(2) ②選択されていない項目の表示
		}
		else {
			DrawString(x, y, menuList[i], ColorWhite);
		}
	}
}

//シーン終了時の後処理
void releaseMenuScene(void) {
}

//当り判定コールバック 　　　ここでは要素を削除しないこと！！
void  MenuSceneCollideCallback(int nSrc, int nTarget, int nCollideID) {
}
