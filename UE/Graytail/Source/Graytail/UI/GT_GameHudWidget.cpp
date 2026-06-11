#include "UI/GT_GameHudWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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
#include "UI/GT_LootResultWidget.h"
#include "UI/GT_MapOverlayWidget.h"
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

	// NewRun 按钮已移除: HUD 打开时若还没有局, 直接自动开一局。
	const UGT_RunContext* RunContext = GetRunContext();
	if (!RunContext || RunContext->GetRunState() == EGT_RunState::NotStarted)
	{
		OnNewRun();
	}
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
		RoomView->OnMapRequested.BindUObject(this, &UGT_GameHudWidget::OpenMapOverlay);
		RoomView->OnExtractRequested.BindUObject(this, &UGT_GameHudWidget::OnExtract);

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
	if (UOverlaySlot* LeftSlot = Screen->AddChildToOverlay(MakeSkinnedPanel(LeftPanel, TEXT("/Game/Graytail/UI/hud/ui_panel_left"))))
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

	// 属性行分色(对齐原版面板配色)。
	PowerText = MakePanelText(Panel, 13, FLinearColor(0.92f, 0.93f, 0.95f, 1.f));
	PendingText = MakePanelText(Panel, 13, FLinearColor(FColor(255, 226, 120)));
	SafeText = MakePanelText(Panel, 13, FLinearColor(FColor(120, 220, 170)));
	PartsText = MakePanelText(Panel, 13, FLinearColor(FColor(130, 200, 255)));
	SearchedText = MakePanelText(Panel, 11, FLinearColor(0.55f, 0.60f, 0.70f, 1.f));
	StateText = MakePanelText(Panel, 13, FLinearColor(0.95f, 0.85f, 0.5f, 1.f));

	UTextBlock* BagTitle = MakePanelText(Panel, 13, FLinearColor(0.65f, 0.75f, 0.95f, 1.f));
	BagTitle->SetText(FText::FromString(TEXT("作业包摘要")));

	ItemsList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->AddChildToVerticalBox(ItemsList);

	// 动作按钮已移除: F=搜索/攻击, E=撤离, 重开走局终弹窗。
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
	if (UOverlaySlot* ProtocolSlot = Screen->AddChildToOverlay(MakeSkinnedPanel(ProtocolPanel, TEXT("/Game/Graytail/UI/hud/ui_panel_protocol"))))
	{
		ProtocolSlot->SetHorizontalAlignment(HAlign_Right);
		ProtocolSlot->SetVerticalAlignment(VAlign_Top);
		ProtocolSlot->SetPadding(FMargin(0.f, 10.f, 10.f, 0.f));
	}

	// 第 4 层: 底部快捷键栏。
	UBorder* Hotbar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Hotbar->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.05f, 0.85f));
	Hotbar->SetPadding(FMargin(16.f, 8.f));
	// 四个键位块, 等宽分布对齐底栏贴图的四个格子, 块内居中(对齐原版底栏)。
	USizeBox* HotSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	HotSize->SetWidthOverride(720.f);
	HotSize->SetHeightOverride(40.f);
	Hotbar->SetContent(HotSize);
	UHorizontalBox* HotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HotSize->SetContent(HotRow);
	auto AddKeyHint = [this, HotRow](const TCHAR* KeyAssetPath, const FString& KeyLabel, const FString& Action)
	{
		UHorizontalBox* Block = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* BlockSlot = HotRow->AddChildToHorizontalBox(Block))
		{
			BlockSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BlockSlot->SetHorizontalAlignment(HAlign_Center);
			BlockSlot->SetVerticalAlignment(VAlign_Center);
		}
		// 键帽: 有贴图用贴图(自带字母), 没有(WASD)画一个键帽样式的小框。
		if (UTexture2D* KeyTexture = KeyAssetPath ? LoadUiTexture(KeyAssetPath) : nullptr)
		{
			USizeBox* KeySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			KeySize->SetWidthOverride(24.f);
			KeySize->SetHeightOverride(24.f);
			UImage* KeyImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			KeyImage->SetBrushFromTexture(KeyTexture);
			KeySize->SetContent(KeyImage);
			if (UHorizontalBoxSlot* KeySlot = Block->AddChildToHorizontalBox(KeySize))
			{
				KeySlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
				KeySlot->SetVerticalAlignment(VAlign_Center);
			}
		}
		else
		{
			UBorder* KeyCap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			KeyCap->SetBrushColor(FLinearColor(0.13f, 0.12f, 0.10f, 1.f));
			KeyCap->SetPadding(FMargin(6.f, 3.f));
			UTextBlock* KeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			KeyText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 10));
			KeyText->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(214, 178, 92))));
			KeyText->SetText(FText::FromString(KeyLabel));
			KeyCap->SetContent(KeyText);
			if (UHorizontalBoxSlot* KeySlot = Block->AddChildToHorizontalBox(KeyCap))
			{
				KeySlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
				KeySlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		UTextBlock* ActionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ActionText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 13));
		ActionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.80f, 0.86f, 1.f)));
		ActionText->SetText(FText::FromString(Action));
		if (UHorizontalBoxSlot* ActionSlot = Block->AddChildToHorizontalBox(ActionText))
		{
			ActionSlot->SetVerticalAlignment(VAlign_Center);
		}
	};
	AddKeyHint(nullptr, TEXT("WASD"), TEXT("移动"));
	AddKeyHint(TEXT("/Game/Graytail/UI/keys/ui_key_m"), TEXT("M"), TEXT("扫描图"));
	AddKeyHint(TEXT("/Game/Graytail/UI/keys/ui_key_f"), TEXT("F"), TEXT("搜索/攻击"));
	AddKeyHint(TEXT("/Game/Graytail/UI/keys/ui_key_e"), TEXT("E"), TEXT("撤离"));
	if (UOverlaySlot* HotbarSlot = Screen->AddChildToOverlay(MakeSkinnedPanel(Hotbar, TEXT("/Game/Graytail/UI/hud/ui_bottom_bar"))))
	{
		HotbarSlot->SetHorizontalAlignment(HAlign_Center);
		HotbarSlot->SetVerticalAlignment(VAlign_Bottom);
		HotbarSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}

	// 第 5 层(最顶): 全屏区域扫描图, 默认收起。
	MapOverlay = CreateWidget<UGT_MapOverlayWidget>(this, UGT_MapOverlayWidget::StaticClass());
	if (MapOverlay)
	{
		MapOverlay->OnClosed.BindUObject(this, &UGT_GameHudWidget::HandleMapOverlayClosed);
		MapOverlay->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* MapSlot = Screen->AddChildToOverlay(MapOverlay))
		{
			MapSlot->SetHorizontalAlignment(HAlign_Fill);
			MapSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// 第 6 层(模态): 搜索/开箱结果弹窗, 默认收起。
	LootResult = CreateWidget<UGT_LootResultWidget>(this, UGT_LootResultWidget::StaticClass());
	if (LootResult)
	{
		LootResult->OnClosed.BindUObject(this, &UGT_GameHudWidget::HandleLootResultClosed);
		LootResult->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* LootSlot = Screen->AddChildToOverlay(LootResult))
		{
			LootSlot->SetHorizontalAlignment(HAlign_Fill);
			LootSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// 第 7 层(最顶): 局终结算弹窗(死亡"信号中断"/撤离成功), 默认收起。
	{
		UOverlay* EndRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UBorder* EndMask = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		EndMask->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.66f));
		if (UOverlaySlot* MaskSlot = EndRoot->AddChildToOverlay(EndMask))
		{
			MaskSlot->SetHorizontalAlignment(HAlign_Fill);
			MaskSlot->SetVerticalAlignment(VAlign_Fill);
		}

		RunEndFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		RunEndFrame->SetPadding(FMargin(2.f));
		if (UOverlaySlot* FrameSlot = EndRoot->AddChildToOverlay(RunEndFrame))
		{
			FrameSlot->SetHorizontalAlignment(HAlign_Center);
			FrameSlot->SetVerticalAlignment(VAlign_Center);
		}
		UBorder* EndBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		EndBg->SetBrushColor(FLinearColor(FColor(26, 16, 18, 244)));
		EndBg->SetPadding(FMargin(34.f, 24.f));
		RunEndFrame->SetContent(EndBg);

		USizeBox* EndWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		EndWidth->SetWidthOverride(380.f);
		EndBg->SetContent(EndWidth);
		UVerticalBox* EndColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		EndWidth->SetContent(EndColumn);

		RunEndTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		RunEndTitle->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 24));
		RunEndTitle->SetJustification(ETextJustify::Center);
		EndColumn->AddChildToVerticalBox(RunEndTitle);

		RunEndBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		RunEndBody->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 13));
		RunEndBody->SetColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.86f, 0.84f, 1.f)));
		RunEndBody->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* BodySlot = EndColumn->AddChildToVerticalBox(RunEndBody))
		{
			BodySlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		}

		UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UVerticalBoxSlot* ButtonSlot = EndColumn->AddChildToVerticalBox(ButtonRow))
		{
			ButtonSlot->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		}
		RunEndButton = MakeButton(ButtonRow, TEXT("重新出发"));
		RunEndButton->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnNewRun);

		RunEndRoot = EndRoot;
		RunEndRoot->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* EndSlot = Screen->AddChildToOverlay(EndRoot))
		{
			EndSlot->SetHorizontalAlignment(HAlign_Fill);
			EndSlot->SetVerticalAlignment(VAlign_Fill);
		}
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

	// 局终(死亡/撤离成功)弹结算面板, 每局只弹一次。
	RefreshRunEndPanel();
}

void UGT_GameHudWidget::RefreshRunEndPanel()
{
	const UGT_RunContext* RunContext = GetRunContext();
	if (!RunContext || !RunEndRoot)
	{
		return;
	}

	const EGT_RunState RunState = RunContext->GetRunState();
	if (RunState != EGT_RunState::Failed && RunState != EGT_RunState::Succeeded)
	{
		bRunEndShown = false;
		RunEndRoot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (bRunEndShown)
	{
		return;
	}
	bRunEndShown = true;

	const bool bSuccess = RunState == EGT_RunState::Succeeded;
	RunEndTitle->SetText(FText::FromString(bSuccess ? TEXT("撤离成功") : TEXT("信号中断")));
	RunEndTitle->SetColorAndOpacity(FSlateColor(bSuccess
		? FLinearColor(FColor(120, 230, 150))
		: FLinearColor(FColor(255, 90, 80))));
	RunEndFrame->SetBrushColor(bSuccess
		? FLinearColor(FColor(80, 170, 110, 220))
		: FLinearColor(FColor(180, 60, 55, 220)));

	const FName Reason = RunContext->GetRunEndReason();
	FString ReasonLine;
	if (bSuccess)
	{
		ReasonLine = TEXT("作业体已回收, 本次作业结算如下。");
	}
	else if (Reason == FName(TEXT("Mine")))
	{
		ReasonLine = TEXT("踩雷! 血量归零, 作业体信号丢失。");
	}
	else if (Reason == FName(TEXT("Protocol")))
	{
		ReasonLine = TEXT("协议压力满载, 调度台强制中断作业。");
	}
	else if (Reason == FName(TEXT("CombatDeath")))
	{
		ReasonLine = TEXT("作业体损毁于异常体交战。");
	}
	else
	{
		ReasonLine = TEXT("作业体信号丢失。");
	}

	const FGT_RunInventoryState& Inventory = RunContext->GetRunInventory();
	RunEndBody->SetText(FText::FromString(FString::Printf(
		TEXT("%s\n\n待结算: %d%s\n已锁定: %d\n回收物: %d 件%s\n已搜索: %d 格"),
		*ReasonLine,
		Inventory.PendingGold,
		bSuccess ? TEXT("") : TEXT(" (已遗失)"),
		Inventory.SafeGold,
		Inventory.GetCarriedItemCount(),
		bSuccess ? TEXT("") : TEXT(" (已遗失)"),
		Inventory.SearchedRooms.Num())));

	RunEndRoot->SetVisibility(ESlateVisibility::Visible);
	// 按钮拿焦点: 房间视图失焦后移动闸门关闭, 局终不再响应 WASD。
	if (RunEndButton)
	{
		RunEndButton->SetFocus();
	}
}

void UGT_GameHudWidget::RefreshStatusPanel()
{
	const UGT_RunContext* RunContext = GetRunContext();
	if (!RunContext)
	{
		HpBar->SetPercent(0.f);
		HpText->SetText(FText::GetEmpty());
		PowerText->SetText(FText::GetEmpty());
		PendingText->SetText(FText::GetEmpty());
		SafeText->SetText(FText::GetEmpty());
		PartsText->SetText(FText::GetEmpty());
		SearchedText->SetText(FText::GetEmpty());
		StateText->SetText(FText::FromString(GetRunStateLabel(EGT_RunState::NotStarted)));
		return;
	}

	const FGT_PlayerCombatState& Combat = RunContext->GetPlayerCombatState();
	const FGT_RunInventoryState& Inventory = RunContext->GetRunInventory();

	HpBar->SetPercent(Combat.MaxHp > 0 ? static_cast<float>(Combat.Hp) / Combat.MaxHp : 0.f);
	HpText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Combat.Hp, Combat.MaxHp)));
	PowerText->SetText(FText::FromString(FString::Printf(TEXT("战斗力: %d"), Combat.Power)));
	PendingText->SetText(FText::FromString(FString::Printf(TEXT("待结算: %d"), Inventory.PendingGold)));
	SafeText->SetText(FText::FromString(FString::Printf(TEXT("已锁定: %d"), Inventory.SafeGold)));
	PartsText->SetText(FText::FromString(FString::Printf(TEXT("回收物: %d 件"), Inventory.GetCarriedItemCount())));
	SearchedText->SetText(FText::FromString(FString::Printf(TEXT("已搜索: %d 格"), Inventory.SearchedRooms.Num())));
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

	const TArray<FGT_ItemStack>& CarriedItems = RunContext->GetRunInventory().CarriedItems;
	if (CarriedItems.Num() == 0)
	{
		// 空态占位, 面板不留白(对齐原版摘要区始终有内容)。
		UTextBlock* EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		EmptyText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 11));
		EmptyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.54f, 0.62f, 1.f)));
		EmptyText->SetText(FText::FromString(TEXT("(回收包为空, 搜索房间获取物资)")));
		ItemsList->AddChildToVerticalBox(EmptyText);
		return;
	}

	constexpr int32 MaxItemRows = 5;
	int32 ShownRows = 0;
	for (const FGT_ItemStack& Stack : CarriedItems)
	{
		if (ShownRows >= MaxItemRows)
		{
			UTextBlock* MoreText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			MoreText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 11));
			MoreText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.54f, 0.62f, 1.f)));
			MoreText->SetText(FText::FromString(FString::Printf(TEXT("  另有 %d 项"), CarriedItems.Num() - MaxItemRows)));
			ItemsList->AddChildToVerticalBox(MoreText);
			break;
		}
		++ShownRows;
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

UWidget* UGT_GameHudWidget::MakeSkinnedPanel(UBorder* Panel, const FString& AssetPath)
{
	UTexture2D* Texture = LoadUiTexture(AssetPath);
	if (!Texture)
	{
		// 资产缺失: 保留面板自身的纯色底(对齐 Lua drawPanel 的 fallback)。
		return Panel;
	}

	// 两层: 原版边框贴图(无垫底, 直接透出场景) -> 内容面板(自身透明)。
	UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());

	UImage* Skin = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Skin->SetBrushFromTexture(Texture);
	Skin->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.85f));
	if (UOverlaySlot* SkinSlot = Stack->AddChildToOverlay(Skin))
	{
		SkinSlot->SetHorizontalAlignment(HAlign_Fill);
		SkinSlot->SetVerticalAlignment(VAlign_Fill);
	}

	Panel->SetBrushColor(FLinearColor::Transparent);
	if (UOverlaySlot* PanelSlot = Stack->AddChildToOverlay(Panel))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Fill);
		PanelSlot->SetVerticalAlignment(VAlign_Fill);
	}
	return Stack;
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
	// 点左上角小地图 -> 放大成全屏区域扫描图(对齐原版; 插旗/回传都在大图里操作)。
	if (MiniMapGrid)
	{
		const FGeometry GridGeometry = MiniMapGrid->GetCachedGeometry();
		const FVector2D LocalPos = GridGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		const FVector2D GridSize = GridGeometry.GetLocalSize();
		if (LocalPos.X >= 0.f && LocalPos.Y >= 0.f && LocalPos.X < GridSize.X && LocalPos.Y < GridSize.Y)
		{
			OpenMapOverlay();
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UGT_GameHudWidget::OpenMapOverlay()
{
	if (MapOverlay)
	{
		MapOverlay->Open();
	}
}

void UGT_GameHudWidget::HandleMapOverlayClosed()
{
	// 回传可能已换房: 整体刷新(RefreshAll 会把焦点还给房间视图, WASD 立即可用)。
	RefreshAll();
}

FReply UGT_GameHudWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// 焦点在弹窗/按钮上时 WASD 事件冒泡到这里, 转发给房间视图记账,
	// 保证持键状态与物理按键一致(关弹窗后按住的键立刻续走, 不卡键)。
	const FKey Key = InKeyEvent.GetKey();
	if (RoomView && (Key == EKeys::W || Key == EKeys::A || Key == EKeys::S || Key == EKeys::D))
	{
		RoomView->SetHeldMovementKey(Key, true);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UGT_GameHudWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (RoomView && (Key == EKeys::W || Key == EKeys::A || Key == EKeys::S || Key == EKeys::D))
	{
		RoomView->SetHeldMovementKey(Key, false);
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

void UGT_GameHudWidget::OnSearch()
{
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	if (!Debug)
	{
		return;
	}

	// F = 搜索/攻击(对齐原版底栏): 战斗激活时 F 是攻击。
	FGT_DebugRunSnapshot Probe;
	if (Debug->GetDebugRunSnapshot(Probe) && Probe.bCombatActive)
	{
		FGT_DebugRunSnapshot AttackSnapshot;
		Debug->DebugAttack(AttackSnapshot);
		RefreshAll();
		return;
	}

	FGT_DebugRunSnapshot Snapshot;
	const bool bAccepted = Debug->DebugSearch(Snapshot);
	RefreshAll();

	// 搜索成功 -> 弹结果面板(对齐 Lua OpenLootResultPanel; 明细从内核缓存的结算读取)。
	if (bAccepted && LootResult)
	{
		if (const UGT_RunContext* RunContext = GetRunContext())
		{
			LootResult->Open(RunContext->GetLastSearchOutcome());
		}
	}
}

void UGT_GameHudWidget::HandleLootResultClosed()
{
	// 确认入包: 整体刷新(背包/小地图/宝箱开闭状态), 焦点还给房间视图。
	RefreshAll();

	// 关弹窗瞬间播放开箱金光+奖励飘字(对齐 Lua TriggerChestOpen;
	// 弹窗盖着房间时播放看不见, 故移到关闭时)。
	if (RoomView)
	{
		if (const UGT_RunContext* RunContext = GetRunContext())
		{
			const FGT_SearchReward& Reward = RunContext->GetLastSearchOutcome().Reward;
			RoomView->PlayChestRewardBurst(Reward.Gold, Reward.Parts);
		}
	}
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
	if (MapOverlay)
	{
		// 上一局的标雷旗随新局清空。
		MapOverlay->ResetFlags();
	}
	if (RunEndRoot)
	{
		bRunEndShown = false;
		RunEndRoot->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshAll();

	// 焦点交给房间视图, WASD 立即可用。
	if (RoomView)
	{
		RoomView->SetFocus();
	}
}
