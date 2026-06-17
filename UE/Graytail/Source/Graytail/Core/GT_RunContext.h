#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/GT_ActorTypes.h"
#include "Core/GT_ProtocolState.h"
#include "Domains/Map/GT_MapTypes.h"
#include "Domains/Inventory/GT_InventoryTypes.h"
#include "Domains/Combat/GT_CombatRules.h"
#include "Domains/Combat/GT_MonsterCatalog.h"
#include "Domains/Events/GT_EventTypes.h"
#include "Domains/Meta/GT_MetaTypes.h"
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

	// Standard 模式真怪信息(bStandardEnemy=false 时为 BasicDebug 的 1 血 dummy)。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	bool bStandardEnemy = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	FString EnemyName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 EnemyPower = 0;

	// Standard 实时战斗怪物状态(对齐 Combat.lua ensureMonsterState)。
	// monsterMaxHP = HpBase + power; monsterDamage = max(4, power/3)。BasicDebug 不用(走 DummyEnemyHp)。
	// EnemyType = 行为原型(决定移动/攻击模式/数值基底, 见 GT_MonsterCatalog)。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	EGT_MonsterType EnemyType = EGT_MonsterType::Slime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 EnemyHp = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 EnemyMaxHp = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat")
	int32 EnemyDamage = 0;
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

	// Standard 实时战斗: 怪物对玩家造成一次伤害(对齐 Combat.UpdateEnemy 的 active 命中分支)。
	// 无敌帧由表现层(RoomView)门控, 此处只在战斗激活、怪未死时按 EnemyDamage 扣血。
	// OutDamage = 实际扣血; bOutDead = 扣血后是否归零。返回是否真打到(战斗未激活/怪已死则 false)。
	bool MonsterHitPlayer(int32& OutDamage, bool& bOutDead);
	bool ResolveDummyCombat(FName ResultId, FGT_CombatRuntimeState& OutState);
	bool GetCombatStateSnapshot(FGT_CombatRuntimeState& OutState) const;
	bool GenerateExtractSummary(int32 TotalEventCount);
	bool GetRunSummarySnapshot(FGT_RunSummary& OutSummary) const;

	const FGT_RunInventoryState& GetRunInventory() const;

	const FGT_PlayerCombatState& GetPlayerCombatState() const;
	EGT_MapMode GetMapMode() const;
	bool IsPlayerAlive() const;

	// 踩雷扣血(Standard 模式规则, 对齐 Combat.TakeMineHit)。返回实际伤害与是否致死。
	void ApplyMineHitToPlayer(int32& OutDamage, bool& bOutDead);

	// 开局应用局外 loadout(S3): 设属性加成 + 存规则层加成 + 灌带入消耗品。
	// 仅 Standard 局由 RunSubsystem 在初始化后调用(RunContext 不依赖 MetaProgress 子系统, 只收纯数据)。
	// EquippedItemIds = 已装备装备 id 列表(纯数据), 用于激活 S6 触发型物品(异常体犬牙/封锁区结晶/回收磁石)。
	void ApplyMetaLoadout(const FGT_EquipBonus& Equip, const FGT_TalentEffects& Talents, const TMap<FName, int32>& Consumables, const TArray<FName>& EquippedItemIds);

	// 协议压力系统(对齐 Protocol.lua): 压力随行动上升, 触发阈值时等级下降, 满压强制败北。
	// 返回值包含 level/pressure/changed/bForcedFail(压力满时触发败北)。
	// 已探索格子不会重复加压(对齐 Lua 里只在首次探索未知房时加压)。
	bool MarkExploredForPressure(int32 X, int32 Y);
	bool IsExploredForPressure(int32 X, int32 Y) const;

	struct FProtocolPressureResult
	{
		int32 Level = 5;
		int32 Pressure = 0;
		bool bLevelChanged = false;
		bool bForcedFail = false;
	};
	FProtocolPressureResult AddProtocolPressure(int32 Amount);

	// Standard 满压惩罚(对齐"信号中断"改设计): 已满压时每进一个新房按难度扣血,
	// 替代原来的"满压直接败北"。返回是否真扣了血; OutDead = 扣血后是否归零。仅 Standard 生效。
	bool ApplyMaxPressureRoomPenalty(int32& OutDamage, bool& bOutDead);

	UFUNCTION(BlueprintPure, Category = "Graytail|Protocol")
	const FGT_ProtocolState& GetProtocolState() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Protocol")
	int32 GetProtocolLevel() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Protocol")
	int32 GetProtocolPressure() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Protocol")
	int32 GetProtocolMaxPressure() const;

	UFUNCTION(BlueprintPure, Category = "Graytail|Protocol")
	float GetProtocolPressureRatio() const;

	// 玩家当前格能否搜索。不能搜索时 OutReason 给出原因(对齐 Lua GetSearchState 的 reason)。
	bool EvaluateSearchAtPlayer(FName& OutReason, bool& bOutIsChest) const;

	// 搜索玩家当前格: 判定 -> 确定性结奖 -> 入账并标记已搜。失败时 Outcome.Status 是原因。
	bool SearchCurrentRoom(FGT_SearchOutcome& OutOutcome);

	// 最近一次成功搜索的结算明细(开新局重置), 供 UI 结果弹窗读取。
	const FGT_SearchOutcome& GetLastSearchOutcome() const { return LastSearchOutcome; }

	// 使用玩家背包里的消耗品(对齐 Lua RunInventory.UseConsumable)。
	// 止血贴: 回血 min(25, MaxHp-Hp), 满血/无库存/非消耗品时失败并给出 Status。
	// 必须经 Command 管线(UseConsumable 命令)调到这里, 不直接给 UI 调。
	bool UseConsumableAtPlayer(FName ItemId, FGT_ConsumableOutcome& OutOutcome);

	// 最近一次消耗品使用结果, 供 UI 提示(回血量/失败原因)。
	const FGT_ConsumableOutcome& GetLastConsumableOutcome() const { return LastConsumableOutcome; }

	// 事件房真实规则(旅商/赌徒/祭坛/机关, 仅 Standard 模式; BasicDebug 走注册表占位路径)。
	// 菜单为只读查询; 执行必须经 Command 管线(ChooseEventOption 命令)调到这里。
	bool GetEventMenuAtPlayer(FGT_EventMenuView& OutMenu) const;
	bool ExecuteEventOptionAtPlayer(FName OptionId, FGT_EventOutcome& OutOutcome);

	// 最近一次事件选项执行的结果(含失败原因文案), 供事件面板显示。
	const FGT_EventOutcome& GetLastEventOutcome() const { return LastEventOutcome; }

	// 本局结束原因(Mine/Protocol/CombatDeath 等), 进行中为 None。供局终结算面板显示。
	FName GetRunEndReason() const { return RunEndReason; }

private:
	// 新老开局路径共享的初始化逻辑: 生成地图、放置玩家、重置运行态。
	void InitializeFromSpec(const FGT_MapGenerationSpec& MapSpec);

	// 事件房运行态查找(按坐标懒创建)。
	const FGT_EventRoomState* FindEventRoomState(int32 X, int32 Y) const;
	FGT_EventRoomState& FindOrAddEventRoomState(int32 X, int32 Y);

	UPROPERTY(Transient)
	FGT_SearchOutcome LastSearchOutcome;

	UPROPERTY(Transient)
	FGT_EventOutcome LastEventOutcome;

	UPROPERTY(Transient)
	FGT_ConsumableOutcome LastConsumableOutcome;

	// 事件房运行态(Standard 模式: 完成标记/祭坛档数), 开局重置。
	UPROPERTY(Transient)
	TArray<FGT_EventRoomState> EventRoomStates;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat", meta = (AllowPrivateAccess = "true"))
	FGT_PlayerCombatState PlayerCombatState;

	// S3: 局外 loadout 在本局生效的加成(开局由 ApplyMetaLoadout 灌入; InitializeFromSpec/ResetRun 重置)。
	int32 LoadoutMineDmgReduce = 0;
	bool bLoadoutMineImmunityAvailable = false;
	int32 LoadoutSearchBonusPercent = 0;
	int32 LoadoutTradeBonusPercent = 0;   // S4: 议价天赋 → 旅商收购价 +N%(0 = 无议价, 基础 0.75 不变)
	// 本局难度(Standard 满压惩罚按难度分档扣血; BasicDebug 不用)。
	EGT_Difficulty CurrentDifficulty = EGT_Difficulty::Standard;

	// S6 触发型物品本局状态(开局 ApplyMetaLoadout 按已装备物品激活; InitializeFromSpec/ResetRun 重置)。
	bool bLoadoutKillPowerStack = false;   // 异常体犬牙: 战斗胜利叠攻
	int32 KillPowerStackCap = 0;
	int32 KillPowerStackAmount = 0;
	int32 KillPowerStacksUsed = 0;
	bool bLoadoutProtocolHeal = false;     // 封锁区结晶: 协议升级回血
	int32 ProtocolHealCap = 0;
	int32 ProtocolHealAmount = 0;
	int32 ProtocolHealsUsed = 0;
	bool bLoadoutChestBonusLoot = false;   // 回收磁石: 进宝箱房额外掉
	TArray<FIntPoint> ChestBonusGrantedCells;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Protocol", meta = (AllowPrivateAccess = "true"))
	FGT_ProtocolState ProtocolState;

	// 本局地图模式: Standard 用真实战斗/雷伤规则, BasicDebug 保持测试夹具行为。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Run", meta = (AllowPrivateAccess = "true"))
	EGT_MapMode MapMode = EGT_MapMode::Unknown;

	// Standard 模式已击杀的战斗房坐标(注意: TruthCell.bResolved 在进房时就被置位, 不能用来判断)。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Combat", meta = (AllowPrivateAccess = "true"))
	TArray<FIntPoint> DefeatedCombatRooms;

	// 已因探索而加过协议压力的格子坐标(防重复加压)。
	UPROPERTY(Transient, meta = (AllowPrivateAccess = "true"))
	TArray<FIntPoint> PressureExploredCells;
};
