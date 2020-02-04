#pragma once
#include "Type.h"
#include "Util.h"

struct CudaInputMaster
{
	// seed�v�Z�萔
	_u32 constantTermVector[2];
	_u32 answerFlag[128];
	_u32 coefficientData[32];
	_u32 searchPattern[16];

	// ��������
	int ecBit;
	bool ecMod[3][6];
	int ivs[6];
	PokemonData pokemon[4];
};

// ����
extern CudaInputMaster* cu_HostMaster;

// ����
extern int* cu_HostResultCount;
extern _u64* cu_HostResult;

void CudaInitializeImpl();
void CudaSetMasterData();

void CudaProcess(_u32 ivs, int freeBit); //�����֐�
void CudaFinalize();
