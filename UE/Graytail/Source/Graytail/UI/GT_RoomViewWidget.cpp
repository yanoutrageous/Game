#include "UI/GT_RoomViewWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Core/GT_RunContext.h"
#include "Core/GT_RunSubsystem.h"
#include "Debug/GT_DebugSubsystem.h"
#include "Domains/Combat/GT_MonsterCatalog.h"
#include "Domains/Events/GT_EventRules.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "UI/GT_UIStyle.h"
#include "Input/Events.h"
#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr float GTRoomSize = 560.f;        // 房间画布边长(px)
	constexpr float GTPlayerSize = 64.f;
	constexpr float GTDoorSize = 72.f;
	constexpr float GTMoveSpeed = 0.385f;      // 归一化坐标/秒(原值, 横穿整间房≈2.6s)
	constexpr float GTMoveAccel = 16.f;        // 方案B 起步加速平滑率(越大越跟手), PIE 可调
	constexpr float GTMoveDecel = 22.f;        // 方案B 松手减速率(>Accel 防溜冰), PIE 可调
	constexpr float GTDoorHalfWidth = 0.13f;   // 门对准判定半宽(归一化)
	constexpr float GTWalkFrameTime = 0.18f;   // 走路两帧轮播间隔
	constexpr float GTChestOpenDuration = 1.4f;    // 开箱金光时长(对齐 Lua CHEST_OPEN_DURATION)
	constexpr float GTChestRewardDuration = 2.0f;  // 奖励飘字时长(对齐 Lua CHEST_REWARD_DURATION)
	constexpr float GTHealGlowDuration = 1.0f;     // 止血绿色光子上升时长
	constexpr float GTMineFlashDuration = 0.6f;    // 踩雷红闪时长
	constexpr float GTPlayerAttackCooldown = 0.45f;     // 玩家挥砍冷却(Combat.lua playerAttackCooldown)
	constexpr float GTPlayerInvincibleDuration = 0.9f;  // 受击无敌帧(Combat.lua playerInvincibleDuration)
	constexpr float GTEdgeHintDuration = 1.4f;          // 撞地图边缘提示显示+淡出时长
	// 道具方形空气墙(AABB)归一化半边长 = (道具半宽 + 玩家碰撞半宽)/房宽。撞墙即停(不绕不抽搐);
	// 宝藏房宝箱墙较大(对应箱体)、一般事件房 NPC 墙较小。PIE 可微调。怪物会动+影响走位风筝, 不做碰撞。
	constexpr float GTChestCollideHalf = 0.060f;        // 宝藏房宝箱(贴箱体, 碰到才挡)
	constexpr float GTEventCollideHalf = 0.055f;        // 一般事件房 NPC(更小)

	const FLinearColor GTFloor_Normal(0.16f, 0.17f, 0.20f, 1.f);
	const FLinearColor GTFloor_Mine(0.45f, 0.12f, 0.10f, 1.f);
	const FLinearColor GTFloor_Exit(0.10f, 0.35f, 0.16f, 1.f);
	const FLinearColor GTFloor_Combat(0.40f, 0.22f, 0.08f, 1.f);
	const FLinearColor GTFloor_Event(0.28f, 0.16f, 0.38f, 1.f);
	const FLinearColor GTFloor_Chest(0.40f, 0.33f, 0.10f, 1.f);
	const FLinearColor GTDoorColor(0.55f, 0.45f, 0.30f, 1.f);

	// 浣熊角色帧(对齐 Lua DungeonRoom 的 idleFrames/animFrames 帧表)。索引: 下/上/左/右。
	int32 GTDirIndex(int32 DirX, int32 DirY)
	{
		return DirY > 0 ? 0 : DirY < 0 ? 1 : DirX < 0 ? 2 : 3;
	}
	const TCHAR* GTRaccoonIdleFrames[4] = {
		TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/00_front_idle"),
		TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/01_back_idle"),
		TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/02_left_idle"),
		TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/03_right_idle"),
	};
	const TCHAR* GTRaccoonWalkFrames[2][4] = {
		{
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/04_front_walk_1"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/05_back_walk_1"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/06_left_walk_1"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/07_right_walk_1"),
		},
		{
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/08_front_walk_2"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/09_back_walk_2"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/10_left_walk_2"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/11_right_walk_2"),
		},
	};
}

TSharedRef<SWidget> UGT_RoomViewWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UGT_RoomViewWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SyncToCurrentCell(true);
}

void UGT_RoomViewWidget::BuildWidgetTree()
{
	RoomCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RoomCanvas;

	// 地板: 整个画布铺底, 颜色按房型变。
	FloorBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	FloorBorder->SetBrushColor(GTFloor_Normal);
	if (UCanvasPanelSlot* FloorSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(FloorBorder)))
	{
		FloorSlot->SetPosition(FVector2D::ZeroVector);
		FloorSlot->SetSize(FVector2D(GTRoomSize, GTRoomSize));
	}

	// 四扇门: 上下左右边中点(对齐 Lua GetDoors)。顺序: 上 下 左 右。
	const FVector2D DoorPositions[4] = {
		FVector2D(GTRoomSize * 0.5f - GTDoorSize * 0.5f, 0.f),
		FVector2D(GTRoomSize * 0.5f - GTDoorSize * 0.5f, GTRoomSize - 14.f),
		FVector2D(0.f, GTRoomSize * 0.5f - GTDoorSize * 0.5f),
		FVector2D(GTRoomSize - 14.f, GTRoomSize * 0.5f - GTDoorSize * 0.5f),
	};
	for (int32 DoorIndex = 0; DoorIndex < 4; ++DoorIndex)
	{
		DoorImages[DoorIndex] = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		DoorImages[DoorIndex]->SetColorAndOpacity(GTDoorColor);
		// 地板贴图自带门洞美术, 色块隐藏(过门判定只看坐标, 与可见性无关)。
		DoorImages[DoorIndex]->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* DoorSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(DoorImages[DoorIndex])))
		{
			DoorSlot->SetPosition(DoorPositions[DoorIndex]);
			const bool bHorizontalDoor = DoorIndex < 2;
			DoorSlot->SetSize(bHorizontalDoor ? FVector2D(GTDoorSize, 14.f) : FVector2D(14.f, GTDoorSize));
		}
	}

	// 开箱光晕双圆(黄色, 内实外淡), 垫在宝箱下层, 仅演出期间可见。
	GlowOuter = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	GlowOuter->SetVisibility(ESlateVisibility::Collapsed);
	RoomCanvas->AddChild(GlowOuter);
	GlowInner = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	GlowInner->SetVisibility(ESlateVisibility::Collapsed);
	RoomCanvas->AddChild(GlowInner);

	// 搜索点宝箱 + 两侧金币/零件堆 + 提示文字(布局对齐 Lua drawSearchPoint, 整体缩小到约原版一半:
	// 判定 rect = 房中心 58x36, 两堆在 ±42, 文字在 rect 底+16)。
	ChestImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	ChestImage->SetVisibility(ESlateVisibility::Collapsed);
	// 开箱演出的上抬/缩放以箱底为支点。
	ChestImage->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
	if (UCanvasPanelSlot* ChestSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(ChestImage)))
	{
		// 实际尺寸随开/关状态在 RefreshRoomDecor 里调整(两张贴图留白不同)。
		ChestSlot->SetPosition(FVector2D(GTRoomSize * 0.5f - 20.f, GTRoomSize * 0.5f - 7.f));
		ChestSlot->SetSize(FVector2D(39.f, 39.f));
	}
	GoldPileImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	GoldPileImage->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* GoldSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(GoldPileImage)))
	{
		GoldSlot->SetPosition(FVector2D(GTRoomSize * 0.5f - 42.f - 14.f, GTRoomSize * 0.5f + 8.f));
		GoldSlot->SetSize(FVector2D(28.f, 28.f));
	}
	PartsPileImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	PartsPileImage->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* PartsSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(PartsPileImage)))
	{
		PartsSlot->SetPosition(FVector2D(GTRoomSize * 0.5f + 42.f - 14.f, GTRoomSize * 0.5f + 8.f));
		PartsSlot->SetSize(FVector2D(28.f, 28.f));
	}
	ChestCaption = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ChestCaption->SetFont(GT_UIStyle::Font(12));
	ChestCaption->SetJustification(ETextJustify::Center);
	ChestCaption->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* CaptionSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(ChestCaption)))
	{
		CaptionSlot->SetPosition(FVector2D(GTRoomSize * 0.5f - 100.f, GTRoomSize * 0.5f + 34.f));
		CaptionSlot->SetSize(FVector2D(200.f, 18.f));
	}

	// 事件房 NPC(对齐 Lua DungeonRoom.lua 事件绘制): 房宽 50% / 高 35% 处,
	// 头顶 "T:xxx" 提示 -> 顶部标识(骰子方块/菱形) -> 圆形身体 -> 眼睛 -> 脚下名字。
	// 形状用 RoundedBox 画(圆/方/旋转45°菱形), 颜色逐项对齐 evtVisual, 每局按事件类型着色。
	{
		const FVector2D NpcCenter(GTRoomSize * 0.5f, GTRoomSize * 0.35f);
		auto MakeEventImage = [this](const FVector2D& Center, float Size) -> UImage*
		{
			UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Image->SetVisibility(ESlateVisibility::Collapsed);
			if (UCanvasPanelSlot* ImageSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(Image)))
			{
				ImageSlot->SetPosition(Center - FVector2D(Size * 0.5f, Size * 0.5f));
				ImageSlot->SetSize(FVector2D(Size, Size));
			}
			return Image;
		};

		EventMarkerImage = MakeEventImage(NpcCenter + FVector2D(0.f, -19.f), 20.f);
		EventMarkerImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		EventBodyImage = MakeEventImage(NpcCenter, 36.f);
		EventEyeLeft = MakeEventImage(NpcCenter + FVector2D(-6.f, -3.f), 6.f);
		EventEyeRight = MakeEventImage(NpcCenter + FVector2D(6.f, -3.f), 6.f);

		EventNameLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		EventNameLabel->SetFont(GT_UIStyle::Font(13));
		EventNameLabel->SetJustification(ETextJustify::Center);
		EventNameLabel->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* NameSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EventNameLabel)))
		{
			NameSlot->SetPosition(FVector2D(NpcCenter.X - 60.f, NpcCenter.Y + 24.f));
			NameSlot->SetSize(FVector2D(120.f, 18.f));
		}

		EventCaption = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		EventCaption->SetFont(GT_UIStyle::Font(12));
		EventCaption->SetJustification(ETextJustify::Center);
		EventCaption->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(255, 255, 255, 230))));
		EventCaption->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* EventCaptionSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EventCaption)))
		{
			EventCaptionSlot->SetPosition(FVector2D(NpcCenter.X - 100.f, NpcCenter.Y - 56.f));
			EventCaptionSlot->SetSize(FVector2D(200.f, 18.f));
		}
	}

	// 奖励飘字(金币/零件图标 + "+N" 文本), 仅演出期间可见, 位置每帧由动画驱动。
	auto MakeBurstImage = [this]() -> UImage*
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Image->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* ImageSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(Image)))
		{
			ImageSlot->SetSize(FVector2D(56.f, 56.f));
		}
		return Image;
	};
	auto MakeBurstText = [this](const FColor& Color) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetFont(GT_UIStyle::Font(15));
		Text->SetColorAndOpacity(FSlateColor(FLinearColor(Color)));
		Text->SetJustification(ETextJustify::Center);
		Text->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* TextSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(Text)))
		{
			TextSlot->SetSize(FVector2D(80.f, 24.f));
		}
		return Text;
	};
	BurstGoldImage = MakeBurstImage();
	BurstGoldText = MakeBurstText(FColor(255, 235, 130));
	BurstPartsImage = MakeBurstImage();
	BurstPartsText = MakeBurstText(FColor(170, 230, 255));

	// 周围雷险标牌已挪到 HUD 中央列(房间正下方), 不再占用房间画布。

	// 近战预警圈(地面图层, 在怪/人之下): 对齐原版 DungeonRoom 的判定圈。
	// 固定按史莱姆 AttackRadius=0.20 的直径建(=0.40*560=224px), 逐帧重定位 + 改透明度表达 warning/active。
	EnemyAttackCircle = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	EnemyAttackCircle->SetVisibility(ESlateVisibility::Collapsed);
	{
		const float Diameter = 0.20f * 2.f * GTRoomSize;
		FSlateBrush CircleBrush;
		CircleBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		CircleBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		CircleBrush.OutlineSettings.CornerRadii = FVector4(Diameter * 0.5f, Diameter * 0.5f, Diameter * 0.5f, Diameter * 0.5f);
		CircleBrush.OutlineSettings.Color = FSlateColor(FLinearColor(FColor(255, 150, 40, 230)));
		CircleBrush.OutlineSettings.Width = 3.f;
		CircleBrush.TintColor = FSlateColor(FLinearColor(FColor(255, 120, 30)));
		EnemyAttackCircle->SetBrush(CircleBrush);
		if (UCanvasPanelSlot* CircleSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyAttackCircle)))
		{
			CircleSlot->SetSize(FVector2D(Diameter, Diameter));
		}
	}

	// 怪物(史莱姆), 战斗激活时显示在房间偏左上(对齐 Lua monsterPosition 0.35/0.45)。
	EnemyImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	EnemyImage->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* EnemySlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyImage)))
	{
		EnemySlot->SetPosition(FVector2D(GTRoomSize * 0.35f - 40.f, GTRoomSize * 0.45f - 40.f));
		EnemySlot->SetSize(FVector2D(80.f, 80.f));
	}

	// ==========================================
	// 1. 创建预警瞄准线 (半透明红色细长条)
	EnemyWarningLine = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	EnemyWarningLine->SetVisibility(ESlateVisibility::Collapsed);
	EnemyWarningLine->SetBrushTintColor(FSlateColor(FLinearColor(1.0f, 0.1f, 0.1f, 0.4f))); // 半透明红色
	EnemyWarningLine->SetRenderTransformPivot(FVector2D(0.f, 0.5f)); // 轴心设为左侧中心，方便做旋转
	if (UCanvasPanelSlot* LineSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyWarningLine)))
	{
		LineSlot->SetSize(FVector2D(800.f, 4.f)); // 长度800保证贯穿房间，宽度4像素
	}

	// 2. 创建子弹 (实心红点)
	EnemyProjectileImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	EnemyProjectileImage->SetVisibility(ESlateVisibility::Collapsed);
	EnemyProjectileImage->SetBrushTintColor(FSlateColor(FLinearColor(1.0f, 0.2f, 0.2f, 1.f))); // 亮红色
	if (UCanvasPanelSlot* ProjSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyProjectileImage)))
	{
		ProjSlot->SetSize(FVector2D(16.f, 16.f));
	}
	// ==========================================

	// 玩家(走路贴图)。
	PlayerImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UCanvasPanelSlot* PlayerSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(PlayerImage)))
	{
		PlayerSlot->SetSize(FVector2D(GTPlayerSize, GTPlayerSize));
	}

	// 止血绿色光子: 8 个小绿圆从玩家身上放射上升淡出(玩家上层, 红光之下)。
	for (int32 ParticleIndex = 0; ParticleIndex < 8; ++ParticleIndex)
	{
		UImage* Particle = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Particle->SetVisibility(ESlateVisibility::Collapsed);
		FSlateBrush DotBrush;
		DotBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		DotBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		DotBrush.OutlineSettings.CornerRadii = FVector4(6.f, 6.f, 6.f, 6.f);
		DotBrush.TintColor = FSlateColor(FLinearColor(FColor(130, 255, 160)));
		Particle->SetBrush(DotBrush);
		if (UCanvasPanelSlot* ParticleSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(Particle)))
		{
			ParticleSlot->SetSize(FVector2D(11.f, 11.f));
		}
		HealParticles[ParticleIndex] = Particle;
	}

	// 踩雷红光: 盖满整个房间的红色覆盖层(最上层), 闪烁淡出。
	MineFlash = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	MineFlash->SetVisibility(ESlateVisibility::Collapsed);
	{
		FSlateBrush RedBrush;
		RedBrush.DrawAs = ESlateBrushDrawType::Image;
		RedBrush.TintColor = FSlateColor(FLinearColor(FColor(220, 40, 30)));
		MineFlash->SetBrush(RedBrush);
	}
	if (UCanvasPanelSlot* FlashSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(MineFlash)))
	{
		FlashSlot->SetPosition(FVector2D::ZeroVector);
		FlashSlot->SetSize(FVector2D(GTRoomSize, GTRoomSize));
	}

	// 实时战斗血条(怪物/玩家)+ 怪物名牌。逐帧由 NativeTick 更新数值与位置, 非战斗时收起。
	auto MakeBarBrush = [](const FColor& Fill)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(3.f, 3.f, 3.f, 3.f);
		Brush.TintColor = FSlateColor(FLinearColor(Fill));
		return Brush;
	};

	// 怪物名牌(怪头顶, 显示名称 + 战力)。
	EnemyNameLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	EnemyNameLabel->SetVisibility(ESlateVisibility::Collapsed);
	EnemyNameLabel->SetFont(GT_UIStyle::Font(11));
	EnemyNameLabel->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(255, 210, 200, 240))));
	EnemyNameLabel->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* NameSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyNameLabel)))
	{
		NameSlot->SetSize(FVector2D(160.f, 16.f));
		NameSlot->SetPosition(FVector2D(GTRoomSize * 0.35f - 80.f, GTRoomSize * 0.45f - 60.f));
	}

	// 怪物血条底 + 填充(怪头顶, 名牌下方)。
	EnemyHpBarBg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	EnemyHpBarBg->SetVisibility(ESlateVisibility::Collapsed);
	EnemyHpBarBg->SetBrush(MakeBarBrush(FColor(18, 20, 26, 210)));
	if (UCanvasPanelSlot* BgSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyHpBarBg)))
	{
		BgSlot->SetSize(FVector2D(72.f, 8.f));
		BgSlot->SetPosition(FVector2D(GTRoomSize * 0.35f - 36.f, GTRoomSize * 0.45f - 44.f));
	}
	EnemyHpBarFill = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	EnemyHpBarFill->SetVisibility(ESlateVisibility::Collapsed);
	EnemyHpBarFill->SetBrush(MakeBarBrush(FColor(214, 72, 60)));
	if (UCanvasPanelSlot* FillSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyHpBarFill)))
	{
		FillSlot->SetSize(FVector2D(72.f, 8.f));
		FillSlot->SetPosition(FVector2D(GTRoomSize * 0.35f - 36.f, GTRoomSize * 0.45f - 44.f));
	}

	// 玩家血条底 + 填充(玩家头顶, 位置逐帧跟随)。
	PlayerHpBarBg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	PlayerHpBarBg->SetVisibility(ESlateVisibility::Collapsed);
	PlayerHpBarBg->SetBrush(MakeBarBrush(FColor(18, 20, 26, 210)));
	if (UCanvasPanelSlot* PBgSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(PlayerHpBarBg)))
	{
		PBgSlot->SetSize(FVector2D(56.f, 7.f));
	}
	PlayerHpBarFill = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	PlayerHpBarFill->SetVisibility(ESlateVisibility::Collapsed);
	PlayerHpBarFill->SetBrush(MakeBarBrush(FColor(92, 200, 112)));
	if (UCanvasPanelSlot* PFillSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(PlayerHpBarFill)))
	{
		PFillSlot->SetSize(FVector2D(56.f, 7.f));
	}

	// 战斗提示(玩家未进入攻击射程时提示"靠近 F 攻击")。
	CombatHintLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CombatHintLabel->SetVisibility(ESlateVisibility::Collapsed);
	CombatHintLabel->SetFont(GT_UIStyle::Font(13));
	CombatHintLabel->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(255, 225, 150, 235))));
	CombatHintLabel->SetJustification(ETextJustify::Center);
	CombatHintLabel->SetText(FText::FromString(TEXT("靠近怪物 · F 攻击")));
	if (UCanvasPanelSlot* HintSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(CombatHintLabel)))
	{
		HintSlot->SetSize(FVector2D(220.f, 20.f));
		HintSlot->SetPosition(FVector2D(GTRoomSize * 0.5f - 110.f, GTRoomSize - 40.f));
	}

	// 撞地图边缘提示(房顶中央, 推向无相邻格的边被内核拒时短暂显示, 然后淡出)。
	EdgeHintLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	EdgeHintLabel->SetVisibility(ESlateVisibility::Collapsed);
	EdgeHintLabel->SetFont(GT_UIStyle::Font(13));
	EdgeHintLabel->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(255, 180, 150, 240))));
	EdgeHintLabel->SetJustification(ETextJustify::Center);
	EdgeHintLabel->SetText(FText::FromString(TEXT("已到地图边缘, 无法继续前进")));
	if (UCanvasPanelSlot* EdgeSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EdgeHintLabel)))
	{
		EdgeSlot->SetSize(FVector2D(260.f, 20.f));
		EdgeSlot->SetPosition(FVector2D(GTRoomSize * 0.5f - 130.f, 22.f));
	}
}

UGT_DebugSubsystem* UGT_RoomViewWidget::GetDebugSubsystem() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UGT_DebugSubsystem>() : nullptr;
}

const UGT_RunContext* UGT_RoomViewWidget::GetRunContext() const
{
	const UGT_RunSubsystem* RunSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGT_RunSubsystem>() : nullptr;
	return RunSubsystem ? RunSubsystem->GetCurrentRunContext() : nullptr;
}

UTexture2D* UGT_RoomViewWidget::LoadTextureAsset(const FString& AssetPath)
{
	if (UTexture2D** Cached = TextureCache.Find(AssetPath))
	{
		return *Cached;
	}

	// 包路径只到包名, 对象与包同名(import_png_assets.py 的导入约定)。
	const FString ObjectPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("GT_RoomViewWidget: missing texture asset %s"), *AssetPath);
	}
	TextureCache.Add(AssetPath, Texture);
	return Texture;
}

UTexture2D* UGT_RoomViewWidget::GetWalkFrame(int32 DirX, int32 DirY, int32 FrameIndex)
{
	return LoadTextureAsset(GTRaccoonWalkFrames[FMath::Clamp(FrameIndex, 0, 1)][GTDirIndex(DirX, DirY)]);
}

UTexture2D* UGT_RoomViewWidget::GetIdleFrame(int32 DirX, int32 DirY)
{
	return LoadTextureAsset(GTRaccoonIdleFrames[GTDirIndex(DirX, DirY)]);
}

void UGT_RoomViewWidget::SyncToCurrentCell(bool bCenterPlayer)
{
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	FGT_DebugRunSnapshot Snapshot;
	if (!Debug || !Debug->GetDebugRunSnapshot(Snapshot))
	{
		CurrentCellX = -1;
		CurrentCellY = -1;
		return;
	}

	// 只有格子真的变了(开局/瞬移/外部移动)才把人物放回房中央; 原地刷新(点按钮)不动人。
	const bool bCellChanged = Snapshot.PlayerX != CurrentCellX || Snapshot.PlayerY != CurrentCellY;
	CurrentCellX = Snapshot.PlayerX;
	CurrentCellY = Snapshot.PlayerY;
	if (bCenterPlayer || bCellChanged)
	{
		PlayerPos = FVector2D(0.5, 0.5);
		MoveVelocity = FVector2D::ZeroVector;
	}
	// 同步血量基准(开局/瞬移/外部移动后), 避免下次踩雷误判扣血。
	PrevPlayerHp = Snapshot.PlayerHp;
	RefreshRoomDecor();
	UpdatePlayerImagePosition();
}

void UGT_RoomViewWidget::RefreshRoomDecor()
{
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	FGT_DebugRunSnapshot Snapshot;
	if (!Debug || !Debug->GetDebugRunSnapshot(Snapshot))
	{
		return;
	}

	// 房间底图: 复用 Lua 版 room_*.png(资产化在 Sprites/Misc 下), 按房型切换(对齐 DungeonRoom.lua)。
	// 房型不再加文字标签(用户要求): 地板美术本身已表达房型。
	FLinearColor FloorColor = GTFloor_Normal;
	const TCHAR* FloorAsset = TEXT("/Game/Graytail/Sprites/Misc/room_safe");
	switch (Snapshot.CurrentRoomBaseType)
	{
	case EGT_RoomBaseType::Mine:
		FloorColor = GTFloor_Mine; FloorAsset = TEXT("/Game/Graytail/Sprites/Misc/room_danger"); break;
	case EGT_RoomBaseType::Exit:
		FloorColor = GTFloor_Exit; FloorAsset = TEXT("/Game/Graytail/Sprites/Misc/room_exit"); break;
	case EGT_RoomBaseType::Combat:
		FloorColor = GTFloor_Combat; FloorAsset = TEXT("/Game/Graytail/Sprites/Misc/room_monster"); break;
	case EGT_RoomBaseType::Event:
		FloorColor = GTFloor_Event; FloorAsset = TEXT("/Game/Graytail/Sprites/Misc/room_event"); break;
	case EGT_RoomBaseType::Chest:
		FloorColor = GTFloor_Chest; FloorAsset = TEXT("/Game/Graytail/Sprites/Misc/room_treasure"); break;
	default:
		break;
	}

	if (UTexture2D* FloorTexture = LoadTextureAsset(FloorAsset))
	{
		FSlateBrush FloorBrush;
		FloorBrush.SetResourceObject(FloorTexture);
		FloorBrush.DrawAs = ESlateBrushDrawType::Image;
		FloorBorder->SetBrush(FloorBrush);
		FloorBorder->SetBrushColor(FLinearColor::White);
	}
	else
	{
		FloorBorder->SetBrushColor(FloorColor);
	}

	// 战斗激活时显示史莱姆。
	const bool bShowEnemy = Snapshot.bCombatActive
		&& Snapshot.CurrentRoomBaseType == EGT_RoomBaseType::Combat;
	if (bShowEnemy && !EnemyImage->GetBrush().GetResourceObject())
	{
		if (UTexture2D* SlimeTexture = LoadTextureAsset(TEXT("/Game/Graytail/Sprites/enemy_slime")))
		{
			EnemyImage->SetBrushFromTexture(SlimeTexture);
		}
	}
	EnemyImage->SetVisibility(bShowEnemy ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// 搜索点宝箱(对齐 Lua drawSearchPoint): 可搜索或已搜过的房间中央摆物资箱,
	// 已搜=开箱贴图; 未搜时两侧预览金币/零件堆。
	FName SearchReason;
	bool bChestRoom = false;
	const UGT_RunContext* RunContext = GetRunContext();
	const bool bCanSearch = RunContext && RunContext->EvaluateSearchAtPlayer(SearchReason, bChestRoom);
	const bool bSearched = !bCanSearch && SearchReason == FName(TEXT("searched"));
	const bool bShowChest = bCanSearch || bSearched;
	bChestRoom = bChestRoom || Snapshot.CurrentRoomBaseType == EGT_RoomBaseType::Chest;
	if (bShowChest)
	{
		if (UTexture2D* ChestTexture = LoadTextureAsset(bSearched
			? TEXT("/Game/Graytail/Sprites/Props/00_baoxiang_kai")
			: TEXT("/Game/Graytail/Sprites/Props/03_baoxiang_guan")))
		{
			ChestImage->SetBrushFromTexture(ChestTexture);
		}
		// 槽位居中锚定; 宝箱房用大箱盖住地板美术里的大宝箱, 普通房用小箱。
		// 开箱图内容只占画布宽 75%(关箱 97.5%), 两态槽位按可见宽度对齐。
		if (UCanvasPanelSlot* ChestSlot = Cast<UCanvasPanelSlot>(ChestImage->Slot))
		{
			const float SlotSize = bChestRoom ? (bSearched ? 101.f : 78.f) : (bSearched ? 51.f : 39.f);
			ChestSlot->SetPosition(FVector2D(GTRoomSize * 0.5f - SlotSize * 0.5f, GTRoomSize * 0.5f - SlotSize * 0.5f));
			ChestSlot->SetSize(FVector2D(SlotSize, SlotSize));
		}
		// 提示文字跟着箱体大小下移。
		if (UCanvasPanelSlot* CaptionSlot = Cast<UCanvasPanelSlot>(ChestCaption->Slot))
		{
			CaptionSlot->SetPosition(FVector2D(GTRoomSize * 0.5f - 100.f, GTRoomSize * 0.5f + (bChestRoom ? 56.f : 30.f)));
		}
		if (UTexture2D* GoldTexture = LoadTextureAsset(TEXT("/Game/Graytail/Sprites/Props/09_jinbi_dui")))
		{
			GoldPileImage->SetBrushFromTexture(GoldTexture);
		}
		if (UTexture2D* PartsTexture = LoadTextureAsset(TEXT("/Game/Graytail/Sprites/Props/07_lingjian_dui")))
		{
			PartsPileImage->SetBrushFromTexture(PartsTexture);
		}
		// 已搜过的房间不再显示提示文字(用户要求), 开箱状态由贴图本身表达。
		ChestCaption->SetText(FText::FromString(bChestRoom ? TEXT("F 开启物资箱") : TEXT("F 搜索")));
		ChestCaption->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(255, 230, 140, 230))));
	}
	ChestImage->SetVisibility(bShowChest ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ChestCaption->SetVisibility(bCanSearch ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	// 两侧堆只在普通房显示(宝箱房地板美术自带金币堆)。
	const bool bShowPiles = bCanSearch && !bChestRoom;
	GoldPileImage->SetVisibility(bShowPiles ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	PartsPileImage->SetVisibility(bShowPiles ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// 事件房(Standard): 原版简单图形 NPC(对齐 DungeonRoom.lua evtVisual 配色与构图)。
	bool bShowEvent = false;
	bool bShowEyes = false;
	bool bShowCaption = false;
	FGT_EventMenuView EventMenu;
	if (Snapshot.CurrentRoomBaseType == EGT_RoomBaseType::Event
		&& RunContext && RunContext->GetEventMenuAtPlayer(EventMenu))
	{
		bShowEvent = true;
		const bool bDone = EventMenu.bCompleted;

		// 四类事件配色(身体/顶饰/强调), 完成统一转灰(对齐 Lua completed 分支)。
		FColor BodyColor(40, 140, 150, 230);
		FColor HatColor(60, 180, 190, 240);
		FColor AccentColor(80, 220, 230, 255);
		switch (EventMenu.Kind)
		{
		case EGT_EventKind::Dice:
			BodyColor = FColor(180, 120, 40, 230); HatColor = FColor(220, 160, 50, 240); AccentColor = FColor(255, 200, 80, 255); break;
		case EGT_EventKind::Altar:
			BodyColor = FColor(120, 50, 150, 230); HatColor = FColor(160, 70, 200, 240); AccentColor = FColor(200, 130, 255, 255); break;
		case EGT_EventKind::Trap:
			BodyColor = FColor(150, 80, 40, 230); HatColor = FColor(190, 100, 50, 240); AccentColor = FColor(240, 150, 70, 255); break;
		default:
			break;
		}
		if (bDone)
		{
			BodyColor = FColor(50, 60, 60, 160);
			HatColor = FColor(60, 70, 70, 150);
			AccentColor = FColor(80, 100, 100, 150);
		}

		auto MakeCircleBrush = [](float Radius, const FColor& Fill, const FColor& Outline, float OutlineWidth)
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.TintColor = FSlateColor(FLinearColor(Fill));
			Brush.OutlineSettings = FSlateBrushOutlineSettings(
				FVector4(Radius, Radius, Radius, Radius), FLinearColor(Outline), OutlineWidth);
			Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			return Brush;
		};

		// 身体: 圆 + 强调色描边。
		EventBodyImage->SetBrush(MakeCircleBrush(18.f, BodyColor, AccentColor, 2.f));

		// 顶部标识: 骰子=圆角方块, 其余=旋转 45° 菱形(近似原版三角帽/倒三角/六角)。
		EventMarkerImage->SetBrush(MakeCircleBrush(EventMenu.Kind == EGT_EventKind::Dice ? 3.f : 2.f, HatColor, HatColor, 0.f));
		EventMarkerImage->SetRenderTransformAngle(EventMenu.Kind == EGT_EventKind::Dice ? 0.f : 45.f);

		// 眼睛(仅 NPC 类: 旅商/赌徒)。
		bShowEyes = EventMenu.Kind == EGT_EventKind::Trader || EventMenu.Kind == EGT_EventKind::Dice;
		if (bShowEyes)
		{
			const FColor EyeColor = bDone ? FColor(100, 120, 120, 150) : FColor(200, 255, 255, 255);
			const FSlateBrush EyeBrush = MakeCircleBrush(3.f, EyeColor, EyeColor, 0.f);
			EventEyeLeft->SetBrush(EyeBrush);
			EventEyeRight->SetBrush(EyeBrush);
		}

		// 脚下名字 / 已完成; 头顶 "T:xxx" 提示仅未完成时显示。
		EventNameLabel->SetText(FText::FromString(bDone
			? TEXT("已完成")
			: GT_EventRules::GetEventShortLabel(EventMenu.Kind)));
		EventNameLabel->SetColorAndOpacity(FSlateColor(FLinearColor(bDone
			? FColor(120, 140, 140, 180)
			: AccentColor)));
		bShowCaption = !bDone;
		EventCaption->SetText(FText::FromString(GT_EventRules::GetEventHotkeyCaption(EventMenu.Kind)));
	}
	const ESlateVisibility EventVisibility = bShowEvent ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	EventBodyImage->SetVisibility(EventVisibility);
	EventMarkerImage->SetVisibility(EventVisibility);
	EventNameLabel->SetVisibility(EventVisibility);
	EventEyeLeft->SetVisibility(bShowEvent && bShowEyes ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	EventEyeRight->SetVisibility(bShowEvent && bShowEyes ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	EventCaption->SetVisibility(bShowEvent && bShowCaption ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// 初始站立帧。
	if (UTexture2D* IdleFrame = GetIdleFrame(LastDirX, LastDirY))
	{
		PlayerImage->SetBrushFromTexture(IdleFrame);
	}
}

void UGT_RoomViewWidget::UpdatePlayerImagePosition()
{
	if (UCanvasPanelSlot* PlayerSlot = Cast<UCanvasPanelSlot>(PlayerImage->Slot))
	{
		PlayerSlot->SetPosition(FVector2D(
			PlayerPos.X * GTRoomSize - GTPlayerSize * 0.5f,
			PlayerPos.Y * GTRoomSize - GTPlayerSize * 0.5f));
	}
}

FReply UGT_RoomViewWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::W) { SetHeldMovementKey(Key, true); return FReply::Handled(); }
	if (Key == EKeys::A) { SetHeldMovementKey(Key, true); return FReply::Handled(); }
	if (Key == EKeys::S) { SetHeldMovementKey(Key, true); return FReply::Handled(); }
	if (Key == EKeys::D) { SetHeldMovementKey(Key, true); return FReply::Handled(); }
	if (Key == EKeys::F)
	{
		// F = 搜索/开箱(对齐原版底部快捷键栏)。
		if (OnSearchRequested.IsBound())
		{
			OnSearchRequested.Execute();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::M)
	{
		// M = 打开全屏区域扫描图。
		if (OnMapRequested.IsBound())
		{
			OnMapRequested.Execute();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::E)
	{
		// E = 撤离(对齐原版底栏; 是否在撤离点由内核裁决)。
		if (OnExtractRequested.IsBound())
		{
			OnExtractRequested.Execute();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::T)
	{
		// T = 处理事件(对齐原版 [T] 交互; 是否在事件房由内核裁决)。
		if (OnEventRequested.IsBound())
		{
			OnEventRequested.Execute();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Q)
	{
		// Q = 使用左下道具栏选中的道具(满血/无库存由内核拒绝)。
		if (OnConsumableRequested.IsBound())
		{
			OnConsumableRequested.Execute();
		}
		return FReply::Handled();
	}
	// 数字键 1-9 = 选择左下道具栏对应槽位(配合 Q 使用)。
	{
		static const FKey NumberKeys[9] = {
			EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
			EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
		for (int32 SlotIdx = 0; SlotIdx < 9; ++SlotIdx)
		{
			if (Key == NumberKeys[SlotIdx])
			{
				if (OnItemSlotRequested.IsBound()) { OnItemSlotRequested.Execute(SlotIdx + 1); }
				return FReply::Handled();
			}
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UGT_RoomViewWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 点房间 = 夺回键盘焦点(被按钮抢走后)。
	SetFocus();
	return FReply::Handled();
}

FReply UGT_RoomViewWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::W || Key == EKeys::A || Key == EKeys::S || Key == EKeys::D)
	{
		SetHeldMovementKey(Key, false);
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

void UGT_RoomViewWidget::SetHeldMovementKey(const FKey& Key, bool bDown)
{
	if (Key == EKeys::W) { HeldKeys[0] = bDown; }
	else if (Key == EKeys::A) { HeldKeys[1] = bDown; }
	else if (Key == EKeys::S) { HeldKeys[2] = bDown; }
	else if (Key == EKeys::D) { HeldKeys[3] = bDown; }
}

void UGT_RoomViewWidget::PlayChestRewardBurst(int32 Gold, int32 Parts)
{
	ChestOpenTimer = GTChestOpenDuration;
	RewardBurstTimer = GTChestRewardDuration;
	BurstParts = Parts;

	if (UTexture2D* GoldTexture = LoadTextureAsset(TEXT("/Game/Graytail/Sprites/Props/09_jinbi_dui")))
	{
		BurstGoldImage->SetBrushFromTexture(GoldTexture);
	}
	if (UTexture2D* PartsTexture = LoadTextureAsset(TEXT("/Game/Graytail/Sprites/Props/07_lingjian_dui")))
	{
		BurstPartsImage->SetBrushFromTexture(PartsTexture);
	}
	BurstGoldText->SetText(FText::FromString(FString::Printf(TEXT("+%d"), Gold)));
	BurstPartsText->SetText(FText::FromString(FString::Printf(TEXT("+%d"), Parts)));
	UpdateChestBurstAnim(0.f);
}

void UGT_RoomViewWidget::UpdateChestBurstAnim(float DeltaTime)
{
	// 开箱瞬间: 箱体极轻微上抬回落 + 黄色双圆光晕扩散消散(对齐 Lua 的双 nvgCircle)。
	if (ChestOpenTimer > 0.f)
	{
		ChestOpenTimer = FMath::Max(0.f, ChestOpenTimer - DeltaTime);
		const float Flash = ChestOpenTimer / GTChestOpenDuration;
		ChestImage->SetRenderTransform(FWidgetTransform(
			FVector2D(0.f, -3.f * Flash),
			FVector2D(1.f + 0.05f * Flash, 1.f + 0.05f * Flash),
			FVector2D::ZeroVector,
			0.f));

		auto UpdateGlow = [](UImage* Glow, float Diameter, float Opacity, float CenterY)
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			const float Radius = Diameter * 0.5f;
			Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
			Brush.TintColor = FSlateColor(FLinearColor(FColor(255, 205, 70)));
			Glow->SetBrush(Brush);
			Glow->SetRenderOpacity(Opacity);
			if (UCanvasPanelSlot* GlowSlot = Cast<UCanvasPanelSlot>(Glow->Slot))
			{
				GlowSlot->SetPosition(FVector2D(GTRoomSize * 0.5f - Radius, CenterY - Radius));
				GlowSlot->SetSize(FVector2D(Diameter, Diameter));
			}
			Glow->SetVisibility(Opacity > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		};
		// 光晕中心对箱体中心; 内圆实、外圆淡, 随时间扩散(对齐 Lua 60+40/90+50 的比例, 按缩小后的箱体等比)。
		const float GlowCenterY = GTRoomSize * 0.5f;
		UpdateGlow(GlowInner, 50.f + 36.f * (1.f - Flash), 0.62f * Flash, GlowCenterY);
		UpdateGlow(GlowOuter, 76.f + 52.f * (1.f - Flash), 0.25f * Flash, GlowCenterY);
	}

	// 奖励飘字(对齐 Lua chestRewardBurst: 上升 120, 散开 44->80, 70% 后淡出, sin 缩放脉冲)。
	if (RewardBurstTimer > 0.f)
	{
		RewardBurstTimer = FMath::Max(0.f, RewardBurstTimer - DeltaTime);
		const float Progress = 1.f - RewardBurstTimer / GTChestRewardDuration;
		const float EaseOut = 1.f - (1.f - Progress) * (1.f - Progress);
		const float Rise = 120.f * EaseOut;
		const float Spread = 44.f + 36.f * EaseOut;
		const float FadeStart = 0.7f;
		const float Alpha = Progress < FadeStart ? 1.f : FMath::Max(0.f, (1.f - Progress) / (1.f - FadeStart));
		const float Scale = 1.f + 0.4f * FMath::Sin(Progress * PI);
		const float IconSize = 40.f * Scale;
		const float IconCenterY = GTRoomSize * 0.5f - 2.f - Rise;
		const bool bActive = RewardBurstTimer > 0.f;
		const ESlateVisibility BurstVisibility = bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		const ESlateVisibility PartsVisibility = bActive && BurstParts > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

		auto PlaceBurst = [this](UWidget* Widget, float CenterX, float CenterY, float Width, float Height)
		{
			if (UCanvasPanelSlot* WidgetSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
			{
				WidgetSlot->SetPosition(FVector2D(CenterX - Width * 0.5f, CenterY - Height * 0.5f));
				WidgetSlot->SetSize(FVector2D(Width, Height));
			}
		};
		const float CenterX = GTRoomSize * 0.5f;
		PlaceBurst(BurstGoldImage, CenterX - Spread, IconCenterY, IconSize, IconSize);
		PlaceBurst(BurstGoldText, CenterX - Spread, IconCenterY + 30.f, 80.f, 24.f);
		PlaceBurst(BurstPartsImage, CenterX + Spread, IconCenterY + 4.f, IconSize, IconSize);
		PlaceBurst(BurstPartsText, CenterX + Spread, IconCenterY + 34.f, 80.f, 24.f);

		BurstGoldImage->SetRenderOpacity(Alpha);
		BurstGoldText->SetRenderOpacity(Alpha);
		BurstPartsImage->SetRenderOpacity(Alpha);
		BurstPartsText->SetRenderOpacity(Alpha);
		BurstGoldImage->SetVisibility(BurstVisibility);
		BurstGoldText->SetVisibility(BurstVisibility);
		BurstPartsImage->SetVisibility(PartsVisibility);
		BurstPartsText->SetVisibility(PartsVisibility);
	}

	// 止血绿色光子: 8 个小绿点环绕玩家漂浮(随机角度/半径/相位), 实时跟随玩家位置,
	// 缓慢绕转 + 半径呼吸 + 上下浮动, 淡入淡出。包住人物周围而非只在上方。
	if (HealGlowTimer > 0.f)
	{
		HealGlowTimer = FMath::Max(0.f, HealGlowTimer - DeltaTime);
		const float Progress = 1.f - HealGlowTimer / GTHealGlowDuration; // 0 -> 1
		const float FadeIn = FMath::Clamp(Progress / 0.15f, 0.f, 1.f);
		const float FadeOut = Progress < 0.6f ? 1.f : FMath::Max(0.f, (1.f - Progress) / 0.4f);
		const float Alpha = 0.9f * FadeIn * FadeOut;
		const FVector2D PlayerCenter = PlayerPos * GTRoomSize; // 每帧重算 = 跟随玩家移动
		const float Spin = Progress * 1.8f;                     // 整体缓慢绕转
		const bool bActive = HealGlowTimer > 0.f;
		for (int32 ParticleIndex = 0; ParticleIndex < 8; ++ParticleIndex)
		{
			UImage* Particle = HealParticles[ParticleIndex];
			if (!Particle)
			{
				continue;
			}
			const float Phase = HealParticlePhase[ParticleIndex];
			const float Angle = HealParticleAngle[ParticleIndex] + Spin;
			// 半径呼吸 + 随 progress 轻微外扩; 上下浮动错落; 大小 5-8px 随相位变。
			const float Radius = HealParticleRadius[ParticleIndex] * (0.85f + 0.15f * FMath::Sin(Phase + Progress * PI * 2.f)) + 5.f * Progress;
			const float Bob = FMath::Sin(Phase + Progress * PI * 3.f) * 3.f;
			const float Size = 5.f + 3.f * FMath::Abs(FMath::Sin(Phase * 1.7f));
			const float CenterX = PlayerCenter.X + FMath::Cos(Angle) * Radius;
			const float CenterY = PlayerCenter.Y - 8.f + FMath::Sin(Angle) * Radius + Bob;
			Particle->SetRenderOpacity(Alpha);
			if (UCanvasPanelSlot* ParticleSlot = Cast<UCanvasPanelSlot>(Particle->Slot))
			{
				ParticleSlot->SetPosition(FVector2D(CenterX - Size * 0.5f, CenterY - Size * 0.5f));
				ParticleSlot->SetSize(FVector2D(Size, Size));
			}
			Particle->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	// 踩雷红光: 全房间红覆盖, sin 脉冲闪 3 下 + 整体淡出。
	if (MineFlashTimer > 0.f && MineFlash)
	{
		MineFlashTimer = FMath::Max(0.f, MineFlashTimer - DeltaTime);
		const float Frac = MineFlashTimer / GTMineFlashDuration; // 1 -> 0
		const float Pulse = FMath::Abs(FMath::Sin(Frac * PI * 3.f));
		MineFlash->SetRenderOpacity(0.5f * Frac * Pulse);
		MineFlash->SetVisibility(MineFlashTimer > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 撞地图边缘提示: 保持后线性淡出(最后约 0.4s 渐隐)。
	if (EdgeHintTimer > 0.f && EdgeHintLabel)
	{
		EdgeHintTimer = FMath::Max(0.f, EdgeHintTimer - DeltaTime);
		EdgeHintLabel->SetRenderOpacity(FMath::Clamp(EdgeHintTimer / 0.4f, 0.f, 1.f));
		EdgeHintLabel->SetVisibility(EdgeHintTimer > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UGT_RoomViewWidget::PlayHealGlow()
{
	HealGlowTimer = GTHealGlowDuration;
	// 每颗光子随机环绕参数(纯表现, 用 FRand 即可): 角度铺满一圈, 半径 14-32 包住人物。
	for (int32 ParticleIndex = 0; ParticleIndex < 8; ++ParticleIndex)
	{
		HealParticleAngle[ParticleIndex] = FMath::FRand() * 2.f * PI;
		HealParticleRadius[ParticleIndex] = 14.f + FMath::FRand() * 18.f;
		HealParticlePhase[ParticleIndex] = FMath::FRand() * 2.f * PI;
	}
	UpdateChestBurstAnim(0.f);
}

void UGT_RoomViewWidget::PlayMineFlash()
{
	MineFlashTimer = GTMineFlashDuration;
	UpdateChestBurstAnim(0.f);
}

bool UGT_RoomViewWidget::TryConsumePlayerAttack()
{
	// 攻击射程门控(对齐 Combat.lua playerAttackRange): 玩家必须在怪物攻击射程内才能挥砍。
	if (FVector2D::Distance(PlayerPos, EnemyNormPos) > CurrentPlayerAttackRange)
	{
		return false;
	}
	// 挥砍冷却未到则拒绝(防 F 自动重复连发秒杀); 就绪则起冷却并放行。
	if (PlayerAttackCooldownTimer > 0.f)
	{
		return false;
	}
	PlayerAttackCooldownTimer = GTPlayerAttackCooldown;
	return true;
}

void UGT_RoomViewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CurrentCellX < 0)
	{
		return;
	}

	// 开箱演出不受焦点影响(弹窗刚关/按钮持焦时也要继续播)。
	UpdateChestBurstAnim(InDeltaTime);

	// ==========================================
		// 怪物远近战混合 AI 逻辑
		// ==========================================
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	FGT_DebugRunSnapshot Snapshot;
	if (Debug && Debug->GetDebugRunSnapshot(Snapshot))
	{
		const bool bIsCombatRoom = Snapshot.bCombatActive && Snapshot.CurrentRoomBaseType == EGT_RoomBaseType::Combat;

		if (bIsCombatRoom && EnemyImage->GetVisibility() == ESlateVisibility::HitTestInvisible)
		{
			// 行为原型(决定移动/攻击模式/数值; 见 GT_MonsterCatalog)。
			const FGT_MonsterArchetype& Arch = GT_MonsterCatalog::GetArchetype(Snapshot.EnemyType);
			CurrentPlayerAttackRange = Arch.PlayerAttackRange;
			CombatAnimTime += InDeltaTime;

			// 玩家挥砍冷却 / 受击无敌帧逐帧倒计时(表现层门控, 对齐 Combat.lua)。
			if (PlayerAttackCooldownTimer > 0.f) PlayerAttackCooldownTimer = FMath::Max(0.f, PlayerAttackCooldownTimer - InDeltaTime);
			if (PlayerIFrameTimer > 0.f) PlayerIFrameTimer = FMath::Max(0.f, PlayerIFrameTimer - InDeltaTime);

			// 怪物一次攻击落地: 非无敌帧才真扣血(经命令管线 DebugMonsterHit -> MonsterHitPlayer),
			// 命中后给玩家无敌帧 + 全屏红闪反馈, 并通知 HUD 刷新血量/失败界面。
			auto TryApplyMonsterHit = [&]()
			{
				if (PlayerIFrameTimer > 0.f) return;
				FGT_DebugRunSnapshot HitSnapshot;
				if (Debug->DebugMonsterHit(HitSnapshot))
				{
					PlayerIFrameTimer = GTPlayerInvincibleDuration;
					PlayMineFlash();
					Snapshot = HitSnapshot;            // 本帧血条用最新血量
					OnCombatStateChanged.ExecuteIfBound();
				}
			};

			// 移动: 追击型朝玩家逼近, 进攻击圈(略内)即停 -> 形成走位风筝。
			float Distance = FVector2D::Distance(PlayerPos, EnemyNormPos);
			if (Arch.MovePattern == EGT_MonsterMovePattern::ChasePlayer)
			{
				const float StopDist = Arch.AttackRadius * 0.85f;
				if (Distance > StopDist)
				{
					const FVector2D Dir = (PlayerPos - EnemyNormPos).GetSafeNormal();
					EnemyNormPos += Dir * Arch.MoveSpeed * InDeltaTime;
					EnemyNormPos.X = FMath::Clamp(EnemyNormPos.X, 0.08f, 0.92f);
					EnemyNormPos.Y = FMath::Clamp(EnemyNormPos.Y, 0.08f, 0.92f);
					Distance = FVector2D::Distance(PlayerPos, EnemyNormPos);
				}
			}
			bPlayerInAttackRange = Distance <= CurrentPlayerAttackRange;

			if (Arch.AttackPattern == EGT_MonsterAttackPattern::MeleeCircle)
			{
				// 近战预警圈相位机(对齐原版 DungeonRoom): idle -> warning(脉动圈) -> active(填充判定, 圈内命中) -> cooldown。
				EnemyMeleePhaseTimer -= InDeltaTime;
				if (EnemyMeleePhaseTimer <= 0.f)
				{
					switch (EnemyMeleePhase)
					{
					case 0: EnemyMeleePhase = 1; EnemyMeleePhaseTimer = Arch.WarningDuration; bEnemyMeleeHitResolved = false; break; // idle->warning
					case 1: // warning->active: 圈内判定一次命中
						EnemyMeleePhase = 2; EnemyMeleePhaseTimer = Arch.ActiveDuration;
						EnemyShakeTimer = 0.3f;
						if (!bEnemyMeleeHitResolved && Distance <= Arch.AttackRadius) { TryApplyMonsterHit(); }
						bEnemyMeleeHitResolved = true;
						break;
					case 2: EnemyMeleePhase = 3; EnemyMeleePhaseTimer = Arch.CooldownDuration; break; // active->cooldown
					default: EnemyMeleePhase = 0; EnemyMeleePhaseTimer = Arch.IdleDuration; break;     // cooldown->idle
					}
				}

				// 预警圈表现: warning=脉动橙圈(低透明), active=亮填充。
				if (EnemyAttackCircle)
				{
					const bool bShowCircle = (EnemyMeleePhase == 1 || EnemyMeleePhase == 2);
					EnemyAttackCircle->SetVisibility(bShowCircle ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
					if (bShowCircle)
					{
						const float Diameter = Arch.AttackRadius * 2.f * GTRoomSize;
						if (UCanvasPanelSlot* CircleSlot = Cast<UCanvasPanelSlot>(EnemyAttackCircle->Slot))
						{
							CircleSlot->SetPosition(FVector2D(EnemyNormPos.X * GTRoomSize - Diameter * 0.5f, EnemyNormPos.Y * GTRoomSize - Diameter * 0.5f));
						}
						const float Pulse = (FMath::Sin(CombatAnimTime * 11.f) + 1.f) * 0.5f;
						const float Alpha = (EnemyMeleePhase == 2) ? 0.82f : (0.28f + 0.18f * Pulse);
						EnemyAttackCircle->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, Alpha));
					}
				}
			}
			else // RangedProjectile: 红线瞄准 -> 飞弹(PR#9 远程, 留给未来远程怪类型)。
			{
				if (!bProjectileActive && EnemyAimTimer <= 0.f)
				{
					if (EnemyAttackCooldownTimer > 0.f)
					{
						EnemyAttackCooldownTimer -= InDeltaTime;
					}
					else
					{
						EnemyAimTimer = 1.0f;
						EnemyWarningLine->SetVisibility(ESlateVisibility::HitTestInvisible);
					}
				}

				if (EnemyAimTimer > 0.f)
				{
					EnemyAimTimer -= InDeltaTime;
					AimDirection = (PlayerPos - EnemyNormPos).GetSafeNormal();
					if (UCanvasPanelSlot* LineSlot = Cast<UCanvasPanelSlot>(EnemyWarningLine->Slot))
					{
						LineSlot->SetPosition(FVector2D(EnemyNormPos.X * GTRoomSize, EnemyNormPos.Y * GTRoomSize) - FVector2D(0.f, 2.f));
					}
					float AngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(AimDirection.Y, AimDirection.X));
					EnemyWarningLine->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D::ZeroVector, AngleDegrees));
					if (EnemyAimTimer <= 0.f)
					{
						EnemyWarningLine->SetVisibility(ESlateVisibility::Collapsed);
						bProjectileActive = true;
						ProjectilePos = EnemyNormPos;
						EnemyProjectileImage->SetVisibility(ESlateVisibility::HitTestInvisible);
						EnemyShakeTimer = 0.3f;
						EnemyAttackCooldownTimer = 2.0f;
					}
				}

				if (bProjectileActive)
				{
					ProjectilePos += AimDirection * ProjectileSpeed * InDeltaTime;
					if (UCanvasPanelSlot* ProjSlot = Cast<UCanvasPanelSlot>(EnemyProjectileImage->Slot))
					{
						ProjSlot->SetPosition(FVector2D(ProjectilePos.X * GTRoomSize - 8.f, ProjectilePos.Y * GTRoomSize - 8.f));
					}
					if (FVector2D::Distance(ProjectilePos, PlayerPos) < 0.06f)
					{
						bProjectileActive = false;
						EnemyProjectileImage->SetVisibility(ESlateVisibility::Collapsed);
						TryApplyMonsterHit();
					}
					else if (ProjectilePos.X < 0.f || ProjectilePos.X > 1.f || ProjectilePos.Y < 0.f || ProjectilePos.Y > 1.f)
					{
						bProjectileActive = false;
						EnemyProjectileImage->SetVisibility(ESlateVisibility::Collapsed);
					}
				}
			}

			// 怪物图像: 跟随 EnemyNormPos(逐帧移动)+ 攻击抖动偏移。
			{
				const FVector2D BasePx(EnemyNormPos.X * GTRoomSize - 40.f, EnemyNormPos.Y * GTRoomSize - 40.f);
				FVector2D Offset = FVector2D::ZeroVector;
				if (EnemyShakeTimer > 0.f)
				{
					EnemyShakeTimer -= InDeltaTime;
					Offset.X = FMath::Sin(EnemyShakeTimer * 60.f) * 8.f;
				}
				if (UCanvasPanelSlot* EnemySlot = Cast<UCanvasPanelSlot>(EnemyImage->Slot))
				{
					EnemySlot->SetPosition(BasePx + Offset);
				}
			}

			// 攻击射程提示: 未进入射程时提示靠近(怪物会追击, 射程很快闭合)。
			if (CombatHintLabel)
			{
				CombatHintLabel->SetVisibility(bPlayerInAttackRange ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
			}

			// 怪物血条 + 名牌(头顶跟随移动)。
			const float EnemyRatio = Snapshot.EnemyMaxHp > 0
				? FMath::Clamp(static_cast<float>(Snapshot.EnemyHp) / Snapshot.EnemyMaxHp, 0.f, 1.f)
				: 0.f;
			const FVector2D EnemyCenterPx = EnemyNormPos * GTRoomSize;
			const FVector2D EnemyBarTopLeft(EnemyCenterPx.X - 36.f, EnemyCenterPx.Y - 40.f - 16.f);
			if (EnemyHpBarBg)
			{
				EnemyHpBarBg->SetVisibility(ESlateVisibility::HitTestInvisible);
				if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(EnemyHpBarBg->Slot)) S->SetPosition(EnemyBarTopLeft);
			}
			if (EnemyHpBarFill)
			{
				EnemyHpBarFill->SetVisibility(ESlateVisibility::HitTestInvisible);
				if (UCanvasPanelSlot* FillSlot = Cast<UCanvasPanelSlot>(EnemyHpBarFill->Slot))
				{
					FillSlot->SetPosition(EnemyBarTopLeft);
					FillSlot->SetSize(FVector2D(72.f * EnemyRatio, 8.f));
				}
			}
			if (EnemyNameLabel)
			{
				EnemyNameLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
				if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(EnemyNameLabel->Slot))
					S->SetPosition(FVector2D(EnemyCenterPx.X - 80.f, EnemyCenterPx.Y - 40.f - 34.f));
				EnemyNameLabel->SetText(FText::FromString(Snapshot.EnemyName.IsEmpty()
					? FString::Printf(TEXT("敌人  战力%d"), Snapshot.EnemyPower)
					: FString::Printf(TEXT("%s  战力%d"), *Snapshot.EnemyName, Snapshot.EnemyPower)));
			}

			// 玩家血条(头顶跟随)。
			const float PlayerRatio = Snapshot.PlayerMaxHp > 0
				? FMath::Clamp(static_cast<float>(Snapshot.PlayerHp) / Snapshot.PlayerMaxHp, 0.f, 1.f)
				: 0.f;
			const FVector2D PlayerCenterPx = PlayerPos * GTRoomSize;
			const FVector2D PlayerBarTopLeft(PlayerCenterPx.X - 28.f, PlayerCenterPx.Y - GTPlayerSize * 0.5f - 12.f);
			if (PlayerHpBarBg)
			{
				PlayerHpBarBg->SetVisibility(ESlateVisibility::HitTestInvisible);
				if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(PlayerHpBarBg->Slot)) S->SetPosition(PlayerBarTopLeft);
			}
			if (PlayerHpBarFill)
			{
				PlayerHpBarFill->SetVisibility(ESlateVisibility::HitTestInvisible);
				if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(PlayerHpBarFill->Slot))
				{
					S->SetPosition(PlayerBarTopLeft);
					S->SetSize(FVector2D(56.f * PlayerRatio, 7.f));
				}
			}
		}
		else
		{
			// 清理 + 重置(下一场战斗从头开始)。
			EnemyAttackCooldownTimer = 0.f;
			EnemyAimTimer = 0.f;
			EnemyShakeTimer = 0.f;
			bProjectileActive = false;
			PlayerAttackCooldownTimer = 0.f;
			PlayerIFrameTimer = 0.f;
			EnemyNormPos = FVector2D(0.35f, 0.45f);
			EnemyMeleePhase = 0;
			EnemyMeleePhaseTimer = 0.f;
			bEnemyMeleeHitResolved = false;
			bPlayerInAttackRange = false;
			if (EnemyWarningLine) EnemyWarningLine->SetVisibility(ESlateVisibility::Collapsed);
			if (EnemyProjectileImage) EnemyProjectileImage->SetVisibility(ESlateVisibility::Collapsed);
			if (EnemyAttackCircle) EnemyAttackCircle->SetVisibility(ESlateVisibility::Collapsed);
			if (CombatHintLabel) CombatHintLabel->SetVisibility(ESlateVisibility::Collapsed);
			if (EnemyHpBarBg) EnemyHpBarBg->SetVisibility(ESlateVisibility::Collapsed);
			if (EnemyHpBarFill) EnemyHpBarFill->SetVisibility(ESlateVisibility::Collapsed);
			if (EnemyNameLabel) EnemyNameLabel->SetVisibility(ESlateVisibility::Collapsed);
			if (PlayerHpBarBg) PlayerHpBarBg->SetVisibility(ESlateVisibility::Collapsed);
			if (PlayerHpBarFill) PlayerHpBarFill->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	// ==========================================
	

	// 焦点在弹窗/按钮上时暂停移动(持键状态仍经 HUD 冒泡转发保持准确, 关弹窗立刻续走)。
	if (!HasKeyboardFocus())
	{
		return;
	}

	const float InputX = (HeldKeys[3] ? 1.f : 0.f) - (HeldKeys[1] ? 1.f : 0.f);
	const float InputY = (HeldKeys[2] ? 1.f : 0.f) - (HeldKeys[0] ? 1.f : 0.f);
	const bool bHasInput = (InputX != 0.f || InputY != 0.f);

	// 方案B: 速度朝目标平滑(起步缓入 / 松手缓停滑行), 而非瞬时开关。
	const FVector2D TargetVelocity = bHasInput
		? FVector2D(InputX, InputY).GetSafeNormal() * GTMoveSpeed
		: FVector2D::ZeroVector;
	MoveVelocity = FMath::Vector2DInterpTo(MoveVelocity, TargetVelocity, InDeltaTime,
		bHasInput ? GTMoveAccel : GTMoveDecel);

	// 无输入且已滑停 -> 站立帧(对齐 Lua idleFrames)。
	if (!bHasInput && MoveVelocity.IsNearlyZero(1.e-4f))
	{
		MoveVelocity = FVector2D::ZeroVector;
		if (WalkAnimTime > 0.f)
		{
			if (UTexture2D* IdleFrame = GetIdleFrame(LastDirX, LastDirY))
			{
				PlayerImage->SetBrushFromTexture(IdleFrame);
			}
			WalkAnimTime = 0.f;
		}
		return;
	}

	if (bHasInput)
	{
		LastDirX = InputY != 0.f ? 0 : FMath::Sign(InputX);
		LastDirY = FMath::Sign(InputY);
	}

	FVector2D NewPos = PlayerPos + MoveVelocity * InDeltaTime;

	// 道具碰撞(中间宝箱 / 事件房 NPC): 方形空气墙(AABB) + 分轴阻挡 —— 撞墙即停, 横移自然贴墙滑过,
	// 不再做切向 nudge(那是圆形墙顶死的补丁, 会让直撞时人物抽搐/横偏)。
	// 只挡"越来越深入方块"的移动, 已重叠(如在门口生成于箱体内)时仍可移出, 避免卡死。
	// 怪物会动+影响走位风筝, 不做碰撞。
	{
		struct FBoxObstacle { FVector2D Center; float Half; bool bActive; };
		const FBoxObstacle Obstacles[] = {
			{ FVector2D(0.5f, 0.47f), GTChestCollideHalf, ChestImage && ChestImage->GetVisibility() == ESlateVisibility::HitTestInvisible },
			{ FVector2D(0.5f, 0.35f), GTEventCollideHalf, EventBodyImage && EventBodyImage->GetVisibility() == ESlateVisibility::HitTestInvisible },
		};
		for (const FBoxObstacle& Ob : Obstacles)
		{
			if (!Ob.bActive) { continue; }
			// X 轴: 候选 X 落入方块 X 跨度 且 当前 Y 落入方块 Y 跨度 且 X 比原来更靠近中心 -> 挡住 X(贴 X 面停)。
			if (FMath::Abs(NewPos.X - Ob.Center.X) < Ob.Half
				&& FMath::Abs(PlayerPos.Y - Ob.Center.Y) < Ob.Half
				&& FMath::Abs(NewPos.X - Ob.Center.X) < FMath::Abs(PlayerPos.X - Ob.Center.X))
			{
				NewPos.X = PlayerPos.X;
			}
			// Y 轴: 用已修正的 X 复判, 同理挡 Y(贴 Y 面停)。
			if (FMath::Abs(NewPos.X - Ob.Center.X) < Ob.Half
				&& FMath::Abs(NewPos.Y - Ob.Center.Y) < Ob.Half
				&& FMath::Abs(NewPos.Y - Ob.Center.Y) < FMath::Abs(PlayerPos.Y - Ob.Center.Y))
			{
				NewPos.Y = PlayerPos.Y;
			}
		}
	}

	// 越界 + 对准门 -> 尝试过门(对齐 Lua isAlignedWithDoor)。
	// 刚被内核拒绝过(地图边界等)的冷却内不重试, 落到下方撞墙逻辑: 人物贴墙原地踏步, 不来回闪现。
	const float EdgeMargin = GTPlayerSize * 0.5f / GTRoomSize;
	CrossRetryCooldown = FMath::Max(0.f, CrossRetryCooldown - InDeltaTime);
	if (CrossRetryCooldown <= 0.f)
	{
		if (NewPos.Y < EdgeMargin && InputY < 0.f && FMath::Abs(NewPos.X - 0.5f) <= GTDoorHalfWidth) { TryCrossDoor(0, -1); return; }
		if (NewPos.Y > 1.f - EdgeMargin && InputY > 0.f && FMath::Abs(NewPos.X - 0.5f) <= GTDoorHalfWidth) { TryCrossDoor(0, 1); return; }
		if (NewPos.X < EdgeMargin && InputX < 0.f && FMath::Abs(NewPos.Y - 0.5f) <= GTDoorHalfWidth) { TryCrossDoor(-1, 0); return; }
		if (NewPos.X > 1.f - EdgeMargin && InputX > 0.f && FMath::Abs(NewPos.Y - 0.5f) <= GTDoorHalfWidth) { TryCrossDoor(1, 0); return; }
	}	
	
	// 撞墙: 夹回房间内。
	NewPos.X = FMath::Clamp(NewPos.X, EdgeMargin, 1.f - EdgeMargin);
	NewPos.Y = FMath::Clamp(NewPos.Y, EdgeMargin, 1.f - EdgeMargin);
	PlayerPos = NewPos;
	UpdatePlayerImagePosition();

	// 两帧走路动画轮播。
	WalkAnimTime += InDeltaTime;
	const int32 FrameIndex = static_cast<int32>(WalkAnimTime / GTWalkFrameTime) % 2;
	if (UTexture2D* WalkFrame = GetWalkFrame(LastDirX, LastDirY, FrameIndex))
	{
		PlayerImage->SetBrushFromTexture(WalkFrame);
	}
}

void UGT_RoomViewWidget::TryCrossDoor(int32 DirX, int32 DirY)
{
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	if (!Debug)
	{
		return;
	}

	FGT_DebugRunSnapshot Snapshot;
	const bool bMoved = Debug->DebugMoveTo(CurrentCellX + DirX, CurrentCellY + DirY, Snapshot);
	if (!bMoved)
	{
		// 内核拒绝(地图边界/死亡等): 当作撞墙 — 人物留在门口, 进入重试冷却,
		// 冷却内持续推门走撞墙逻辑(贴墙原地踏步)。状态没变, 不刷 HUD。
		CrossRetryCooldown = 0.4f;
		// 仅当该方向是地图边界(无相邻格)时弹"到边缘"提示; 死亡等其它拒绝不弹。
		const UGT_RunContext* RC = GetRunContext();
		if (RC && !RC->IsValidMapCoord(CurrentCellX + DirX, CurrentCellY + DirY))
		{
			EdgeHintTimer = GTEdgeHintDuration;
			if (EdgeHintLabel)
			{
				EdgeHintLabel->SetRenderOpacity(1.f);
				EdgeHintLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
		return;
	}

	CurrentCellX = Snapshot.PlayerX;
	CurrentCellY = Snapshot.PlayerY;
	// 从新房间对面的门走进来。
	PlayerPos = FVector2D(0.5 - DirX * 0.42, 0.5 - DirY * 0.42);
	MoveVelocity = FVector2D::ZeroVector;
	RefreshRoomDecor();
	UpdatePlayerImagePosition();

	// 进入雷格 = 踩雷, 红光闪烁反馈(内核已扣血, 这里只做表现)。
	// 只在本次真扣了血(新引爆)时红闪; 重进已引爆的废墟雷格 HP 不变 → 不重复闪。
	if (Snapshot.CurrentRoomBaseType == EGT_RoomBaseType::Mine
		&& PrevPlayerHp >= 0 && Snapshot.PlayerHp < PrevPlayerHp)
	{
		PlayMineFlash();
	}
	PrevPlayerHp = Snapshot.PlayerHp;

	// 通知 HUD 整体刷新(状态行/小地图)。
	if (OnRoomChanged.IsBound())
	{
		OnRoomChanged.Execute();
	}
}
