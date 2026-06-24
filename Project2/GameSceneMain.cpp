#include "GameSceneMain.h"
#include <assert.h>		//changeScene 範囲外チェックのアサート用 (Debug ビルドでのみ発火)
#include <math.h>		//timerCalcScore で fabsf を使用 (δ-1 新スコア式の誤差絶対値)

//全てのシーンのヘッダファイルをインクルードする
#include "Game1Scene.h"
#include "Game2Scene.h"
#include "Game3Scene.h"
#include "Game4Scene.h"
#include "MenuScene.h"

//タイマー乱数定数 (timerReset 専用、ファイルスコープに閉じる)
//目標時間: GetRand(19)+1 = 1〜19秒の乱数 (整数)
#define RANDOM_TARGET_RANGE   19     //GetRand に渡す範囲 (0〜18 を生成)
#define RANDOM_TARGET_OFFSET  1      //最短目標秒数 (= 1秒スタート)

//倍速値: GetRand(39)+10 = 10〜48 を /10 で 1.0〜4.8 倍に変換
#define RANDOM_SPEED_RANGE    39     //GetRand に渡す範囲 (0〜38 を生成)
#define RANDOM_SPEED_OFFSET   10     //最小倍速 ×10 (= 1.0倍始まり)
#define SPEED_MULT_DIVISOR    10     //倍速を実数化する除数

//スコア計算定数 (timerCalcScore 専用、δ-1 で再設計)
//正規化スコア (0〜100) の満点。目標時間 (RandomTgt) が 1 秒でも 19 秒でも満点は常に 100。
#define SCORE_MAX_RATE          100.0f
//ピッタリ判定のしきい値 (スコア基準)。Score 表示書式 %3.1f で "100.0" と見える条件と完全一致させる
//(99.95 以上は四捨五入で 100.0 表示)。画面に見える数値と PERFECT 表示がずれないため違和感ゼロ。
//倍速 4.8 や長目標では事実上「フレーム単位ピッタリ」のみ到達可能となり、PERFECT の希少価値も担保。
#define PERFECT_SCORE_THRESHOLD 99.95f

//このファイル内だけで使用する関数のプロトタイプ宣言
//現在のシーンの初期化処理 (BOOL: 成功 TRUE / 失敗 FALSE、FrameMove 内のフォールバック判定で参照)
BOOL initCurrentScene(void);
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
	/* [SCENE_GAME4] */ { initGame4Scene, moveGame4Scene, renderGame4Scene, releaseGame4Scene, Game4SceneCollideCallback },
};

//sceneNo が sceneTable の有効インデックス範囲内か判定するヘルパ
static BOOL isValidSceneIndex(SCENE_NO no) {
	return (no > SCENE_NONE && no < SCENE_MAX);
}

//タイマー状態を乱数初期化 (Game1Scene/Game2Scene 共通)
void timerReset(TIMER_STATE* state) {
	//目標時間をセット (1〜19秒の整数乱数)、FPS でフレーム換算
	state->RandomTgt = (float)(GetRand(RANDOM_TARGET_RANGE) + RANDOM_TARGET_OFFSET);
	state->CalFrame = state->RandomTgt * FPS;

	//倍速値をセット (10〜48 を /10 で 1.0〜4.8 倍に変換)
	state->RandomMtp = (float)(GetRand(RANDOM_SPEED_RANGE) + RANDOM_SPEED_OFFSET);
	state->CalMulti = state->RandomMtp / SPEED_MULT_DIVISOR;

	//ピッタリフラグもリセット (前ラウンドの達成状態を持ち越さない、δ-1)
	state->IsPerfect = FALSE;
}

//計測終了時のスコア計算 (Game1Scene/Game2Scene 共通、δ-1 で正規化スコア式に再設計)
//誤差率 |FrameTmp - CalFrame| / CalFrame を 0〜1.0 に正規化し、(1 - 誤差率) × 100 で 0〜100 を得る。
//満点は目標時間 (RandomTgt) に依存せず常に 100 で、ネット対戦の公平性とリアルタイム表示の安定性を両立。
//ピッタリ達成は IsPerfect フラグに格納し、スコア式に不連続段差は持たせない (描画側で演出に分離)。
void timerCalcScore(TIMER_STATE* state) {
	//誤差フレーム数 (絶対値): FrameTmp は CalMulti が float のため非整数になりうる
	float diffFrame = fabsf(state->FrameTmp - state->CalFrame);

	//相対誤差率: 0.0 でピッタリ、1.0 で目標と同じだけずれた状態。
	//CalFrame >= FPS (RandomTgt >= 1 保証) のためゼロ除算なし。
	//将来 RANDOM_TARGET_OFFSET を 0 にする場合はゼロ除算ガードが必要。
	float errorRate = diffFrame / state->CalFrame;

	//達成率を 0〜1.0 に正規化 (床あり、誤差率 100% 超は全て 0 に張り付く)
	float rate = 1.0f - errorRate;
	if (rate < 0.0f) rate = 0.0f;

	//0〜100 スケールに変換 (満点は目標時間によらず 100 固定)
	state->Score = rate * SCORE_MAX_RATE;

	//ピッタリ達成フラグ (Score が四捨五入で "100.0" 表示になる条件、画面表示と完全整合)。
	//スコア式に不連続段差は持たせず、描画側で "PERFECT!" 等の演出に使う。
	state->IsPerfect = (state->Score >= PERFECT_SCORE_THRESHOLD) ? TRUE : FALSE;
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
		//新しいシーンの初期化処理 (失敗時は SCENE_MENU にフォールバック、それも失敗なら諦める)
		if (!initCurrentScene()) {
			MyOutputDebugString(_T("initCurrentScene failed for scene %d, falling back to SCENE_MENU\n"), (int)sceneNo);
			if (sceneNo != SCENE_MENU) {
				//通常シーン失敗 → SCENE_MENU を試す
				sceneNo = SCENE_MENU;
				if (!initCurrentScene()) {
					//SCENE_MENU も失敗 → 諦める (sceneNo = SCENE_NONE で以後 move/render 無効化)
					MyOutputDebugString(_T("SCENE_MENU init also failed, giving up\n"));
					sceneNo = SCENE_NONE;
				}
			} else {
				//SCENE_MENU 自体が失敗 → 無限再試行を避け諦める
				sceneNo = SCENE_NONE;
			}
			//無限ループ防止: nextScene を sceneNo に合わせる (sceneNo != nextScene の再侵入を防ぐ)
			nextScene = sceneNo;
		}
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
	// 正しくないシーン番号は無視 (Debug はログ + assert で停止、Release は無視継続)
	if (no >= SCENE_MAX || no <= SCENE_NONE) {
		MyOutputDebugString(_T("changeScene: invalid scene number %d (ignored)\n"), (int)no);
		assert(0 && "changeScene called with invalid scene number");
		return;
	}
	// シーンを変更する
	nextScene = no;
}

//３(3) 現在のシーンの初期化処理 (各シーンの init を呼び、BOOL 戻り値をそのまま返す)
BOOL initCurrentScene(void) {
	if (isValidSceneIndex(sceneNo)) return sceneTable[sceneNo].init();
	return FALSE;	//無効インデックス時は失敗扱い (FrameMove 側でフォールバックされる)
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
