#pragma once

#include "CoreMinimal.h"

class UGT_RunContext;
class UGT_MetaProgressSubsystem;
struct FGT_MetaOperationResult;

// 局终结算桥:把一局结束的 RunContext 数据结算进局外 MetaProgress。
// 仅由 RunSubsystem 在 Standard 模式局终时调用(BasicDebug 不结算)。
// 隔离 RunSubsystem 与"如何读背包/算奖励"的细节, MetaProgress 仍不依赖 RunContext。
namespace GT_MetaSettlement
{
	// 撤离成功:加币(pending+safe) + 回收物入仓库 + 统计(对齐 Lua GetExtractionReward + RecordExtractionReward)。
	FGT_MetaOperationResult SettleExtraction(const UGT_RunContext& Run, UGT_MetaProgressSubsystem& Meta);

	// 失败抢救:保留 SafeGold + 撤离天赋补币 + 自动抢救最高价值 1 件(对齐 Lua GetFailureSalvageOptions/ApplyFailureSalvage)。
	FGT_MetaOperationResult SettleFailure(const UGT_RunContext& Run, UGT_MetaProgressSubsystem& Meta);
}
