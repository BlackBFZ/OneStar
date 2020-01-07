#include <iostream>
#include "Util.h"
#include "Calculator.h"
#include "Const.h"
#include "XoroshiroState.h"
#include "Data.h"

// ���������ݒ�
static PokemonData l_First;
static PokemonData l_Second;

static int g_Rerolls;
static int g_FixedIndex;
static int g_VCount;

// �i�荞�ݏ����ݒ�

// V�m��p�Q��
const int* g_IvsRef[30] = {
	&l_First.ivs[1], &l_First.ivs[2], &l_First.ivs[3], &l_First.ivs[4], &l_First.ivs[5],
	&l_First.ivs[0], &l_First.ivs[2], &l_First.ivs[3], &l_First.ivs[4], &l_First.ivs[5],
	&l_First.ivs[0], &l_First.ivs[1], &l_First.ivs[3], &l_First.ivs[4], &l_First.ivs[5],
	&l_First.ivs[0], &l_First.ivs[1], &l_First.ivs[2], &l_First.ivs[4], &l_First.ivs[5],
	&l_First.ivs[0], &l_First.ivs[1], &l_First.ivs[2], &l_First.ivs[3], &l_First.ivs[5],
	&l_First.ivs[0], &l_First.ivs[1], &l_First.ivs[2], &l_First.ivs[3], &l_First.ivs[4]
};

#define LENGTH (57)

void SetFirstCondition(int iv0, int iv1, int iv2, int iv3, int iv4, int iv5, int ability, int nature, bool isNoGender, bool isEnableDream)
{
	l_First.ivs[0] = iv0;
	l_First.ivs[1] = iv1;
	l_First.ivs[2] = iv2;
	l_First.ivs[3] = iv3;
	l_First.ivs[4] = iv4;
	l_First.ivs[5] = iv5;
	l_First.ability = ability;
	l_First.nature = nature;
	l_First.isNoGender = isNoGender;
	l_First.isEnableDream = isEnableDream;
	g_FixedIndex = 0;
	for (int i = 0; i < 6; ++i)
	{
		if (l_First.ivs[i] == 31)
		{
			g_FixedIndex = i;
		}
	}
}

void SetNextCondition(int iv0, int iv1, int iv2, int iv3, int iv4, int iv5, int ability, int nature, bool isNoGender, bool isEnableDream)
{
	l_Second.ivs[0] = iv0;
	l_Second.ivs[1] = iv1;
	l_Second.ivs[2] = iv2;
	l_Second.ivs[3] = iv3;
	l_Second.ivs[4] = iv4;
	l_Second.ivs[5] = iv5;
	l_Second.ability = ability;
	l_Second.nature = nature;
	l_Second.isNoGender = isNoGender;
	l_Second.isEnableDream = isEnableDream;
	g_VCount = 0;
	for (int i = 0; i < 6; ++i)
	{
		if (l_Second.ivs[i] == 31)
		{
			++g_VCount;
		}
	}
}

void Prepare(int rerolls)
{
	g_Rerolls = rerolls;

	// �g�p����s��l���Z�b�g
	// �g�p����萔�x�N�g�����Z�b�g
	g_ConstantTermVector = 0;
	for (int i = 0; i < LENGTH - 1; ++i)
	{
		int index = (i < 6 ? rerolls * 10 + (i / 3) * 5 + 2 + i % 3 : i - 6 + (rerolls + 1) * 10); // r[3+rerolls]��V�ӏ��Ar[4+rerolls]����r[8+rerolls]���̒l�Ƃ��Ďg��
		g_InputMatrix[i] = Const::c_Matrix[index];
		if (Const::c_ConstList[index] > 0)
		{
			g_ConstantTermVector |= (1ull << (LENGTH - 1 - i));
		}
	}
	// Ability��2�����k r[9+rerolls]
	int index = (rerolls + 6) * 10 + 4;
	g_InputMatrix[LENGTH - 1] = Const::c_Matrix[index] ^ Const::c_Matrix[index + 5];
	if ((Const::c_ConstList[index] ^ Const::c_ConstList[index + 5]) != 0)
	{
		g_ConstantTermVector |= 1;
	}

	// �s��{�ό`�ŋ��߂�
	CalculateInverseMatrix(LENGTH);

	// ���O�f�[�^���v�Z
	CalculateCoefficientData(LENGTH);
}

_u64 Search(_u64 ivs)
{
	XoroshiroState xoroshiro;
	XoroshiroState oshiroTemp;

	_u64 target = l_First.ability;

	// ���3bit = V�ӏ�����
	target |= (ivs & 0xE000000ul) << 29; // fixedIndex0

	// ����25bit = �̒l
	target |= (ivs & 0x1F00000ul) << 26; // iv0_0
	target |= (ivs &   0xF8000ul) << 21; // iv1_0
	target |= (ivs &    0x7C00ul) << 16; // iv2_0
	target |= (ivs &     0x3E0ul) << 11; // iv3_0
	target |= (ivs &      0x1Ful) <<  6; // iv4_0

	// �B���ꂽ�l�𐄒�
	target |= ((8ul + g_FixedIndex - ((ivs & 0xE000000ul) >> 25)) & 7) << 51;

	target |= ((32ul + *g_IvsRef[g_FixedIndex * 5    ] - ((ivs & 0x1F00000ul) >> 20)) & 0x1F) << 41;
	target |= ((32ul + *g_IvsRef[g_FixedIndex * 5 + 1] - ((ivs &   0xF8000ul) >> 15)) & 0x1F) << 31;
	target |= ((32ul + *g_IvsRef[g_FixedIndex * 5 + 2] - ((ivs &    0x7C00ul) >> 10)) & 0x1F) << 21;
	target |= ((32ul + *g_IvsRef[g_FixedIndex * 5 + 3] - ((ivs &     0x3E0ul) >> 5)) & 0x1F) << 11;
	target |= ((32ul + *g_IvsRef[g_FixedIndex * 5 + 4] -  (ivs &      0x1Ful)) & 0x1F) << 1;

	// target�x�N�g�����͊���

	target ^= g_ConstantTermVector;

	// 57bit���̌v�Z���ʃL���b�V��
	_u64 processedTarget = 0;
	int offset = 0;
	for (int i = 0; i < LENGTH; ++i)
	{
		while (g_FreeBit[i + offset] > 0)
		{
			++offset;
		}
		processedTarget |= (GetSignature(g_AnswerFlag[i] & target) << (63 - (i + offset)));
	}

	// ����7bit�����߂�
	_u64 max = ((1 << (64 - LENGTH)) - 1);
	for (_u64 search = 0; search <= max; ++search)
	{
		_u64 seed = (processedTarget ^ g_CoefficientData[search]) | g_SearchPattern[search];

		// ��������i�荞��
		{
			xoroshiro.SetSeed(seed);
			xoroshiro.Next(); // EC
			xoroshiro.Next(); // OTID
			xoroshiro.Next(); // PID

			// V�ӏ�
			int offset = -1;
			int fixedIndex = 0;
			do {
				fixedIndex = xoroshiro.Next(7); // V�ӏ�
				++offset;
			} while (fixedIndex >= 6);
			if (offset != g_Rerolls)
			{
				continue;
			}

			xoroshiro.Next(); // �̒l1
			xoroshiro.Next(); // �̒l2
			xoroshiro.Next(); // �̒l3
			xoroshiro.Next(); // �̒l4
			xoroshiro.Next(); // �̒l5
			xoroshiro.Next(); // ����

			if (!l_First.isNoGender)
			{
				int gender = 0;
				do {
					gender = xoroshiro.Next(0xFF); // ���ʒl
				} while (gender >= 253);
			}

			int nature = 0;
			do {
				nature = xoroshiro.Next(0x1F); // ���i
			} while (nature >= 25);

			if (nature != l_First.nature)
			{
				continue;
			}
		}

		// 2�C��
		_u64 nextSeed = seed + 0x82a2b175229d6a5bull;
		xoroshiro.SetSeed(nextSeed);
		xoroshiro.Next(); // EC
		xoroshiro.Next(); // OTID
		xoroshiro.Next(); // PID
		oshiroTemp.Copy(&xoroshiro); // ��Ԃ�ۑ�

		for(int ivVCount = g_VCount; ivVCount >= 1; --ivVCount)
		{
			xoroshiro.Copy(&oshiroTemp); // �Â�����

			int ivs[6] = { -1, -1, -1, -1, -1, -1 };
			int fixedCount = 0;
			do {
				int fixedIndex = 0;
				do {
					fixedIndex = xoroshiro.Next(7); // V�ӏ�
				} while (fixedIndex >= 6);

				if (ivs[fixedIndex] == -1)
				{
					ivs[fixedIndex] = 31;
					++fixedCount;
				}
			} while (fixedCount < ivVCount);

			// �̒l
			bool isPassed = true;
			for (int i = 0; i < 6; ++i)
			{
				if (ivs[i] == 31)
				{
					if (l_Second.ivs[i] != 31)
					{
						isPassed = false;
						break;
					}
				}
				else if(l_Second.ivs[i] != xoroshiro.Next(0x1F))
				{
					isPassed = false;
					break;
				}
			}
			if (!isPassed)
			{
				continue;
			}

			// ����
			int ability = 0;
			if(l_Second.isEnableDream)
			{
				do {
					ability = xoroshiro.Next(3);
				} while(ability >= 3);
			}
			else
			{
				ability = xoroshiro.Next(1);
			}
			if((l_Second.ability >= 0 && l_Second.ability != ability) || (l_Second.ability == -1 && ability >= 2))
			{
				continue;
			}

			// ���ʒl
			if (!l_Second.isNoGender)
			{
				int gender = 0;
				do {
					gender = xoroshiro.Next(0xFF);
				} while (gender >= 253);
			}

			// ���i
			int nature = 0;
			do {
				nature = xoroshiro.Next(0x1F);
			} while (nature >= 25);

			if (nature != l_Second.nature)
			{
				continue;
			}

			return seed;
		}
	}
	return 0;
}
