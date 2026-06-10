#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/GT_ActorTypes.h"
#include "Domains/Map/GT_MapTypes.h"
#include "Domains/Inventory/GT_InventoryTypes.h"
#include "GT_RunContext.generated.h"

struct FGT_MapGenerationSpec;

UENUM(BlueprintType)
enum class EGT_RunState : uint8
{
	NotStarted UMETA(DisplayName = "Not Started"),
	Running UMETA(DisplayName = "Running"),
	Failed UMETA(DisplayName = "Failed"),
	Succeeded UMETA(DisplayName = "Succeeded"),
	Ended UMETA(DisplayName = "Ended")
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_CombatRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	bool bCombatActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	bool bCombatResolved = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 DummyEnemyHp = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 CombatX = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 CombatY = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	FName RoomContentId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	FName RoomRuleId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	FName LastCombatResultId = NAME_None;
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_RunSummary
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run")
	bool bSummaryAvailable = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run")
	FName Outcome = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run")
	bool bExtracted = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run")
	int32 FinalPlayerX = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run")
	int32 FinalPlayerY = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run")
	int32 TotalEventCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run")
	int32 Seed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run")
	int32 MapWidth = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run")
	int32 MapHeight = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run")
	bool bCombatActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run")
	bool bCombatResolved = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run")
	FName LastCombatResultId = NAME_None;
};

UCLASS(BlueprintType)
class GRAYTAIL_API UGT_RunContext : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Graytail|Run")
	void InitializeRun(int32 InSeed, int32 InWidth = 10, int32 InHeight = 10);

	// 按难度档位开局(Standard 随机地图)。与 InitializeRun 共享后半初始化逻辑。
	UFUNCTION(BlueprintCallable, Category = "Graytail|Run")
	void InitializeRunStandard(int32 InSeed, EGT_Difficulty Difficulty);

	UFUNCTION(BlueprintCallable, Category = "Graytail|Run")
	void ResetRun();

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	FGuid GetRunId() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	int32 GetSeed() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	int32 GetMapWidth() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	int32 GetMapHeight() const;

	const FGT_TruthMap& GetTruthMapForDebugOnly() const;
	const FGT_IntelMap& GetPlayerIntelMap() const;
	const TArray<FGT_ActorRuntimeState>& GetActorStates() const;
	FGT_ActorRuntimeState* FindActorStateMutable(FName ActorId);
	const FGT_ActorRuntimeState* FindActorState(FName ActorId) const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	FName GetPlayerActorId() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	bool TryGetPlayerPosition(int32& OutX, int32& OutY) const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	bool IsValidMapCoord(int32 X, int32 Y) const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	EGT_RunState GetRunState() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	bool IsRunActive() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	bool IsRunFailed() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Run")
	bool IsRunSucceeded() const;

	bool MarkRunFailed(FName Reason);
	bool MarkRunSucceeded(FName Reason);

	bool MarkPlayerIntelCellExplored(int32 X, int32 Y);
	bool MarkPlayerIntelCellVisible(int32 X, int32 Y, bool bVisible);
	bool CountAdjacentMines8(int32 X, int32 Y, int32& OutMineCount) const;
	bool SetPlayerIntelCellScannedNumber(int32 X, int32 Y, int32 InDisplayedNumber);
	bool GetTruthCellSnapshot(int32 X, int32 Y, FGT_TruthCell& OutCell) const;
	bool MarkTruthCellEntered(int32 X, int32 Y);
	bool MarkTruthCellResolved(int32 X, int32 Y);
	bool StartDummyCombat(int32 X, int32 Y, FName RoomContentId, FName RoomRuleId, int32 InitialDummyHp = 1);
	bool AttackDummyCombat(FGT_CombatRuntimeState& OutState);
	bool ResolveDummyCombat(FName ResultId, FGT_CombatRuntimeState& OutState);
	bool GetCombatStateSnapshot(FGT_CombatRuntimeState& OutState) const;
	bool GenerateExtractSummary(int32 TotalEventCount);
	bool GetRunSummarySnapshot(FGT_RunSummary& OutSummary) const;

	const FGT_RunInventoryState& GetRunInventory() const;

	// 玩家当前格能否搜索。不能搜索时 OutReason 给出原因(对齐 Lua GetSearchState 的 reason)。
	bool EvaluateSearchAtPlayer(FName& OutReason, bool& bOutIsChest) const;

	// 搜索玩家当前格: 判定 -> 确定性结奖 -> 入账并标记已搜。失败时 Outcome.Status 是原因。
	bool SearchCurrentRoom(FGT_SearchOutcome& OutOutcome);

private:
	// 新老开局路径共享的初始化逻辑: 生成地图、放置玩家、重置运行态。
	void InitializeFromSpec(const FGT_MapGenerationSpec& MapSpec);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	FGuid RunId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	EGT_RunState RunState = EGT_RunState::NotStarted;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	FName RunEndReason = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	int32 Seed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	int32 MapWidth = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	int32 MapHeight = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	FGT_TruthMap TruthMap;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	FGT_IntelMap PlayerIntelMap;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	TArray<FGT_ActorRuntimeState> ActorStates;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	FName PlayerActorId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat", meta = (AllowPrivateAccess = "true"))
	FGT_CombatRuntimeState CombatRuntimeState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	FGT_RunSummary RunSummary;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Inventory", meta = (AllowPrivateAccess = "true"))
	FGT_RunInventoryState RunInventory;
};
