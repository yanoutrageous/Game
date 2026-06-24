#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Domains/Combat/GT_MonsterCatalog.h"   // EGT_MonsterType (帧动画/按怪种死亡选图)
#include "GT_RoomViewWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UGT_DebugSubsystem;
class UGT_RunContext;
class FGTMovementKeyProcessor;

// 2D 房间视图(对齐 Lua DungeonRoom.lua / TapTap 版):
// 一屏一个房间, 人物 WASD 实时行走, 对准四边的门走出边界 -> 发 Move 命令切相邻格。
// 表现层薄壳: 只发命令/读状态, 移动合法性/踩雷/战斗全由内核裁决。
UCLASS()
class GRAYTAIL_API UGT_RoomViewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

	// 记账 WASD 持键状态。焦点在弹窗/按钮上时由 HUD 把冒泡事件转发过来,
	// 保证状态始终与物理按键一致(关弹窗后按住的键立刻续走, 不卡键不断手感)。
	void SetHeldMovementKey(const FKey& Key, bool bDown);

	// 播放开箱金光 + 奖励飘字(对齐 Lua TriggerChestOpen/chestRewardBurst)。
	void PlayChestRewardBurst(int32 Gold, int32 Parts);

	// 止血绿光(玩家脚下绿圆扩散淡出) / 踩雷红光(全房间红闪)。纯表现层反馈。
	void PlayHealGlow();
	void PlayMineFlash();

	// 房间状态变化后(开局/外部移动)由 HUD 调用, 重读当前格并重绘。
	void SyncToCurrentCell(bool bCenterPlayer);

	// 玩家挥砍冷却闸门(对齐 Combat.lua playerAttackCooldown): 冷却到则起冷却并返回 true 放行攻击。
	// HUD 的 F 攻击分支调用, 防 F 自动重复连发秒杀。
	bool TryConsumePlayerAttack();

	// WASD 过门移动成功后通知 HUD 刷新信息面板(不含房间视图自身)。
	FSimpleDelegate OnRoomChanged;

	// 实时战斗中玩家血量/战斗状态变化后, 通知 HUD 轻量刷新(血量/协议/失败界面, 不抢焦点)。
	FSimpleDelegate OnCombatStateChanged;

	// F 键搜索/开箱(对齐原版快捷键), 由 HUD 绑定到 OnSearch。
	FSimpleDelegate OnSearchRequested;

	// M 键打开全屏区域扫描图, 由 HUD 绑定到 OpenMapOverlay。
	FSimpleDelegate OnMapRequested;

	// E 键撤离(对齐原版底栏), 由 HUD 绑定到 OnExtract。
	FSimpleDelegate OnExtractRequested;

	// T 键处理事件(对齐原版 [T] 交互), 由 HUD 绑定到 OpenEventPanel。
	FSimpleDelegate OnEventRequested;

	// Q 键使用消耗品/止血(对齐原版底栏), 由 HUD 绑定到 OnUseConsumable。
	FSimpleDelegate OnConsumableRequested;

	// 数字键 1-9 选择左下道具栏槽位(1-based), 由 HUD 绑定到 SelectConsumableSlot。
	DECLARE_DELEGATE_OneParam(FGT_ItemSlotDelegate, int32);
	FGT_ItemSlotDelegate OnItemSlotRequested;

private:
	void BuildWidgetTree();
	UGT_DebugSubsystem* GetDebugSubsystem() const;
	const UGT_RunContext* GetRunContext() const;
	void PlaySfx(FName Key);   // 经 GT_SettingsSubsystem 播一次性音效(走 SfxVolume)
	UTexture2D* LoadTextureAsset(const FString& AssetPath);
	UTexture2D* GetWalkFrame(int32 DirX, int32 DirY, int32 FrameIndex);
	UTexture2D* GetIdleFrame(int32 DirX, int32 DirY);
	void TryCrossDoor(int32 DirX, int32 DirY);
	void UpdatePlayerImagePosition();
	void RefreshRoomDecor();
	void UpdateChestBurstAnim(float DeltaTime);

	UPROPERTY(Transient) UCanvasPanel* RoomCanvas = nullptr;
	UPROPERTY(Transient) UBorder* FloorBorder = nullptr;
	UPROPERTY(Transient) UImage* DoorImages[4] = {};
	UPROPERTY(Transient) UImage* GlowOuter = nullptr;
	UPROPERTY(Transient) UImage* GlowInner = nullptr;
	// 止血绿色光子(玩家上层放射上升淡出) / 踩雷红光(全房间最上层)。
	UPROPERTY(Transient) UImage* HealParticles[8] = {};
	UPROPERTY(Transient) UImage* MineFlash = nullptr;
	UPROPERTY(Transient) UImage* ChestImage = nullptr;
	UPROPERTY(Transient) UImage* GoldPileImage = nullptr;
	UPROPERTY(Transient) UImage* PartsPileImage = nullptr;
	UPROPERTY(Transient) UTextBlock* ChestCaption = nullptr;
	// 事件房 NPC(对齐 Lua 原版简单图形): 圆身体+顶部标识+眼睛+名字+头顶 T 提示, 仅 Standard 事件房可见。
	UPROPERTY(Transient) UImage* EventBodyImage = nullptr;
	UPROPERTY(Transient) UImage* EventMarkerImage = nullptr;
	UPROPERTY(Transient) UImage* EventEyeLeft = nullptr;
	UPROPERTY(Transient) UImage* EventEyeRight = nullptr;
	UPROPERTY(Transient) UTextBlock* EventNameLabel = nullptr;
	UPROPERTY(Transient) UTextBlock* EventCaption = nullptr;
	UPROPERTY(Transient) UImage* BurstGoldImage = nullptr;
	UPROPERTY(Transient) UTextBlock* BurstGoldText = nullptr;
	UPROPERTY(Transient) UImage* BurstPartsImage = nullptr;
	UPROPERTY(Transient) UTextBlock* BurstPartsText = nullptr;
	UPROPERTY(Transient) UImage* EnemyImage = nullptr;
	UPROPERTY(Transient) UImage* DeathEffectImage = nullptr;   // 怪物死亡碎裂特效层(独立于 EnemyImage)
	UPROPERTY(Transient) UImage* PlayerImage = nullptr;
	// 实时战斗血条(怪物/玩家)+ 怪物名牌 + 近战预警圈 + 攻击射程提示。
	UPROPERTY(Transient) UImage* EnemyHpBarBg = nullptr;
	UPROPERTY(Transient) UImage* EnemyHpBarFill = nullptr;
	UPROPERTY(Transient) UTextBlock* EnemyNameLabel = nullptr;
	UPROPERTY(Transient) UImage* PlayerHpBarBg = nullptr;
	UPROPERTY(Transient) UImage* PlayerHpBarFill = nullptr;
	UPROPERTY(Transient) UImage* EnemyAttackCircle = nullptr;
	UPROPERTY(Transient) UTextBlock* CombatHintLabel = nullptr;
	// 撞地图边缘提示(房顶, 推向无相邻格的边被拒时短暂显示)。
	UPROPERTY(Transient) UTextBlock* EdgeHintLabel = nullptr;

	// 贴图资产缓存(防 GC): key = /Game 包路径。
	UPROPERTY(Transient) TMap<FString, UTexture2D*> TextureCache;

	// 房间内归一化坐标(0-1), 对齐 Lua playerPos。
	FVector2D PlayerPos = FVector2D(0.5, 0.5);
	FVector2D MoveVelocity = FVector2D::ZeroVector;  // 方案B: 平滑后的移动速度(归一化/秒), 防瞬时开关感
	FVector2D HeldInput = FVector2D::ZeroVector;
	bool HeldKeys[4] = {};   // W A S D
	float WalkAnimTime = 0.f;
	int32 LastDirX = 0;
	int32 LastDirY = 1;       // 默认朝下

	// 开箱演出计时(对齐 Lua chestOpenTimer/chestRewardBurst)。
	float ChestOpenTimer = 0.f;
	float RewardBurstTimer = 0.f;
	// 止血绿光 / 踩雷红光 演出计时。
	float HealGlowTimer = 0.f;
	float MineFlashTimer = 0.f;
	// 绿光子每颗的随机参数(PlayHealGlow 时生成): 环绕角度 / 半径 / 相位。
	float HealParticleAngle[8] = {};
	float HealParticleRadius[8] = {};
	float HealParticlePhase[8] = {};
	// 过门被内核拒绝(地图边界等)后的重试冷却: 冷却内按撞墙处理, 防止逐帧重试抽搐。
	float CrossRetryCooldown = 0.f;
	// 撞地图边缘提示淡出计时。
	float EdgeHintTimer = 0.f;
	int32 BurstParts = 0;
	int32 CurrentCellX = -1;
	int32 CurrentCellY = -1;

	// 上一次已知的玩家血量(用于踩雷红闪门控: 仅本次真扣血才闪)。-1 = 未初始化。
	int32 PrevPlayerHp = -1;

	// --- 怪物混合攻击系统状态变量 ---
	UPROPERTY(Transient) UImage* EnemyWarningLine = nullptr;
	UPROPERTY(Transient) UImage* EnemyProjectileImage = nullptr;

	float EnemyAttackCooldownTimer = 0.f; // 攻击大循环间隔
	float EnemyAimTimer = 0.f;            // 远程预警的红线瞄准时间
	float EnemyShakeTimer = 0.f;          // 攻击时的后坐力/近战冲撞抖动时间
	FVector2D EnemyBasePos = FVector2D(560.f * 0.35f - 40.f, 560.f * 0.45f - 40.f);

	bool bProjectileActive = false;                       // 子弹是否在飞行
	FVector2D AimDirection = FVector2D::ZeroVector;
	FVector2D ProjectilePos = FVector2D::ZeroVector;
	float ProjectileSpeed = 0.8f;

	// 散射飞弹(蝙蝠 RangedSpread): 多发并发, 预建固定上限 5。
	static constexpr int32 GTMaxSpreadProjectiles = 5;
	UPROPERTY(Transient) UImage* SpreadProjImages[5] = {};
	FVector2D SpreadProjPos[5] = {};
	FVector2D SpreadProjDir[5] = {};
	bool bSpreadProjActive[5] = {};

	// 激光(无人机 RangedLaser): 蓄力 -> 锁向粗光束持续 + 站内每隔扣血。
	UPROPERTY(Transient) UImage* LaserBeamImage = nullptr;
	UPROPERTY(Transient) UImage* LaserMuzzleImage = nullptr;   // 无人机镜头起手闪光(发射端)
	UPROPERTY(Transient) UImage* LaserImpactImage = nullptr;   // 激光命中玩家的灼烧爆点
	bool bLaserFiring = false;
	float LaserActiveTimer = 0.f;     // 光束持续剩余
	float LaserTickTimer = 0.f;       // 站内扣血间隔倒计时
	float LaserImpactTimer = 0.f;     // 命中爆点闪现剩余
	FVector2D LaserDir = FVector2D::ZeroVector;

	// 远程怪 kiting 随机游走(让移动别太规律): 每隔一段换随机方向叠加。
	FVector2D WanderDir = FVector2D::ZeroVector;
	float WanderTimer = 0.f;

	// 纯代码 juice(无素材): 阴影 = RoundedBox 深色软椭圆(玩家常驻 + 战斗怪物脚下)。
	UPROPERTY(Transient) UImage* PlayerShadow = nullptr;
	UPROPERTY(Transient) UImage* EnemyShadow = nullptr;
	// 受击闪白: 亮白 tint + scale punch, HP 下降帧触发后衰减。
	float EnemyHitFlashTimer = 0.f;
	int32 PrevEnemyHp = -1;
	float PlayerHitFlashTimer = 0.f;
	// 散弹弹道拖尾: 每发 2 个渐隐残影点跟在弹丸后。
	UPROPERTY(Transient) UImage* SpreadTrailImages[GTMaxSpreadProjectiles * 2] = {};

	// 玩家实时战斗计时(表现层门控, 对齐 Combat.lua): 挥砍冷却 + 受击无敌帧。
	float PlayerAttackCooldownTimer = 0.f;
	float PlayerIFrameTimer = 0.f;

	// 怪物避让天赋(monsterFleeBonus): 战斗刚开始的"犹豫窗口", 期间怪不追不攻; 给玩家缓冲。
	bool bPrevInCombat = false;
	float CombatFleeTimer = 0.f;

	// 全局键盘预处理器: 不受焦点/UIOnly 影响地维护 WASD 持键真值(防漏 KeyUp 卡键 / 关弹窗续走)。
	TSharedPtr<FGTMovementKeyProcessor> MovementProcessor;

	// 怪物实时战斗状态(表现层): 归一化位置(追击移动)、近战相位机、攻击射程缓存。
	FVector2D EnemyNormPos = FVector2D(0.35f, 0.45f);
	int32 EnemyMeleePhase = 0;          // 0 idle / 1 warning / 2 active / 3 cooldown

	// 怪物本体帧动画 + 朝向(贴图源朝右, 玩家在左侧则水平翻转面向玩家)。
	float EnemyAnimTime = 0.f;            // 待机/蓄力帧动画时钟
	float EnemyFireFlashTimer = 0.f;      // 蝙蝠发射张嘴帧的短暂闪现窗口
	FString CurrentEnemyFrameKey;         // 当前已设贴图路径缓存(跳过逐帧重复 SetBrush)
	EGT_MonsterType LastEnemyType = EGT_MonsterType::Slime;       // 逐帧记录当前怪种(死亡碎裂选图用)
	EGT_MonsterType DeathShatterType = EGT_MonsterType::Slime;    // 触发碎裂时定格的怪种
	// 逐帧记录开战所在格: 战斗结束时若仍在此格=击杀(播碎裂); 换了格=逃跑(不播)。
	int32 LastCombatCellX = -1;
	int32 LastCombatCellY = -1;

	// 怪物死亡碎裂演出(独立于战斗状态机): 怪物被击杀时在其末位置叠播 5 帧。
	bool bPlayingDeathShatter = false;
	float DeathShatterTimer = 0.f;
	int32 DeathShatterFrame = -1;
	FVector2D DeathEffectPos = FVector2D(0.35f, 0.45f);
	FVector2D LastEnemyNormPos = FVector2D(0.35f, 0.45f);
	float EnemyMeleePhaseTimer = 0.f;
	bool bEnemyMeleeHitResolved = false;
	bool bPlayerInAttackRange = false;
	float CurrentPlayerAttackRange = 0.21f;  // 当前怪原型的玩家挥砍射程(逐帧从原型表刷新)
	float CombatAnimTime = 0.f;              // 战斗内累计时间(预警圈脉动用)

};
