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

#define MENU_MAX_G 2
SCENE_NO menu[MENU_MAX_G] = { SCENE_GAME1, SCENE_GAME2 };
//�{����SCENE_NO menu[MENU_MAX] = { SCENE_GAME1, SCENE_GAME2, SCENE_GAME3 };
char* menuList2[3] = { "���ޑ�","���܂�鑤","" };
//�I�����ꂽ�Q�[����\�����j���[�ԍ��̏������imenu�̓Y�����j
static int selectedGame2 = 0;

int startfont2;


//�O����`(GameMain.cpp�ɂĐ錾)+�ϐ��V�[�h�Ɏg�����~���b�f�[�^
extern int Input, EdgeInput, rdSeed;

//�����p�ϐ���錾�FINT�^�͖ڕW���ԁBFLOAT�^�͔{���l
float RandomTgt2, CalFrame2;
float RandomMtp2, CalMulti2;
void targetTimeSet2();
//�l�b�g���[�N�ΐ�p�̊֐����ǉ�����
int netBattle(float score);

//�v���p�ϐ��A�L�^�p�ϐ���錾
float FrameTmp2 = 0.0;
float ScMulti2, Score2 = 0.0;

//��M�����X�R�A���i�[����ϐ�
char rcScore[256];

//��ԑJ�ڃ}�l�[�W�����g�ϐ�
//0=�ڕW���ԂƔ{���l�Z�b�g�O�A1=�ڕW���ԂƔ{���l�Z�b�g�����A�X�^�[�g�҂�
//2=�v�����A3=�v�������A�X�R�A���ZOK�i�����܂ł͉��j
//4=�l�b�g�ΐ�̑���M��I�΂���
int status2 = 4;
//�l�b�g���[�N�ΐ�p�̃X�e�[�^�X�ϐ�
//0=������ԁA1=�ڑ��ҋ@���[�h�A2=��M�����[�h�H
int netStatus = 0;
//���Ȃ��͒��܂��̂��A���ނ̂��i��M���A���M���j
//0=���ޑ�(��ɑ��M����N���C�A���g)�A1=���܂�鑤�i��M����T�[�o�[�j
//2=���I�����
int sideSelect = 2;

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
	status2 = 4;

	//���j���[�֌W�̏�����
	SetFontSize(32);
	ChangeFontType(DX_FONTTYPE_ANTIALIASING_EDGE_8X8);

	selectedGame2 = 0;

	return TRUE;

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
int netBattle(float score)
{
	//�l�b�g���[�N�������ł��邩�������ϐ�
//����������������0�ɕύX����
	int cFunc = 1;

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
	//char		rcScore[256];	//�󂯎�����X�R�A���i�[����
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

	//������A����M�ǂ��炩��I�񂾂��ɂ�菈�����ω�����
	switch (sideSelect)
	{
		//�����炪���M���̏ꍇ
		case 0:
			//����v���O�����Ƃ̐ڑ��������s��
			LPHOSTENT	lpHostEntry;
			SOCKET		s;
			SOCKADDR_IN saSv;
			int			nRet;
			int			cnt = 1;
			short		nPort = 3500;  //�ʐM�Ɏg�p����|�[�g�ԍ�
			char		szBuf[256];

			//�ڑ�������[�J���z�X�g�Ɍ��肷��
			char szServer[128] = "localhost";

			MyOutputDebugString(_T("\n %s�̊m�F���ł��c"), szServer);
			lpHostEntry = gethostbyname(szServer);
			if (lpHostEntry == NULL) return FALSE;  //�ڑ���̎w��G���[��Ԃ�
			MyOutputDebugString("OK!\n");

			//�\�P�b�g�̍쐬����
			s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

			saSv.sin_family = AF_INET;
			saSv.sin_addr = *((LPIN_ADDR)*lpHostEntry->h_addr_list);
			saSv.sin_port = htons(nPort);
			nRet = connect(s, (LPSOCKADDR)&saSv, sizeof(struct sockaddr));  
			//���ő���T�[�o�[�ɐڑ�����

			//�����F���������_���當����ɕϊ�����֐�
			//�g�����F(�������ݐ敶����, ������T�C�Y, �����w�蕶��, �ϊ�������)
			//���̊֐��ŁA�����ł͎����̃X�R�A��ϊ�����������
			snprintf(szBuf, 256, "%f", Score2);

			//���M����
			nRet = send(remS, szBuf, strlen(szBuf), 0);

			break;
		//�����炪��M���̏ꍇ
		case 1:
			//Connect�v��������܂őҋ@
			nRet = listen(lisS, SOMAXCONN);
			MyOutputDebugString("�ڑ��ҋ@��Ԃɓ���܂��c");

			//��M����������
			remS = accept(lisS, NULL, NULL);
			MyOutputDebugString("�ڑ��������܂���\n");
			MyOutputDebugString("�����҂��Ă��܂��c\n");

			//��M�̌J��Ԃ�
			while (szBuf != NULL)
			{
				memset(szBuf, 0, sizeof(szBuf));
				//��M�v���Z�X
				nRet = recv(remS, szBuf, sizeof(szBuf), 0);
				if (nRet == SOCKET_ERROR)
				{
					closesocket(lisS);
					closesocket(remS);
					return 0;
				}
				//��M���e��\������
				MyOutputDebugString(_T("%ld>%s\n"), cnt++, szBuf);

				//��M��������̃X�R�A���i�[����
				for (int i = 0; i < 256; i++)
				{
					rcScore[i] = szBuf[i];
				}

				//�I���̃L�[���[�h�𔻕ʂ���
				//if (strcmp(szBuf, "byebye") == 0)break;

				//���M�f�[�^����͂�����
				//printf("����>>");
				//scanf("%s", szBuf);

				//�����F���������_���當����ɕϊ�����֐�
				//�g�����F(�������ݐ敶����, ������T�C�Y, �����w�蕶��, �ϊ�������)
				//���̊֐��ŁA�����ł͎����̃X�R�A��ϊ�����������
				snprintf(szBuf, 256, "%f", Score2);

				//���M����
				nRet = send(remS, szBuf, strlen(szBuf), 0);

				//���̕�����byebye�ؒf�R�[�h�����邩�H��U�ۗ��ŁB

				if (nRet == SOCKET_ERROR)break;
				if (strcmp(szBuf, "byebye") == 0)break;
			}
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
			//�����̃X�R�A�𑗐M����
			netBattle(Score2);
		}

		break;

	case 4:		//�܂����M�҂��A��M�҂���I�����Ă��炤
		//�V���j���[���ڂ̑I��
		//�V(1) �@�V���Ɂ��������ꂽ��A
		if ((EdgeInput & PAD_INPUT_UP)) 
		{
			//�V(1) �A�P��̃��j���[���ڂ��I�����ꂽ�Ƃ���B
			//      �@�������A�������̃��j���[���ڂ��Ȃ��Ƃ��́A�ŉ��i�̃��j���[���ڂ��I�����ꂽ�Ƃ���
			if (--selectedGame2 < 0) 
			{
				selectedGame2 = MENU_MAX_G - 1;
			}
		}

		//�V(2) �@�V���Ɂ��������ꂽ��A
		if ((EdgeInput & PAD_INPUT_DOWN))
		{
			//�V(2) �A�P���̃��j���[���ڂ��I�����ꂽ�Ƃ���B�B
			//      �@�������A�����艺�̃��j���[���ڂ��Ȃ��Ƃ��́A�ŏ�i�̃��j���[���ڂ��I�����ꂽ�Ƃ���
			if (++selectedGame2 >= MENU_MAX_G) 
			{
				selectedGame2 = 0;
			}
		}

		//�V(3) �V���Ƀ{�^���P�������ꂽ�猈��
		if ((EdgeInput & PAD_INPUT_1)) 
		{
			//0���I�΂ꂽ�瑗�M�ҁA1���I�΂ꂽ���M��
			//���̌�A�X�e�[�^�X���Q�[���{�҂Ɉڂ�
			if (selectedGame2 == 0)sideSelect = 0;
			else if (selectedGame2 == 1)sideSelect = 1;
			status2 = 0;
		}
		if(sideSelect != 2)break;
	}
}

//	�����_�����O����
void renderGame2Scene(void)
{
	//���ނ��A���܂�邩��I��������
	switch (sideSelect)
	{

	}
	
	//R���Z�b�g�\�ł��邱�Ƃ�ʒm
	if (status2 == 3)
	{
		DrawString(30, 50, "R�L�[�Ń��Z�b�g�AN�L�[�Ńl�b�g�ΐ�", ColorWhite2);
	}
	else
	{
		DrawString(30, 50, "G�L�[�ŊJ�n�AS�L�[�Œ�~", ColorWhite2);
	}

	//�l�b�g�ΐ���I�����ꍇ�F���s�𔻒f���A�\������
	if (netStatus == 2)
	{
		//��M�����X�R�A�𕂓������_���ɖ߂�
		float scJudge = strtof(rcScore, NULL);

		scJudge = floor(scJudge);
		float Score2J = floor(Score2);

		//�����̃X�R�A��菭�Ȃ��ꍇ
		if (scJudge < Score2J)
		{
			DrawString(30, 50, "���Ȃ��̏����ł��I", ColorSkyLike2);
		}
		//�����ꍇ
		else if (scJudge == Score2J)
		{
			DrawString(30, 50, "�Ȃ�Ɓc���������I", ColorYellow2);
		}
		//�����̃X�R�A��葽���ꍇ
		else if (scJudge > Score2J)
		{
			DrawString(30, 50, "���Ȃ��̔s�k�ł��c", ColorBlue2);
		}
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
		//�l�b�g��ł̃X�R�A�������I������Ȃ�΁A����̃X�R�A�ɕ\����ς���
		if (netStatus == 2)
		{
			DrawFormatString(30, 250, ColorRed2, "����̃X�R�A��%s", rcScore);
		}
	}

	//�t���[���l�́�60���ĕ\�����邱�ƁI
	DrawFormatString(30, 350, ColorGreen2, "���݂̎��ԁF%3.2f�b", FrameTmp2 / 60);
	DrawFormatString(30, 400, ColorSkyLike2, "�X�R�A�F%3.1f�_", Score2);
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