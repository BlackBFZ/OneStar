#include <iostream>
#include "Util.h"
#include "SixivCalculator.h"
#include "Const.h"
#include "XoroshiroState.h"
#include "Data.h"

// ���������ݒ�
static PokemonData l_First;
static PokemonData l_Second;
static PokemonData l_Third;

static int g_FixedIvs;
static int g_Ivs[6];
static int g_SecondIvCount;

static int g_IvOffset;

//#define LENGTH (60)

void SetSixFirstCondition(int iv1, int iv2, int iv3, int iv4, int iv5, int iv6, int ability, int nature, int characteristic, bool isNoGender, bool isEnableDream)
{
	l_First.ivs[0] = iv1;
	l_First.ivs[1] = iv2;
	l_First.ivs[2] = iv3;
	l_First.ivs[3] = iv4;
	l_First.ivs[4] = iv5;
	l_First.ivs[5] = iv6;
	l_First.ability = ability;
	l_First.nature = nature;
	l_First.characteristic = characteristic;
	l_First.isNoGender = isNoGender;
	l_First.isEnableDream = isEnableDream;
}

void SetSixSecondCondition(int iv1, int iv2, int iv3, int iv4, int iv5, int iv6, int ability, int nature, int characteristic, bool isNoGender, bool isEnableDream)
{
	l_Second.ivs[0] = iv1;
	l_Second.ivs[1] = iv2;
	l_Second.ivs[2] = iv3;
	l_Second.ivs[3] = iv4;
	l_Second.ivs[4] = iv5;
	l_Second.ivs[5] = iv6;
	l_Second.ability = ability;
	l_Second.nature = nature;
	l_Second.characteristic = characteristic;
	l_Second.isNoGender = isNoGender;
	l_Second.isEnableDream = isEnableDream;
	g_SecondIvCount = 0;
	for (int i = 0; i < 6; ++i)
	{
		if (l_Second.ivs[i] == 31)
		{
			++g_SecondIvCount;
		}
	}
	if(g_SecondIvCount > 4)
	{
		g_SecondIvCount = 4;
	}
}

void SetSixThirdCondition(int iv1, int iv2, int iv3, int iv4, int iv5, int iv6, int ability, int nature, int characteristic, bool isNoGender, bool isEnableDream)
{
	l_Third.ivs[0] = iv1;
	l_Third.ivs[1] = iv2;
	l_Third.ivs[2] = iv3;
	l_Third.ivs[3] = iv4;
	l_Third.ivs[4] = iv5;
	l_Third.ivs[5] = iv6;
	l_Third.ability = ability;
	l_Third.nature = nature;
	l_Third.characteristic = characteristic;
	l_Third.isNoGender = isNoGender;
	l_Third.isEnableDream = isEnableDream;
}

void SetTargetCondition6(int iv1, int iv2, int iv3, int iv4, int iv5, int iv6)
{
	g_FixedIvs = 6;
	g_Ivs[0] = iv1;
	g_Ivs[1] = iv2;
	g_Ivs[2] = iv3;
	g_Ivs[3] = iv4;
	g_Ivs[4] = iv5;
	g_Ivs[5] = iv6;
}

void SetTargetCondition5(int iv1, int iv2, int iv3, int iv4, int iv5)
{
	g_FixedIvs = 5;
	g_Ivs[0] = iv1;
	g_Ivs[1] = iv2;
	g_Ivs[2] = iv3;
	g_Ivs[3] = iv4;
	g_Ivs[4] = iv5;
}

void PrepareSix(int ivOffset)
{
	const int length = g_FixedIvs * 10;

	g_IvOffset = ivOffset;

	// �g�p����s��l���Z�b�g
	// �g�p����萔�x�N�g�����Z�b�g

	g_ConstantTermVector = 0;

	// r[(11 - FixedIvs) + offset]����r[(11 - FixedIvs) + FixedIvs - 1 + offset]�܂Ŏg��

	// �ϊ��s����v�Z
	InitializeTransformationMatrix(); // r[1]��������ϊ��s�񂪃Z�b�g�����
	for(int i = 0; i <= 9 - g_FixedIvs + ivOffset; ++i)
	{
		ProceedTransformationMatrix(); // r[2 + i]��������
	}

	for(int a = 0; a < g_FixedIvs; ++a)
	{
		for(int i = 0; i < 10; ++i)
		{
			int index = 59 + (i / 5) * 64 + (i % 5);
			int bit = a * 10 + i;
			g_InputMatrix[bit] = GetMatrixMultiplier(index);
			if(GetMatrixConst(index) != 0)
			{
				g_ConstantTermVector |= (1ull << (length - 1 - bit));
			}
		}
		ProceedTransformationMatrix();
	}

	// �s��{�ό`�ŋ��߂�
	CalculateInverseMatrix(length);

	// ���O�f�[�^���v�Z
	CalculateCoefficientData(length);
}

_u64 SearchSix(_u64 ivs)
{
	const int length = g_FixedIvs * 10;

	XoroshiroState xoroshiro;
	XoroshiroState oshiroTemp;

	_u64 target = 0;

	if(g_FixedIvs == 6)
	{
		// ����30bit = �̒l
		target |= (ivs & 0x3E000000ul) << 30; // iv0_0
		target |= (ivs & 0x1F00000ul) << 25; // iv1_0
		target |= (ivs & 0xF8000ul) << 20; // iv2_0
		target |= (ivs & 0x7C00ul) << 15; // iv3_0
		target |= (ivs & 0x3E0ul) << 10; // iv4_0
		target |= (ivs & 0x1Ful) << 5; // iv5_0

		// �B���ꂽ�l�𐄒�
		target |= ((32ul + g_Ivs[0] - ((ivs & 0x3E000000ul) >> 25)) & 0x1F) << 50;
		target |= ((32ul + g_Ivs[1] - ((ivs & 0x1F00000ul) >> 20)) & 0x1F) << 40;
		target |= ((32ul + g_Ivs[2] - ((ivs & 0xF8000ul) >> 15)) & 0x1F) << 30;
		target |= ((32ul + g_Ivs[3] - ((ivs & 0x7C00ul) >> 10)) & 0x1F) << 20;
		target |= ((32ul + g_Ivs[4] - ((ivs & 0x3E0ul) >> 5)) & 0x1F) << 10;
		target |= ((32ul + g_Ivs[5] - (ivs & 0x1Ful)) & 0x1F);
	}
	else if(g_FixedIvs == 5)
	{
		// ����25bit = �̒l
		target |= (ivs & 0x1F00000ul) << 25; // iv0_0
		target |= (ivs & 0xF8000ul) << 20; // iv1_0
		target |= (ivs & 0x7C00ul) << 15; // iv2_0
		target |= (ivs & 0x3E0ul) << 10; // iv3_0
		target |= (ivs & 0x1Ful) << 5; // iv4_0

		// �B���ꂽ�l�𐄒�
		target |= ((32ul + g_Ivs[0] - ((ivs & 0x1F00000ul) >> 20)) & 0x1F) << 40;
		target |= ((32ul + g_Ivs[1] - ((ivs & 0xF8000ul) >> 15)) & 0x1F) << 30;
		target |= ((32ul + g_Ivs[2] - ((ivs & 0x7C00ul) >> 10)) & 0x1F) << 20;
		target |= ((32ul + g_Ivs[3] - ((ivs & 0x3E0ul) >> 5)) & 0x1F) << 10;
		target |= ((32ul + g_Ivs[4] - (ivs & 0x1Ful)) & 0x1F);
	}
	else
	{
		return 0;
	}

	// target�x�N�g�����͊���

	target ^= g_ConstantTermVector;

	// 60bit���̌v�Z���ʃL���b�V��
	_u64 processedTarget = 0;
	int offset = 0;
	for (int i = 0; i < length; ++i)
	{
		while (g_FreeBit[i + offset] > 0)
		{
			++offset;
		}
		processedTarget |= (GetSignature(g_AnswerFlag[i] & target) << (63 - (i + offset)));
	}

	// ���ʂ����߂�
	_u64 max = ((1 << (64 - length)) - 1);
	for (_u64 search = 0; search <= max; ++search)
	{
		_u64 seed = (processedTarget ^ g_CoefficientData[search]) | g_SearchPattern[search];

		// ��������i�荞��
		xoroshiro.SetSeed(seed);

		// EC
		unsigned int ec = xoroshiro.Next(0xFFFFFFFFu);
		// 1�C�ڌ�
		{
			int characteristic = ec % 6;
			for (int i = 0; i < 6; ++i)
			{
				if (l_First.IsCharacterized((characteristic + i) % 6))
				{
					characteristic = (characteristic + i) % 6;
					break;
				}
			}
			if (characteristic != l_First.characteristic)
			{
				continue;
			}
		}
		// 2�C�ڌ�
		{
			int characteristic = ec % 6;
			for (int i = 0; i < 6; ++i)
			{
				if (l_Second.IsCharacterized((characteristic + i) % 6))
				{
					characteristic = (characteristic + i) % 6;
					break;
				}
			}
			if (characteristic != l_Second.characteristic)
			{
				continue;
			}
		}

		xoroshiro.Next(); // OTID
		xoroshiro.Next(); // PID
		oshiroTemp.Copy(&xoroshiro); // ��Ԃ�ۑ�

		// 1�C��
		{
			int ivs[6] = { -1, -1, -1, -1, -1, -1 };
			int fixedCount = 0;
			int offset = -(8 - g_FixedIvs);
			do {
				int fixedIndex = 0;
				do {
					fixedIndex = xoroshiro.Next(7); // V�ӏ�
					++offset;
				} while (fixedIndex >= 6);

				if (ivs[fixedIndex] == -1)
				{
					ivs[fixedIndex] = 31;
					++fixedCount;
				}
			} while (fixedCount < (8 - g_FixedIvs));

			// reroll��
			if (offset != g_IvOffset)
			{
				continue;
			}

			// �̒l
			bool isPassed = true;
			for (int i = 0; i < 6; ++i)
			{
				if (ivs[i] == 31)
				{
					if (l_First.ivs[i] != 31)
					{
						isPassed = false;
						break;
					}
				}
				else if (l_First.ivs[i] != xoroshiro.Next(0x1F))
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
			if (l_First.isEnableDream)
			{
				do {
					ability = xoroshiro.Next(3);
				} while (ability >= 3);
			}
			else
			{			
				ability = xoroshiro.Next(1);
			}
			if ((l_First.ability >= 0 && l_First.ability != ability) || (l_First.ability == -1 && ability >= 2))
			{
				continue;
			}

			// ���ʒl
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
			} while (fixedCount < g_SecondIvCount);

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
				else if (l_Second.ivs[i] != xoroshiro.Next(0x1F))
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
			if (l_Second.isEnableDream)
			{
				do {
					ability = xoroshiro.Next(3);
				} while (ability >= 3);
			}
			else
			{
				ability = xoroshiro.Next(1);
			}
			if ((l_Second.ability >= 0 && l_Second.ability != ability) || (l_Second.ability == -1 && ability >= 2))
			{
				continue;
			}

			// ���ʒl
			if (!l_Second.isNoGender)
			{ 
				int gender = 0;
				do {
					gender = xoroshiro.Next(0xFF); // ���ʒl
				} while (gender >= 253);
			}

			// ���i
			int nature = 0;
			do {
				nature = xoroshiro.Next(0x1F); // ���i
			} while (nature >= 25);

			if (nature != l_Second.nature)
			{
				continue;
			}
		}

		// 2�C��
		_u64 nextSeed = seed + 0x82a2b175229d6a5bull;
		xoroshiro.SetSeed(nextSeed);

		// EC
		ec = xoroshiro.Next(0xFFFFFFFFu);
		// 3�C�ڌ�
		{
			int characteristic = ec % 6;
			for (int i = 0; i < 6; ++i)
			{
				if (l_Third.IsCharacterized((characteristic + i) % 6))
				{
					characteristic = (characteristic + i) % 6;
					break;
				}
			}
			if (characteristic != l_Third.characteristic)
			{
				continue;
			}
		}

		xoroshiro.Next(); // OTID
		xoroshiro.Next(); // PID
		oshiroTemp.Copy(&xoroshiro); // ��Ԃ�ۑ�
		{
			// V��2�`4
			for (int vCount = 2; vCount <= 4; ++vCount)
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
				} while (fixedCount < vCount);

				// �̒l
				bool isPassed = true;
				for (int i = 0; i < 6; ++i)
				{
					if (ivs[i] == 31)
					{
						if (l_Third.ivs[i] != 31)
						{
							isPassed = false;
							break;
						}
					}
					else if (l_Third.ivs[i] != xoroshiro.Next(0x1F))
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
				if (l_Third.isEnableDream)
				{
					do {
						ability = xoroshiro.Next(3);
					} while (ability >= 3);
				}
				else
				{
					ability = xoroshiro.Next(1);
				}
				if ((l_Third.ability >= 0 && l_Third.ability != ability) || (l_Third.ability == -1 && ability >= 2))
				{
					continue;
				}

				// ���ʒl
				if (!l_Third.isNoGender)
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

				if (nature != l_Third.nature)
				{
					continue;
				}

				return seed;
			}
		}
	}
	return 0;
}
