#include "GameMain.h"
#include "GameSceneMain.h"
#include "Game1Scene.h"
#include <math.h>
#include <string.h>

//�O����`(GameMain.cpp�ɂĐ錾)+�ϐ��V�[�h�Ɏg�����~���b�f�[�^
extern int Input, EdgeInput, rdSeed;

//�����p�ϐ���錾�FINT�^�͖ڕW���ԁBFLOAT�^�͔{���l
float RandomTgt, CalFrame;
float RandomMtp, CalMulti;
void targetTimeSet();

//�v���p�ϐ��A�L�^�p�ϐ���錾
float FrameTmp = 0.0;
float ScMulti, Score = 0.0;

//��ԑJ�ڃ}�l�[�W�����g�ϐ�
//0=�ڕW���ԂƔ{���l�Z�b�g�O�A1=�ڕW���ԂƔ{���l�Z�b�g�����A�X�^�[�g�҂�
//2=�v�����A3=�v�������A�X�R�A���ZOK�i�����܂ł͉��j
int status = 0;

//�F�ϐ��Z�b�g
unsigned int ColorWhite = GetColor(255, 255, 255);
unsigned int ColorRed = GetColor(255, 0, 0);
unsigned int ColorGreen = GetColor(0, 255, 0);
unsigned int ColorBlue = GetColor(0, 0, 255);
unsigned int ColorYellow = GetColor(255, 255, 0);
unsigned int ColorPurple = GetColor(255, 0, 255);
unsigned int ColorSkyLike = GetColor(0, 255, 255);

// �V�[���J�n�O�̏��������s��
BOOL initGame1Scene(void)
{
	//����Ƃ��͕K�����Z�b�g
	status = 0;
	return TRUE;
}

void targetTimeSet()
{
	//�ڕW���Ԃ��Z�b�g����i�����Ŏ擾�A�t���[�����Z���Čv�Z�̏����j
	RandomTgt = GetRand(19) + 1;
	CalFrame = RandomTgt * 60;

	//�{���l���Z�b�g����i�����Ŏ擾�A�t���[�����Z���Čv�Z�̏����j
	RandomMtp = GetRand(39) + 10;
	CalMulti = RandomMtp / 10;
}

//	�t���[������
void moveGame1Scene()
{
	switch (status)
	{
	case 0://�����ł̓v���C���[�̓��͂�҂�
		//�^�C�}�[��������
		FrameTmp = 0.0;
		Score = 0.0;

		//�ڕW�b���A�{���l�̐ݒ�֐����Ăяo���B
		//���̌�A�X�e�[�^�X�ԍ���ύX����
		targetTimeSet();
		status = 1;
		break;

	case 1:

		//G�L�[�iGO�j�Ōv���J�n
		if (CheckHitKey(KEY_INPUT_G) == 1)
		{
			if (status == 1)
			{
				status = 2;
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
		FrameTmp += CalMulti;

		//S�L�[�iSTOP�j�Ōv���I��
		if (CheckHitKey(KEY_INPUT_S) == 1)
		{
			if (status == 2)
			{
				status = 3;
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
		if (CalFrame > FrameTmp)//�ڕW��葁���p�^�[��
		{
			//�덷�������قǃX�R�A���瑽���������
			ScMulti = FrameTmp - CalFrame;
			Score = CalFrame - (-ScMulti);
		}
		else if (CalFrame < FrameTmp)//�ڕW���x���p�^�[��
		{
			//�덷�������قǃX�R�A���瑽���������
			ScMulti = CalFrame - FrameTmp;
			Score = CalFrame - (-ScMulti);
		}
		else if (CalFrame == FrameTmp)//�ڕW�s�b�^���I�H
		{
			//�t���[���P�ʂō��킹��Ƃ͖��f�Ȃ�ʁB�{�[�i�X�I
			Score = CalFrame * 1.25;
		}

		//R�L�[�ŏ�ԃ��Z�b�g
		if (CheckHitKey(KEY_INPUT_R) == 1)
		{
			status = 0;
		}

		//X�L�[�Ń^�C�g���ɖ߂�
		if ((EdgeInput & KEY_INPUT_X))
		{
			changeScene(SCENE_MENU);
		}

		break;
	}
}

//	�����_�����O����
void renderGame1Scene(void)
{
	//R���Z�b�g�\�ł��邱�Ƃ�ʒm
	if (status == 3)
	{
		DrawString(30, 50, "R�L�[�Ń��Z�b�g", ColorWhite);
	}
	else
	{
		DrawString(30, 50, "G�L�[�ŊJ�n�AS�L�[�Œ�~", ColorWhite);
	}

	DrawString(30, 100, "X�{�^���Ń^�C�g���ɖ߂�", ColorWhite);

	//�\���`���ɂ��ă����FINT�͐����^%d�Ŗ��Ȃ��BFLOAT�͎����^%f���g�p
	//�Ȃ��A%3.1f�����v3���A������1�ʈȓ��Ŏ����\���Ƃ����Ӗ�
	DrawFormatString(30, 200, ColorWhite, "%3.1f�b�ŃX�g�b�v�I", RandomTgt, CalFrame);
	//�v���I���܂ł͔{������J
	if (status != 3)
	{
		DrawString(30, 250, "�������܁F�H.�H�{��", ColorYellow);
	}
	else if (status == 3)
	{
		DrawFormatString(30, 250, ColorYellow, "�������܁F%3.1f�{��", CalMulti);
	}

	//�t���[���l�́�60���ĕ\�����邱�ƁI
	DrawFormatString(30, 350, ColorGreen, "���݂̎��ԁF%3.2f�b", FrameTmp / 60);
	DrawFormatString(30, 400, ColorSkyLike, "�X�R�A�F%3.1f", Score);
}

//	�V�[���I�����̌㏈��
void releaseGame1Scene(void)
{
}

// ���蔻��R�[���o�b�N �@�@�@�����ł͗v�f���폜���Ȃ����ƁI�I
void  Game1SceneCollideCallback(int nSrc, int nTarget, int nCollideID)
{
}