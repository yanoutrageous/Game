#pragma once

#include "CoreMinimal.h"
#include "Domains/Inventory/GT_InventoryTypes.h"
#include "GT_EventTypes.generated.h"

// 事件房类型(对齐 Lua EventSystem.EVENT_TYPES): 旅商/赌徒/祭坛/机关。
// 按 (seed, x, y) 哈希加权指派, 一局内同一格的事件类型固定。
UENUM(BlueprintType)
enum class EGT_EventKind : uint8
{
	None UMETA(DisplayName = "None"),
	Trader UMETA(DisplayName = "Trader"),
	Dice UMETA(DisplayName = "Dice"),
	Altar UMETA(DisplayName = "Altar"),
	Trap UMETA(DisplayName = "Trap")
};

// 事件面板里的一个选项(对齐 Lua EventSystem option 表)。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_EventOptionView
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	FName OptionId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	FString Label;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	FString Description;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	FString CostText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	FString RewardText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	FString RiskText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	bool bEnabled = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	FString DisabledReason;
};

// 玩家当前格的事件菜单快照(对齐 Lua EventSystem.GetOptions 的返回)。
// 仅 Standard 模式可用; BasicDebug 的事件房走注册表占位路径。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_EventMenuView
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	bool bAvailable = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	EGT_EventKind Kind = EGT_EventKind::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	FString Title;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	FString Description;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	bool bCompleted = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	TArray<FGT_EventOptionView> Options;
};

// 一次事件选项执行的结果(对齐 Lua EventSystem result 表 + ApplyEventResult 的入账)。
USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_EventOutcome
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	bool bOk = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	FString Message;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	EGT_EventKind Kind = EGT_EventKind::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	FName OptionId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	int32 PendingGoldDelta = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	int32 SafeGoldDelta = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	int32 HpDelta = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	int32 PressureDelta = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	TArray<FGT_ItemStack> GrantedItems;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	FName SoldItemId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	bool bCompleted = false;

	// true = 面板应关闭(旅商成交/赌局结束/机关处理); 祭坛可连续献祭, 面板保持打开。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Event")
	bool bClosePanel = false;
};

// 一个事件房的运行态(Standard 模式, 按坐标懒创建, 开局重置)。
USTRUCT()
struct GRAYTAIL_API FGT_EventRoomState
{
	GENERATED_BODY()

	UPROPERTY()
	int32 X = INDEX_NONE;

	UPROPERTY()
	int32 Y = INDEX_NONE;

	UPROPERTY()
	bool bCompleted = false;

	// 祭坛已献祭档数(0-5), 对齐 Lua optionState.altarStep。
	UPROPERTY()
	int32 AltarStep = 0;
};
