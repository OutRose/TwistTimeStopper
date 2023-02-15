#include "GameMain.h"
#include "GameSceneMain.h"
#include "MenuScene.h"

//メニュー項目のシーン番号の配列
#define MENU_MAX 2
SCENE_NO menu[MENU_MAX] = { SCENE_GAME1, SCENE_GAME2 };
//本来→SCENE_NO menu[MENU_MAX] = { SCENE_GAME1, SCENE_GAME2, SCENE_GAME3 };
char* menuList[3] = { "ゲームスタート","ネットワークバトル","" };
//選択されたゲームを表すメニュー番号の初期化（menuの添え字）
static int selectedGame = 0;

int startfont;

//外部定義(GameMain.cppにて宣言)
extern int Input, EdgeInput;

//シーン開始前の初期化を行う
BOOL initMenuScene(void) {
	SetFontSize(32);
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
	DrawString(85, 50, "ねじれストップウォッチ（仮）", GetColor(255, 255, 255));
	DrawString(215, 100, "Version 0.6", GetColor(255, 255, 255));
	DrawString(85, 400, "Zキーで決定、ESCキーで終了", GetColor(255, 255, 255));

	//６(2) メニュー項目の表示
	int x = 195, y = 200, gapY = 80;	//（x,y)：表示開始座標　gapY：行の高さ
	for (int i = 0; i < MENU_MAX; i++, y += gapY) {
		//６(2) ①選択された項目の表示
		if (i == selectedGame) {
			DrawString(x, y, menuList[i], GetColor(255, 0, 0));
			//６(2) ②選択されていない項目の表示
		}
		else {
			DrawString(x, y, menuList[i], GetColor(255, 255, 255));
		}
	}
}

//シーン終了時の後処理
void releaseMenuScene(void) {
}

//当り判定コールバック 　　　ここでは要素を削除しないこと！！
void  MenuSceneCollideCallback(int nSrc, int nTarget, int nCollideID) {
}