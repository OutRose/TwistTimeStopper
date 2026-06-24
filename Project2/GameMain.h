#pragma once

//ゲームの初期化
#include "DxLib.h"
#include "GameStatus.h"

//MyOutputDebugString 用 (Debug ビルド時の OutputDebugString ラッパ)
#include <stdio.h>
#include <Windows.h>
#include <tchar.h>

//色変数 (定義は GameMain.cpp、全シーン共通)
extern unsigned int ColorWhite, ColorRed, ColorGreen, ColorBlue, ColorYellow, ColorPurple, ColorSkyLike;

#pragma warning(disable : 4996)

//デバッグ文字列出力マクロ (Debug ビルドのみ実体化、Release は空展開)
//使用方法: MyOutputDebugString(_T("出力したい文字 %s"), 指定する対応変数);
//※_T は変数を混ぜたい場合に使用 (ワイド/マルチバイト両対応)
#ifdef _DEBUG
#   define MyOutputDebugString( str, ... ) \
      { \
        TCHAR c[256]; \
        _stprintf( c, str, __VA_ARGS__ ); \
        OutputDebugString( c ); \
      }
#else
#    define MyOutputDebugString( str, ... ) // 空実装
#endif

//画面設定 (WinMain で SetGraphMode に渡す)
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 700
#define SCREEN_BPP    24

//FPS関連 (フレーム周期管理、メインループ + 各シーンの時間換算で共通使用)
#define FPS         60               //60FPS固定
#define MS_PER_SEC  1000             //ミリ秒/秒

//フォント設定 (各シーンの init 内 SetFontSize に渡す共通サイズ)
#define FONT_SIZE_DEFAULT 32

//描画レイアウト座標 (Game1/2/3 共通、X は全て左マージン)
//Y 軸は意味づけ別に固定: ステータス → 戻り操作 → 目標 → 倍速 → 経過時間 → スコア
#define LAYOUT_X_DEFAULT        30
#define LAYOUT_Y_STATUS         50   //操作説明 / 結果メッセージ
#define LAYOUT_Y_BACK_TO_TITLE  100  //タイトル戻り操作の案内
#define LAYOUT_Y_TARGET         200  //目標時間表示
#define LAYOUT_Y_SPEED          250  //倍速表示 (Game2 はネット相手スコアも同位置で兼用)
#define LAYOUT_Y_CURRENT_TIME   350  //現在の経過時間
#define LAYOUT_Y_SCORE          400  //最終スコア

//メニュー画面専用レイアウト座標 (タイトル / バージョン / ヒント)
#define MENU_X_TITLE      85
#define MENU_Y_TITLE      50
#define MENU_X_VERSION    215
#define MENU_Y_VERSION    100
#define MENU_X_HINT       85
#define MENU_Y_HINT       400
