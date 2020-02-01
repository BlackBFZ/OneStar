#include <iostream>
#include "Util.h"
#include "CudaCalculator.h"
#include "Const.h"
#include "XoroshiroState.h"
#include "Data.h"
#include "CudaProcess.cuh"

// ���������ݒ�
static PokemonData l_CudaPokemon[3];

static int g_CudaFixedIvs;
static int g_CudaIvs[6];

static int g_CudaIvOffset;

static int g_CudaECbit; // -1�͗��p�s��

static bool l_CudaEnableEcMod[3][6];

inline bool IsEnableECBit()
{
	return g_CudaECbit >= 0;
}

void SetCudaCondition(int index, int iv0, int iv1, int iv2, int iv3, int iv4, int iv5, int ability, int nature, int characteristic, bool isNoGender, int abilityFlag, int flawlessIvs)
{
	if(index < 0 || index >= 3)
	{
		return;
	}

	// ������
	if(index == 0)
	{
		g_CudaECbit = -1;
	}

	l_CudaPokemon[index].ivs[0] = iv0;
	l_CudaPokemon[index].ivs[1] = iv1;
	l_CudaPokemon[index].ivs[2] = iv2;
	l_CudaPokemon[index].ivs[3] = iv3;
	l_CudaPokemon[index].ivs[4] = iv4;
	l_CudaPokemon[index].ivs[5] = iv5;
	l_CudaPokemon[index].ability = ability;
	l_CudaPokemon[index].nature = nature;
	l_CudaPokemon[index].characteristic = characteristic;
	l_CudaPokemon[index].isNoGender = isNoGender;
	l_CudaPokemon[index].abilityFlag = abilityFlag;
	l_CudaPokemon[index].flawlessIvs = flawlessIvs;

	// ECbit�����p�ł��邩�H
	if(g_CudaECbit == -1)
	{
		int target = (characteristic == 0 ? 5 : characteristic - 1);
		if(l_CudaPokemon[index].IsCharacterized(target))
		{
			// EC mod6 ��characteristic�Ŋm��
			if(index != 2) // Seed��ECbit�Ȃ̂Ŕ��]������
			{
				g_CudaECbit = 1 - characteristic % 2;
			}
			else // Next�Ȃ̂ł���ɔ��]����Ă��̂܂�
			{
				g_CudaECbit = characteristic % 2;
			}
		}
	}

	// EC mod6�Ƃ��čl��������̂̃t���O�𗧂Ă�
	bool flag = true;
	l_CudaEnableEcMod[index][characteristic] = true;
	for(int i = 1; i < 6; ++i)
	{
		int target = (characteristic + 6 - i) % 6;
		if(flag && l_CudaPokemon[index].IsCharacterized(target) == false)
		{
			l_CudaEnableEcMod[index][target] = true;
		}
		else
		{
			l_CudaEnableEcMod[index][target] = false;
			flag = false;
		}
	}
}

void SetCudaTargetCondition6(int iv1, int iv2, int iv3, int iv4, int iv5, int iv6)
{
	g_CudaFixedIvs = 6;
	g_CudaIvs[0] = iv1;
	g_CudaIvs[1] = iv2;
	g_CudaIvs[2] = iv3;
	g_CudaIvs[3] = iv4;
	g_CudaIvs[4] = iv5;
	g_CudaIvs[5] = iv6;
}

void SetCudaTargetCondition5(int iv1, int iv2, int iv3, int iv4, int iv5)
{
	g_CudaFixedIvs = 5;
	g_CudaIvs[0] = iv1;
	g_CudaIvs[1] = iv2;
	g_CudaIvs[2] = iv3;
	g_CudaIvs[3] = iv4;
	g_CudaIvs[4] = iv5;
}

void PrepareCuda(int ivOffset)
{
	const int length = g_CudaFixedIvs * 10;

	g_CudaIvOffset = ivOffset;

	// �g�p����s��l���Z�b�g
	// �g�p����萔�x�N�g�����Z�b�g

	g_ConstantTermVector = 0;

	// r[(11 - FixedIvs) + offset]����r[(11 - FixedIvs) + FixedIvs - 1 + offset]�܂Ŏg��

	// �ϊ��s����v�Z
	InitializeTransformationMatrix(IsEnableECBit()); // r[1]��������ϊ��s�񂪃Z�b�g�����
	for(int i = 0; i <= 9 - g_CudaFixedIvs + ivOffset; ++i)
	{
		ProceedTransformationMatrix(); // r[2 + i]��������
	}

	for(int a = 0; a < g_CudaFixedIvs; ++a)
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

	// Cuda������
	CudaInitialize(g_CudaIvs);
}

void PreCalc(_u32 ivs, int freeBit)
{
	CudaProcess(ivs << 24, 24);
}
_u64 SearchCuda(int threadId)
{
	const int length = g_CudaFixedIvs * 10;

	XoroshiroState xoroshiro;
	XoroshiroState nextoshiro;
	XoroshiroState oshiroTemp;

	// ���ʂ����߂�
	_u64 max = ((1 << (64 - length)) - 1);
	for(_u64 search = 0; search <= max; ++search)
	{
		_u64 processedTarget = ((_u64)cu_HostResult[threadId * 2] << 32 | cu_HostResult[threadId * 2 + 1]);
		_u64 seed = (processedTarget ^ g_CoefficientData[search]) | g_SearchPattern[search];

		if(g_CudaECbit >= 0 && ((seed & 1) != g_CudaECbit))
		{
			continue;
		}

		_u64 nextSeed = seed + 0x82a2b175229d6a5bull;

		// ��������i�荞��

		// ���`�F�b�N
		{
			xoroshiro.SetSeed(seed);
			nextoshiro.SetSeed(nextSeed);

			// EC
			unsigned int ec = xoroshiro.Next(0xFFFFFFFFu);
			// 1�C�ڌ�
			if(l_CudaEnableEcMod[0][ec % 6] == false)
			{
				continue;
			}
			// 2�C�ڌ�
			if(l_CudaEnableEcMod[1][ec % 6] == false)
			{
				continue;
			}

			// EC
			ec = nextoshiro.Next(0xFFFFFFFFu);
			// 3�C�ڌ�
			if(l_CudaEnableEcMod[2][ec % 6] == false)
			{
				continue;
			}
		}

		// 2�C�ڂ��Ƀ`�F�b�N
		nextoshiro.Next(); // OTID
		nextoshiro.Next(); // PID

		int vCount = l_CudaPokemon[2].flawlessIvs;

		int ivs[6] = { -1, -1, -1, -1, -1, -1 };
		int fixedCount = 0;
		do {
			int fixedIndex = 0;
			do {
				fixedIndex = nextoshiro.Next(7); // V�ӏ�
			} while(fixedIndex >= 6);

			if(ivs[fixedIndex] == -1)
			{
				ivs[fixedIndex] = 31;
				++fixedCount;
			}
		} while(fixedCount < vCount);

		// �̒l
		bool isPassed = true;
		for(int i = 0; i < 6; ++i)
		{
			if(ivs[i] == 31)
			{
				if(l_CudaPokemon[2].ivs[i] != 31)
				{
					isPassed = false;
					break;
				}
			}
			else if(l_CudaPokemon[2].ivs[i] != nextoshiro.Next(0x1F))
			{
				isPassed = false;
				break;
			}
		}
		if(!isPassed)
		{
			continue;
		}

		// ����
		int ability = 0;
		if(l_CudaPokemon[2].abilityFlag == 3)
		{
			ability = nextoshiro.Next(1);
		}
		else
		{
			do {
				ability = nextoshiro.Next(3);
			} while(ability >= 3);
		}
		if((l_CudaPokemon[2].ability >= 0 && l_CudaPokemon[2].ability != ability) || (l_CudaPokemon[2].ability == -1 && ability >= 2))
		{
			continue;
		}

		// ���ʒl
		if(!l_CudaPokemon[2].isNoGender)
		{
			int gender = 0;
			do {
				gender = nextoshiro.Next(0xFF);
			} while(gender >= 253);
		}

		// ���i
		int nature = 0;
		do {
			nature = nextoshiro.Next(0x1F);
		} while(nature >= 25);

		if(nature != l_CudaPokemon[2].nature)
		{
			continue;
		}

		// 1�C��
		xoroshiro.Next(); // OTID
		xoroshiro.Next(); // PID

		{
			// ��Ԃ�ۑ�
			oshiroTemp.Copy(&xoroshiro);

			int ivs[6] = { -1, -1, -1, -1, -1, -1 };
			int fixedCount = 0;
			int offset = -(8 - g_CudaFixedIvs);
			do {
				int fixedIndex = 0;
				do {
					fixedIndex = xoroshiro.Next(7); // V�ӏ�
					++offset;
				} while(fixedIndex >= 6);

				if(ivs[fixedIndex] == -1)
				{
					ivs[fixedIndex] = 31;
					++fixedCount;
				}
			} while(fixedCount < (8 - g_CudaFixedIvs));

			// reroll��
			if(offset != g_CudaIvOffset)
			{
				continue;
			}

			// �̒l
			bool isPassed = true;
			for(int i = 0; i < 6; ++i)
			{
				if(ivs[i] == 31)
				{
					if(l_CudaPokemon[0].ivs[i] != 31)
					{
						isPassed = false;
						break;
					}
				}
				else if(l_CudaPokemon[0].ivs[i] != xoroshiro.Next(0x1F))
				{
					isPassed = false;
					break;
				}
			}
			if(!isPassed)
			{
				continue;
			}

			// ����
			int ability = 0;
			if(l_CudaPokemon[0].abilityFlag == 3)
			{
				ability = xoroshiro.Next(1);
			}
			else
			{
				do {
					ability = xoroshiro.Next(3);
				} while(ability >= 3);
			}
			if((l_CudaPokemon[0].ability >= 0 && l_CudaPokemon[0].ability != ability) || (l_CudaPokemon[0].ability == -1 && ability >= 2))
			{
				continue;
			}

			// ���ʒl
			if(!l_CudaPokemon[0].isNoGender)
			{
				int gender = 0;
				do {
					gender = xoroshiro.Next(0xFF); // ���ʒl
				} while(gender >= 253);
			}

			int nature = 0;
			do {
				nature = xoroshiro.Next(0x1F); // ���i
			} while(nature >= 25);

			if(nature != l_CudaPokemon[0].nature)
			{
				continue;
			}
		}

		{
			xoroshiro.Copy(&oshiroTemp); // �Â�����

			int vCount = l_CudaPokemon[1].flawlessIvs;

			int ivs[6] = { -1, -1, -1, -1, -1, -1 };
			int fixedCount = 0;
			do {
				int fixedIndex = 0;
				do {
					fixedIndex = xoroshiro.Next(7); // V�ӏ�
				} while(fixedIndex >= 6);

				if(ivs[fixedIndex] == -1)
				{
					ivs[fixedIndex] = 31;
					++fixedCount;
				}
			} while(fixedCount < vCount);

			// �̒l
			bool isPassed = true;
			for(int i = 0; i < 6; ++i)
			{
				if(ivs[i] == 31)
				{
					if(l_CudaPokemon[1].ivs[i] != 31)
					{
						isPassed = false;
						break;
					}
				}
				else if(l_CudaPokemon[1].ivs[i] != xoroshiro.Next(0x1F))
				{
					isPassed = false;
					break;
				}
			}
			if(!isPassed)
			{
				continue;
			}

			// ����
			int ability = 0;
			if(l_CudaPokemon[1].abilityFlag == 3)
			{
				ability = xoroshiro.Next(1);
			}
			else
			{
				do {
					ability = xoroshiro.Next(3);
				} while(ability >= 3);
			}
			if((l_CudaPokemon[1].ability >= 0 && l_CudaPokemon[1].ability != ability) || (l_CudaPokemon[1].ability == -1 && ability >= 2))
			{
				continue;
			}

			// ���ʒl
			if(!l_CudaPokemon[1].isNoGender)
			{
				int gender = 0;
				do {
					gender = xoroshiro.Next(0xFF); // ���ʒl
				} while(gender >= 253);
			}

			// ���i
			int nature = 0;
			do {
				nature = xoroshiro.Next(0x1F); // ���i
			} while(nature >= 25);

			if(nature != l_CudaPokemon[1].nature)
			{
				continue;
			}
		}

		return seed;
	}
	return 0;
}
