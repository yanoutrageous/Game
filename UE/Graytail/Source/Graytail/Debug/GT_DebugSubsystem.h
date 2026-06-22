#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Debug/GT_DebugTypes.h"
#include "Domains/Events/GT_EventTypes.h"
#include "Domains/Map/GT_MapTypes.h"
#include "Debug/GT_RuntimeSmokeValidator.h"
#include "UI/ViewModels/GT_MiniMapViewModel.h"
#include "GT_DebugSubsystem.generated.h"

class UGT_RunSubsystem;

UCLASS()
class GRAYTAIL_API UGT_DebugSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Graytail|Debug")
	FString GetCurrentRunDebugSummary() const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugStartNewRun(int32 Seed, int32 Width, int32 Height, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugStartStandardRun(int32 Seed, EGT_Difficulty Difficulty, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugMoveTo(int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugScanCell(int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugSearch(FGT_DebugRunSnapshot& OutSnapshot);

	// 上帝模式瞬移: 直接改写玩家坐标, 不走命令管线, 不触发踩雷/房间结算。仅调试用。
	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugTeleport(int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugExtract(FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugChooseEventOption(FName OptionId, FGT_DebugRunSnapshot& OutSnapshot);

	// 玩家当前格的事件菜单(只读查询, 仅 Standard 模式事件房可用)。
	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugGetEventMenu(FGT_EventMenuView& OutMenu) const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugResolveCombat(FName ResultId, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugAttack(FGT_DebugRunSnapshot& OutSnapshot);

	// 怪物对玩家落地一次攻击(Standard 实时战斗, 由 RoomView 在命中时机调用)。
	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugMonsterHit(FGT_DebugRunSnapshot& OutSnapshot);

	// 使用玩家背包里的消耗品(默认应急止血贴)。经命令管线走真规则。
	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool DebugUseConsumable(FName ItemId, FGT_DebugRunSnapshot& OutSnapshot);

	// ---- 调试作弊命令(gt.God/gt.AddGold/gt.GiveItem/gt.SetHp/gt.Goto)----
	// 切换无敌(危险伤害归零)。
	bool DebugSetGodMode(bool bEnabled, FGT_DebugRunSnapshot& OutSnapshot);
	// 直接加本局待结算金币。
	bool DebugAddGold(int32 Amount, FGT_DebugRunSnapshot& OutSnapshot);
	// 直接塞物品进背包(无视容量); 未知 ItemId 返回 false。
	bool DebugGiveItem(FName ItemId, int32 Count, FGT_DebugRunSnapshot& OutSnapshot);
	// 直接设玩家当前生命(夹到 1..MaxHp)。
	bool DebugSetHp(int32 NewHp, FGT_DebugRunSnapshot& OutSnapshot);
	// 快速进入最近的指定房型(chest/combat/event/exit, 或事件子类 trader/dice/altar/trap)。
	// 实现 = god 瞬移到目标邻格 + 真实走入目标格(触发房间内容, 如怪物房开战)。
	bool DebugGotoRoomType(const FString& TypeArg, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool GetDebugRunSnapshot(FGT_DebugRunSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool GetDebugMiniMapViewData(TArray<FGT_MiniMapCellViewData>& OutCells, int32& OutWidth, int32& OutHeight) const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	void GetDebugEventSummary(TArray<FGT_DebugEventSummary>& OutEvents) const;

	// 生成一张 Standard 随机地图并返回 ASCII 预览行 (不影响当前 run, 仅用于调试验证)。
	void BuildStandardMapPreviewLines(int32 Seed, int32 Width, int32 Height, TArray<FString>& OutLines) const;

	void GetDebugCommandHelpLines(TArray<FString>& OutLines) const;
	bool GetDebugStatusText(FString& OutStatus) const;
	bool GetDebugInventoryText(FString& OutInventoryText) const;
	bool GetDebugRoomText(FString& OutRoomText) const;
	bool GetDebugRunSummaryText(FString& OutSummaryText) const;
	bool DebugRunDemo(TArray<FString>& OutLogLines, FGT_DebugRunSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	void GetCurrentMiniMapDebugCells(TArray<FGT_MiniMapCellViewData>& OutCells, int32& OutWidth, int32& OutHeight) const;

	UFUNCTION(BlueprintCallable, Category = "Graytail|Debug")
	bool RunMinimalMovementSmokeTest(TArray<FGT_RuntimeSmokeCheckResult>& OutResults);

private:
	UGT_RunSubsystem* GetRunSubsystem() const;
	bool SubmitDebugCommand(FName CommandType, int32 X, int32 Y, FGT_DebugRunSnapshot& OutSnapshot, FName PayloadId = NAME_None);
};
