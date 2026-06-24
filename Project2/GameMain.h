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
