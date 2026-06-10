#include "UI/GT_GameHudWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/GT_RunContext.h"
#include "Core/GT_RunSubsystem.h"
#include "Debug/GT_DebugSubsystem.h"
#include "Domains/Inventory/GT_ItemCatalog.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Events.h"
#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"
#include "UI/GT_RoomViewWidget.h"

namespace
{
	constexpr float GTMiniMapCellSize = 28.f;

	FString GetRunStateLabel(EGT_RunState RunState)
	{
		switch (RunState)
		{
		case EGT_RunState::Running:
			return TEXT("正常作业");
		case EGT_RunState::Failed:
			return TEXT("信号中断 - NewRun 重开");
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
	// 全屏 Overlay: 房间铺底, 各面板悬浮其上(对齐 Lua 原版构图)。3D 世界被完全盖住。
	UOverlay* Screen = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Screen;

	// 第 0 层: 不透明深色底。
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Backdrop->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.025f, 1.f));
	if (UOverlaySlot* BackdropSlot = Screen->AddChildToOverlay(Backdrop))
	{
		BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
		BackdropSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// 第 1 层: 房间视图居中自适应放大(内部逻辑坐标固定 560, ScaleBox 只缩放显示)。
	RoomView = CreateWidget<UGT_RoomViewWidget>(this, UGT_RoomViewWidget::StaticClass());
	if (RoomView)
	{
		RoomView->OnRoomChanged.BindUObject(this, &UGT_GameHudWidget::RefreshPanels);
		RoomView->OnSearchRequested.BindUObject(this, &UGT_GameHudWidget::OnSearch);

		UScaleBox* RoomScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		RoomScale->SetStretch(EStretch::ScaleToFit);
		USizeBox* RoomSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RoomSize->SetWidthOverride(560.f);
		RoomSize->SetHeightOverride(560.f);
		RoomSize->AddChild(RoomView);
		RoomScale->AddChild(RoomSize);
		if (UOverlaySlot* RoomSlot = Screen->AddChildToOverlay(RoomScale))
		{
			RoomSlot->SetHorizontalAlignment(HAlign_Fill);
			RoomSlot->SetVerticalAlignment(VAlign_Fill);
			RoomSlot->SetPadding(FMargin(360.f, 16.f, 16.f, 40.f));
		}
	}

	// 第 2 层: 左侧信息面板(对齐原版: 区域扫描图/生命/属性/作业包摘要)。
	UBorder* LeftPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	LeftPanel->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.05f, 0.92f));
	LeftPanel->SetPadding(FMargin(14.f));
	if (UOverlaySlot* LeftSlot = Screen->AddChildToOverlay(LeftPanel))
	{
		LeftSlot->SetHorizontalAlignment(HAlign_Left);
		LeftSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	LeftPanel->SetContent(Panel);

	UTextBlock* MapTitle = MakePanelText(Panel, 15, FLinearColor(0.85f, 0.88f, 0.95f, 1.f));
	MapTitle->SetText(FText::FromString(TEXT("区域扫描图")));

	MiniMapGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	MiniMapGrid->SetSlotPadding(FMargin(1.f));
	Panel->AddChildToVerticalBox(MiniMapGrid);

	UTextBlock* Legend = MakePanelText(Panel, 10, FLinearColor(0.55f, 0.60f, 0.70f, 1.f));
	Legend->SetText(FText::FromString(TEXT("数字 = 周围8格雷险 · 蓝点 = 可点击回传")));

	// 生命条(原版红条)。
	UTextBlock* HpTitle = MakePanelText(Panel, 13, FLinearColor(0.9f, 0.45f, 0.40f, 1.f));
	HpTitle->SetText(FText::FromString(TEXT("生命")));
	HpBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	HpBar->SetFillColorAndOpacity(FLinearColor(0.78f, 0.16f, 0.14f, 1.f));
	if (UVerticalBoxSlot* HpSlot = Panel->AddChildToVerticalBox(HpBar))
	{
		HpSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 2.f));
	}
	HpText = MakePanelText(Panel, 12, FLinearColor(0.9f, 0.9f, 0.9f, 1.f));

	StatsText = MakePanelText(Panel, 13, FLinearColor(0.85f, 0.85f, 0.9f, 1.f));
	StateText = MakePanelText(Panel, 13, FLinearColor(0.95f, 0.85f, 0.5f, 1.f));

	UTextBlock* BagTitle = MakePanelText(Panel, 13, FLinearColor(0.65f, 0.75f, 0.95f, 1.f));
	BagTitle->SetText(FText::FromString(TEXT("作业包摘要")));

	ItemsList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->AddChildToVerticalBox(ItemsList);

	// 动作按钮(Search 由可搜索性启用)。
	UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* ActionSlot = Panel->AddChildToVerticalBox(ActionRow))
	{
		ActionSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 2.f));
	}
	SearchButton = MakeButton(ActionRow, TEXT("Search"));
	SearchButton->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnSearch);
	MakeButton(ActionRow, TEXT("Attack"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnAttack);
	MakeButton(ActionRow, TEXT("Extract"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnExtract);
	MakeButton(ActionRow, TEXT("NewRun"))->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnNewRun);

	LogText = MakePanelText(Panel, 11, FLinearColor(0.6f, 0.65f, 0.72f, 1.f));
	LogText->SetAutoWrapText(true);

	// 第 3 层: 右上协议面板(占位, 数值待协议系统迁入)。
	UBorder* ProtocolPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ProtocolPanel->SetBrushColor(FLinearColor(0.05f, 0.03f, 0.03f, 0.9f));
	ProtocolPanel->SetPadding(FMargin(12.f, 8.f));
	ProtocolText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ProtocolText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 16));
	ProtocolText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.55f, 0.45f, 1.f)));
	ProtocolText->SetText(FText::FromString(TEXT("协议 5")));
	ProtocolPanel->SetContent(ProtocolText);
	if (UOverlaySlot* ProtocolSlot = Screen->AddChildToOverlay(ProtocolPanel))
	{
		ProtocolSlot->SetHorizontalAlignment(HAlign_Right);
		ProtocolSlot->SetVerticalAlignment(VAlign_Top);
		ProtocolSlot->SetPadding(FMargin(0.f, 10.f, 10.f, 0.f));
	}

	// 第 4 层: 底部快捷键栏。
	UBorder* Hotbar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Hotbar->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.05f, 0.85f));
	Hotbar->SetPadding(FMargin(16.f, 4.f));
	UTextBlock* HotbarText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	HotbarText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 13));
	HotbarText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.78f, 0.85f, 1.f)));
	HotbarText->SetText(FText::FromString(TEXT("WASD 移动  |  F 搜索/开箱  |  小地图点已探索格回传")));
	Hotbar->SetContent(HotbarText);
	if (UOverlaySlot* HotbarSlot = Screen->AddChildToOverlay(Hotbar))
	{
		HotbarSlot->SetHorizontalAlignment(HAlign_Center);
		HotbarSlot->SetVerticalAlignment(VAlign_Bottom);
		HotbarSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}
}

UTextBlock* UGT_GameHudWidget::MakePanelText(UVerticalBox* Panel, int32 FontSize, const FLinearColor& Color)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", FontSize));
	Text->SetColorAndOpacity(FSlateColor(Color));
	if (UVerticalBoxSlot* TextSlot = Panel->AddChildToVerticalBox(Text))
	{
		TextSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	}
	return Text;
}

UButton* UGT_GameHudWidget::MakeButton(UHorizontalBox* Row, const FString& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(FText::FromString(Label));
	Text->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 12));
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
	// 房间视图重读状态(格子没变就不动人物位置), 并把键盘焦点还给它。
	if (RoomView)
	{
		RoomView->SyncToCurrentCell(false);
		RoomView->SetFocus();
	}
}

void UGT_GameHudWidget::RefreshPanels()
{
	RefreshStatusPanel();
	RefreshMiniMapGrid();
	RefreshItemsList();

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

	// 右上协议面板(对齐原版: 协议等级 + 描述 + 压力值)。
	if (ProtocolText)
	{
		if (const UGT_RunContext* RunContext = GetRunContext())
		{
			const FGT_ProtocolState& Protocol = RunContext->GetProtocolState();
			ProtocolText->SetText(FText::FromString(FString::Printf(
				TEXT("协议 %d  %s\n协议压力 %d / %d"),
				Protocol.Level,
				*GT_ProtocolRules::GetLevelDescription(Protocol.Level),
				Protocol.Pressure,
				Protocol.MaxPressure)));
		}
		else
		{
			ProtocolText->SetText(FText::FromString(TEXT("协议 5")));
		}
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

void UGT_GameHudWidget::RefreshStatusPanel()
{
	const UGT_RunContext* RunContext = GetRunContext();
	if (!RunContext)
	{
		HpBar->SetPercent(0.f);
		HpText->SetText(FText::GetEmpty());
		StatsText->SetText(FText::GetEmpty());
		StateText->SetText(FText::FromString(GetRunStateLabel(EGT_RunState::NotStarted)));
		return;
	}

	const FGT_PlayerCombatState& Combat = RunContext->GetPlayerCombatState();
	const FGT_RunInventoryState& Inventory = RunContext->GetRunInventory();

	HpBar->SetPercent(Combat.MaxHp > 0 ? static_cast<float>(Combat.Hp) / Combat.MaxHp : 0.f);
	HpText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Combat.Hp, Combat.MaxHp)));
	StatsText->SetText(FText::FromString(FString::Printf(
		TEXT("战斗力: %d\n待结算: %d\n已锁定: %d\n回收物: %d 件\n已搜索: %d 格"),
		Combat.Power,
		Inventory.PendingGold,
		Inventory.SafeGold,
		Inventory.GetCarriedItemCount(),
		Inventory.SearchedRooms.Num())));
	StateText->SetText(FText::FromString(FString::Printf(TEXT("[%s]"), *GetRunStateLabel(RunContext->GetRunState()))));
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
			CellSize->SetWidthOverride(GTMiniMapCellSize);
			CellSize->SetHeightOverride(GTMiniMapCellSize);

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
				IconImage->SetDesiredSizeOverride(FVector2D((GTMiniMapCellSize - 2.f) * Scale, (GTMiniMapCellSize - 2.f) * Scale));
				if (UOverlaySlot* IconSlot = CellOverlay->AddChildToOverlay(IconImage))
				{
					IconSlot->SetHorizontalAlignment(HAlign_Center);
					IconSlot->SetVerticalAlignment(VAlign_Center);
				}
			};

			if (!bKnown)
			{
				// 未知格: ? 砖块贴图。
				AddCellIcon(LoadUiTexture(TEXT("/Game/Graytail/UI/Icons64/01_weizhi_ge")), 0.95f);
			}
			else
			{
				// 房型图标(对齐 drawRoomIcon: 宝箱07/怪物06/事件03/撤离08/地刺05)。
				const TCHAR* IconPath = nullptr;
				if (Icon == TEXT("Chest")) { IconPath = TEXT("/Game/Graytail/UI/Icons64/07_baoxiang_icon"); }
				else if (Icon == TEXT("Combat")) { IconPath = TEXT("/Game/Graytail/UI/Icons64/06_guaiwu_icon"); }
				else if (Icon == TEXT("Event")) { IconPath = TEXT("/Game/Graytail/UI/Icons64/03_saomiao_ge"); }
				else if (Icon == TEXT("Exit")) { IconPath = TEXT("/Game/Graytail/UI/Icons64/08_cheli_icon"); }
				else if (Icon == TEXT("Mine")) { IconPath = TEXT("/Game/Graytail/UI/Icons64/05_dici_xianjing_icon"); }
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
						Badge->SetPadding(FMargin(2.f, 0.f));
						UTextBlock* BadgeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
						BadgeText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 8));
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
						AddCellIcon(LoadUiTexture(FString::Printf(TEXT("/Game/Graytail/UI/Icons64/1%d_shuzi_%d"), Cell.DisplayedNumber, Cell.DisplayedNumber)), 0.8f);
					}
					else if (!bSpecialIcon)
					{
						// 0 与 4+: 文本数字(0 也显示, 用户要求)。
						UTextBlock* NumberText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
						NumberText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 13));
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
				DotSize->SetWidthOverride(4.f);
				DotSize->SetHeightOverride(4.f);
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
				AddCellIcon(LoadUiTexture(TEXT("/Game/Graytail/UI/Icons64/00_wanjia_dingwei")), 0.9f);
			}

			MiniMapGrid->AddChildToUniformGrid(CellSize, Y, X);
		}
	}
}

void UGT_GameHudWidget::RefreshItemsList()
{
	ItemsList->ClearChildren();

	const UGT_RunContext* RunContext = GetRunContext();
	if (!RunContext)
	{
		return;
	}

	for (const FGT_ItemStack& Stack : RunContext->GetRunInventory().CarriedItems)
	{
		const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(Stack.ItemId);

		UHorizontalBox* ItemRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		ItemsList->AddChildToVerticalBox(ItemRow);

		if (UTexture2D* Icon = GetItemIcon(Stack.ItemId))
		{
			USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			IconSize->SetWidthOverride(20.f);
			IconSize->SetHeightOverride(20.f);
			UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			IconImage->SetBrushFromTexture(Icon);
			IconSize->SetContent(IconImage);
			ItemRow->AddChild(IconSize);
		}

		UTextBlock* ItemText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ItemText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 12));
		ItemText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.9f, 1.f)));
		ItemText->SetText(FText::FromString(FString::Printf(TEXT(" %s x%d"),
			Def ? *Def->DisplayName : *Stack.ItemId.ToString(),
			Stack.Count)));
		ItemRow->AddChild(ItemText);
	}
}

UTexture2D* UGT_GameHudWidget::GetItemIcon(FName ItemId)
{
	// 复用 Lua 版美术: 按物品大类映射到通用图标资产(缓存在 LoadUiTexture 里)。
	const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(ItemId);
	return LoadUiTexture(Def && Def->Kind == EGT_ItemKind::Consumable
		? TEXT("/Game/Graytail/Items/Consumable/item_consumable_medkit")
		: TEXT("/Game/Graytail/Items/Recovered/item_recovered_ore"));
}

UTexture2D* UGT_GameHudWidget::LoadUiTexture(const FString& AssetPath)
{
	if (UTexture2D** Cached = UiTextureCache.Find(AssetPath))
	{
		return *Cached;
	}

	// 包路径只到包名, 对象与包同名(import_png_assets.py 的导入约定)。
	const FString ObjectPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("GT_GameHudWidget: missing texture asset %s"), *AssetPath);
	}
	UiTextureCache.Add(AssetPath, Texture);
	return Texture;
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
		// 每局随机种子(对齐原版); 局内随机仍由该种子完全确定, 复现指定地图用 gt.StartStd。
		Debug->DebugStartStandardRun(FMath::RandRange(1, MAX_int32 - 1), EGT_Difficulty::Standard, Snapshot);
	}
	RefreshAll();

	// 焦点交给房间视图, WASD 立即可用。
	if (RoomView)
	{
		RoomView->SetFocus();
	}
}
