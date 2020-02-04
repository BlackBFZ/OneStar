#include "cuda_runtime.h"
#include "CudaProcess.cuh"
#include "Data.h"

//�z�X�g�������̃|�C���^
CudaInputMaster* cu_HostMaster;
int* cu_HostResultCount;
_u64* cu_HostResult;

//�f�o�C�X�������̃|�C���^
static CudaInputMaster* pDeviceMaster;
static int* pDeviceResultCount;
static _u64* pDeviceResult;

// ������s�萔
const int c_SizeBlockX = 1024;
//const int c_SizeBlockX = 1;
const int c_SizeBlockY = 1;
const int c_SizeGridX = 1024 * 512;
const int c_SizeGridY = 1;
//const int c_SizeGrid = 1;
const int c_SizeResult = 32;

// GPU�R�[�h
__device__ inline _u32 GetSignature(_u32 value)
{
	value ^= (value >> 16);
	value ^= (value >>  8);
	value ^= (value >>  4);
	value ^= (value >>  2);
	return (value ^ (value >> 1)) & 1;
}
__device__ inline _u32 Next(_u32* seeds, _u32 mask)
{
	_u32 value = (seeds[1] + seeds[3]) & mask;

	// m_S1 = m_S0 ^ m_S1;
	seeds[4] = seeds[0] ^ seeds[2];
	seeds[5] = seeds[1] ^ seeds[3];

	// m_S1 = RotateLeft(m_S1, 37);
	seeds[2] = seeds[5] << 5 | seeds[4] >> 27;
	seeds[3] = seeds[4] << 5 | seeds[5] >> 27;

	// m_S0 = RotateLeft(m_S0, 24) ^ m_S1 ^ (m_S1 << 16)
	seeds[6] = (seeds[0] << 24 | seeds[1] >> 8) ^ seeds[4] ^ (seeds[4] << 16 | seeds[5] >> 16);
	seeds[1] = (seeds[1] << 24 | seeds[0] >> 8) ^ seeds[5] ^ (seeds[5] << 16);

	seeds[0] = seeds[6];

	return value;
}
__device__ inline void Next(_u32* seeds)
{
	// m_S1 = m_S0 ^ m_S1;
	seeds[4] = seeds[0] ^ seeds[2];
	seeds[5] = seeds[1] ^ seeds[3];

	// m_S1 = RotateLeft(m_S1, 37);
	seeds[2] = seeds[5] << 5 | seeds[4] >> 27;
	seeds[3] = seeds[4] << 5 | seeds[5] >> 27;

	// m_S0 = RotateLeft(m_S0, 24) ^ m_S1 ^ (m_S1 << 16)
	seeds[6] = (seeds[0] << 24 | seeds[1] >> 8) ^ seeds[4] ^ (seeds[4] << 16 | seeds[5] >> 16);
	seeds[1] = (seeds[1] << 24 | seeds[0] >> 8) ^ seeds[5] ^ (seeds[5] << 16);

	seeds[0] = seeds[6];
}

// �v�Z����J�[�l��
__global__ void kernel_calc(CudaInputMaster* pSrc, int* pResultCount, _u64 *pResult, _u32 ivs)
{
//	int idx = blockDim.x * blockIdx.x + threadIdx.x; //�����̃X���b�hx��index
//	int idy = blockDim.y * blockIdx.y + threadIdx.y;
	int targetId = (blockIdx.x / 16) * 1024 + threadIdx.x;
	int chunkId = blockIdx.x % 16;

	ivs |= targetId;

	_u32 targetUpper = 0;
	_u32 targetLower = 0;

	// ����25bit = �̒l
	targetUpper |= (ivs &  0x1F00000ul); // iv0_0
	targetLower |= ((ivs &     0x3E0ul) << 10); // iv3_0
	targetUpper |= ((ivs &   0xF8000ul) >> 5); // iv1_0
	targetLower |= ((ivs &      0x1Ful) << 5); // iv4_0
	targetUpper |= ((ivs &    0x7C00ul) >> 10); // iv2_0

	// �B���ꂽ�l�𐄒�
	targetUpper |= ((32ul + pSrc->ivs[0] - ((ivs & 0x1F00000ul) >> 20)) & 0x1F) << 15;
	targetLower |= ((32ul + pSrc->ivs[3] - ((ivs &     0x3E0ul) >> 5))  & 0x1F) << 10;
	targetUpper |= ((32ul + pSrc->ivs[1] - ((ivs &   0xF8000ul) >> 15)) & 0x1F) <<  5;
	targetLower |= ((32ul + pSrc->ivs[4] - (ivs &      0x1Ful))         & 0x1F);
	targetLower |= ((32ul + pSrc->ivs[2] - ((ivs &    0x7C00ul) >> 10)) & 0x1F) << 20;
//	targetLower |= ((32ul + pSrc->ivs[5] - (ivs &        0x1Ful)) & 0x1F);
//	targetLower |= ((32ul + idy - (ivs &        0x1Ful)) & 0x1F);

	// target�x�N�g�����͊���

	targetUpper ^= pSrc->constantTermVector[0];
	targetLower ^= pSrc->constantTermVector[1];

	// ���������L���b�V��

	__shared__ _u32 answerFlag[128];
	__shared__ _u32 coefficientData[1024 * 2];
	__shared__ _u32 searchPattern[1024];
	__shared__ PokemonData pokemon[4];
	__shared__ int ecBit;
	__shared__ bool ecMod[3][6];

	if(threadIdx.x % 8 == 0)
	{
		answerFlag[threadIdx.x / 8] = pSrc->answerFlag[threadIdx.x / 8];
	}
	else if(threadIdx.x % 8 == 1)
	{
		pokemon[0] = pSrc->pokemon[0];
	}
	else if(threadIdx.x % 8 == 2)
	{
		pokemon[1] = pSrc->pokemon[1];
	}
	else if(threadIdx.x % 8 == 3)
	{
		pokemon[2] = pSrc->pokemon[2];
	}
	else if(threadIdx.x % 8 == 4)
	{
		pokemon[3] = pSrc->pokemon[3];
	}
	else if(threadIdx.x % 8 == 5)
	{
		ecBit = pSrc->ecBit;
	}
	else if(threadIdx.x % 8 == 6)
	{
		ecMod[0][0] = pSrc->ecMod[0][0];
		ecMod[0][1] = pSrc->ecMod[0][1];
		ecMod[0][2] = pSrc->ecMod[0][2];
		ecMod[0][3] = pSrc->ecMod[0][3];
		ecMod[0][4] = pSrc->ecMod[0][4];
		ecMod[0][5] = pSrc->ecMod[0][5];
		ecMod[1][0] = pSrc->ecMod[1][0];
		ecMod[1][1] = pSrc->ecMod[1][1];
		ecMod[1][2] = pSrc->ecMod[1][2];
	}
	else if(threadIdx.x % 8 == 7)
	{
		ecMod[1][3] = pSrc->ecMod[1][3];
		ecMod[1][4] = pSrc->ecMod[1][4];
		ecMod[1][5] = pSrc->ecMod[1][5];
		ecMod[2][0] = pSrc->ecMod[2][0];
		ecMod[2][1] = pSrc->ecMod[2][1];
		ecMod[2][2] = pSrc->ecMod[2][2];
		ecMod[2][3] = pSrc->ecMod[2][3];
		ecMod[2][4] = pSrc->ecMod[2][4];
		ecMod[2][5] = pSrc->ecMod[2][5];
	}
	coefficientData[threadIdx.x * 2]     = pSrc->coefficientData[chunkId * 2048 + threadIdx.x * 2];
	coefficientData[threadIdx.x * 2 + 1] = pSrc->coefficientData[chunkId * 2048 + threadIdx.x * 2 + 1];
	searchPattern[threadIdx.x] = pSrc->searchPattern[chunkId * 1024 + threadIdx.x];

	__syncthreads();

	_u32 processedTargetUpper = 0;
	_u32 processedTargetLower = 0;
	for(int i = 0; i < 32; ++i)
	{
		processedTargetUpper |= (GetSignature(answerFlag[i * 2] & targetUpper) ^ GetSignature(answerFlag[i * 2 + 1] & targetLower)) << (31 - i);
		processedTargetLower |= (GetSignature(answerFlag[(i + 32) * 2] & targetUpper) ^ GetSignature(answerFlag[(i + 32) * 2 + 1] & targetLower)) << (31 - i);
	}

	_u32 seeds[7]; // S0Upper�AS0Lower�AS1Upper�AS1Lower
	_u32 next[7]; // S0Upper�AS0Lower�AS1Upper�AS1Lower
	_u64 temp64;
	_u32 temp32;
	for(int i = 0; i < 1024; ++i)
	{
		seeds[0] = processedTargetUpper ^ coefficientData[i * 2];
		seeds[1] = processedTargetLower ^ coefficientData[i * 2 + 1] | searchPattern[i];

		// ��`�ӏ�

		if(ecBit >= 0 && (seeds[1] & 1) != ecBit)
		{
			continue;
		}

		temp64 = ((_u64)seeds[0] << 32 | seeds[1]) + 0x82a2b175229d6a5bull;

		seeds[2] = 0x82a2b175ul;
		seeds[3] = 0x229d6a5bul;

		next[0] = (_u32)(temp64 >> 32);
		next[1] = (_u32)temp64;
		next[2] = 0x82a2b175ul;
		next[3] = 0x229d6a5bul;

		temp64 = ((_u64)seeds[0] << 32 | seeds[1]);

		// ��������i�荞��

		// EC
		temp32 = Next(seeds, 0xFFFFFFFFu);
		// 1�C�ڌ�
		if(ecMod[0][temp32 % 6] == false)
		{
			continue;
		}
		// 2�C�ڌ�
		if(ecMod[1][temp32 % 6] == false)
		{
			continue;
		}

		// EC
		temp32 = Next(next, 0xFFFFFFFFu);
		// 3�C�ڌ�
		if(ecMod[2][temp32 % 6] == false)
		{
			continue;
		}

		// 2�C�ڂ��Ƀ`�F�b�N
		Next(next); // OTID
		Next(next); // PID

		{
			int ivs[6] = { -1, -1, -1, -1, -1, -1 };
			temp32 = 0;
			do {
				int fixedIndex = 0;
				do {
					fixedIndex = Next(next, 7); // V�ӏ�
				} while(fixedIndex >= 6);

				if(ivs[fixedIndex] == -1)
				{
					ivs[fixedIndex] = 31;
					++temp32;
				}
			} while(temp32 < pokemon[2].flawlessIvs);

			// �̒l
			temp32 = 1;
			for(int i = 0; i < 6; ++i)
			{
				if(ivs[i] == 31)
				{
					if(pokemon[2].ivs[i] != 31)
					{
						temp32 = 0;
						break;
					}
				}
				else if(pokemon[2].ivs[i] != Next(next, 0x1F))
				{
					temp32 = 0;
					break;
				}
			}
			if(temp32 == 0)
			{
				continue;
			}
			
			// ����
			temp32 = 0;
			if(pokemon[2].abilityFlag == 3)
			{
				temp32 = Next(next, 1);
			}
			else
			{
				do {
					temp32 = Next(next, 3);
				} while(temp32 >= 3);
			}
			if((pokemon[2].ability >= 0 && pokemon[2].ability != temp32) || (pokemon[2].ability == -1 && temp32 >= 2))
			{
				continue;
			}

			// ���ʒl
			if(!pokemon[2].isNoGender)
			{
				temp32 = 0;
				do {
					temp32 = Next(next, 0xFF);
				} while(temp32 >= 253);
			}

			// ���i
			temp32 = 0;
			do {
				temp32 = Next(next, 0x1F);
			} while(temp32 >= 25);

			if(temp32 != pokemon[2].nature)
			{
				continue;
			}
		}

		// 1�C��
		Next(seeds); // OTID
		Next(seeds); // PIT

		{
			// ��Ԃ�ۑ�
			next[0] = seeds[0];
			next[1] = seeds[1];
			next[2] = seeds[2];
			next[3] = seeds[3];

			{
				int ivs[6] = { -1, -1, -1, -1, -1, -1 };
				temp32 = 0;
				do {
					int fixedIndex = 0;
					do {
						fixedIndex = Next(seeds, 7); // V�ӏ�
					} while(fixedIndex >= 6);

					if(ivs[fixedIndex] == -1)
					{
						ivs[fixedIndex] = 31;
						++temp32;
					}
				} while(temp32 < pokemon[0].flawlessIvs);

				// �̒l
				temp32 = 1;
				for(int i = 0; i < 6; ++i)
				{
					if(ivs[i] == 31)
					{
						if(pokemon[0].ivs[i] != 31)
						{
							temp32 = 0;
							break;
						}
					}
					else if(pokemon[0].ivs[i] != Next(seeds, 0x1F))
					{
						temp32 = 0;
						break;
					}
				}
				if(temp32 == 0)
				{
					continue;
				}
			}
			{
				int ivs[6] = { -1, -1, -1, -1, -1, -1 };
				temp32 = 0;
				do {
					int fixedIndex = 0;
					do {
						fixedIndex = Next(next, 7); // V�ӏ�
					} while(fixedIndex >= 6);

					if(ivs[fixedIndex] == -1)
					{
						ivs[fixedIndex] = 31;
						++temp32;
					}
				} while(temp32 < pokemon[1].flawlessIvs);

				// �̒l
				temp32 = 1;
				for(int i = 0; i < 6; ++i)
				{
					if(ivs[i] == 31)
					{
						if(pokemon[1].ivs[i] != 31)
						{
							temp32 = 0;
							break;
						}
					}
					else if(pokemon[1].ivs[i] != Next(next, 0x1F))
					{
						temp32 = 0;
						break;
					}
				}
				if(temp32 == 0)
				{
					continue;
				}
			}

			// ����
			temp32 = 0;
			if(pokemon[0].abilityFlag == 3)
			{
				temp32 = Next(seeds, 1);
			}
			else
			{
				do {
					temp32 = Next(seeds, 3);
				} while(temp32 >= 3);
			}
			if((pokemon[0].ability >= 0 && pokemon[0].ability != temp32) || (pokemon[0].ability == -1 && temp32 >= 2))
			{
				continue;
			}
			temp32 = 0;
			if(pokemon[1].abilityFlag == 3)
			{
				temp32 = Next(next, 1);
			}
			else
			{
				do {
					temp32 = Next(next, 3);
				} while(temp32 >= 3);
			}
			if((pokemon[1].ability >= 0 && pokemon[1].ability != temp32) || (pokemon[1].ability == -1 && temp32 >= 2))
			{
				continue;
			}

			// ���ʒl
			if(!pokemon[0].isNoGender)
			{
				temp32 = 0;
				do {
					temp32 = Next(seeds, 0xFF);
				} while(temp32 >= 253);
			}
			if(!pokemon[1].isNoGender)
			{
				temp32 = 0;
				do {
					temp32 = Next(next, 0xFF);
				} while(temp32 >= 253);
			}

			// ���i
			temp32 = 0;
			do {
				temp32 = Next(seeds, 0x1F);
			} while(temp32 >= 25);
			if(temp32 != pokemon[0].nature)
			{
				continue;
			}
			temp32 = 0;
			do {
				temp32 = Next(next, 0x1F);
			} while(temp32 >= 25);
			if(temp32 != pokemon[1].nature)
			{
				continue;
			}
		}
		// ���ʂ���������
		int old = atomicAdd(pResultCount, 1);
		pResult[old] = temp64;
	}
	return;
}

// ������
void CudaInitializeImpl()
{
	// �z�X�g�������̊m��
	cudaMallocHost(&cu_HostMaster, sizeof(CudaInputMaster));
	cudaMallocHost(&cu_HostResult, sizeof(_u64) * c_SizeResult);
	cudaMallocHost(&cu_HostResultCount, sizeof(int));

	// �f�[�^�̏�����
	cu_HostMaster->ecBit = -1;

	// �f�o�C�X�������̊m��
	cudaMalloc(&pDeviceMaster, sizeof(CudaInputMaster));
	cudaMalloc(&pDeviceResult, sizeof(_u64) * c_SizeResult);
	cudaMalloc(&pDeviceResultCount, sizeof(int));
}

// �f�[�^�Z�b�g
void CudaSetMasterData(int length)
{
	cu_HostMaster->constantTermVector[0] = (_u32)(g_ConstantTermVector >> 25);
	cu_HostMaster->constantTermVector[1] = (_u32)(g_ConstantTermVector & 0x1FFFFFFull);
//	cu_HostMaster->constantTermVector[0] = (_u32)(g_ConstantTermVector >> (length / 2));
//	cu_HostMaster->constantTermVector[1] = (_u32)(g_ConstantTermVector & (1 << (length / 2 + 1) - 1));
	for(int i = 0; i < 64; ++i)
	{
		cu_HostMaster->answerFlag[i * 2] = (_u32)(g_AnswerFlag[i] >> 25);
		cu_HostMaster->answerFlag[i * 2 + 1] = (_u32)(g_AnswerFlag[i] & 0x1FFFFFFull);
	}
	for(int i = 0; i < 16 * 1024; ++i)
	{
		cu_HostMaster->coefficientData[i * 2] = (_u32)(g_CoefficientData[i] >> 32);
		cu_HostMaster->coefficientData[i * 2 + 1] = (_u32)(g_CoefficientData[i] & 0xFFFFFFFFull);
		cu_HostMaster->searchPattern[i] = (_u32)g_SearchPattern[i];
	}
	*cu_HostResultCount = 0;

	// �f�[�^��]��
	cudaMemcpy(pDeviceMaster, cu_HostMaster, sizeof(CudaInputMaster), cudaMemcpyHostToDevice);
	cudaMemcpy(pDeviceResultCount, cu_HostResultCount, sizeof(int), cudaMemcpyHostToDevice);
}

// �v�Z
void CudaProcess(_u32 ivs, int freeBit)
{
	//�J�[�l��
	dim3 block(c_SizeBlockX, c_SizeBlockY, 1);
	dim3 grid(c_SizeGridX, c_SizeGridY, 1);
	kernel_calc << < grid, block >> > (pDeviceMaster, pDeviceResultCount, pDeviceResult, ivs);

	//�f�o�C�X->�z�X�g�֌��ʂ�]��
	cudaMemcpy(cu_HostResult, pDeviceResult, sizeof(_u64) * c_SizeResult, cudaMemcpyDeviceToHost);
	cudaMemcpy(cu_HostResultCount, pDeviceResultCount, sizeof(int), cudaMemcpyDeviceToHost);
}

void Finish()
{
	//�f�o�C�X�������̊J��
	cudaFree(pDeviceResultCount);
	cudaFree(pDeviceResult);
	cudaFree(pDeviceMaster);
	//�z�X�g�������̊J��
	cudaFreeHost(cu_HostResultCount);
	cudaFreeHost(cu_HostResult);
	cudaFreeHost(cu_HostMaster);
}
