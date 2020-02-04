#include <iostream>
#include "Util.h"
#include "CudaCalculator.h"
#include "Const.h"
#include "XoroshiroState.h"
#include "Data.h"
#include "CudaProcess.cuh"

// ���������ݒ�
static int g_CudaFixedIvs;
static int g_CudaIvOffset;
//static int g_CudaIvs[6];


/*
inline bool IsEnableECBit()
{
	return g_CudaECbit >= 0;
}
*/

void CudaInitialize()
{
	CudaInitializeImpl();
}

void SetCudaCondition(int index, int iv0, int iv1, int iv2, int iv3, int iv4, int iv5, int ability, int nature, int characteristic, bool isNoGender, int abilityFlag, int flawlessIvs)
{
	if(index < 0 || index >= 4)
	{
		return;
	}

	PokemonData* pokemon = &cu_HostMaster->pokemon[index];

	pokemon->ivs[0] = iv0;
	pokemon->ivs[1] = iv1;
	pokemon->ivs[2] = iv2;
	pokemon->ivs[3] = iv3;
	pokemon->ivs[4] = iv4;
	pokemon->ivs[5] = iv5;
	pokemon->ability = ability;
	pokemon->nature = nature;
	pokemon->characteristic = characteristic;
	pokemon->isNoGender = isNoGender;
	pokemon->abilityFlag = abilityFlag;
	pokemon->flawlessIvs = flawlessIvs;

	// ECbit�����p�ł��邩�H
	if(cu_HostMaster->ecBit == -1)
	{
		int target = (characteristic == 0 ? 5 : characteristic - 1);
		if(pokemon->IsCharacterized(target))
		{
			// EC mod6 ��characteristic�Ŋm��
			if(index != 2) // Seed��ECbit�Ȃ̂Ŕ��]������
			{
				cu_HostMaster->ecBit = 1 - characteristic % 2;
			}
			else // Next�Ȃ̂ł���ɔ��]����Ă��̂܂�
			{
				cu_HostMaster->ecBit = characteristic % 2;
			}
		}
	}

	// EC mod6�Ƃ��čl��������̂̃t���O�𗧂Ă�
	bool flag = true;
	cu_HostMaster->ecMod[index][characteristic] = true;
	for(int i = 1; i < 6; ++i)
	{
		int target = (characteristic + 6 - i) % 6;
		if(flag && pokemon->IsCharacterized(target) == false)
		{
			cu_HostMaster->ecMod[index][target] = true;
		}
		else
		{
			cu_HostMaster->ecMod[index][target] = false;
			flag = false;
		}
	}
}

void SetCudaTargetCondition6(int iv1, int iv2, int iv3, int iv4, int iv5, int iv6)
{
	g_CudaFixedIvs = 6;
	cu_HostMaster->ivs[0] = iv1;
	cu_HostMaster->ivs[1] = iv2;
	cu_HostMaster->ivs[2] = iv3;
	cu_HostMaster->ivs[3] = iv4;
	cu_HostMaster->ivs[4] = iv5;
	cu_HostMaster->ivs[5] = iv6;
}

void SetCudaTargetCondition5(int iv1, int iv2, int iv3, int iv4, int iv5)
{
	g_CudaFixedIvs = 5;
	cu_HostMaster->ivs[0] = iv1;
	cu_HostMaster->ivs[1] = iv2;
	cu_HostMaster->ivs[2] = iv3;
	cu_HostMaster->ivs[3] = iv4;
	cu_HostMaster->ivs[4] = iv5;
	cu_HostMaster->ivs[5] = 0;
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
	InitializeTransformationMatrix(); // r[1]��������ϊ��s�񂪃Z�b�g�����
	for(int i = 0; i <= 9 - g_CudaFixedIvs + ivOffset - 1; ++i)
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
	CudaSetMasterData(length);
}

void PreCalc(_u32 ivs, int freeBit)
{
	CudaProcess(ivs << 20, 24);
}
_u64 SearchCuda(int threadId)
{
	return cu_HostResult[0];
}