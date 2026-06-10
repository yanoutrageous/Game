#include "UI/GT_GameHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
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

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Background->SetContent(Root);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 18));
	Root->AddChildToVerticalBox(StatusText);

	MiniMapGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	MiniMapGrid->SetSlotPadding(FMargin(1.f));
	Root->AddChildToVerticalBox(MiniMapGrid);

	ItemsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Root->AddChildToVerticalBox(ItemsRow);

	UHorizontalBox* MoveRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Root->AddChildToVerticalBox(MoveRow);
	MakeButton(MoveRow, TEXT("Up"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnMoveUp);
	MakeButton(MoveRow, TEXT("Down"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnMoveDown);
	MakeButton(MoveRow, TEXT("Left"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnMoveLeft);
	MakeButton(MoveRow, TEXT("Right"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnMoveRight);

	UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Root->AddChildToVerticalBox(ActionRow);
	MakeButton(ActionRow, TEXT("Search"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnSearch);
	MakeButton(ActionRow, TEXT("Attack"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnAttack);
	MakeButton(ActionRow, TEXT("Extract"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnExtract);
	MakeButton(ActionRow, TEXT("NewRun"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnNewRun);

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

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const FGT_MiniMapCellViewData& Cell = Cells[Y * Width + X];
			const bool bPlayerHere = X == Snapshot.PlayerX && Y == Snapshot.PlayerY;
			const bool bKnown = Cell.bExplored || Cell.bVisible;

			// 配色: 玩家蓝 > 已知房型色 > 已探索灰 > 未知深灰。
			FLinearColor CellColor = GTCellColor_Unknown;
			FString CellLabel;
			if (bKnown)
			{
				CellColor = GTCellColor_Explored;
				const FString Icon = Cell.VisibleRoomIcon.ToString();
				if (Icon == TEXT("Mine")) { CellColor = GTCellColor_Mine; }
				else if (Icon == TEXT("Exit")) { CellColor = GTCellColor_Exit; CellLabel = TEXT("出"); }
				else if (Icon == TEXT("Combat")) { CellColor = GTCellColor_Combat; CellLabel = TEXT("怪"); }
				else if (Icon == TEXT("Event")) { CellColor = GTCellColor_Event; CellLabel = TEXT("事"); }
				else if (Icon == TEXT("Chest")) { CellColor = GTCellColor_Chest; CellLabel = TEXT("宝"); }
				else if (Cell.bScanned)
				{
					// 扫雷数字: 0 不显示数字(经典扫雷的留白)。
					CellLabel = Cell.DisplayedNumber > 0 ? FString::FromInt(Cell.DisplayedNumber) : TEXT("");
				}
			}
			if (bPlayerHere)
			{
				CellColor = GTCellColor_Player;
				CellLabel = TEXT("@");
			}

			USizeBox* CellSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			CellSize->SetWidthOverride(34.f);
			CellSize->SetHeightOverride(34.f);

			UBorder* CellBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			CellBorder->SetBrushColor(CellColor);
			CellBorder->SetHorizontalAlignment(HAlign_Center);
			CellBorder->SetVerticalAlignment(VAlign_Center);
			CellSize->SetContent(CellBorder);

			UTextBlock* CellText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			CellText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 14));
			CellText->SetText(FText::FromString(CellLabel));
			CellBorder->SetContent(CellText);

			MiniMapGrid->AddChildToUniformGrid(CellSize, Y, X);
		}
	}
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

void UGT_GameHudWidget::TryMove(int32 DeltaX, int32 DeltaY)
{
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	FGT_DebugRunSnapshot Snapshot;
	if (Debug && Debug->GetDebugRunSnapshot(Snapshot))
	{
		Debug->DebugMoveTo(Snapshot.PlayerX + DeltaX, Snapshot.PlayerY + DeltaY, Snapshot);
	}
	RefreshAll();
}

void UGT_GameHudWidget::OnMoveUp() { TryMove(0, -1); }
void UGT_GameHudWidget::OnMoveDown() { TryMove(0, 1); }
void UGT_GameHudWidget::OnMoveLeft() { TryMove(-1, 0); }
void UGT_GameHudWidget::OnMoveRight() { TryMove(1, 0); }

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
}
