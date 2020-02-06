#include "CudaProcess.cuh"

// �萔
static CudaConst* cu_HostConstData;
CudaConst* cu_DeviceConstData;

// �ϐ�����
CudaInputMaster* cu_HostInputMaster;
_u32* cu_HostInputCoefficientData;
_u32* cu_HostInputSearchPattern;

// ���ʋ���
int* cu_HostResultCount;
_u64* cu_HostResult;

// �萔
const int c_SizeResult = 32;

// ������
void CudaInitializeImpl()
{
	// �z�X�g�������̊m��
	cudaMallocHost(&cu_HostConstData, sizeof(CudaConst));
	cudaMallocHost(&cu_HostInputMaster, sizeof(CudaInputMaster));
	cudaMallocHost(&cu_HostResultCount, sizeof(int));
	cudaMallocHost(&cu_HostResult, sizeof(_u64) * c_SizeResult);

	// �f�o�C�X�������̊m��
	cudaMalloc(&cu_DeviceConstData, sizeof(CudaConst));

	// �f�[�^�̏�����
	cu_HostInputMaster->ecBit = -1;

	// �萔�f�[�^��]��
//	cu_HostConstData->natureTable[0].randMax = 0x1F;
//	cu_HostConstData->natureTable[0].randMax = 0x1F;
	cudaMemcpy(cu_DeviceConstData, cu_HostConstData, sizeof(CudaConst), cudaMemcpyHostToDevice);
}

// �I��
void CudaFinalizeImpl()
{
	// �f�o�C�X���������
	cudaFree(cu_DeviceConstData);

	// �z�X�g���������
	cudaFreeHost(cu_HostResult);
	cudaFreeHost(cu_HostResultCount);
	cudaFreeHost(cu_HostInputMaster);
	cudaFreeHost(cu_HostConstData);
}
