#pragma once
#include "Type.h"

extern "C"
{
	__declspec(dllexport) void PrepareSix(int ivOffset);
	__declspec(dllexport) void SetSixFirstCondition(int, int, int, int, int, int, int, int, int, bool, int);
	__declspec(dllexport) void SetSixSecondCondition(int, int, int, int, int, int, int, int, int, bool, int);
	__declspec(dllexport) void SetSixThirdCondition(int, int, int, int, int, int, int, int, int, bool, int);
	__declspec(dllexport) void SetTargetCondition6(int iv1, int iv2, int iv3, int iv4, int iv5, int iv6);
	__declspec(dllexport) void SetTargetCondition5(int iv1, int iv2, int iv3, int iv4, int iv5);
	__declspec(dllexport) _u64 SearchSix(_u64);
}
