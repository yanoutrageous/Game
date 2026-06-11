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
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Events.h"
#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr float GTRoomSize = 560.f;        // 房间画布边长(px)
	constexpr float GTPlayerSize = 64.f;
	constexpr float GTDoorSize = 72.f;
	constexpr float GTMoveSpeed = 0.385f;      // 归一化坐标/秒(Lua moveSpeed 手感的 70%, 用户调速)
	constexpr float GTDoorHalfWidth = 0.13f;   // 门对准判定半宽(归一化)
	constexpr float GTWalkFrameTime = 0.18f;   // 走路两帧轮播间隔
	constexpr float GTChestOpenDuration = 1.4f;    // 开箱金光时长(对齐 Lua CHEST_OPEN_DURATION)
	constexpr float GTChestRewardDuration = 2.0f;  // 奖励飘字时长(对齐 Lua CHEST_REWARD_DURATION)

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
	ChestCaption->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 12));
	ChestCaption->SetJustification(ETextJustify::Center);
	ChestCaption->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* CaptionSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(ChestCaption)))
	{
		CaptionSlot->SetPosition(FVector2D(GTRoomSize * 0.5f - 100.f, GTRoomSize * 0.5f + 34.f));
		CaptionSlot->SetSize(FVector2D(200.f, 18.f));
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
		Text->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 15));
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

	// 周围雷险标牌(原版 ui_mine_risk_tag 底图自带图标), 房间底部居中, 未扫描时隐藏。
	MineRiskTag = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MineRiskTag->SetWidthOverride(220.f);
	MineRiskTag->SetHeightOverride(44.f);
	MineRiskTag->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* TagSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(MineRiskTag)))
	{
		TagSlot->SetPosition(FVector2D(GTRoomSize * 0.5f - 110.f, GTRoomSize - 62.f));
		TagSlot->SetSize(FVector2D(220.f, 44.f));
	}
	UBorder* TagBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	if (UTexture2D* TagTexture = LoadTextureAsset(TEXT("/Game/Graytail/UI/hud/ui_mine_risk_tag")))
	{
		FSlateBrush TagBrush;
		TagBrush.SetResourceObject(TagTexture);
		TagBrush.DrawAs = ESlateBrushDrawType::Image;
		TagBorder->SetBrush(TagBrush);
	}
	// 文字避开底图左侧自带的图标。
	TagBorder->SetPadding(FMargin(52.f, 0.f, 12.f, 0.f));
	MineRiskTag->SetContent(TagBorder);
	RoomLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	RoomLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 16));
	RoomLabel->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(232, 222, 198))));
	TagBorder->SetContent(RoomLabel);
	TagBorder->SetVerticalAlignment(VAlign_Center);

	// 怪物(史莱姆), 战斗激活时显示在房间偏左上(对齐 Lua monsterPosition 0.35/0.45)。
	EnemyImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	EnemyImage->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* EnemySlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(EnemyImage)))
	{
		EnemySlot->SetPosition(FVector2D(GTRoomSize * 0.35f - 40.f, GTRoomSize * 0.45f - 40.f));
		EnemySlot->SetSize(FVector2D(80.f, 80.f));
	}

	// 玩家(走路贴图)。
	PlayerImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UCanvasPanelSlot* PlayerSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(PlayerImage)))
	{
		PlayerSlot->SetSize(FVector2D(GTPlayerSize, GTPlayerSize));
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
	}
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

	// 周围雷险标牌(原版 ui_mine_risk_tag): 本房已扫描才显示。
	TArray<FGT_MiniMapCellViewData> IntelCells;
	int32 IntelWidth = 0;
	int32 IntelHeight = 0;
	bool bShowMineTag = false;
	if (Debug->GetDebugMiniMapViewData(IntelCells, IntelWidth, IntelHeight)
		&& IntelCells.IsValidIndex(CurrentCellY * IntelWidth + CurrentCellX))
	{
		const FGT_MiniMapCellViewData& CurrentIntel = IntelCells[CurrentCellY * IntelWidth + CurrentCellX];
		if (CurrentIntel.bScanned)
		{
			RoomLabel->SetText(FText::FromString(FString::Printf(TEXT("周围雷险: %d"), CurrentIntel.DisplayedNumber)));
			bShowMineTag = true;
		}
	}
	MineRiskTag->SetVisibility(bShowMineTag ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

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

	// 焦点在弹窗/按钮上时暂停移动(持键状态仍经 HUD 冒泡转发保持准确, 关弹窗立刻续走)。
	if (!HasKeyboardFocus())
	{
		return;
	}

	const float InputX = (HeldKeys[3] ? 1.f : 0.f) - (HeldKeys[1] ? 1.f : 0.f);
	const float InputY = (HeldKeys[2] ? 1.f : 0.f) - (HeldKeys[0] ? 1.f : 0.f);
	if (InputX == 0.f && InputY == 0.f)
	{
		if (WalkAnimTime > 0.f)
		{
			// 停步 -> 站立帧(对齐 Lua idleFrames)。
			if (UTexture2D* IdleFrame = GetIdleFrame(LastDirX, LastDirY))
			{
				PlayerImage->SetBrushFromTexture(IdleFrame);
			}
			WalkAnimTime = 0.f;
		}
		return;
	}

	LastDirX = InputY != 0.f ? 0 : FMath::Sign(InputX);
	LastDirY = FMath::Sign(InputY);

	FVector2D NewPos = PlayerPos + FVector2D(InputX, InputY).GetSafeNormal() * GTMoveSpeed * InDeltaTime;

	// 越界 + 对准门 -> 尝试过门(对齐 Lua isAlignedWithDoor)。
	const float EdgeMargin = GTPlayerSize * 0.5f / GTRoomSize;
	if (NewPos.Y < EdgeMargin && InputY < 0.f && FMath::Abs(NewPos.X - 0.5f) <= GTDoorHalfWidth) { TryCrossDoor(0, -1); return; }
	if (NewPos.Y > 1.f - EdgeMargin && InputY > 0.f && FMath::Abs(NewPos.X - 0.5f) <= GTDoorHalfWidth) { TryCrossDoor(0, 1); return; }
	if (NewPos.X < EdgeMargin && InputX < 0.f && FMath::Abs(NewPos.Y - 0.5f) <= GTDoorHalfWidth) { TryCrossDoor(-1, 0); return; }
	if (NewPos.X > 1.f - EdgeMargin && InputX > 0.f && FMath::Abs(NewPos.Y - 0.5f) <= GTDoorHalfWidth) { TryCrossDoor(1, 0); return; }

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
	if (bMoved)
	{
		CurrentCellX = Snapshot.PlayerX;
		CurrentCellY = Snapshot.PlayerY;
		// 从新房间对面的门走进来。
		PlayerPos = FVector2D(0.5 - DirX * 0.42, 0.5 - DirY * 0.42);
		RefreshRoomDecor();
	}
	else
	{
		// 内核拒绝(边界/死亡等): 退回门口。
		PlayerPos = FVector2D(
			FMath::Clamp(PlayerPos.X, 0.1, 0.9),
			FMath::Clamp(PlayerPos.Y, 0.1, 0.9));
	}
	UpdatePlayerImagePosition();

	// 通知 HUD 整体刷新(状态行/小地图)。
	if (OnRoomChanged.IsBound())
	{
		OnRoomChanged.Execute();
	}
}
