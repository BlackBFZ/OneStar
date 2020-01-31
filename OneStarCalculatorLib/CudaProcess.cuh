#pragma once
#include "Type.h"

struct CudaInputMaster
{
	int ivs[6];
	_u64 constantTermVector;
	_u64 answerFlag[64];
};

//�z�X�g�������̃|�C���^
extern CudaInputMaster* pHostMaster; // �Œ�f�[�^
extern _u64* cu_HostResult;

//�f�o�C�X�������̃|�C���^
extern CudaInputMaster* pDeviceMaster;
extern _u64* pDeviceResult;

void CudaInitialize(int* pIvs);
void CudaProcess(_u64 ivs, int freeBit); //�����֐�
void CudaFinalize();
