#include "GameMain.h"
#include "GameSceneMain.h"
#include "Game2Scene.h"
#include <math.h>
#include <string.h>

#include <stdio.h>
#include <Windows.h>
#include <iostream>
#include <winsock.h>
#pragma comment(lib, "wsock32.lib")
#define _CRT_SECURE_NO_WARNINGS

//�f�o�b�O������o�̓}�N��
//�g�p���@�F
//MyOutputDebugString(_T"�o�͂��������� %s", �w�肷��Ή��ϐ�);
//��_T�͕ϐ������������ꍇ�ɂ̂ݎg�p����
#ifdef _DEBUG
#   define MyOutputDebugString( str, ... ) \
      { \
        TCHAR c[256]; \
        _stprintf( c, str, __VA_ARGS__ ); \
        OutputDebugString( c ); \
      }
#else
#    define MyOutputDebugString( str, ... ) // �����
#endif

//�O����`(GameMain.cpp�ɂĐ錾)+�ϐ��V�[�h�Ɏg�����~���b�f�[�^
extern int Input, EdgeInput, rdSeed;

//�����p�ϐ���錾�FINT�^�͖ڕW���ԁBFLOAT�^�͔{���l
float RandomTgt2, CalFrame2;
float RandomMtp2, CalMulti2;
void targetTimeSet2();
//�l�b�g���[�N�ΐ�p�̊֐����ǉ�����
int netBattle(int score);

//�v���p�ϐ��A�L�^�p�ϐ���錾
float FrameTmp2 = 0.0;
float ScMulti2, Score2 = 0.0;

//��ԑJ�ڃ}�l�[�W�����g�ϐ�
//0=�ڕW���ԂƔ{���l�Z�b�g�O�A1=�ڕW���ԂƔ{���l�Z�b�g�����A�X�^�[�g�҂�
//2=�v�����A3=�v�������A�X�R�A���ZOK�i�����܂ł͉��j
int status2 = 0;
//�l�b�g���[�N�ΐ�p�̃X�e�[�^�X�ϐ�
//0=������ԁA1=�ڑ��ҋ@���[�h�A2=��M�����[�h�H
int netStatus = 0;

//�F�ϐ��Z�b�g
unsigned int ColorWhite2 = GetColor(255, 255, 255);
unsigned int ColorRed2 = GetColor(255, 0, 0);
unsigned int ColorGreen2 = GetColor(0, 255, 0);
unsigned int ColorBlue2 = GetColor(0, 0, 255);
unsigned int ColorYellow2 = GetColor(255, 255, 0);
unsigned int ColorPurple2 = GetColor(255, 0, 255);
unsigned int ColorSkyLike2 = GetColor(0, 255, 255);

// �V�[���J�n�O�̏��������s��
BOOL initGame2Scene(void)
{
	//����Ƃ��͕K�����Z�b�g
	status2 = 0;
	return TRUE;
}

void targetTimeSet2()
{
	//�ڕW���Ԃ��Z�b�g����i�����Ŏ擾�A�t���[�����Z���Čv�Z�̏����j
	RandomTgt2 = GetRand(19) + 1;
	CalFrame2 = RandomTgt2 * 60;

	//�{���l���Z�b�g����i�����Ŏ擾�A�t���[�����Z���Čv�Z�̏����j
	RandomMtp2 = GetRand(39) + 10;
	CalMulti2 = RandomMtp2 / 10;
}

//�l�b�g���[�N�ΐ핔���FTCP�����p�����ʐM���s��
//�ݒ肷��|�[�g�ԍ��́u3500�v
int netBattle(int score)
{
	//�R�R���珉��������
	WORD    wVerReq = MAKEWORD(1, 1);
	WSADATA wsaData;
	int     nRet;
	nRet = WSAStartup(wVerReq, &wsaData);

	if (wsaData.wVersion != wVerReq)
	{
		fprintf(stderr, "\n Wrong Version\n");
		return -1;
	}
	MyOutputDebugString("\n�ȈՒʐM�v���O���� �N�������F��1�i�K����");

	nRet = 1;
	int nLen, cnt = 1;
	char        szBuf[256];
	SOCKET      lisS, remS;
	SOCKADDR_IN saSv;
	//�\�P�b�g��`
	lisS = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	saSv.sin_family = AF_INET;
	//Let Winsock supply address
	saSv.sin_addr.s_addr = INADDR_ANY;
	//Use Port from command line
	saSv.sin_port = htons(3500);
	nRet = bind(lisS, (LPSOCKADDR)&saSv, sizeof(struct sockaddr));

	nLen = sizeof(SOCKADDR);
	nRet = gethostname(szBuf, sizeof(szBuf));
	if (nRet == SOCKET_ERROR)
	{
		closesocket(lisS);
		return 0;
	}

	LPHOSTENT lpInAddr = gethostbyname(szBuf);

	//�f�o�b�O�o��
	MyOutputDebugString(_T("\n���̃p�\�R����\n�A%s �܂��� %s �ł�\n"),
		szBuf, inet_ntoa(*(LPIN_ADDR)*lpInAddr->h_addr_list));

	//Connect�v��������܂őҋ@
	nRet = listen(lisS, SOMAXCONN);
	MyOutputDebugString("�ڑ��ҋ@��Ԃɓ���܂��c");
	//�R�R�܂ŏ���������

	switch (netStatus)
	{
		default:
			return TRUE;
			break;
	}
}

//	�t���[������
void moveGame2Scene()
{
	switch (status2)
	{
	case 0://�����ł̓v���C���[�̓��͂�҂�
		//�^�C�}�[��������
		FrameTmp2 = 0.0;
		Score2 = 0.0;
		
		//�ڕW�b���A�{���l�̐ݒ�֐����Ăяo���B
		//���̌�A�X�e�[�^�X�ԍ���ύX����
		targetTimeSet2();
		status2 = 1;
		break;

	case 1:

		//G�L�[�iGO�j�Ōv���J�n
		if (CheckHitKey(KEY_INPUT_G) == 1)
		{
			if (status2 == 1)
			{
				status2 = 2;
			}
		}

		//X�L�[�Ń^�C�g���ɖ߂�
		if ((EdgeInput & KEY_INPUT_X))
		{
			changeScene(SCENE_MENU);
		}

		break;

	case 2://�����ł͌v���������s��
		//�t���[�����Z����
		FrameTmp2 += CalMulti2;

		//S�L�[�iSTOP�j�Ōv���I��
		if (CheckHitKey(KEY_INPUT_S) == 1)
		{
			if (status2 == 2)
			{
				status2 = 3;
			}
		}

		//X�L�[�Ń^�C�g���ɖ߂�
		if ((EdgeInput & KEY_INPUT_X))
		{
			changeScene(SCENE_MENU);
		}

		break;

	case 3://�v�����I���������Ƃ̏������s��
		//�X�R�A�����O�����F�ڕW�b�t���[���ƃX�R�A�b�t���[�����r
		if (CalFrame2 > FrameTmp2)//�ڕW��葁���p�^�[��
		{
			//�덷�������قǃX�R�A���瑽���������
			ScMulti2 = FrameTmp2 - CalFrame2;
			Score2 = CalFrame2 - (-ScMulti2);
		}
		else if (CalFrame2 < FrameTmp2)//�ڕW���x���p�^�[��
		{
			//�덷�������قǃX�R�A���瑽���������
			ScMulti2 = CalFrame2 - FrameTmp2;
			Score2 = CalFrame2 - (-ScMulti2);
		}
		else if (CalFrame2 == FrameTmp2)//�ڕW�s�b�^���I�H
		{
			//�t���[���P�ʂō��킹��Ƃ͖��f�Ȃ�ʁB�{�[�i�X�I
			Score2 = CalFrame2 * 1.25;
		}

		//R�L�[�ŏ�ԃ��Z�b�g
		if (CheckHitKey(KEY_INPUT_R) == 1)
		{
			status2 = 0;
		}

		//X�L�[�Ń^�C�g���ɖ߂�
		if ((EdgeInput & KEY_INPUT_X))
		{
			changeScene(SCENE_MENU);
		}

		//N�L�[�Ńl�b�g���[�N�ΐ������
		if (CheckHitKey(KEY_INPUT_N) == 1)
		{
			//�|�[�g�ԍ���3500�Ɏw�肵�A�X�R�A���ꏏ�ɑ��M����
			netBattle(Score2);
		}

		break;
	}
}

//	�����_�����O����
void renderGame2Scene(void)
{
	//R���Z�b�g�\�ł��邱�Ƃ�ʒm
	if (status2 == 3)
	{
		DrawString(30, 50, "R�L�[�Ń��Z�b�g�AN�L�[�Ńl�b�g�ΐ�", ColorWhite2);
	}
	else
	{
		DrawString(30, 50, "G�L�[�ŊJ�n�AS�L�[�Œ�~", ColorWhite2);
	}

	DrawString(30, 100, "X�{�^���Ń^�C�g���ɖ߂�", ColorWhite2);

	//�\���`���ɂ��ă����FINT�͐����^%d�Ŗ��Ȃ��BFLOAT�͎����^%f���g�p
	//�Ȃ��A%3.1f�����v3���A������1�ʈȓ��Ŏ����\���Ƃ����Ӗ�
	DrawFormatString(30, 200, ColorWhite2, "%3.1f�b�ŃX�g�b�v�I", RandomTgt2, CalFrame2);
	//�v���I���܂ł͔{������J
	if (status2 != 3)
	{
		DrawString(30, 250, "�������܁F�H.�H�{��", ColorYellow2);
	}
	else if (status2 == 3)
	{
		DrawFormatString(30, 250, ColorYellow2, "�������܁F%3.1f�{��", CalMulti2);
	}

	//�t���[���l�́�60���ĕ\�����邱�ƁI
	DrawFormatString(30, 350, ColorGreen2, "���݂̎��ԁF%3.2f�b", FrameTmp2 / 60);
	DrawFormatString(30, 400, ColorSkyLike2, "�X�R�A�F%3.1f", Score2);
}

//	�V�[���I�����̌㏈��
void releaseGame2Scene(void)
{
	WSACleanup();
}

// ���蔻��R�[���o�b�N �@�@�@�����ł͗v�f���폜���Ȃ����ƁI�I
void  Game2SceneCollideCallback(int nSrc, int nTarget, int nCollideID)
{
}