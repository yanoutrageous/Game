#pragma once

#include "CoreMinimal.h"
#include "GT_ProtocolState.generated.h"

// 五四三二一协议运行时状态(对齐 Protocol.lua)。
// 挂在 UGT_RunContext 上, 一局一份。
// 压力随行动上升, 触发阈值时协议等级下降(5→1), 满压强制败北。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_ProtocolState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Protocol")
	int32 Level = 5;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Protocol")
	int32 Pressure = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Protocol")
	int32 MaxPressure = 100;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Protocol")
	bool bLevelChanged = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Protocol")
	int32 LastDelta = 0;

	void Reset()
	{
		Level = 5;
		Pressure = 0;
		MaxPressure = 100;
		bLevelChanged = false;
		LastDelta = 0;
	}

	float GetPressureRatio() const
	{
		return MaxPressure > 0 ? static_cast<float>(Pressure) / static_cast<float>(MaxPressure) : 0.f;
	}
};

// 协议规则常量(对齐 Balance.pressure)。
namespace GT_ProtocolRules
{
	constexpr int32 MaxPressure = 100;
	constexpr int32 ExplorePressure = 2;    // 首次探索未知房
	constexpr int32 MinePressure = 10;      // 踩雷
	constexpr int32 CombatKillPressure = 5; // 击杀怪物

	// 阈值: 压力 >= threshold 时协议等级 = level。从高到低排列。
	struct FPressureThreshold
	{
		int32 Pressure;
		int32 Level;
	};

	constexpr FPressureThreshold Thresholds[] = {
		{ 80, 1 },
		{ 60, 2 },
		{ 40, 3 },
		{ 20, 4 },
		{ 0,  5 },
	};
	constexpr int32 ThresholdCount = sizeof(Thresholds) / sizeof(Thresholds[0]);

	// 对齐 Protocol._ComputeLevel: 从高到低遍历阈值。
	inline int32 ComputeLevel(int32 Pressure)
	{
		for (int32 i = 0; i < ThresholdCount; ++i)
		{
			if (Pressure >= Thresholds[i].Pressure)
			{
				return Thresholds[i].Level;
			}
		}
		return 5;
	}

	// 协议等级描述(对齐 GameText.protocol.levels[*].short)。
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

	// 协议等级播报(对齐 GameText.protocol.broadcasts[*])。
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
