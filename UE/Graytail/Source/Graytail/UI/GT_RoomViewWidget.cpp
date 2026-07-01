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
#include "Core/GT_SettingsSubsystem.h"
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
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

// 全局键盘预处理器: 不受 UMG 焦点 / UIOnly input mode 影响地捕获 WASD 起落,
// 让房间持键状态(HeldKeys)始终= 物理真值 —— 关弹窗后仍按着则续走、松开则停, 且永不漏 KeyUp 卡键。
// 只监听不消费(return false), WASD 仍正常分发给焦点控件(如事件面板 W/S 选择)。
class FGTMovementKeyProcessor : public IInputProcessor
{
public:
	TWeakObjectPtr<UGT_RoomViewWidget> Owner;
	virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& InKeyEvent) override
	{
		if (UGT_RoomViewWidget* W = Owner.Get()) { W->SetHeldMovementKey(InKeyEvent.GetKey(), true); }
		return false;
	}
	virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& InKeyEvent) override
	{
		if (UGT_RoomViewWidget* W = Owner.Get()) { W->SetHeldMovementKey(InKeyEvent.GetKey(), false); }
		return false;
	}
};

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
	constexpr float GTPlayerAttackCooldown = 0.85f;     // 玩家挥砍冷却(原 0.45 对齐 Combat.lua; 2026-06-22 调长加大战斗难度, 防瞬间连刀秒杀)
	constexpr float GTPlayerInvincibleDuration = 0.9f;  // 受击无敌帧(Combat.lua playerInvincibleDuration)
	// 玩家挥砍演出参数(纯表现): 弧光 GTSwingFrameCount 帧 @ GTSwingPerFrame -> 总窗口约 0.24s;
	// 挥剑本体 3 帧均分同一窗口(GTSwingBodyPerFrame), 与 sfx_attack/命中(按下即结算)/冷却 0.85s 同拍。
	constexpr int32 GTSwingFrameCount = 4;              // 弧光帧数占位(3-5 帧), 等素材定
	constexpr float GTSwingPerFrame = 0.06f;            // 弧光每帧时长 -> 总时长约 0.24s
	constexpr float GTSwingBodyPerFrame = (GTSwingFrameCount * GTSwingPerFrame) / 3.f;   // 挥剑本体 3 帧铺满挥砍窗口
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
	// 挥剑本体帧(K=3, 4 向; 结构照搬 walk, 经 GTDirIndex 选向。素材 12-23, 未导入时 GetSwingFrame 返回 null 回退 idle/walk)。
	const TCHAR* GTRaccoonSwingFrames[3][4] = {
		{
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/12_front_swing_1"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/13_back_swing_1"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/14_left_swing_1"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/15_right_swing_1"),
		},
		{
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/16_front_swing_2"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/17_back_swing_2"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/18_left_swing_2"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/19_right_swing_2"),
		},
		{
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/20_front_swing_3"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/21_back_swing_3"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/22_left_swing_3"),
			TEXT("/Game/Graytail/Sprites/Characters/huanxiong/frames/23_right_swing_3"),
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

	// 注册全局键盘预处理器: 持键真值不受焦点切换/UIOnly 影响, 根治"漏 KeyUp 卡键"且关弹窗能续走。
	if (!MovementProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		MovementProcessor = MakeShared<FGTMovementKeyProcessor>();
		MovementProcessor->Owner = this;
		FSlateApplication::Get().RegisterInputPreProcessor(MovementProcessor);
	}
}

void UGT_RoomViewWidget::NativeDestruct()
{
	if (MovementProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(MovementProcessor);
		MovementProcessor.Reset();
	}
	Super::NativeDestruct();
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

	// 阴影(纯代码深色软椭圆, 地面层在怪/人之下, 卖立体/悬空感)。
	auto MakeShadow = [&](float W, float H) -> UImage*
	{
		UImage* Shadow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Shadow->SetVisibility(ESlateVisibility::Collapsed);
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		B.OutlineSettings.CornerRadii = FVector4(H * 0.5f, H * 0.5f, H * 0.5f, H * 0.5f);
		B.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.32f));
		Shadow->SetBrush(B);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(Shadow)))
		{
			S->SetSize(FVector2D(W, H));
		}
		return Shadow;
	};
	EnemyShadow = MakeShadow(56.f, 18.f);     // 战斗中显示
	PlayerShadow = MakeShadow(46.f, 16.f);    // 进房后由 UpdatePlayerImagePosition 显示+定位

	// 怪物(史莱姆), 战斗激活时显示在房间偏左上(对齐 Lua monsterPosition 0.35/0.45)。
	EnemyImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	EnemyImage->SetVisibility(ESlateVisibility::Collapsed);
	EnemyImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));   // 中心轴心: 面向玩家水平翻转时原地镜像不偏移
	if (UCanvasPanelSlot* EnemySlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyImage)))
	{
		EnemySlot->SetPosition(FVector2D(GTRoomSize * 0.35f - 40.f, GTRoomSize * 0.45f - 40.f));
		EnemySlot->SetSize(FVector2D(80.f, 80.f));
	}

	// 怪物死亡碎裂特效层(独立于 EnemyImage, 怪物死后在其末位置叠播 5 帧 80x80 序列)。
	DeathEffectImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	DeathEffectImage->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* DeathSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(DeathEffectImage)))
	{
		DeathSlot->SetSize(FVector2D(80.f, 80.f));
	}

	// 玩家挥砍弧光特效层(单组弧光帧, 贴在玩家前方按 facing 旋转; 中心轴心原地旋转, 素材缺失静默跳过)。
	PlayerSwingImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	PlayerSwingImage->SetVisibility(ESlateVisibility::Collapsed);
	PlayerSwingImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	if (UCanvasPanelSlot* SwingSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(PlayerSwingImage)))
	{
		SwingSlot->SetSize(FVector2D(80.f, 80.f));
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

	// 2a-trail. 散弹拖尾: 每发 2 个渐隐紫色残影点(z 在弹丸之下)。
	for (int32 TrailIndex = 0; TrailIndex < GTMaxSpreadProjectiles * 2; ++TrailIndex)
	{
		UImage* Trail = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Trail->SetVisibility(ESlateVisibility::Collapsed);
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		const float R = (TrailIndex % 2 == 0) ? 7.f : 5.f;
		B.OutlineSettings.CornerRadii = FVector4(R, R, R, R);
		B.TintColor = FSlateColor(FLinearColor(0.72f, 0.40f, 1.0f, 1.f));   // 紫
		Trail->SetBrush(B);
		if (UCanvasPanelSlot* TrailSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(Trail)))
		{
			TrailSlot->SetSize(FVector2D(R * 2.f, R * 2.f));
		}
		SpreadTrailImages[TrailIndex] = Trail;
	}

	// 2b. 散射飞弹(蝙蝠 RangedSpread): 预建 5 个, 用紫色能量弹贴图(无则回退纯色点)。
	UTexture2D* BoltTex = LoadTextureAsset(TEXT("/Game/Graytail/Sprites/Effects/bat_bolt"));
	for (int32 SpreadIndex = 0; SpreadIndex < GTMaxSpreadProjectiles; ++SpreadIndex)
	{
		UImage* Proj = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Proj->SetVisibility(ESlateVisibility::Collapsed);
		if (BoltTex) { Proj->SetBrushFromTexture(BoltTex); }
		else { Proj->SetBrushTintColor(FSlateColor(FLinearColor(1.0f, 0.35f, 0.5f, 1.f))); } // 回退粉红
		Proj->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));   // 中心轴心: 弹丸旋转对齐飞行方向
		if (UCanvasPanelSlot* PSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(Proj)))
		{
			PSlot->SetSize(FVector2D(24.f, 24.f));
		}
		SpreadProjImages[SpreadIndex] = Proj;
	}

	// 2c. 激光光束(无人机 RangedLaser): 青蓝光束贴图(左端亮帽=镜头侧), 左中心轴心旋转。
	LaserBeamImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	LaserBeamImage->SetVisibility(ESlateVisibility::Collapsed);
	if (UTexture2D* BeamTex = LoadTextureAsset(TEXT("/Game/Graytail/Sprites/Effects/drone_laser_beam")))
	{
		LaserBeamImage->SetBrushFromTexture(BeamTex);
	}
	else { LaserBeamImage->SetBrushTintColor(FSlateColor(FLinearColor(0.5f, 0.85f, 1.0f, 0.9f))); } // 回退纯色
	LaserBeamImage->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
	if (UCanvasPanelSlot* LaserSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(LaserBeamImage)))
	{
		LaserSlot->SetSize(FVector2D(800.f, 20.f)); // 长贯穿房间, 粗 20px
	}

	// 2d. 激光起手闪光(镜头端 46px) + 命中爆点(玩家端 52px): 发射时按需闪现, 平时隐藏。
	LaserMuzzleImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	LaserMuzzleImage->SetVisibility(ESlateVisibility::Collapsed);
	LaserMuzzleImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	if (UTexture2D* MzTex = LoadTextureAsset(TEXT("/Game/Graytail/Sprites/Effects/drone_laser_muzzle")))
	{
		LaserMuzzleImage->SetBrushFromTexture(MzTex);
	}
	if (UCanvasPanelSlot* MzSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(LaserMuzzleImage)))
	{
		MzSlot->SetSize(FVector2D(46.f, 46.f));
	}
	LaserImpactImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	LaserImpactImage->SetVisibility(ESlateVisibility::Collapsed);
	LaserImpactImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	if (UTexture2D* ImTex = LoadTextureAsset(TEXT("/Game/Graytail/Sprites/Effects/drone_laser_impact")))
	{
		LaserImpactImage->SetBrushFromTexture(ImTex);
	}
	if (UCanvasPanelSlot* ImSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(LaserImpactImage)))
	{
		ImSlot->SetSize(FVector2D(52.f, 52.f));
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

	// A 步骤 slot-1: 第 2 只子史莱姆的独立 widget 套(预警圈/影子=地面层先建, 本体/碎裂/血条/名牌后建), 初始 Collapsed。
	{
		EnemyAttackCircle1 = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		EnemyAttackCircle1->SetVisibility(ESlateVisibility::Collapsed);
		{
			const float Diameter = 0.20f * 2.f * GTRoomSize;
			FSlateBrush CircleBrush;
			CircleBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			CircleBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			CircleBrush.OutlineSettings.CornerRadii = FVector4(Diameter * 0.5f, Diameter * 0.5f, Diameter * 0.5f, Diameter * 0.5f);
			CircleBrush.OutlineSettings.Color = FSlateColor(FLinearColor(FColor(255, 150, 40, 230)));
			CircleBrush.OutlineSettings.Width = 3.f;
			CircleBrush.TintColor = FSlateColor(FLinearColor(FColor(255, 120, 30)));
			EnemyAttackCircle1->SetBrush(CircleBrush);
			if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyAttackCircle1)))
			{
				S->SetSize(FVector2D(Diameter, Diameter));
			}
		}

		EnemyShadow1 = MakeShadow(56.f, 18.f);

		EnemyImage1 = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		EnemyImage1->SetVisibility(ESlateVisibility::Collapsed);
		EnemyImage1->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyImage1)))
		{
			S->SetSize(FVector2D(80.f, 80.f));
		}

		DeathEffectImage1 = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		DeathEffectImage1->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(DeathEffectImage1)))
		{
			S->SetSize(FVector2D(80.f, 80.f));
		}

		EnemyHpBarBg1 = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		EnemyHpBarBg1->SetVisibility(ESlateVisibility::Collapsed);
		EnemyHpBarBg1->SetBrush(MakeBarBrush(FColor(18, 20, 26, 210)));
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyHpBarBg1)))
		{
			S->SetSize(FVector2D(72.f, 8.f));
		}
		EnemyHpBarFill1 = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		EnemyHpBarFill1->SetVisibility(ESlateVisibility::Collapsed);
		EnemyHpBarFill1->SetBrush(MakeBarBrush(FColor(214, 72, 60)));
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyHpBarFill1)))
		{
			S->SetSize(FVector2D(72.f, 8.f));
		}

		EnemyNameLabel1 = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		EnemyNameLabel1->SetVisibility(ESlateVisibility::Collapsed);
		EnemyNameLabel1->SetFont(GT_UIStyle::Font(11));
		EnemyNameLabel1->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(255, 210, 200, 240))));
		EnemyNameLabel1->SetJustification(ETextJustify::Center);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyNameLabel1)))
		{
			S->SetSize(FVector2D(160.f, 16.f));
		}
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
	CombatHintLabel->SetText(FText::FromString(TEXT("靠近并面向怪物 · F 挥砍")));
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

	// 怪物房逃跑警示条(站门口自动显示, 单次 F 即逃跑): HitTestInvisible 绝不夺键盘焦点 —— 否则战斗模拟的
	// HasKeyboardFocus 门控会冻结怪物, 失去"逃跑有代价(逃跑期间仍被打)"的意义。初始 Collapsed。
	FleeConfirmLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FleeConfirmLabel->SetVisibility(ESlateVisibility::Collapsed);
	FleeConfirmLabel->SetFont(GT_UIStyle::Font(13));
	FleeConfirmLabel->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(255, 150, 150, 245))));
	FleeConfirmLabel->SetJustification(ETextJustify::Center);
	FleeConfirmLabel->SetText(FText::FromString(TEXT("按 F 逃跑(掉钱 + 掉白货)")));
	if (UCanvasPanelSlot* FleeSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(FleeConfirmLabel)))
	{
		FleeSlot->SetSize(FVector2D(340.f, 20.f));
		FleeSlot->SetPosition(FVector2D(GTRoomSize * 0.5f - 170.f, 52.f));
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

void UGT_RoomViewWidget::PlaySfx(FName Key)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UGT_SettingsSubsystem* Settings = GI->GetSubsystem<UGT_SettingsSubsystem>())
		{
			Settings->PlaySfx(this, Key);
		}
	}
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

UTexture2D* UGT_RoomViewWidget::GetSwingFrame(int32 DirX, int32 DirY, int32 FrameIndex)
{
	return LoadTextureAsset(GTRaccoonSwingFrames[FMath::Clamp(FrameIndex, 0, 2)][GTDirIndex(DirX, DirY)]);
}

bool UGT_RoomViewWidget::TryApplySwingBodyFrame()
{
	// 挥砍中本体切挥剑帧(4 向, 3 帧均分挥砍窗口, 按 facing 经 GTDirIndex 选向)。
	// 素材未导入(GetSwingFrame 返回 null)则返回 false, 让调用方回退 idle/walk(不锁脚、不空白、不崩)。
	if (!bPlayingSwing || !PlayerImage)
	{
		return false;
	}
	const int32 BodyFrame = FMath::Clamp(FMath::FloorToInt(SwingTimer / GTSwingBodyPerFrame), 0, 2);
	if (UTexture2D* SwingBody = GetSwingFrame(LastDirX, LastDirY, BodyFrame))
	{
		PlayerImage->SetBrushFromTexture(SwingBody);
		return true;
	}
	return false;
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

void UGT_RoomViewWidget::ResetForTitle()
{
	// 置 -1 → NativeTick 顶部直接早退(不再模拟/绘制); 再显式隐藏所有战斗视觉, 防残留一帧透到标题。
	CurrentCellX = -1;
	CurrentCellY = -1;
	bPrevInCombat = false;
	CombatFleeTimer = 0.f;
	DroneDashTimer = 0.f;
	DroneDashTarget = FVector2D::ZeroVector;
	ClearFleePrompt();              // 收起逃跑确认条(防残留透到标题)
	bPlayingDeathShatter = false;
	bPlayingSwing = false;
	bLaserFiring = false;
	CurrentEnemyFrameKey.Reset();   // 本体帧缓存清掉, 下局按怪种重刷(防 desync)
	// A 步骤 slot-1(第 2 只子史莱姆)复位。
	RepEnemyId = -1;
	Slot1EnemyId = -1;
	bSlot1Shatter = false;
	Slot1MeleePhase = 0;
	Slot1MeleePhaseTimer = 0.f;
	bSlot1MeleeHitResolved = false;
	Slot1HitFlashTimer = 0.f;
	Slot1PrevHp = -1;
	Slot1ShakeTimer = 0.f;

	auto Hide = [](UWidget* W) { if (W) { W->SetVisibility(ESlateVisibility::Collapsed); } };
	Hide(EnemyImage);
	Hide(EnemyShadow);
	Hide(EnemyHpBarBg);
	Hide(EnemyHpBarFill);
	Hide(EnemyNameLabel);
	Hide(PlayerHpBarBg);
	Hide(PlayerHpBarFill);
	Hide(EnemyAttackCircle);
	Hide(EnemyWarningLine);
	Hide(EnemyProjectileImage);
	Hide(CombatHintLabel);
	Hide(DeathEffectImage);
	Hide(PlayerSwingImage);
	Hide(LaserBeamImage);
	Hide(LaserMuzzleImage);
	Hide(LaserImpactImage);
	// slot-1 全套(第 2 只子史莱姆)。
	Hide(EnemyImage1);
	Hide(EnemyShadow1);
	Hide(EnemyHpBarBg1);
	Hide(EnemyHpBarFill1);
	Hide(EnemyNameLabel1);
	Hide(EnemyAttackCircle1);
	Hide(DeathEffectImage1);
	for (int32 i = 0; i < GTMaxSpreadProjectiles; ++i)
	{
		bSpreadProjActive[i] = false;
		Hide(SpreadProjImages[i]);
	}
	for (int32 t = 0; t < GTMaxSpreadProjectiles * 2; ++t)
	{
		Hide(SpreadTrailImages[t]);
	}
}

void UGT_RoomViewWidget::RefreshRoomDecor()
{
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	FGT_DebugRunSnapshot Snapshot;
	if (!Debug || !Debug->GetDebugRunSnapshot(Snapshot))
	{
		return;
	}

	// 死亡碎裂是上一场战斗的遗留, 换房/刷新即取消, 防残留到新房间。
	bPlayingDeathShatter = false;
	DeathShatterTimer = 0.f;
	DeathShatterFrame = -1;
	if (DeathEffectImage) { DeathEffectImage->SetVisibility(ESlateVisibility::Collapsed); }
	bSlot1Shatter = false;
	Slot1ShatterTimer = 0.f;
	Slot1ShatterFrame = -1;
	if (DeathEffectImage1) { DeathEffectImage1->SetVisibility(ESlateVisibility::Collapsed); }

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
	if (bShowEnemy)
	{
		// 每次进战斗房都重置占位贴图 + 帧缓存(换房时上一场怪的末帧/攻击帧不能残留到新房间)。
		if (UTexture2D* SlimeTexture = LoadTextureAsset(TEXT("/Game/Graytail/Sprites/enemy_slime")))
		{
			EnemyImage->SetBrushFromTexture(SlimeTexture);
		}
		CurrentEnemyFrameKey.Reset();   // 让战斗 tick 重新按怪种刷新本体帧
		EnemyImage->SetBrushTintColor(FSlateColor(FLinearColor::White));
		EnemyImage->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D::ZeroVector, 0.f));
		EnemyImage->SetRenderOpacity(1.f);
	}
	EnemyImage->SetVisibility(bShowEnemy ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// A 步骤: 非战斗(或换房)时收起 slot-1(第 2 只子史莱姆)整套, 防残留闪帧; 战斗中由 NativeTick 逐帧驱动显隐。
	if (!bShowEnemy)
	{
		Slot1EnemyId = -1;
		auto HideSlot1 = [](UWidget* W) { if (W) { W->SetVisibility(ESlateVisibility::Collapsed); } };
		HideSlot1(EnemyImage1); HideSlot1(EnemyShadow1); HideSlot1(EnemyHpBarBg1);
		HideSlot1(EnemyHpBarFill1); HideSlot1(EnemyNameLabel1); HideSlot1(EnemyAttackCircle1);
	}

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
	// 玩家脚下阴影(46x16, 进房后常驻跟随)。
	if (PlayerShadow)
	{
		PlayerShadow->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* PShadowSlot = Cast<UCanvasPanelSlot>(PlayerShadow->Slot))
		{
			PShadowSlot->SetPosition(FVector2D(
				PlayerPos.X * GTRoomSize - 23.f,
				PlayerPos.Y * GTRoomSize + GTPlayerSize * 0.30f));
		}
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
		// 战斗房门口 F = 逃跑确认流程(仅 Standard 怪物房 + 站在有效门边); 否则 F = 搜索/开箱。
		if (TryHandleFleeKey())
		{
			return FReply::Handled();
		}
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
	// 左键 = 挥砍(与夺焦同一次点击并存)。是否真扣血/阻塞层门控由 HUD::OnAttack 裁决。
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnAttackRequested.ExecuteIfBound();
	}
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
	PlaySfx(FName(TEXT("sfx_pickup")));
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
	PlaySfx(FName(TEXT("sfx_heal")));
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
	PlayerHitFlashTimer = 0.14f;   // 玩家受击本体闪白(配合全屏红光)
	UpdateChestBurstAnim(0.f);
}

bool UGT_RoomViewWidget::TryConsumePlayerAttack(bool& bOutHit, TArray<int32>& OutHitEnemyIds)
{
	bOutHit = false;
	OutHitEnemyIds.Reset();

	// 发起只看冷却: 冷却未到则整次忽略(防 F 自动重复连发秒杀)。
	if (PlayerAttackCooldownTimer > 0.f)
	{
		return false;
	}
	// 冷却到 -> 发起挥砍: 起冷却 + 播挥砍音(挥空/命中通用)。
	PlayerAttackCooldownTimer = GTPlayerAttackCooldown;
	PlaySfx(FName(TEXT("sfx_attack")));

	// 朝向: facing 为主(LastDirX/LastDirY 已被移动输入维护, 战斗段不重置), 兜底朝下。
	FVector2D Facing(static_cast<float>(LastDirX), static_cast<float>(LastDirY));
	if (Facing.IsNearlyZero()) { Facing = FVector2D(0.f, 1.f); }
	Facing = Facing.GetSafeNormal();

	// 触发朝向弧光 VFX(纯表现, 与命中判定解耦; SwingAngleDeg 让弧光按朝向旋转, 贴图源朝右=0°)。
	SwingDir = Facing;
	SwingAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Facing.Y, Facing.X));
	bPlayingSwing = true;
	SwingTimer = 0.f;
	SwingFrame = -1;

	// 朝向锥形命中测试: 射程内 + 落在前方 120° 弧(半角 60°, cos60°=0.5)。
	// 对 Standard 战斗房里每只怪, 取它当前的渲染位置(rep 槽 / slot-1)做判定; 收集命中 EnemyId 给 HUD 提交命令。
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	FGT_DebugRunSnapshot Snapshot;
	if (Debug && Debug->GetDebugRunSnapshot(Snapshot)
		&& Snapshot.bCombatActive && Snapshot.CurrentRoomBaseType == EGT_RoomBaseType::Combat
		&& Snapshot.Enemies.Num() > 0)
	{
		for (const FGT_CombatEnemyView& E : Snapshot.Enemies)
		{
			FVector2D EnemyPos = FVector2D::ZeroVector;
			bool bHasPos = false;
			if (E.EnemyId == RepEnemyId)
			{
				EnemyPos = EnemyNormPos;
				bHasPos = true;
			}
			else if (E.EnemyId == Slot1EnemyId)
			{
				EnemyPos = Slot1NormPos;
				bHasPos = true;
			}
			if (!bHasPos) continue;

			const FVector2D D = EnemyPos - PlayerPos;
			const float Dist = D.Size();
			if (Dist <= CurrentPlayerAttackRange)
			{
				bool bInCone = D.IsNearlyZero();
				if (!bInCone)
				{
					bInCone = FVector2D::DotProduct(D / Dist, Facing) >= 0.5f;
				}
				if (bInCone)
				{
					bOutHit = true;
					OutHitEnemyIds.Add(E.EnemyId);
				}
			}
		}
	}
	return true;
}

void UGT_RoomViewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CurrentCellX < 0)
	{
		return;
	}

	if (bSimulationPaused)
	{
		MoveVelocity = FVector2D::ZeroVector;
		return;
	}

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
			// 进入战斗时确保上一场的碎裂特效已隐藏(防换房残留)。
			bPlayingDeathShatter = false;
			if (DeathEffectImage) { DeathEffectImage->SetVisibility(ESlateVisibility::Collapsed); }
			bSlot1Shatter = false;
			if (DeathEffectImage1) { DeathEffectImage1->SetVisibility(ESlateVisibility::Collapsed); }

			// 行为原型(决定移动/攻击模式/数值; 见 GT_MonsterCatalog)。
			const FGT_MonsterArchetype& Arch = GT_MonsterCatalog::GetArchetype(Snapshot.EnemyType);
			CurrentPlayerAttackRange = Arch.PlayerAttackRange;
			LastCombatCellX = CurrentCellX;   // 记开战格(战斗结束时比对: 同格=击杀播碎裂, 换格=逃跑不播)
			LastCombatCellY = CurrentCellY;
			// A 步骤: 代表槽按 EnemyId 对账(不再假设 Enemies[0] 稳定, 异步死时排序会变)。
			// 优先保持当前 RepEnemyId; 若它已死, 尝试把 slot-1 存活的怪提升为代表槽; 否则接 Enemies[0]。
			{
				int32 CurRepIndex = INDEX_NONE;
				for (int32 i = 0; i < Snapshot.Enemies.Num(); ++i)
				{
					if (Snapshot.Enemies[i].EnemyId == RepEnemyId)
					{
						CurRepIndex = i;
						break;
					}
				}

				if (CurRepIndex == INDEX_NONE && Snapshot.Enemies.Num() > 0)
				{
					// 当前代表怪已消失(RepEnemyId 有效但不在 Enemies 里): 分裂瞬间 或 代表槽那只先死。
					if (!bPlayingDeathShatter && Snapshot.PlayerHp > 0)
					{
						bPlayingDeathShatter = true;
						PlaySfx(FName(TEXT("sfx_death")));
						DeathShatterTimer = 0.f;
						DeathShatterFrame = -1;
						DeathEffectPos = LastEnemyNormPos;
						DeathShatterStartFrame = (LastEnemyType == EGT_MonsterType::Slimeling) ? 1 : 0;
						DeathShatterType = LastEnemyType;
					}

					// 若 slot-1 还活着, 把它提升为代表槽并继承其状态(无缝接管)。
					int32 PromoteIndex = INDEX_NONE;
					for (int32 i = 0; i < Snapshot.Enemies.Num(); ++i)
					{
						if (Snapshot.Enemies[i].EnemyId == Slot1EnemyId)
						{
							PromoteIndex = i;
							break;
						}
					}
					if (PromoteIndex != INDEX_NONE)
					{
						RepEnemyId = Slot1EnemyId;
						CurRepIndex = PromoteIndex;
						EnemyNormPos = Slot1NormPos;
						EnemyMeleePhase = Slot1MeleePhase;
						EnemyMeleePhaseTimer = Slot1MeleePhaseTimer;
						bEnemyMeleeHitResolved = bSlot1MeleeHitResolved;
						EnemyHitFlashTimer = Slot1HitFlashTimer;
						PrevEnemyHp = Slot1PrevHp;
						EnemyAnimTime = 0.f;
						CurrentEnemyFrameKey.Reset();
						WanderTimer = Slot1WanderTimer;
					}
					else
					{
						// 分裂瞬间(母体消失, slot-1 尚未绑定): 取 Enemies[0] 作为代表槽子体0, 重置状态。
						RepEnemyId = Snapshot.Enemies[0].EnemyId;
						CurRepIndex = 0;
						const FVector2D Child0Pos = LastEnemyNormPos + FVector2D(-0.18f - FMath::FRand() * 0.06f, -0.08f + (FMath::FRand() - 0.5f) * 0.10f);
						EnemyNormPos.X = FMath::Clamp(Child0Pos.X, 0.12f, 0.88f);
						EnemyNormPos.Y = FMath::Clamp(Child0Pos.Y, 0.12f, 0.88f);
						EnemyMeleePhase = 0; EnemyMeleePhaseTimer = 0.f; bEnemyMeleeHitResolved = false;
						EnemyHitFlashTimer = 0.f; PrevEnemyHp = -1; EnemyAnimTime = 0.f; CurrentEnemyFrameKey.Reset(); WanderTimer = 0.f;
					}
				}
				else if (RepEnemyId == -1 && Snapshot.Enemies.Num() > 0)
				{
					// 开场/切换战斗房: 没绑定过代表怪, 取 Enemies[0] 初始化。
					RepEnemyId = Snapshot.Enemies[0].EnemyId;
					CurRepIndex = 0;
				}
			}
			// 怪物受击闪白: HP 下降帧触发亮白 tint(配合本体 scale punch), 随后衰减。
			const bool bEnemyHpDroppedThisFrame = (PrevEnemyHp >= 0 && Snapshot.EnemyHp < PrevEnemyHp);
			if (bEnemyHpDroppedThisFrame) { EnemyHitFlashTimer = 0.13f; PlaySfx(FName(TEXT("sfx_hit"))); }
			PrevEnemyHp = Snapshot.EnemyHp;
			if (EnemyHitFlashTimer > 0.f) { EnemyHitFlashTimer = FMath::Max(0.f, EnemyHitFlashTimer - InDeltaTime); }
			const float EnemyHitFlash = FMath::Clamp(EnemyHitFlashTimer / 0.13f, 0.f, 1.f);
			EnemyImage->SetBrushTintColor(FSlateColor(FMath::Lerp(FLinearColor::White, FLinearColor(3.5f, 3.5f, 3.5f, 1.f), EnemyHitFlash)));
			CombatAnimTime += InDeltaTime;

			// 怪物避让天赋(monsterFleeBonus): 战斗刚开始给一段"犹豫窗口", 期间怪既不追击也不攻击,
			// 给玩家时间看清局势/逃跑(对齐 Lua monsterFleeActive)。窗口秒数 = 天赋值(开局存入 RunContext)。
			if (!bPrevInCombat)
			{
				const UGT_RunContext* FleeRC = GetRunContext();
				CombatFleeTimer = FleeRC ? static_cast<float>(FleeRC->GetLoadoutMonsterFleeBonus()) : 0.f;
				bPrevInCombat = true;
			}
			if (CombatFleeTimer > 0.f) { CombatFleeTimer -= InDeltaTime; }
			const bool bMonsterFleeing = CombatFleeTimer > 0.f;
			EnemyImage->SetRenderOpacity(bMonsterFleeing ? 0.55f : 1.f);   // 犹豫中半透明示意

			// 无人机受击冲刺脱困(纯表现层 juice, 仅 bDashAwayOnHit): 掉血瞬间起 0.5s 目标点冲刺,
			// 落点 = 远离玩家的可玩区一侧(0.80/0.20, 从角内收不贴墙); 被逼角时位移 ~3/4 房间对角线, 远遁后回 kiting。
			if (Arch.bDashAwayOnHit && bEnemyHpDroppedThisFrame && !bMonsterFleeing)
			{
				DroneDashTimer = 0.5f;
				const float FarX = (PlayerPos.X < 0.5f) ? 0.80f : 0.20f;   // 朝玩家对侧落点(永不冲向玩家)
				const float FarY = (PlayerPos.Y < 0.5f) ? 0.80f : 0.20f;
				DroneDashTarget = FVector2D(FarX, FarY);
			}

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
					PlaySfx(FName(TEXT("sfx_hurt")));
					Snapshot = HitSnapshot;            // 本帧血条用最新血量
					OnCombatStateChanged.ExecuteIfBound();
				}
			};

			// 移动: 追击型朝玩家逼近, 进攻击圈(略内)即停 -> 形成走位风筝。
			float Distance = FVector2D::Distance(PlayerPos, EnemyNormPos);
			if (DroneDashTimer > 0.f) { DroneDashTimer = FMath::Max(0.f, DroneDashTimer - InDeltaTime); }
			if (!bMonsterFleeing && DroneDashTimer > 0.f)
			{
				// 目标点式冲刺: 高速(~2.0 归一化/s)直奔落点(0.5s 内覆盖整间房); 覆盖常规 kiting, 到点/超时即停。
				const float DashStep = 2.0f * InDeltaTime;
				const FVector2D ToTarget = DroneDashTarget - EnemyNormPos;
				if (ToTarget.Size() <= DashStep) { EnemyNormPos = DroneDashTarget; DroneDashTimer = 0.f; }
				else { EnemyNormPos += ToTarget.GetSafeNormal() * DashStep; }
				EnemyNormPos.X = FMath::Clamp(EnemyNormPos.X, 0.12f, 0.88f);
				EnemyNormPos.Y = FMath::Clamp(EnemyNormPos.Y, 0.12f, 0.88f);
				Distance = FVector2D::Distance(PlayerPos, EnemyNormPos);
			}
			else if (!bMonsterFleeing && Arch.MovePattern == EGT_MonsterMovePattern::ChasePlayer)
			{
				const float StopDist = Arch.AttackRadius * 0.85f;
				if (Distance > StopDist)
				{
					const FVector2D Dir = (PlayerPos - EnemyNormPos).GetSafeNormal();
					EnemyNormPos += Dir * Arch.MoveSpeed * InDeltaTime;
					EnemyNormPos.X = FMath::Clamp(EnemyNormPos.X, 0.12f, 0.88f);
					EnemyNormPos.Y = FMath::Clamp(EnemyNormPos.Y, 0.12f, 0.88f);
					Distance = FVector2D::Distance(PlayerPos, EnemyNormPos);
				}
			}
			else if (!bMonsterFleeing && Arch.MovePattern == EGT_MonsterMovePattern::KeepDistance)
			{
				// 远程走位: 太近后撤(权重 KiteStrength)叠加随机游走(WanderWeight); 只有 bChaseWhenFar 的怪才太远跟近。
				const float Ideal = Arch.IdealDistance;
				const float Buffer = 0.08f;
				FVector2D KiteDir = FVector2D::ZeroVector;
				if (Distance < Ideal - Buffer)
				{
					KiteDir = (EnemyNormPos - PlayerPos).GetSafeNormal();   // 太近: 远离玩家
				}
				else if (Arch.bChaseWhenFar && Distance > Ideal + Buffer)
				{
					KiteDir = (PlayerPos - EnemyNormPos).GetSafeNormal();   // 太远跟近保持射程
				}
				// 随机游走: 每隔一段换一个随机方向叠加, 让怪到处乱窜、别太规律。
				WanderTimer -= InDeltaTime;
				if (WanderTimer <= 0.f)
				{
					WanderTimer = 0.35f + FMath::FRand() * 0.45f;
					const float WanderRad = FMath::FRandRange(-PI, PI);
					WanderDir = FVector2D(FMath::Cos(WanderRad), FMath::Sin(WanderRad));
				}
				const FVector2D MoveDir = KiteDir * Arch.KiteStrength + WanderDir * Arch.WanderWeight;   // 方向混合, 速度恒为 MoveSpeed
				if (!MoveDir.IsNearlyZero())
				{
					EnemyNormPos += MoveDir.GetSafeNormal() * Arch.MoveSpeed * InDeltaTime;
					EnemyNormPos.X = FMath::Clamp(EnemyNormPos.X, 0.12f, 0.88f);
					EnemyNormPos.Y = FMath::Clamp(EnemyNormPos.Y, 0.12f, 0.88f);
					Distance = FVector2D::Distance(PlayerPos, EnemyNormPos);
				}
			}
			else if (!bMonsterFleeing && Arch.MovePattern == EGT_MonsterMovePattern::RandomRoam)
			{
				// 子史莱姆乱窜: 大体随机方向 + 轻微朝玩家偏置(免得飘墙角变无害), 撞墙(clamp)反弹, 不直线逼近。
				WanderTimer -= InDeltaTime;
				if (WanderTimer <= 0.f)
				{
					WanderTimer = 0.35f + FMath::FRand() * 0.45f;
					const float WanderRad = FMath::FRandRange(-PI, PI);
					WanderDir = FVector2D(FMath::Cos(WanderRad), FMath::Sin(WanderRad));
				}
				const FVector2D ToPlayerRoam = (PlayerPos - EnemyNormPos).GetSafeNormal();
				const FVector2D RoamDir = (WanderDir * 0.8f + ToPlayerRoam * 0.2f).GetSafeNormal();
				FVector2D RoamPos = EnemyNormPos + RoamDir * Arch.MoveSpeed * InDeltaTime;
				if (RoamPos.X < 0.12f || RoamPos.X > 0.88f) { WanderDir.X = -WanderDir.X; }
				if (RoamPos.Y < 0.12f || RoamPos.Y > 0.88f) { WanderDir.Y = -WanderDir.Y; }
				EnemyNormPos.X = FMath::Clamp(RoamPos.X, 0.12f, 0.88f);
				EnemyNormPos.Y = FMath::Clamp(RoamPos.Y, 0.12f, 0.88f);
				Distance = FVector2D::Distance(PlayerPos, EnemyNormPos);
			}
			bPlayerInAttackRange = Distance <= CurrentPlayerAttackRange;

			if (bMonsterFleeing)
			{
				// 怪物避让窗口: 不发起任何攻击, 隐藏预警圈/瞄准线。
				if (EnemyAttackCircle) { EnemyAttackCircle->SetVisibility(ESlateVisibility::Collapsed); }
				if (EnemyWarningLine) { EnemyWarningLine->SetVisibility(ESlateVisibility::Collapsed); }
			}
			else if (Arch.AttackPattern == EGT_MonsterAttackPattern::MeleeCircle)
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
			else if (Arch.AttackPattern == EGT_MonsterAttackPattern::RangedSpread)
			{
				// 蝙蝠散射: 短蓄力瞄准 -> 发射 SpreadCount 发 ±SpreadHalfAngle 直线飞弹 -> AttackInterval 冷却。
				bool bAnySpreadActive = false;
				for (int32 i = 0; i < GTMaxSpreadProjectiles; ++i) { if (bSpreadProjActive[i]) { bAnySpreadActive = true; break; } }

				if (!bAnySpreadActive && EnemyAimTimer <= 0.f)
				{
					if (EnemyAttackCooldownTimer > 0.f) { EnemyAttackCooldownTimer -= InDeltaTime; }
					else { EnemyAimTimer = Arch.AimDuration; }   // 蝙蝠不用长条预警, 改怪物本体闪烁(见下)
				}

				if (EnemyAimTimer > 0.f)
				{
					EnemyAimTimer -= InDeltaTime;
					AimDirection = (PlayerPos - EnemyNormPos).GetSafeNormal();
					// 蝙蝠本体预警(替代指向玩家的长条红线): 蓄力时怪物快速闪烁示意"要攻击了"。
					EnemyImage->SetRenderOpacity(0.35f + 0.55f * FMath::Abs(FMath::Sin(CombatAnimTime * 22.f)));
					if (EnemyAimTimer <= 0.f)
					{
						EnemyImage->SetRenderOpacity(1.f);   // 发射瞬间恢复不透明
						const int32 Count = FMath::Clamp(Arch.SpreadCount, 1, GTMaxSpreadProjectiles);
						const float BaseDeg = FMath::RadiansToDegrees(FMath::Atan2(AimDirection.Y, AimDirection.X));
						for (int32 i = 0; i < Count; ++i)
						{
							const float Frac = (Count > 1) ? (static_cast<float>(i) / (Count - 1)) : 0.5f;
							const float OffsetDeg = -Arch.SpreadHalfAngleDeg + Frac * (2.f * Arch.SpreadHalfAngleDeg);
							const float Rad = FMath::DegreesToRadians(BaseDeg + OffsetDeg);
							SpreadProjDir[i] = FVector2D(FMath::Cos(Rad), FMath::Sin(Rad));
							SpreadProjPos[i] = EnemyNormPos;
							bSpreadProjActive[i] = true;
							if (SpreadProjImages[i]) { SpreadProjImages[i]->SetVisibility(ESlateVisibility::HitTestInvisible); }
						}
						EnemyShakeTimer = 0.3f;
						EnemyFireFlashTimer = 0.18f;   // 张大嘴发射帧短暂闪现
						EnemyAttackCooldownTimer = Arch.AttackInterval;
					}
				}

				for (int32 i = 0; i < GTMaxSpreadProjectiles; ++i)
				{
					auto HideTrail = [&]()
					{
						for (int32 g = 0; g < 2; ++g) { if (SpreadTrailImages[i * 2 + g]) SpreadTrailImages[i * 2 + g]->SetVisibility(ESlateVisibility::Collapsed); }
					};
					if (!bSpreadProjActive[i]) { continue; }
					SpreadProjPos[i] += SpreadProjDir[i] * ProjectileSpeed * InDeltaTime;
					if (SpreadProjImages[i])
					{
						if (UCanvasPanelSlot* PSlot = Cast<UCanvasPanelSlot>(SpreadProjImages[i]->Slot))
						{
							PSlot->SetPosition(FVector2D(SpreadProjPos[i].X * GTRoomSize - 12.f, SpreadProjPos[i].Y * GTRoomSize - 12.f));
						}
						const float BoltDeg = FMath::RadiansToDegrees(FMath::Atan2(SpreadProjDir[i].Y, SpreadProjDir[i].X));
						SpreadProjImages[i]->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D::ZeroVector, BoltDeg));
					}
					// 拖尾: 两残影点跟在弹丸后(越后越淡越小)。
					for (int32 g = 0; g < 2; ++g)
					{
						UImage* Trail = SpreadTrailImages[i * 2 + g];
						if (!Trail) { continue; }
						const float Back = (g == 0) ? 0.022f : 0.044f;
						const float Rr = (g == 0) ? 7.f : 5.f;
						const FVector2D TP = SpreadProjPos[i] - SpreadProjDir[i] * Back;
						Trail->SetVisibility(ESlateVisibility::HitTestInvisible);
						Trail->SetRenderOpacity((g == 0) ? 0.5f : 0.28f);
						if (UCanvasPanelSlot* TS = Cast<UCanvasPanelSlot>(Trail->Slot))
						{
							TS->SetPosition(FVector2D(TP.X * GTRoomSize - Rr, TP.Y * GTRoomSize - Rr));
						}
					}
					if (FVector2D::Distance(SpreadProjPos[i], PlayerPos) < 0.06f)
					{
						bSpreadProjActive[i] = false;
						if (SpreadProjImages[i]) { SpreadProjImages[i]->SetVisibility(ESlateVisibility::Collapsed); }
						HideTrail();
						TryApplyMonsterHit();
					}
					else if (SpreadProjPos[i].X < 0.f || SpreadProjPos[i].X > 1.f || SpreadProjPos[i].Y < 0.f || SpreadProjPos[i].Y > 1.f)
					{
						bSpreadProjActive[i] = false;
						if (SpreadProjImages[i]) { SpreadProjImages[i]->SetVisibility(ESlateVisibility::Collapsed); }
						HideTrail();
					}
				}
			}
			else if (Arch.AttackPattern == EGT_MonsterAttackPattern::RangedLaser)
			{
				// 无人机激光: 蓄力(红线跟随玩家) -> 锁向发射粗光束(持续, 站内每隔扣血) -> AttackInterval 冷却。
				if (!bLaserFiring && EnemyAimTimer <= 0.f)
				{
					if (EnemyAttackCooldownTimer > 0.f) { EnemyAttackCooldownTimer -= InDeltaTime; }
					else { EnemyAimTimer = Arch.AimDuration; AimDirection = (PlayerPos - EnemyNormPos).GetSafeNormal(); EnemyWarningLine->SetVisibility(ESlateVisibility::HitTestInvisible); }
				}

				if (EnemyAimTimer > 0.f && !bLaserFiring)
				{
					EnemyAimTimer -= InDeltaTime;
					// 蓄力期: AimTurnRateDeg>0 限速跟随; =0 保持蓄力开始锁定的方向(固定预警红线, 蓄力期移开即脱靶)。
					if (Arch.AimTurnRateDeg > 0.f)
					{
						const FVector2D ToPlayerAim = (PlayerPos - EnemyNormPos).GetSafeNormal();
						if (!ToPlayerAim.IsNearlyZero() && !AimDirection.IsNearlyZero())
						{
							const float CurAimAng = FMath::Atan2(AimDirection.Y, AimDirection.X);
							const float TgtAimAng = FMath::Atan2(ToPlayerAim.Y, ToPlayerAim.X);
							const float MaxAimStep = FMath::DegreesToRadians(Arch.AimTurnRateDeg) * InDeltaTime;
							const float AimStep = FMath::Clamp(FMath::FindDeltaAngleRadians(CurAimAng, TgtAimAng), -MaxAimStep, MaxAimStep);
							AimDirection = FVector2D(FMath::Cos(CurAimAng + AimStep), FMath::Sin(CurAimAng + AimStep));
						}
					}
					if (UCanvasPanelSlot* LineSlot = Cast<UCanvasPanelSlot>(EnemyWarningLine->Slot))
					{
						LineSlot->SetPosition(FVector2D(EnemyNormPos.X * GTRoomSize, EnemyNormPos.Y * GTRoomSize) - FVector2D(0.f, 2.f));
					}
					const float AimDeg = FMath::RadiansToDegrees(FMath::Atan2(AimDirection.Y, AimDirection.X));
					EnemyWarningLine->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D::ZeroVector, AimDeg));
					if (EnemyAimTimer <= 0.f)
					{
						EnemyWarningLine->SetVisibility(ESlateVisibility::Collapsed);
						LaserDir = AimDirection;   // 锁定方向, 之后不再追
						bLaserFiring = true;
						LaserActiveTimer = Arch.LaserDuration;
						LaserTickTimer = 0.f;
						EnemyShakeTimer = 0.3f;
					}
				}

				if (bLaserFiring)
				{
					LaserActiveTimer -= InDeltaTime;
					// 发射后光束朝玩家限速旋转(发射端始终锚在无人机 EnemyNormPos, 随其移动)。
					if (Arch.LaserTurnRateDeg > 0.f)
					{
						const FVector2D ToPlayerDir = (PlayerPos - EnemyNormPos).GetSafeNormal();
						if (!ToPlayerDir.IsNearlyZero())
						{
							const float CurAng = FMath::Atan2(LaserDir.Y, LaserDir.X);
							const float TgtAng = FMath::Atan2(ToPlayerDir.Y, ToPlayerDir.X);
							const float MaxStep = FMath::DegreesToRadians(Arch.LaserTurnRateDeg) * InDeltaTime;
							const float Step = FMath::Clamp(FMath::FindDeltaAngleRadians(CurAng, TgtAng), -MaxStep, MaxStep);
							const float NewAng = CurAng + Step;
							LaserDir = FVector2D(FMath::Cos(NewAng), FMath::Sin(NewAng));
						}
					}
					if (LaserBeamImage)
					{
						LaserBeamImage->SetVisibility(ESlateVisibility::HitTestInvisible);
						if (UCanvasPanelSlot* LaserSlot = Cast<UCanvasPanelSlot>(LaserBeamImage->Slot))
						{
							LaserSlot->SetPosition(FVector2D(EnemyNormPos.X * GTRoomSize, EnemyNormPos.Y * GTRoomSize) - FVector2D(0.f, 10.f));
						}
						const float LaserDeg = FMath::RadiansToDegrees(FMath::Atan2(LaserDir.Y, LaserDir.X));
						LaserBeamImage->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D::ZeroVector, LaserDeg));
					}
					// 镜头起手闪光: 跟随发射端(无人机本体)。
					if (LaserMuzzleImage)
					{
						LaserMuzzleImage->SetVisibility(ESlateVisibility::HitTestInvisible);
						if (UCanvasPanelSlot* MzSlot = Cast<UCanvasPanelSlot>(LaserMuzzleImage->Slot))
						{
							MzSlot->SetPosition(FVector2D(EnemyNormPos.X * GTRoomSize - 23.f, EnemyNormPos.Y * GTRoomSize - 23.f));
						}
					}
					LaserTickTimer -= InDeltaTime;
					if (LaserTickTimer <= 0.f)
					{
						const FVector2D ToPlayer = PlayerPos - EnemyNormPos;
						const float Proj = FVector2D::DotProduct(ToPlayer, LaserDir);
						const FVector2D Closest = EnemyNormPos + LaserDir * FMath::Max(0.f, Proj);
						const float PerpDist = FVector2D::Distance(PlayerPos, Closest);
						if (Proj > 0.f && PerpDist < 0.05f)
						{
							TryApplyMonsterHit();   // 站光束前方且贴近线 -> 扣血
							if (LaserImpactImage)
							{
								LaserImpactTimer = 0.14f;
								LaserImpactImage->SetVisibility(ESlateVisibility::HitTestInvisible);
								if (UCanvasPanelSlot* ImSlot = Cast<UCanvasPanelSlot>(LaserImpactImage->Slot))
								{
									ImSlot->SetPosition(FVector2D(PlayerPos.X * GTRoomSize - 26.f, PlayerPos.Y * GTRoomSize - 26.f));
								}
							}
						}
						LaserTickTimer = Arch.LaserTickInterval;
					}
					if (LaserActiveTimer <= 0.f)
					{
						bLaserFiring = false;
						if (LaserBeamImage) { LaserBeamImage->SetVisibility(ESlateVisibility::Collapsed); }
						if (LaserMuzzleImage) { LaserMuzzleImage->SetVisibility(ESlateVisibility::Collapsed); }
						EnemyAttackCooldownTimer = Arch.AttackInterval;
					}
				}
			}
			else // RangedProjectile: 红线瞄准 -> 单发飞弹(PR#9 远程, 未指定远程模式的兜底)。
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

			// 激光命中爆点衰减 + 非发射时确保镜头闪光隐藏。
			if (LaserImpactTimer > 0.f)
			{
				LaserImpactTimer -= InDeltaTime;
				if (LaserImpactTimer <= 0.f && LaserImpactImage) { LaserImpactImage->SetVisibility(ESlateVisibility::Collapsed); }
			}
			if (!bLaserFiring && LaserMuzzleImage && LaserMuzzleImage->GetVisibility() != ESlateVisibility::Collapsed)
			{
				LaserMuzzleImage->SetVisibility(ESlateVisibility::Collapsed);
			}

			// 怪物本体: 帧动画(待机循环/蓄力姿态/蝙蝠发射张嘴) + 朝向(源贴图朝右, 玩家在左则水平翻转面向玩家)。
			{
				EnemyAnimTime += InDeltaTime;
				if (EnemyFireFlashTimer > 0.f) { EnemyFireFlashTimer -= InDeltaTime; }
				const bool bCharging = (EnemyAimTimer > 0.f) || bLaserFiring;
				FString FrameKey;
				switch (Snapshot.EnemyType)
				{
				case EGT_MonsterType::Bat:
					if (EnemyFireFlashTimer > 0.f)  { FrameKey = TEXT("/Game/Graytail/Sprites/Monsters/bat_attack_1"); }
					else if (bCharging)             { FrameKey = TEXT("/Game/Graytail/Sprites/Monsters/bat_attack_0"); }
					else { FrameKey = FString::Printf(TEXT("/Game/Graytail/Sprites/Monsters/bat_idle_%d"), FMath::FloorToInt(EnemyAnimTime * 7.f) % 4); }
					break;
				case EGT_MonsterType::Drone:
					if (bCharging)
					{
						const float Prog = (Arch.AimDuration > 0.f && EnemyAimTimer > 0.f) ? (1.f - EnemyAimTimer / Arch.AimDuration) : 1.f;
						FrameKey = FString::Printf(TEXT("/Game/Graytail/Sprites/Monsters/drone_charge_%d"), FMath::Clamp(FMath::FloorToInt(Prog * 3.f), 0, 2));
					}
					else { FrameKey = FString::Printf(TEXT("/Game/Graytail/Sprites/Monsters/drone_idle_%d"), FMath::FloorToInt(EnemyAnimTime * 5.f) % 4); }
					break;
				default:
					// 史莱姆/小史莱姆: idle 用对应贴图; MeleeCircle warning 相显示蓄力帧, active 相显示扑击帧。
					if (Arch.Type == EGT_MonsterType::Slimeling)
					{
						FrameKey = TEXT("/Game/Graytail/Sprites/Monsters/enemy_slimeling");
					}
					else if (EnemyMeleePhase == 1)
					{
						FrameKey = TEXT("/Game/Graytail/Sprites/Monsters/slime_attack_0");
					}
					else if (EnemyMeleePhase == 2)
					{
						FrameKey = TEXT("/Game/Graytail/Sprites/Monsters/slime_attack_1");
					}
					else
					{
						FrameKey = TEXT("/Game/Graytail/Sprites/enemy_slime");
					}
					break;
				}
				if (FrameKey != CurrentEnemyFrameKey)
				{
					if (UTexture2D* BodyTex = LoadTextureAsset(FrameKey)) { EnemyImage->SetBrushFromTexture(BodyTex); CurrentEnemyFrameKey = FrameKey; }
				}

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
				// 怪物脚下阴影(地面层, 不随抖动/翻转/punch, 卖悬空感)。
				if (EnemyShadow)
				{
					EnemyShadow->SetVisibility(ESlateVisibility::HitTestInvisible);
					if (UCanvasPanelSlot* EShadowSlot = Cast<UCanvasPanelSlot>(EnemyShadow->Slot))
					{
						EShadowSlot->SetPosition(FVector2D(EnemyNormPos.X * GTRoomSize - 28.f, EnemyNormPos.Y * GTRoomSize + 22.f));
					}
				}
				const float FaceScaleX = (PlayerPos.X < EnemyNormPos.X) ? -1.f : 1.f;
				const float HitPunch = 1.f + 0.18f * EnemyHitFlash;   // 受击瞬间放大
				const float SlimeScale = (Snapshot.EnemyType == EGT_MonsterType::Slimeling) ? 0.7f : 1.f;   // 子体显"小"(纯表现)
				EnemyImage->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(FaceScaleX * HitPunch * SlimeScale, HitPunch * SlimeScale), FVector2D::ZeroVector, 0.f));
			}
			LastEnemyNormPos = EnemyNormPos;   // 记录怪物当前位置, 供死亡碎裂在其末位置播放
			LastEnemyType = Snapshot.EnemyType;

			// "靠近怪物"提示已移除(用户要求), CombatHintLabel 永久隐藏。
			if (CombatHintLabel) { CombatHintLabel->SetVisibility(ESlateVisibility::Collapsed); }

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

			// ===== A 步骤 slot-1: 第 2 只子史莱姆渲染(Enemies 中不等于 RepEnemyId 的怪); 不足 2 只优雅隐藏 =====
			// 异步死安全: 不再假设 Enemies[1] 稳定, 而是按 EnemyId 找 rep 槽之外的另一只。
			const FGT_CombatEnemyView* Slot1EnemyPtr = nullptr;
			for (const FGT_CombatEnemyView& E : Snapshot.Enemies)
			{
				if (E.EnemyId != RepEnemyId)
				{
					Slot1EnemyPtr = &E;
					break;
				}
			}
			if (Slot1EnemyPtr)
			{
				const FGT_CombatEnemyView& E1 = *Slot1EnemyPtr;
				const FGT_MonsterArchetype& Arch1 = GT_MonsterCatalog::GetArchetype(E1.Type);
				if (Slot1EnemyId != E1.EnemyId)
				{
					// slot-1 首次绑定/切换: 落点放母体/上一代表死亡位附近(向右偏移)+ 相位清零。
					Slot1EnemyId = E1.EnemyId;
					const FVector2D P = LastEnemyNormPos + FVector2D(0.18f + FMath::FRand() * 0.06f, 0.08f + (FMath::FRand() - 0.5f) * 0.10f);
					Slot1NormPos.X = FMath::Clamp(P.X, 0.12f, 0.88f);
					Slot1NormPos.Y = FMath::Clamp(P.Y, 0.12f, 0.88f);
					Slot1MeleePhase = 0; Slot1MeleePhaseTimer = 0.f; bSlot1MeleeHitResolved = false;
					Slot1HitFlashTimer = 0.f; Slot1PrevHp = E1.Hp; Slot1WanderTimer = 0.f;
				}
				// 受击闪白(独立 PrevHp)。
				if (Slot1PrevHp >= 0 && E1.Hp < Slot1PrevHp) { Slot1HitFlashTimer = 0.13f; PlaySfx(FName(TEXT("sfx_hit"))); }
				Slot1PrevHp = E1.Hp;
				if (Slot1HitFlashTimer > 0.f) { Slot1HitFlashTimer = FMath::Max(0.f, Slot1HitFlashTimer - InDeltaTime); }
				const float Flash1 = FMath::Clamp(Slot1HitFlashTimer / 0.13f, 0.f, 1.f);
				// 移动: RandomRoam(大体随机 + 轻微朝玩家偏置, 撞墙反弹); 犹豫窗口内不动。
				float Dist1 = FVector2D::Distance(PlayerPos, Slot1NormPos);
				if (!bMonsterFleeing)
				{
					Slot1WanderTimer -= InDeltaTime;
					if (Slot1WanderTimer <= 0.f)
					{
						Slot1WanderTimer = 0.35f + FMath::FRand() * 0.45f;
						const float R = FMath::FRandRange(-PI, PI);
						Slot1WanderDir = FVector2D(FMath::Cos(R), FMath::Sin(R));
					}
					const FVector2D ToP = (PlayerPos - Slot1NormPos).GetSafeNormal();
					const FVector2D Dir = (Slot1WanderDir * 0.8f + ToP * 0.2f).GetSafeNormal();
					FVector2D NP = Slot1NormPos + Dir * Arch1.MoveSpeed * InDeltaTime;
					if (NP.X < 0.12f || NP.X > 0.88f) { Slot1WanderDir.X = -Slot1WanderDir.X; }
					if (NP.Y < 0.12f || NP.Y > 0.88f) { Slot1WanderDir.Y = -Slot1WanderDir.Y; }
					Slot1NormPos.X = FMath::Clamp(NP.X, 0.12f, 0.88f);
					Slot1NormPos.Y = FMath::Clamp(NP.Y, 0.12f, 0.88f);
					Dist1 = FVector2D::Distance(PlayerPos, Slot1NormPos);
				}
				// 近战相位机(独立): idle->warning->active(圈内命中一次)->cooldown; 犹豫窗口内隐圈。
				if (bMonsterFleeing)
				{
					if (EnemyAttackCircle1) { EnemyAttackCircle1->SetVisibility(ESlateVisibility::Collapsed); }
				}
				else
				{
					Slot1MeleePhaseTimer -= InDeltaTime;
					if (Slot1MeleePhaseTimer <= 0.f)
					{
						switch (Slot1MeleePhase)
						{
						case 0: Slot1MeleePhase = 1; Slot1MeleePhaseTimer = Arch1.WarningDuration; bSlot1MeleeHitResolved = false; break;
						case 1:
							Slot1MeleePhase = 2; Slot1MeleePhaseTimer = Arch1.ActiveDuration; Slot1ShakeTimer = 0.3f;
							if (!bSlot1MeleeHitResolved && Dist1 <= Arch1.AttackRadius) { TryApplyMonsterHit(); }
							bSlot1MeleeHitResolved = true; break;
						case 2: Slot1MeleePhase = 3; Slot1MeleePhaseTimer = Arch1.CooldownDuration; break;
						default: Slot1MeleePhase = 0; Slot1MeleePhaseTimer = Arch1.IdleDuration; break;
						}
					}
					if (EnemyAttackCircle1)
					{
						const bool bShow1 = (Slot1MeleePhase == 1 || Slot1MeleePhase == 2);
						EnemyAttackCircle1->SetVisibility(bShow1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
						if (bShow1)
						{
							const float D = Arch1.AttackRadius * 2.f * GTRoomSize;
							if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(EnemyAttackCircle1->Slot)) { CS->SetPosition(FVector2D(Slot1NormPos.X * GTRoomSize - D * 0.5f, Slot1NormPos.Y * GTRoomSize - D * 0.5f)); }
							const float Pulse = (FMath::Sin(CombatAnimTime * 11.f) + 1.f) * 0.5f;
							EnemyAttackCircle1->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, (Slot1MeleePhase == 2) ? 0.82f : (0.28f + 0.18f * Pulse)));
						}
					}
				}
				// 本体: 小史莱姆独立图(加载失败回退母体图, 防透明隐身) + 受击提亮 + 朝向翻转 + scale 0.7 + punch + 抖动。
				if (EnemyImage1)
				{
					EnemyImage1->SetVisibility(ESlateVisibility::HitTestInvisible);
					FString Tex1;
					if (E1.Type == EGT_MonsterType::Slimeling) { Tex1 = TEXT("/Game/Graytail/Sprites/Monsters/enemy_slimeling"); }
					else if (Slot1MeleePhase == 1) { Tex1 = TEXT("/Game/Graytail/Sprites/Monsters/slime_attack_0"); }
					else if (Slot1MeleePhase == 2) { Tex1 = TEXT("/Game/Graytail/Sprites/Monsters/slime_attack_1"); }
					else { Tex1 = TEXT("/Game/Graytail/Sprites/enemy_slime"); }
					UTexture2D* T1 = LoadTextureAsset(*Tex1);
					if (!T1) { T1 = LoadTextureAsset(TEXT("/Game/Graytail/Sprites/enemy_slime")); }   // 回退: 防贴图缺失透明隐身
					if (T1) { EnemyImage1->SetBrushFromTexture(T1); }
					EnemyImage1->SetBrushTintColor(FSlateColor(FMath::Lerp(FLinearColor::White, FLinearColor(3.5f, 3.5f, 3.5f, 1.f), Flash1)));
					FVector2D Off1 = FVector2D::ZeroVector;
					if (Slot1ShakeTimer > 0.f) { Slot1ShakeTimer -= InDeltaTime; Off1.X = FMath::Sin(Slot1ShakeTimer * 60.f) * 8.f; }
					if (UCanvasPanelSlot* BS = Cast<UCanvasPanelSlot>(EnemyImage1->Slot)) { BS->SetPosition(FVector2D(Slot1NormPos.X * GTRoomSize - 40.f, Slot1NormPos.Y * GTRoomSize - 40.f) + Off1); }
					const float Face1 = (PlayerPos.X < Slot1NormPos.X) ? -1.f : 1.f;
					const float Punch1 = (1.f + 0.18f * Flash1) * 0.7f;
					EnemyImage1->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(Face1 * Punch1, Punch1), FVector2D::ZeroVector, 0.f));
				}
				if (EnemyShadow1)
				{
					EnemyShadow1->SetVisibility(ESlateVisibility::HitTestInvisible);
					if (UCanvasPanelSlot* SS = Cast<UCanvasPanelSlot>(EnemyShadow1->Slot)) { SS->SetPosition(FVector2D(Slot1NormPos.X * GTRoomSize - 28.f, Slot1NormPos.Y * GTRoomSize + 16.f)); }
				}
				// 血条 + 名牌(头顶跟随)。
				const float Ratio1 = (E1.MaxHp > 0) ? FMath::Clamp(static_cast<float>(E1.Hp) / E1.MaxHp, 0.f, 1.f) : 0.f;
				const FVector2D C1 = Slot1NormPos * GTRoomSize;
				const FVector2D Bar1(C1.X - 36.f, C1.Y - 40.f - 16.f);
				if (EnemyHpBarBg1) { EnemyHpBarBg1->SetVisibility(ESlateVisibility::HitTestInvisible); if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(EnemyHpBarBg1->Slot)) { S->SetPosition(Bar1); } }
				if (EnemyHpBarFill1) { EnemyHpBarFill1->SetVisibility(ESlateVisibility::HitTestInvisible); if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(EnemyHpBarFill1->Slot)) { S->SetPosition(Bar1); S->SetSize(FVector2D(72.f * Ratio1, 8.f)); } }
				if (EnemyNameLabel1)
				{
					EnemyNameLabel1->SetVisibility(ESlateVisibility::HitTestInvisible);
					if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(EnemyNameLabel1->Slot)) { S->SetPosition(FVector2D(C1.X - 80.f, C1.Y - 40.f - 34.f)); }
					EnemyNameLabel1->SetText(FText::FromString(E1.Name.IsEmpty() ? FString::Printf(TEXT("敌人  战力%d"), E1.Power) : FString::Printf(TEXT("%s  战力%d"), *E1.Name, E1.Power)));
				}
				Slot1LastNormPos = Slot1NormPos;
			}
			else
			{
				// 不足 2 只(母体未裂/单怪/蝙蝠/无人机): slot-1 优雅隐藏(不崩、不残留)。
				Slot1EnemyId = -1;
				if (EnemyImage1) { EnemyImage1->SetVisibility(ESlateVisibility::Collapsed); }
				if (EnemyShadow1) { EnemyShadow1->SetVisibility(ESlateVisibility::Collapsed); }
				if (EnemyHpBarBg1) { EnemyHpBarBg1->SetVisibility(ESlateVisibility::Collapsed); }
				if (EnemyHpBarFill1) { EnemyHpBarFill1->SetVisibility(ESlateVisibility::Collapsed); }
				if (EnemyNameLabel1) { EnemyNameLabel1->SetVisibility(ESlateVisibility::Collapsed); }
				if (EnemyAttackCircle1) { EnemyAttackCircle1->SetVisibility(ESlateVisibility::Collapsed); }
			}
		}
		else
		{
			// 战斗刚结束(上一帧还在战斗)且玩家未死 → 怪物被击杀, 在其末位置触发碎裂演出。
			// 战斗结束=击杀(仍在开战格)才播碎裂+死亡音; 逃跑(换了格)不播。
			if (bPrevInCombat && Snapshot.PlayerHp > 0 && !bPlayingDeathShatter
				&& CurrentCellX == LastCombatCellX && CurrentCellY == LastCombatCellY)
			{
				bPlayingDeathShatter = true;
				PlaySfx(FName(TEXT("sfx_death")));
				DeathShatterTimer = 0.f;
				DeathShatterFrame = -1;
				DeathShatterStartFrame = (LastEnemyType == EGT_MonsterType::Slimeling) ? 1 : 0;
				DeathEffectPos = LastEnemyNormPos;
				DeathShatterType = LastEnemyType;   // 按怪种选碎裂图(蝙蝠紫羽/无人机火花/史莱姆)
			}
			// slot-1(第 2 只子史莱姆)同刀死: 在其末位置补一发碎裂(cleave 同帧双死, 各槽各一发)。
			if (Slot1EnemyId != -1 && Snapshot.PlayerHp > 0 && !bSlot1Shatter
				&& CurrentCellX == LastCombatCellX && CurrentCellY == LastCombatCellY)
			{
				bSlot1Shatter = true;
				Slot1ShatterTimer = 0.f;
				Slot1ShatterFrame = -1;
				Slot1ShatterStartFrame = 1;
				Slot1ShatterPos = Slot1LastNormPos;
			}
			// slot-1 槽 + 代表 id 复位 + 隐藏(下场战斗从头来)。
			Slot1EnemyId = -1;
			RepEnemyId = -1;
			Slot1MeleePhase = 0; Slot1MeleePhaseTimer = 0.f; bSlot1MeleeHitResolved = false;
			Slot1HitFlashTimer = 0.f; Slot1PrevHp = -1; Slot1ShakeTimer = 0.f;
			if (EnemyImage1) { EnemyImage1->SetVisibility(ESlateVisibility::Collapsed); EnemyImage1->SetBrushTintColor(FSlateColor(FLinearColor::White)); EnemyImage1->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D::ZeroVector, 0.f)); }
			if (EnemyShadow1) { EnemyShadow1->SetVisibility(ESlateVisibility::Collapsed); }
			if (EnemyHpBarBg1) { EnemyHpBarBg1->SetVisibility(ESlateVisibility::Collapsed); }
			if (EnemyHpBarFill1) { EnemyHpBarFill1->SetVisibility(ESlateVisibility::Collapsed); }
			if (EnemyNameLabel1) { EnemyNameLabel1->SetVisibility(ESlateVisibility::Collapsed); }
			if (EnemyAttackCircle1) { EnemyAttackCircle1->SetVisibility(ESlateVisibility::Collapsed); }
			// 清理 + 重置(下一场战斗从头开始)。
			bPrevInCombat = false;
			CombatFleeTimer = 0.f;
			DroneDashTimer = 0.f;                       // 受击冲刺复位(防残留进下场战斗)
			DroneDashTarget = FVector2D::ZeroVector;
			if (EnemyImage) { EnemyImage->SetRenderOpacity(1.f); }
			EnemyAttackCooldownTimer = 0.f;
			EnemyAimTimer = 0.f;
			EnemyShakeTimer = 0.f;
			bProjectileActive = false;
			PlayerAttackCooldownTimer = 0.f;
			PlayerIFrameTimer = 0.f;
			// 挥砍弧光复位(切房/战斗结束防残留)。
			bPlayingSwing = false;
			if (PlayerSwingImage) PlayerSwingImage->SetVisibility(ESlateVisibility::Collapsed);
			EnemyNormPos = FVector2D(0.35f, 0.45f);
			EnemyMeleePhase = 0;
			EnemyMeleePhaseTimer = 0.f;
			bEnemyMeleeHitResolved = false;
			bPlayerInAttackRange = false;
			if (EnemyWarningLine) EnemyWarningLine->SetVisibility(ESlateVisibility::Collapsed);
			if (EnemyProjectileImage) EnemyProjectileImage->SetVisibility(ESlateVisibility::Collapsed);
			// 散射飞弹 / 拖尾 / 激光重置(防下场战斗残留)。
			for (int32 i = 0; i < GTMaxSpreadProjectiles; ++i)
			{
				bSpreadProjActive[i] = false;
				if (SpreadProjImages[i]) SpreadProjImages[i]->SetVisibility(ESlateVisibility::Collapsed);
			}
			for (int32 t = 0; t < GTMaxSpreadProjectiles * 2; ++t)
			{
				if (SpreadTrailImages[t]) SpreadTrailImages[t]->SetVisibility(ESlateVisibility::Collapsed);
			}
			bLaserFiring = false;
			LaserActiveTimer = 0.f;
			LaserTickTimer = 0.f;
			LaserImpactTimer = 0.f;
			if (LaserBeamImage) LaserBeamImage->SetVisibility(ESlateVisibility::Collapsed);
			if (LaserMuzzleImage) LaserMuzzleImage->SetVisibility(ESlateVisibility::Collapsed);
			if (LaserImpactImage) LaserImpactImage->SetVisibility(ESlateVisibility::Collapsed);
			// 本体帧动画/朝向复位(下场战斗从头来; 防缓存 desync)。
			EnemyAnimTime = 0.f;
			EnemyFireFlashTimer = 0.f;
			CurrentEnemyFrameKey.Reset();
			EnemyHitFlashTimer = 0.f;
			PrevEnemyHp = -1;
			if (EnemyShadow) EnemyShadow->SetVisibility(ESlateVisibility::Collapsed);
			if (EnemyImage)
			{
				EnemyImage->SetBrushTintColor(FSlateColor(FLinearColor::White));
				EnemyImage->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D::ZeroVector, 0.f));
			}
			if (EnemyAttackCircle) EnemyAttackCircle->SetVisibility(ESlateVisibility::Collapsed);
			if (CombatHintLabel) CombatHintLabel->SetVisibility(ESlateVisibility::Collapsed);
			if (EnemyHpBarBg) EnemyHpBarBg->SetVisibility(ESlateVisibility::Collapsed);
			if (EnemyHpBarFill) EnemyHpBarFill->SetVisibility(ESlateVisibility::Collapsed);
			if (EnemyNameLabel) EnemyNameLabel->SetVisibility(ESlateVisibility::Collapsed);
			if (PlayerHpBarBg) PlayerHpBarBg->SetVisibility(ESlateVisibility::Collapsed);
			if (PlayerHpBarFill) PlayerHpBarFill->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 怪物死亡碎裂演出推进(独立于战斗状态机; 在怪物末位置叠播 5 帧 80x80, 约 0.45s 后自隐)。
	if (bPlayingDeathShatter && DeathEffectImage)
	{
		DeathShatterTimer += InDeltaTime;
		const float DeathShatterPerFrame = 0.09f;
		const int32 Frame = FMath::FloorToInt(DeathShatterTimer / DeathShatterPerFrame);
		const int32 TotalFrames = 5 - DeathShatterStartFrame;   // 母体 5 帧(0-4), 小史莱姆 4 帧(1-4)
		if (Frame >= TotalFrames)
		{
			bPlayingDeathShatter = false;
			DeathEffectImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			if (Frame != DeathShatterFrame)
			{
				DeathShatterFrame = Frame;
				const int32 TexFrame = DeathShatterStartFrame + Frame;   // 实际贴图帧号(小史莱姆从 1 开始)
				const TCHAR* ShatterPrefix =
					(DeathShatterType == EGT_MonsterType::Bat) ? TEXT("bat_shatter") :
					(DeathShatterType == EGT_MonsterType::Drone) ? TEXT("drone_shatter") : TEXT("slime_shatter");
				if (UTexture2D* FrameTex = LoadTextureAsset(FString::Printf(TEXT("/Game/Graytail/Sprites/Effects/%s_%d"), ShatterPrefix, TexFrame)))
				{
					DeathEffectImage->SetBrushFromTexture(FrameTex);
				}
			}
			DeathEffectImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* DeathSlot = Cast<UCanvasPanelSlot>(DeathEffectImage->Slot))
			{
				DeathSlot->SetPosition(FVector2D(DeathEffectPos.X * GTRoomSize - 40.f, DeathEffectPos.Y * GTRoomSize - 40.f));
			}
		}
	}

	// slot-1(第 2 只子史莱姆)死亡碎裂推进(独立碎裂层 DeathEffectImage1; 子体一律 slime_shatter)。
	if (bSlot1Shatter && DeathEffectImage1)
	{
		Slot1ShatterTimer += InDeltaTime;
		const int32 Frame = FMath::FloorToInt(Slot1ShatterTimer / 0.09f);
		const int32 TotalFrames = 5 - Slot1ShatterStartFrame;
		if (Frame >= TotalFrames)
		{
			bSlot1Shatter = false;
			DeathEffectImage1->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			if (Frame != Slot1ShatterFrame)
			{
				Slot1ShatterFrame = Frame;
				const int32 TexFrame = Slot1ShatterStartFrame + Frame;
				if (UTexture2D* FrameTex = LoadTextureAsset(FString::Printf(TEXT("/Game/Graytail/Sprites/Effects/slime_shatter_%d"), TexFrame)))
				{
					DeathEffectImage1->SetBrushFromTexture(FrameTex);
				}
			}
			DeathEffectImage1->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* DS = Cast<UCanvasPanelSlot>(DeathEffectImage1->Slot))
			{
				DS->SetPosition(FVector2D(Slot1ShatterPos.X * GTRoomSize - 40.f, Slot1ShatterPos.Y * GTRoomSize - 40.f));
			}
		}
	}

	// 玩家挥砍弧光演出推进(套死亡碎裂模板): 单组弧光帧, 贴玩家前方半身位 + 按 SwingDir 旋转, ~0.24s 后自隐。
	// 帧数/帧长先按占位参数(实际等素材定); 贴图缺失时只 Warning, 不切贴图、不报错(LoadTextureAsset 已静默)。
	if (bPlayingSwing && PlayerSwingImage)
	{
		SwingTimer += InDeltaTime;
		const int32 Frame = FMath::FloorToInt(SwingTimer / GTSwingPerFrame);
		if (Frame >= GTSwingFrameCount)
		{
			bPlayingSwing = false;
			PlayerSwingImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			if (Frame != SwingFrame)
			{
				SwingFrame = Frame;
				// 单组弧光帧(按朝向旋转, 不分四向画); 缺失返回 null 则跳过切图、不崩, 素材后补。
				if (UTexture2D* SwingTex = LoadTextureAsset(FString::Printf(TEXT("/Game/Graytail/Sprites/Effects/player_slash_%d"), Frame)))
				{
					PlayerSwingImage->SetBrushFromTexture(SwingTex);
				}
			}
			PlayerSwingImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			// 定位玩家前方半身位(玩家中心沿 SwingDir 推出), 居中后按 SwingAngleDeg 旋转(参照预警红线旋转)。
			const FVector2D SwingCenterPx = PlayerPos * GTRoomSize + SwingDir * (GTPlayerSize * 0.55f);
			if (UCanvasPanelSlot* SwingSlot = Cast<UCanvasPanelSlot>(PlayerSwingImage->Slot))
			{
				SwingSlot->SetPosition(SwingCenterPx - FVector2D(40.f, 40.f));   // 80x80 半尺寸居中
			}
			PlayerSwingImage->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D::ZeroVector, SwingAngleDeg));
		}
	}
	// ==========================================


	// 玩家受击闪白(亮白 tint 衰减; 配合 PlayMineFlash 全屏红光, 双反馈)。
	if (PlayerImage)
	{
		if (PlayerHitFlashTimer > 0.f) { PlayerHitFlashTimer = FMath::Max(0.f, PlayerHitFlashTimer - InDeltaTime); }
		const float PlayerFlash = FMath::Clamp(PlayerHitFlashTimer / 0.14f, 0.f, 1.f);
		PlayerImage->SetColorAndOpacity(FMath::Lerp(FLinearColor::White, FLinearColor(3.f, 3.f, 3.f, 1.f), PlayerFlash));
	}

	const float InputX = (HeldKeys[3] ? 1.f : 0.f) - (HeldKeys[1] ? 1.f : 0.f);
	const float InputY = (HeldKeys[2] ? 1.f : 0.f) - (HeldKeys[0] ? 1.f : 0.f);
	const bool bHasInput = (InputX != 0.f || InputY != 0.f);

	// 逃跑警示条(单次 F 即逃跑, 不暂停, 怪继续打): 站在 Standard 战斗房有效门边时自动显示警示, 离开门口/清房即收起。
	// 纯位置驱动: 不用两态门控也不用倒计时 —— 在门口就显示, 离开就消失(单次 F 由 TryHandleFleeKey 处理)。
	{
		int32 DoorX = 0, DoorY = 0;
		bool bAtFleeDoor = false;
		if (const UGT_RunContext* FleeRC = GetRunContext())
		{
			if (FleeRC->GetMapMode() == EGT_MapMode::Standard && FindFleeDoorDir(DoorX, DoorY))
			{
				UGT_DebugSubsystem* FleeDebug = GetDebugSubsystem();
				FGT_DebugRunSnapshot FleeSnap;
				bAtFleeDoor = FleeDebug && FleeDebug->GetDebugRunSnapshot(FleeSnap)
					&& FleeSnap.bCombatActive && FleeSnap.CurrentRoomBaseType == EGT_RoomBaseType::Combat;
			}
		}
		if (bAtFleeDoor && FleeConfirmLabel)
		{
			if (FleeConfirmLabel->GetVisibility() != ESlateVisibility::HitTestInvisible)
			{
				FleeConfirmLabel->SetRenderOpacity(1.f);
				FleeConfirmLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
		else
		{
			ClearFleePrompt();
		}
	}

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
		// 挥砍本体帧最高优先级(站立挥砍也切挥剑帧)。WalkAnimTime 置正, 使挥砍结束后下一帧回 idle 帧(不卡在挥剑帧)。
		if (TryApplySwingBodyFrame())
		{
			WalkAnimTime = GTWalkFrameTime;
			return;
		}
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
		// 放开斜向: facing 同时取两轴符号 -> 挥砍锥/弧光(用连续 Facing 向量)立刻支持 8 向瞄准。
		// 本体帧仍 4 向: 斜向 facing 经 GTDirIndex 自动并到正方向(GetIdle/GetWalk/GetSwing 都走它), 0 新美术。
		LastDirX = FMath::Sign(InputX);
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
	const float EdgeMargin = 0.12f;   // 玩家活动边界(也是过门触发线): 缩进留在地板内, 别走到墙/背景外
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
	// 挥砍中本体切挥剑帧, 跳过本帧 walk 轮播(位移已在上方更新, 不锁脚; 素材缺失则回退走 walk)。
	if (TryApplySwingBodyFrame())
	{
		return;
	}
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

	// 战斗中(Standard 怪物房)禁止自动过门: 唯一出路 = 门口 F 确认逃跑(扣惩罚)。
	// 仅当目标是有效相邻格时拦截(地图边界仍走下面的撞墙/边缘提示); 清房后 bCombatActive=false 恢复自由走门。
	{
		const UGT_RunContext* RC = GetRunContext();
		FGT_DebugRunSnapshot CombatSnap;
		if (RC && RC->GetMapMode() == EGT_MapMode::Standard
			&& RC->IsValidMapCoord(CurrentCellX + DirX, CurrentCellY + DirY)
			&& Debug->GetDebugRunSnapshot(CombatSnap)
			&& CombatSnap.bCombatActive && CombatSnap.CurrentRoomBaseType == EGT_RoomBaseType::Combat)
		{
			CrossRetryCooldown = 0.4f;     // 撞门即拦, 防逐帧重试抽搐(贴门原地踏步); 警示条由 NativeTick 按位置自动显示, 单次 F 逃跑
			return;
		}
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
	// 从新房间对面的门走进来(落在活动边界 0.12 内, 避免入场瞬间被夹回)。
	PlayerPos = FVector2D(0.5 - DirX * 0.36, 0.5 - DirY * 0.36);
	MoveVelocity = FVector2D::ZeroVector;
	RefreshRoomDecor();
	UpdatePlayerImagePosition();

	// 进入雷格 = 踩雷, 红光闪烁反馈(内核已扣血, 这里只做表现)。
	// 只在本次真扣了血(新引爆)时红闪; 重进已引爆的废墟雷格 HP 不变 → 不重复闪。
	if (Snapshot.CurrentRoomBaseType == EGT_RoomBaseType::Mine
		&& PrevPlayerHp >= 0 && Snapshot.PlayerHp < PrevPlayerHp)
	{
		PlayMineFlash();
		PlaySfx(FName(TEXT("sfx_explosion")));
	}
	PrevPlayerHp = Snapshot.PlayerHp;

	// 通知 HUD 整体刷新(状态行/小地图)。
	if (OnRoomChanged.IsBound())
	{
		OnRoomChanged.Execute();
	}
}

bool UGT_RoomViewWidget::FindFleeDoorDir(int32& OutDirX, int32& OutDirY) const
{
	// 玩家是否站在某条有效门边(同 TryCrossDoor 的 EdgeMargin + 门宽阈值): 贴边 + 对准门 + 该向有相邻格。
	// 移动会把 PlayerPos 夹在 [EdgeMargin, 1-EdgeMargin], 顶墙时正好落在边界值, 用小容差判"在门口"。
	const UGT_RunContext* RC = GetRunContext();
	if (!RC)
	{
		return false;
	}
	const float EdgeMargin = 0.12f;        // 与 NativeTick 移动段一致
	const float Reach = 0.06f;             // "贴门"容差(顶墙站在 EdgeMargin, 给一点余量)
	auto Aligned = [](float V) { return FMath::Abs(V - 0.5f) <= GTDoorHalfWidth; };

	if (PlayerPos.Y <= EdgeMargin + Reach && Aligned(PlayerPos.X) && RC->IsValidMapCoord(CurrentCellX, CurrentCellY - 1)) { OutDirX = 0; OutDirY = -1; return true; }
	if (PlayerPos.Y >= 1.f - EdgeMargin - Reach && Aligned(PlayerPos.X) && RC->IsValidMapCoord(CurrentCellX, CurrentCellY + 1)) { OutDirX = 0; OutDirY = 1; return true; }
	if (PlayerPos.X <= EdgeMargin + Reach && Aligned(PlayerPos.Y) && RC->IsValidMapCoord(CurrentCellX - 1, CurrentCellY)) { OutDirX = -1; OutDirY = 0; return true; }
	if (PlayerPos.X >= 1.f - EdgeMargin - Reach && Aligned(PlayerPos.Y) && RC->IsValidMapCoord(CurrentCellX + 1, CurrentCellY)) { OutDirX = 1; OutDirY = 0; return true; }
	return false;
}

void UGT_RoomViewWidget::ClearFleePrompt()
{
	if (FleeConfirmLabel && FleeConfirmLabel->GetVisibility() != ESlateVisibility::Collapsed)
	{
		FleeConfirmLabel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UGT_RoomViewWidget::TryHandleFleeKey()
{
	// 仅 Standard 怪物房战斗中、站在有效门边时拦截 F 走逃跑流程; 其它情况返回 false(F 照常搜索/开箱)。
	const UGT_RunContext* RC = GetRunContext();
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	if (!RC || !Debug || RC->GetMapMode() != EGT_MapMode::Standard)
	{
		return false;
	}
	FGT_DebugRunSnapshot Snapshot;
	if (!Debug->GetDebugRunSnapshot(Snapshot)
		|| !Snapshot.bCombatActive
		|| Snapshot.CurrentRoomBaseType != EGT_RoomBaseType::Combat)
	{
		return false;
	}
	int32 DirX = 0, DirY = 0;
	if (!FindFleeDoorDir(DirX, DirY))
	{
		return false;   // 不在门边: 不拦截(战斗房 F 搜索会被内核拒, 无害)
	}

	// 单次 F: 直接走命令管线扣惩罚(掉钱+掉白货), 成功后过门(此时 bCombatActive 已 false)。
	if (OnFleeRequested.IsBound())
	{
		OnFleeRequested.Execute();
	}
	ClearFleePrompt();
	TryCrossDoor(DirX, DirY);
	return true;
}
