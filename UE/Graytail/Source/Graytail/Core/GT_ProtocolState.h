#pragma once

#include "CoreMinimal.h"
#include "GT_ProtocolState.generated.h"

namespace GT_ProtocolRules
{
	GRAYTAIL_API int32 GetMaxPressure();
	GRAYTAIL_API int32 GetExplorePressure();
	GRAYTAIL_API int32 GetMinePressure();
	GRAYTAIL_API int32 GetCombatKillPressure();
	GRAYTAIL_API int32 ComputeLevel(int32 Pressure);
}

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_ProtocolState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Protocol")
	int32 Level = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Protocol")
	int32 Pressure = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Protocol")
	int32 MaxPressure = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Protocol")
	bool bLevelChanged = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Protocol")
	int32 LastDelta = 0;

	void Reset()
	{
		Level = GT_ProtocolRules::ComputeLevel(0);
		Pressure = 0;
		MaxPressure = GT_ProtocolRules::GetMaxPressure();
		bLevelChanged = false;
		LastDelta = 0;
	}

	float GetPressureRatio() const
	{
		return MaxPressure > 0 ? static_cast<float>(Pressure) / static_cast<float>(MaxPressure) : 0.f;
	}
};

namespace GT_ProtocolRules
{
	inline FString GetLevelDescription(int32 Level)
	{
		switch (Level)
		{
		case 1: return TEXT("最终建议");
		case 2: return TEXT("返程建议");
		case 3: return TEXT("风险作业");
		case 4: return TEXT("轻度警戒");
		case 5: return TEXT("正常作业");
		default: return TEXT("未知");
		}
	}

	inline FString GetLevelBroadcast(int32 Level)
	{
		switch (Level)
		{
		case 1: return TEXT("调度台 A-7：协议 1。撤离是建议。不撤离是自主选择。相关条款已说明。");
		case 2: return TEXT("调度台 A-7：协议 2。建议返程。建议不是命令，但也不是玩笑。");
		case 3: return TEXT("调度台 A-7：协议 3。继续深入将提高收益，也提高事故概率。本条为非强制性建议。");
		case 4: return TEXT("调度台 A-7：协议 4。异常读数上升，请保持职业判断。");
		case 5: return TEXT("调度台 A-7：协议 5。区域读数稳定，继续作业。");
		default: return TEXT("调度台 A-7：协议状态异常。");
		}
	}
}
