#include "cuda_runtime.h"
#include "CudaProcess.cuh"
#include "Data.h"

//�z�X�g�������̃|�C���^
static CudaInputMaster* pHostMaster; // �Œ�f�[�^
_u32* cu_HostResult;

//�f�o�C�X�������̃|�C���^
static CudaInputMaster* pDeviceMaster;
static _u32* pDeviceResult;

// ������s�萔
const int c_SizeBlock = 1024;
const int c_SizeGrid = 1024 * 16;

__device__ inline _u32 GetSignature(_u32 value)
{
	value ^= (value >> 16);
	value ^= (value >>  8);
	value ^= (value >>  4);
	value ^= (value >>  2);
	return (value ^ (value >> 1)) & 1;
}

// �v�Z����J�[�l��
__global__ void kernel_calc(CudaInputMaster* pSrc, _u32 *pResult, _u32 ivs)
{
	int idx = blockDim.x * blockIdx.x + threadIdx.x; //�����̃X���b�hx��index

	ivs |= idx;

	_u32 targetUpper = 0;
	_u32 targetLower = 0;

	// ����30bit = �̒l
	targetUpper |= (ivs & 0x3E000000ul); // iv0_0
	targetLower |= ((ivs &    0x7C00ul) << 15); // iv3_0
	targetUpper |= ((ivs & 0x1F00000ul) >> 5); // iv1_0
	targetLower |= ((ivs &     0x3E0ul) << 10); // iv4_0
	targetUpper |= ((ivs &   0xF8000ul) >> 10); // iv2_0
	targetLower |= ((ivs &      0x1Ful) << 5); // iv5_0

	// �B���ꂽ�l�𐄒�
	targetUpper |= ((32ul + pSrc->ivs[0] - ((ivs & 0x3E000000ul) >> 25)) & 0x1F) << 20;
	targetLower |= ((32ul + pSrc->ivs[3] - ((ivs &     0x7C00ul) >> 10)) & 0x1F) << 20;
	targetUpper |= ((32ul + pSrc->ivs[1] - ((ivs &  0x1F00000ul) >> 20)) & 0x1F) << 10;
	targetLower |= ((32ul + pSrc->ivs[4] - ((ivs &      0x3E0ul) >> 5)) & 0x1F) << 10;
	targetUpper |= ((32ul + pSrc->ivs[2] - ((ivs &    0xF8000ul) >> 15)) & 0x1F);
	targetLower |= ((32ul + pSrc->ivs[5] - (ivs &        0x1Ful)) & 0x1F);

	// target�x�N�g�����͊���

	targetUpper ^= pSrc->constantTermVector[0];
	targetLower ^= pSrc->constantTermVector[1];

	// 60bit���̌v�Z���ʃL���b�V��

	_u32 processedTargetUpper = 0;
	_u32 processedTargetLower = 0;
	for(int i = 0; i < 28; ++i)
	{
		processedTargetUpper |= (GetSignature(pSrc->answerFlag[i * 2] & targetUpper) ^ GetSignature(pSrc->answerFlag[i * 2 + 1] & targetLower)) << (31 - i);
		processedTargetLower |= (GetSignature(pSrc->answerFlag[(i + 32) * 2] & targetUpper) ^ GetSignature(pSrc->answerFlag[(i + 32) * 2 + 1] & targetLower)) << (31 - i);
	}
	for(int i = 28; i < 32; ++i)
	{
		processedTargetUpper |= (GetSignature(pSrc->answerFlag[i * 2] & targetUpper) ^ GetSignature(pSrc->answerFlag[i * 2 + 1] & targetLower)) << (31 - i);
	}

	pResult[idx * 2] = processedTargetUpper;
	pResult[idx * 2 + 1] = processedTargetLower;
	return;
}

// ������
void CudaInitialize(int* pIvs)
{
	// �z�X�g�������̊m��
	cudaMallocHost(&pHostMaster, sizeof(CudaInputMaster));
	cudaMallocHost(&cu_HostResult, sizeof(_u32) * c_SizeBlock * c_SizeGrid * 2);

	// �f�o�C�X�������̊m��
	cudaMalloc(&pDeviceMaster, sizeof(CudaInputMaster));
	cudaMalloc(&pDeviceResult, sizeof(_u32) * c_SizeBlock * c_SizeGrid * 2);

	// �}�X�^�[�f�[�^�̃Z�b�g
	for(int i = 0; i < 6; ++i)
	{
		pHostMaster->ivs[i] = pIvs[i];
	}
	pHostMaster->constantTermVector[0] = (_u32)(g_ConstantTermVector >> 30);
	pHostMaster->constantTermVector[1] = (_u32)(g_ConstantTermVector & 0x3FFFFFFFull);
	for(int i = 0; i < 64; ++i)
	{
		pHostMaster->answerFlag[i * 2] = (_u32)(g_AnswerFlag[i] >> 30);
		pHostMaster->answerFlag[i * 2 + 1] = (_u32)(g_AnswerFlag[i] & 0x3FFFFFFFull);
	}

	// �f�[�^��]��
	cudaMemcpy(pDeviceMaster, pHostMaster, sizeof(CudaInputMaster), cudaMemcpyHostToDevice);
}

// �v�Z
void CudaProcess(_u32 ivs, int freeBit)
{
	//�J�[�l��
	dim3 block(c_SizeBlock, 1, 1);
	dim3 grid(c_SizeGrid, 1, 1);
	kernel_calc << < grid, block >> > (pDeviceMaster, pDeviceResult, ivs);

	//�f�o�C�X->�z�X�g�֌��ʂ�]��
	cudaMemcpy(cu_HostResult, pDeviceResult, sizeof(_u32) * c_SizeBlock * c_SizeGrid * 2, cudaMemcpyDeviceToHost);
}

void Finish()
{
	//�f�o�C�X�������̊J��
	cudaFree(pDeviceMaster);
	cudaFree(pDeviceResult);
	//�z�X�g�������̊J��
	cudaFreeHost(pHostMaster);
	cudaFreeHost(cu_HostResult);
}
