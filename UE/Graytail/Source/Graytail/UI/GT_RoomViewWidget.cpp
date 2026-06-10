#include "UI/GT_RoomViewWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Debug/GT_DebugSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr float GTRoomSize = 560.f;        // 房间画布边长(px)
	constexpr float GTPlayerSize = 64.f;
	constexpr float GTDoorSize = 72.f;
	constexpr float GTMoveSpeed = 0.55f;       // 归一化坐标/秒, 对齐 Lua moveSpeed 300px 的手感
	constexpr float GTDoorHalfWidth = 0.13f;   // 门对准判定半宽(归一化)
	constexpr float GTWalkFrameTime = 0.18f;   // 走路两帧轮播间隔

	const FLinearColor GTFloor_Normal(0.16f, 0.17f, 0.20f, 1.f);
	const FLinearColor GTFloor_Mine(0.45f, 0.12f, 0.10f, 1.f);
	const FLinearColor GTFloor_Exit(0.10f, 0.35f, 0.16f, 1.f);
	const FLinearColor GTFloor_Combat(0.40f, 0.22f, 0.08f, 1.f);
	const FLinearColor GTFloor_Event(0.28f, 0.16f, 0.38f, 1.f);
	const FLinearColor GTFloor_Chest(0.40f, 0.33f, 0.10f, 1.f);
	const FLinearColor GTDoorColor(0.55f, 0.45f, 0.30f, 1.f);
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
		if (UCanvasPanelSlot* DoorSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(DoorImages[DoorIndex])))
		{
			DoorSlot->SetPosition(DoorPositions[DoorIndex]);
			const bool bHorizontalDoor = DoorIndex < 2;
			DoorSlot->SetSize(bHorizontalDoor ? FVector2D(GTDoorSize, 14.f) : FVector2D(14.f, GTDoorSize));
		}
	}

	// 房型标签(雷/撤离点/宝箱等提示文字, 居中)。
	RoomLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	RoomLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 22));
	RoomLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.45f)));
	if (UCanvasPanelSlot* LabelSlot = Cast<UCanvasPanelSlot>(RoomCanvas->AddChild(RoomLabel)))
	{
		LabelSlot->SetPosition(FVector2D(GTRoomSize * 0.5f - 180.f, GTRoomSize * 0.5f - 90.f));
		LabelSlot->SetSize(FVector2D(380.f, 40.f));
	}

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

UTexture2D* UGT_RoomViewWidget::LoadAssetTexture(const FString& Folder, const FString& FilePrefix)
{
	if (UTexture2D** Cached = TextureCache.Find(FilePrefix))
	{
		return *Cached;
	}

	// 素材文件名带导出时间戳, 按前缀模糊匹配第一个命中的 png。
	const FString AssetDir = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT(".."), TEXT("assets"), Folder));
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *(AssetDir / FilePrefix + TEXT("*.png")), true, false);

	UTexture2D* Texture = FoundFiles.Num() > 0
		? FImageUtils::ImportFileAsTexture2D(AssetDir / FoundFiles[0])
		: nullptr;
	TextureCache.Add(FilePrefix, Texture);
	return Texture;
}

UTexture2D* UGT_RoomViewWidget::GetWalkFrame(int32 DirX, int32 DirY, int32 FrameIndex)
{
	const TCHAR* DirName = DirY < 0 ? TEXT("up") : DirY > 0 ? TEXT("down") : DirX < 0 ? TEXT("left") : TEXT("right");
	return LoadAssetTexture(TEXT("image"), FString::Printf(TEXT("player_walk_%s_%d_"), DirName, FrameIndex + 1));
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

	// 房间底图: 复用 Lua 版 Textures/room_*.png, 按房型切换(对齐 DungeonRoom.lua)。
	FLinearColor FloorColor = GTFloor_Normal;
	const TCHAR* FloorTexturePrefix = TEXT("room_safe");
	FString Label;
	switch (Snapshot.CurrentRoomBaseType)
	{
	case EGT_RoomBaseType::Mine:
		FloorColor = GTFloor_Mine; FloorTexturePrefix = TEXT("room_danger"); Label = TEXT("雷区废墟"); break;
	case EGT_RoomBaseType::Exit:
		FloorColor = GTFloor_Exit; FloorTexturePrefix = TEXT("room_exit"); Label = TEXT("撤离点 (Extract)"); break;
	case EGT_RoomBaseType::Combat:
		FloorColor = GTFloor_Combat; FloorTexturePrefix = TEXT("room_monster"); break;
	case EGT_RoomBaseType::Event:
		FloorColor = GTFloor_Event; FloorTexturePrefix = TEXT("room_event"); Label = TEXT("异常事件"); break;
	case EGT_RoomBaseType::Chest:
		FloorColor = GTFloor_Chest; FloorTexturePrefix = TEXT("room_treasure"); Label = TEXT("补给箱 (Search)"); break;
	default:
		break;
	}

	if (UTexture2D* FloorTexture = LoadAssetTexture(TEXT("Textures"), FloorTexturePrefix))
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

	// 本房周围雷数直接显示在房间里(不用回头看小地图)。
	TArray<FGT_MiniMapCellViewData> IntelCells;
	int32 IntelWidth = 0;
	int32 IntelHeight = 0;
	if (Debug->GetDebugMiniMapViewData(IntelCells, IntelWidth, IntelHeight)
		&& IntelCells.IsValidIndex(CurrentCellY * IntelWidth + CurrentCellX))
	{
		const FGT_MiniMapCellViewData& CurrentIntel = IntelCells[CurrentCellY * IntelWidth + CurrentCellX];
		if (CurrentIntel.bScanned)
		{
			if (!Label.IsEmpty())
			{
				Label += TEXT("  ");
			}
			Label += FString::Printf(TEXT("周围雷数: %d"), CurrentIntel.DisplayedNumber);
		}
	}
	RoomLabel->SetText(FText::FromString(Label));

	// 战斗激活时显示史莱姆。
	const bool bShowEnemy = Snapshot.bCombatActive
		&& Snapshot.CurrentRoomBaseType == EGT_RoomBaseType::Combat;
	if (bShowEnemy && !EnemyImage->GetBrush().GetResourceObject())
	{
		if (UTexture2D* SlimeTexture = LoadAssetTexture(TEXT("image"), TEXT("enemy_slime_")))
		{
			EnemyImage->SetBrushFromTexture(SlimeTexture);
		}
	}
	EnemyImage->SetVisibility(bShowEnemy ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// 初始站立帧。
	if (UTexture2D* IdleFrame = GetWalkFrame(LastDirX, LastDirY, 0))
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
	if (Key == EKeys::W) { HeldKeys[0] = true; return FReply::Handled(); }
	if (Key == EKeys::A) { HeldKeys[1] = true; return FReply::Handled(); }
	if (Key == EKeys::S) { HeldKeys[2] = true; return FReply::Handled(); }
	if (Key == EKeys::D) { HeldKeys[3] = true; return FReply::Handled(); }
	if (Key == EKeys::F)
	{
		// F = 搜索/开箱(对齐原版底部快捷键栏)。
		if (OnSearchRequested.IsBound())
		{
			OnSearchRequested.Execute();
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
	if (Key == EKeys::W) { HeldKeys[0] = false; return FReply::Handled(); }
	if (Key == EKeys::A) { HeldKeys[1] = false; return FReply::Handled(); }
	if (Key == EKeys::S) { HeldKeys[2] = false; return FReply::Handled(); }
	if (Key == EKeys::D) { HeldKeys[3] = false; return FReply::Handled(); }
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

void UGT_RoomViewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CurrentCellX < 0)
	{
		return;
	}

	const float InputX = (HeldKeys[3] ? 1.f : 0.f) - (HeldKeys[1] ? 1.f : 0.f);
	const float InputY = (HeldKeys[2] ? 1.f : 0.f) - (HeldKeys[0] ? 1.f : 0.f);
	if (InputX == 0.f && InputY == 0.f)
	{
		WalkAnimTime = 0.f;
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
