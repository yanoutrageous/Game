#pragma once

#include "CoreMinimal.h"
#include "GT_MonsterCatalog.generated.h"

// 怪物类型(行为原型 id)。**只在末尾追加新类型**(对齐 UENUM 铁律)。
// 加新怪 = 这里加一项 + GT_MonsterCatalog.cpp 的表里加一行原型 + 需要时给 PickTypeForCell 加选型权重。
UENUM(BlueprintType)
enum class EGT_MonsterType : uint8
{
	Slime UMETA(DisplayName = "Slime"),   // 第 1 个原型: 缓慢追击 + 近战预警圈(对齐原版)
	Bat UMETA(DisplayName = "Bat"),       // 蝙蝠: 快速 kiting + 散射多发偏角子弹
	Drone UMETA(DisplayName = "Drone"),   // 无人机: 慢速 kiting + 蓄力持续激光
};

// 移动模式(表现层 RoomView 按此分派)。
UENUM(BlueprintType)
enum class EGT_MonsterMovePattern : uint8
{
	Stationary UMETA(DisplayName = "Stationary"),       // 原地不动(纯原版)
	ChasePlayer UMETA(DisplayName = "Chase Player"),    // 朝玩家缓慢逼近, 进攻击圈即停
	KeepDistance UMETA(DisplayName = "Keep Distance"),  // 远程 kiting: 太近后撤、到理想距离停下打、太远(快怪)跟近
};

// 攻击模式(表现层 RoomView 按此分派; 伤害仍走 MonsterHit 命令)。
UENUM(BlueprintType)
enum class EGT_MonsterAttackPattern : uint8
{
	MeleeCircle UMETA(DisplayName = "Melee Circle"),        // 地面预警圈 -> 填充判定圈(对齐原版近战)
	RangedProjectile UMETA(DisplayName = "Ranged Projectile"), // 红线瞄准 -> 单发飞弹(PR#9 远程)
	RangedSpread UMETA(DisplayName = "Ranged Spread"),         // 蝙蝠: 朝玩家方向 ±角度散射 N 发直线飞弹
	RangedLaser UMETA(DisplayName = "Ranged Laser"),           // 无人机: 蓄力 -> 锁向粗光束持续 + 站内每隔扣血
};

// 怪物行为原型(纯数据, 非 UPROPERTY): 数值 + AI 画像 + 实时相位时长 + 贴图。
// HP/伤害的战力缩放公式在 GT_CombatRules(共享); 这里给每类的基底与画像。
struct FGT_MonsterArchetype
{
	EGT_MonsterType Type = EGT_MonsterType::Slime;
	EGT_MonsterMovePattern MovePattern = EGT_MonsterMovePattern::ChasePlayer;
	EGT_MonsterAttackPattern AttackPattern = EGT_MonsterAttackPattern::MeleeCircle;

	int32 HpBase = 18;            // monsterMaxHP = HpBase + power(对齐 Combat.lua monsterHpBase)
	float MoveSpeed = 0.16f;      // 追击速度(归一化坐标/秒)
	float AttackRadius = 0.20f;   // 判定圈半径(对齐 CONFIG.monsterAttackRadius)
	float PlayerAttackRange = 0.21f; // 玩家挥砍射程(对齐 CONFIG.playerAttackRange)

	// 攻击相位时长(对齐 Combat.lua CONFIG)。
	float IdleDuration = 1.10f;
	float WarningDuration = 0.75f;
	float ActiveDuration = 0.28f;
	float CooldownDuration = 0.55f;

	// 远程 kiting / 攻击参数(近战型留默认 0)。
	float IdealDistance = 0.f;        // KeepDistance 理想距离(0 = 不保持距离, 走 ChasePlayer/Stationary)
	float AimDuration = 0.f;          // 远程瞄准/蓄力时长
	float AttackInterval = 0.f;       // 远程攻击大循环间隔
	int32 SpreadCount = 0;            // RangedSpread 弹数
	float SpreadHalfAngleDeg = 0.f;   // RangedSpread 左右偏角(度)
	float LaserDuration = 0.f;        // RangedLaser 光束持续时长
	float LaserTickInterval = 0.f;    // RangedLaser 站内扣血间隔
	float LaserTurnRateDeg = 0.f;     // RangedLaser 发射后光束每秒朝玩家旋转的最大角度(0=锁死不追)

	// KeepDistance 走位权重(让不同远程怪手感不同)。
	float KiteStrength = 1.0f;        // kiting 方向权重(低=更随机/只轻微远离, 易被追打)
	float WanderWeight = 0.5f;        // 随机游走权重
	bool bChaseWhenFar = false;       // 太远是否追回保持射程(false=只远离+乱窜, 不黏射程)

	const TCHAR* SpritePath = TEXT("/Game/Graytail/Sprites/enemy_slime");
	FLinearColor TintColor = FLinearColor::White;   // 占位染色(区分怪种; 真贴图后置 White)
};

namespace GT_MonsterCatalog
{
	// 取某类型的原型(未知类型回退到 Slime)。
	GRAYTAIL_API const FGT_MonsterArchetype& GetArchetype(EGT_MonsterType Type);

	// 确定性选型(对齐 MakeEnemyForCell 的 seed+坐标哈希思路): 同 seed 同格稳定。
	// 目前只有 Slime; 加类型后在此按难度/深度/哈希分配权重。
	GRAYTAIL_API EGT_MonsterType PickTypeForCell(int32 Seed, int32 X, int32 Y, int32 AdjacentMineCount);
}
