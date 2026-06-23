#ifndef GAMESCENEMAIN_H_
#define GAMESCENEMAIN_H_

#include "GameMain.h"

// GameMain.cppファイル内の関数のうち、他のファイルから呼び出される関数のプロトタイプ宣言を記述する
BOOL InitGame(void);
void FrameMove();
void RenderScene(void);
void GameRelease(void);
void CollideCallback(int nSrc, int nTarget, int nCollideID);

//１シーン番号を管理する列挙体
//SCENE_NONEとSCENE_MAXの間に、必要なシーン番号を設定する
typedef enum _SCENE_NO {
	SCENE_NONE = -1,		// シーン番号の下限。必ず書く
	SCENE_MENU,			// メニューシーンの番号
	SCENE_GAME1,		// ゲーム１シーンの番号
	SCENE_GAME2,		// ゲーム２シーンの番号
	SCENE_GAME3,		// ゲーム３シーンの番号
	SCENE_MAX			// シーン番号の上限。必ず書く
} SCENE_NO;

//Timer状態を管理する列挙体
//Game1Scene/Game2Sceneの計測進行状態を共有する
typedef enum _TIMER_STATUS {
	TIMER_STATUS_INIT = 0,		// 目標時間と倍速値セット前 (初期化フェーズ)
	TIMER_STATUS_READY,			// セット完了、スタート待ち
	TIMER_STATUS_MEASURING,		// 計測中
	TIMER_STATUS_DONE			// 計測完了、スコア加算OK
} TIMER_STATUS;

//シーンを変更する関数
void changeScene(SCENE_NO no);

#endif