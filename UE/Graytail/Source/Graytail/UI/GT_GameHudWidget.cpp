#include "UI/GT_GameHudWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "UI/GT_RoomViewWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Core/GT_RunContext.h"
#include "Core/GT_RunSubsystem.h"
#include "Debug/GT_DebugSubsystem.h"
#include "Domains/Inventory/GT_ItemCatalog.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

namespace
{
	// 小地图配色: 对齐 Lua 版小地图的语义色(雷红/撤离绿/战斗橙/事件紫/宝箱黄)。
	const FLinearColor GTCellColor_Unknown(0.10f, 0.10f, 0.12f, 1.f);
	const FLinearColor GTCellColor_Explored(0.30f, 0.32f, 0.36f, 1.f);
	const FLinearColor GTCellColor_Player(0.15f, 0.55f, 0.90f, 1.f);
	const FLinearColor GTCellColor_Mine(0.75f, 0.15f, 0.12f, 1.f);
	const FLinearColor GTCellColor_Exit(0.15f, 0.65f, 0.25f, 1.f);
	const FLinearColor GTCellColor_Combat(0.85f, 0.45f, 0.10f, 1.f);
	const FLinearColor GTCellColor_Event(0.55f, 0.30f, 0.75f, 1.f);
	const FLinearColor GTCellColor_Chest(0.85f, 0.70f, 0.15f, 1.f);

	FString GetRunStateLabel(EGT_RunState RunState)
	{
		switch (RunState)
		{
		case EGT_RunState::Running:
			return TEXT("探索中");
		case EGT_RunState::Failed:
			return TEXT("死亡 - 点 NewRun 重开");
		case EGT_RunState::Succeeded:
			return TEXT("撤离成功!");
		default:
			return TEXT("未开局 - 点 NewRun");
		}
	}
}

TSharedRef<SWidget> UGT_GameHudWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UGT_GameHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshAll();
}

void UGT_GameHudWidget::BuildWidgetTree()
{
	// 半透明深色底板铺满屏幕, 保证亮场景下可读。
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Background->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.9f));
	Background->SetPadding(FMargin(24.f));
	Background->SetHorizontalAlignment(HAlign_Left);
	Background->SetVerticalAlignment(VAlign_Top);
	WidgetTree->RootWidget = Background;

	// 左房间视图 + 右信息面板。
	UHorizontalBox* Main = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Background->SetContent(Main);

	RoomView = CreateWidget<UGT_RoomViewWidget>(this, UGT_RoomViewWidget::StaticClass());
	if (RoomView)
	{
		RoomView->OnRoomChanged.BindUObject(this, &UGT_GameHudWidget::RefreshPanels);
		RoomView->OnSearchRequested.BindUObject(this, &UGT_GameHudWidget::OnSearch);
		USizeBox* RoomSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RoomSize->SetWidthOverride(560.f);
		RoomSize->SetHeightOverride(560.f);
		RoomSize->AddChild(RoomView);
		Main->AddChild(RoomSize);
	}

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Main->AddChild(Root);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 18));
	Root->AddChildToVerticalBox(StatusText);

	MiniMapGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	MiniMapGrid->SetSlotPadding(FMargin(1.f));
	Root->AddChildToVerticalBox(MiniMapGrid);

	// 图例(对齐原版左面板说明文字)。
	UTextBlock* LegendText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LegendText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 11));
	LegendText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.65f, 0.75f, 1.f)));
	LegendText->SetText(FText::FromString(TEXT("数字 = 周围8格雷险 · 蓝点 = 可点击回传")));
	Root->AddChildToVerticalBox(LegendText);

	ItemsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Root->AddChildToVerticalBox(ItemsRow);

	// 移动靠 WASD 在房间里走, 不再提供方向按钮。
	UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Root->AddChildToVerticalBox(ActionRow);
	SearchButton = MakeButton(ActionRow, TEXT("Search"));
	SearchButton->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnSearch);
	MakeButton(ActionRow, TEXT("Attack"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnAttack);
	MakeButton(ActionRow, TEXT("Extract"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnExtract);
	MakeButton(ActionRow, TEXT("NewRun"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnNewRun);

	// 快捷键栏(对齐原版底部 hotbar)。
	UTextBlock* HotbarText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	HotbarText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 12));
	HotbarText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.72f, 0.8f, 1.f)));
	HotbarText->SetText(FText::FromString(TEXT("WASD 移动  |  F 搜索/开箱  |  小地图点已探索格回传")));
	Root->AddChildToVerticalBox(HotbarText);

	LogText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LogText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 13));
	LogText->SetAutoWrapText(true);
	Root->AddChildToVerticalBox(LogText);
}

UButton* UGT_GameHudWidget::MakeButton(UHorizontalBox* Row, const FString& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(FText::FromString(Label));
	Text->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 15));
	Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f, 1.f)));
	Button->AddChild(Text);
	Row->AddChild(Button);
	return Button;
}

UGT_DebugSubsystem* UGT_GameHudWidget::GetDebugSubsystem() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UGT_DebugSubsystem>() : nullptr;
}

const UGT_RunContext* UGT_GameHudWidget::GetRunContext() const
{
	const UGT_RunSubsystem* RunSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGT_RunSubsystem>() : nullptr;
	return RunSubsystem ? RunSubsystem->GetCurrentRunContext() : nullptr;
}

void UGT_GameHudWidget::RefreshAll()
{
	RefreshPanels();
	// 房间视图重读状态(格子没变就不动人物位置, 避免点按钮把人吸回房中央)。
	// 顺手把键盘焦点还给房间视图(点按钮会抢走焦点, 不还回去 WASD 就失灵)。
	if (RoomView)
	{
		RoomView->SyncToCurrentCell(false);
		RoomView->SetFocus();
	}
}

void UGT_GameHudWidget::RefreshPanels()
{
	RefreshStatusLine();
	RefreshMiniMapGrid();
	RefreshItemsRow();

	FString RoomText;
	FString Hint;
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	if (Debug && Debug->GetDebugRoomText(RoomText) && RoomText.Split(TEXT("Hint="), nullptr, &Hint))
	{
		LogText->SetText(FText::FromString(Hint));
	}
	else
	{
		LogText->SetText(FText::GetEmpty());
	}

	// 当前格可搜索(普通房出金币/宝箱房出藏品, 各一次)才启用按钮, 判定问内核。
	if (SearchButton)
	{
		const UGT_RunContext* RunContext = GetRunContext();
		FName SearchBlockReason;
		bool bIsChest = false;
		SearchButton->SetIsEnabled(RunContext && RunContext->EvaluateSearchAtPlayer(SearchBlockReason, bIsChest));
	}
}

FReply UGT_GameHudWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 点小地图已探索的安全格 -> 瞬移(对齐 Lua MapOverlay.onTeleport + main.lua TeleportTo)。
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	if (MiniMapGrid && Debug)
	{
		const FGeometry GridGeometry = MiniMapGrid->GetCachedGeometry();
		const FVector2D LocalPos = GridGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		const FVector2D GridSize = GridGeometry.GetLocalSize();
		if (LocalPos.X >= 0.f && LocalPos.Y >= 0.f && LocalPos.X < GridSize.X && LocalPos.Y < GridSize.Y)
		{
			TArray<FGT_MiniMapCellViewData> Cells;
			int32 Width = 0;
			int32 Height = 0;
			if (Debug->GetDebugMiniMapViewData(Cells, Width, Height) && Width > 0 && Height > 0)
			{
				const int32 CellX = FMath::Clamp(static_cast<int32>(LocalPos.X / (GridSize.X / Width)), 0, Width - 1);
				const int32 CellY = FMath::Clamp(static_cast<int32>(LocalPos.Y / (GridSize.Y / Height)), 0, Height - 1);
				const FGT_MiniMapCellViewData& Cell = Cells[CellY * Width + CellX];

				// 仅已探索的安全格可传送("只能传送到已探索的安全房间")。
				if (Cell.bExplored && Cell.VisibleRoomIcon != FName(TEXT("Mine")))
				{
					FGT_DebugRunSnapshot Snapshot;
					Debug->DebugTeleport(CellX, CellY, Snapshot);
					RefreshAll();
				}
				return FReply::Handled();
			}
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UGT_GameHudWidget::RefreshStatusLine()
{
	const UGT_RunContext* RunContext = GetRunContext();
	if (!RunContext)
	{
		StatusText->SetText(FText::FromString(GetRunStateLabel(EGT_RunState::NotStarted)));
		return;
	}

	const FGT_PlayerCombatState& Combat = RunContext->GetPlayerCombatState();
	const FGT_RunInventoryState& Inventory = RunContext->GetRunInventory();
	StatusText->SetText(FText::FromString(FString::Printf(
		TEXT("HP %d/%d  战力 %d  待结算金币 %d  已锁定 %d  回收物 %d  [%s]"),
		Combat.Hp,
		Combat.MaxHp,
		Combat.Power,
		Inventory.PendingGold,
		Inventory.SafeGold,
		Inventory.GetCarriedItemCount(),
		*GetRunStateLabel(RunContext->GetRunState()))));
}

void UGT_GameHudWidget::RefreshMiniMapGrid()
{
	MiniMapGrid->ClearChildren();

	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	TArray<FGT_MiniMapCellViewData> Cells;
	int32 Width = 0;
	int32 Height = 0;
	if (!Debug || !Debug->GetDebugMiniMapViewData(Cells, Width, Height) || Width <= 0)
	{
		return;
	}

	FGT_DebugRunSnapshot Snapshot;
	Debug->GetDebugRunSnapshot(Snapshot);

	// 扫雷数字配色(对齐 MapOverlay.NUMBER_COLORS 风格)。
	static const FLinearColor NumberColors[4] = {
		FLinearColor(0.55f, 0.55f, 0.60f, 1.f),   // 0: 暗灰
		FLinearColor(0.38f, 0.63f, 1.00f, 1.f),   // 1: 蓝
		FLinearColor(0.38f, 0.86f, 0.50f, 1.f),   // 2: 绿
		FLinearColor(1.00f, 0.38f, 0.38f, 1.f),   // 3: 红
	};

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const FGT_MiniMapCellViewData& Cell = Cells[Y * Width + X];
			const bool bPlayerHere = X == Snapshot.PlayerX && Y == Snapshot.PlayerY;
			const bool bKnown = Cell.bExplored || Cell.bVisible;
			const FString Icon = Cell.VisibleRoomIcon.ToString();
			const bool bMineCell = bKnown && Icon == TEXT("Mine");

			USizeBox* CellSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			CellSize->SetWidthOverride(36.f);
			CellSize->SetHeightOverride(36.f);

			// 底色对齐 MapOverlay: 隐藏(70,75,95) / 雷(220,40,40) / 已知(40,45,60)。
			UBorder* CellBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			FLinearColor BgColor = FLinearColor(FColor(40, 45, 60));
			if (!bKnown)
			{
				BgColor = FLinearColor(FColor(70, 75, 95));
			}
			else if (bMineCell)
			{
				BgColor = FLinearColor(FColor(220, 40, 40));
			}
			CellBorder->SetBrushColor(BgColor);
			CellBorder->SetPadding(FMargin(1.f));
			CellSize->SetContent(CellBorder);

			UOverlay* CellOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
			CellBorder->SetContent(CellOverlay);

			auto AddCellIcon = [this, CellOverlay](UTexture2D* Texture, float Scale)
			{
				if (!Texture)
				{
					return;
				}
				UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				IconImage->SetBrushFromTexture(Texture);
				IconImage->SetDesiredSizeOverride(FVector2D(34.f * Scale, 34.f * Scale));
				if (UOverlaySlot* IconSlot = CellOverlay->AddChildToOverlay(IconImage))
				{
					IconSlot->SetHorizontalAlignment(HAlign_Center);
					IconSlot->SetVerticalAlignment(VAlign_Center);
				}
			};

			if (!bKnown)
			{
				// 未知格: ? 砖块贴图。
				AddCellIcon(LoadUiTexture(TEXT("Textures/generated/icons/64/01_weizhi_ge.png")), 0.95f);
			}
			else
			{
				// 房型图标(对齐 drawRoomIcon: 宝箱07/怪物06/事件03/撤离08/地刺05)。
				const TCHAR* IconPath = nullptr;
				if (Icon == TEXT("Chest")) { IconPath = TEXT("Textures/generated/icons/64/07_baoxiang_icon.png"); }
				else if (Icon == TEXT("Combat")) { IconPath = TEXT("Textures/generated/icons/64/06_guaiwu_icon.png"); }
				else if (Icon == TEXT("Event")) { IconPath = TEXT("Textures/generated/icons/64/03_saomiao_ge.png"); }
				else if (Icon == TEXT("Exit")) { IconPath = TEXT("Textures/generated/icons/64/08_cheli_icon.png"); }
				else if (Icon == TEXT("Mine")) { IconPath = TEXT("Textures/generated/icons/64/05_dici_xianjing_icon.png"); }
				const bool bSpecialIcon = IconPath != nullptr;
				if (bSpecialIcon)
				{
					AddCellIcon(LoadUiTexture(IconPath), 0.85f);
				}

				if (Cell.bScanned && !bMineCell)
				{
					if (bSpecialIcon && Cell.DisplayedNumber > 0)
					{
						// 特殊格: 右下角数字角标(对齐 drawNumberBadge)。
						UBorder* Badge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
						Badge->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.06f, 0.88f));
						Badge->SetPadding(FMargin(3.f, 0.f));
						UTextBlock* BadgeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
						BadgeText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 9));
						BadgeText->SetText(FText::FromString(FString::FromInt(Cell.DisplayedNumber)));
						BadgeText->SetColorAndOpacity(FSlateColor(NumberColors[FMath::Clamp(Cell.DisplayedNumber, 0, 3)]));
						Badge->SetContent(BadgeText);
						if (UOverlaySlot* BadgeSlot = CellOverlay->AddChildToOverlay(Badge))
						{
							BadgeSlot->SetHorizontalAlignment(HAlign_Right);
							BadgeSlot->SetVerticalAlignment(VAlign_Bottom);
						}
					}
					else if (!bSpecialIcon && Cell.DisplayedNumber >= 1 && Cell.DisplayedNumber <= 3)
					{
						// 普通格 1-3: 专用数字贴图(11/12/13_shuzi_N)。
						AddCellIcon(LoadUiTexture(FString::Printf(TEXT("Textures/generated/icons/64/1%d_shuzi_%d.png"), Cell.DisplayedNumber, Cell.DisplayedNumber)), 0.8f);
					}
					else if (!bSpecialIcon)
					{
						// 0 与 4+: 文本数字(0 也显示, 用户要求)。
						UTextBlock* NumberText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
						NumberText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 15));
						NumberText->SetText(FText::FromString(FString::FromInt(Cell.DisplayedNumber)));
						NumberText->SetColorAndOpacity(FSlateColor(NumberColors[FMath::Clamp(Cell.DisplayedNumber, 0, 3)]));
						if (UOverlaySlot* NumberSlot = CellOverlay->AddChildToOverlay(NumberText))
						{
							NumberSlot->SetHorizontalAlignment(HAlign_Center);
							NumberSlot->SetVerticalAlignment(VAlign_Center);
						}
					}
				}
			}

			// 可传送小蓝点(左下), 对齐 MapOverlay 已探索标记。
			if (Cell.bExplored && !bMineCell && !bPlayerHere)
			{
				USizeBox* DotSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				DotSize->SetWidthOverride(5.f);
				DotSize->SetHeightOverride(5.f);
				UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				Dot->SetBrushColor(FLinearColor(0.39f, 0.78f, 1.f, 0.8f));
				DotSize->SetContent(Dot);
				if (UOverlaySlot* DotSlot = CellOverlay->AddChildToOverlay(DotSize))
				{
					DotSlot->SetHorizontalAlignment(HAlign_Left);
					DotSlot->SetVerticalAlignment(VAlign_Bottom);
				}
			}

			// 玩家头像盖最上层。
			if (bPlayerHere)
			{
				AddCellIcon(LoadUiTexture(TEXT("Textures/generated/icons/64/00_wanjia_dingwei.png")), 0.9f);
			}

			MiniMapGrid->AddChildToUniformGrid(CellSize, Y, X);
		}
	}
}

UTexture2D* UGT_GameHudWidget::LoadUiTexture(const FString& RelativePathUnderAssets)
{
	if (UTexture2D** Cached = UiTextureCache.Find(RelativePathUnderAssets))
	{
		return *Cached;
	}

	const FString FullPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT(".."), TEXT("assets"), RelativePathUnderAssets));
	UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(FullPath);
	UiTextureCache.Add(RelativePathUnderAssets, Texture);
	return Texture;
}

void UGT_GameHudWidget::RefreshItemsRow()
{
	ItemsRow->ClearChildren();

	const UGT_RunContext* RunContext = GetRunContext();
	if (!RunContext)
	{
		return;
	}

	for (const FGT_ItemStack& Stack : RunContext->GetRunInventory().CarriedItems)
	{
		const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(Stack.ItemId);

		if (UTexture2D* Icon = GetItemIcon(Stack.ItemId))
		{
			USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			IconSize->SetWidthOverride(40.f);
			IconSize->SetHeightOverride(40.f);
			UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			IconImage->SetBrushFromTexture(Icon);
			IconSize->SetContent(IconImage);
			ItemsRow->AddChild(IconSize);
		}

		UTextBlock* ItemText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ItemText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 14));
		ItemText->SetText(FText::FromString(FString::Printf(TEXT("%s x%d  "),
			Def ? *Def->DisplayName : *Stack.ItemId.ToString(),
			Stack.Count)));
		ItemsRow->AddChild(ItemText);
	}
}

UTexture2D* UGT_GameHudWidget::GetItemIcon(FName ItemId)
{
	if (UTexture2D** Cached = IconCache.Find(ItemId))
	{
		return *Cached;
	}

	// 复用 Lua 版美术: 按物品大类映射到 Game/assets 下的通用图标(运行时加载, 不走资产导入)。
	const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(ItemId);
	const TCHAR* RelativePath = Def && Def->Kind == EGT_ItemKind::Consumable
		? TEXT("assets/item_consumable/item_consumable_medkit.png")
		: TEXT("assets/item_recovered/item_recovered_ore.png");
	const FString FullPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT(".."), RelativePath));

	UTexture2D* Icon = FImageUtils::ImportFileAsTexture2D(FullPath);
	IconCache.Add(ItemId, Icon);
	return Icon;
}

void UGT_GameHudWidget::OnSearch()
{
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	FGT_DebugRunSnapshot Snapshot;
	if (Debug)
	{
		Debug->DebugSearch(Snapshot);
	}
	RefreshAll();
}

void UGT_GameHudWidget::OnAttack()
{
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	FGT_DebugRunSnapshot Snapshot;
	if (Debug)
	{
		Debug->DebugAttack(Snapshot);
	}
	RefreshAll();
}

void UGT_GameHudWidget::OnExtract()
{
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	FGT_DebugRunSnapshot Snapshot;
	if (Debug)
	{
		Debug->DebugExtract(Snapshot);
	}
	RefreshAll();
}

void UGT_GameHudWidget::OnNewRun()
{
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	FGT_DebugRunSnapshot Snapshot;
	if (Debug)
	{
		Debug->DebugStartStandardRun(12345, EGT_Difficulty::Standard, Snapshot);
	}
	RefreshAll();

	// 焦点交给房间视图, WASD 立即可用。
	if (RoomView)
	{
		RoomView->SetFocus();
	}
}
