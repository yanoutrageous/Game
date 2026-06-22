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
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/GT_RunContext.h"
#include "Core/GT_RunSubsystem.h"
#include "Debug/GT_DebugSubsystem.h"
#include "Domains/Events/GT_EventTypes.h"
#include "Domains/Inventory/GT_ItemCatalog.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "UI/GT_UIStyle.h"
#include "Input/Events.h"
#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"
#include "UI/GT_EventPanelWidget.h"
#include "UI/GT_LootResultWidget.h"
#include "UI/GT_DeployTerminalWidget.h"
#include "UI/GT_MainMenuWidget.h"
#include "UI/GT_MapOverlayWidget.h"
#include "UI/GT_PauseMenuWidget.h"
#include "UI/GT_SettingsWidget.h"
#include "UI/GT_RoomViewWidget.h"
#include "UI/GT_TutorialContent.h"
#include "UI/GT_TutorialPopupWidget.h"
#include "Kismet/KismetSystemLibrary.h"

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
			return TEXT("信号中断");
		case EGT_RunState::Succeeded:
			return TEXT("撤离成功!");
		default:
			return TEXT("未开局 - 主菜单选择难度");
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

	// 无局时先进主菜单选难度(替代原来的自动开局), 已有局(如 gt.StartStd 后再 gt.HUD)直接进游戏。
	const UGT_RunContext* RunContext = GetRunContext();
	if ((!RunContext || RunContext->GetRunState() == EGT_RunState::NotStarted) && MainMenu)
	{
		MainMenu->Open();
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

	// 第 1 层: 主布局行 = 左侧信息面板列 + 中央列(房间/雷险标牌/底部键位栏)。
	// 真实布局容器排版, 中央列内三者天然同轴居中, 不再用整屏悬浮+边距补偿对位。
	UHorizontalBox* MainRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UOverlaySlot* MainSlot = Screen->AddChildToOverlay(MainRow))
	{
		MainSlot->SetHorizontalAlignment(HAlign_Fill);
		MainSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// 左列: 信息面板(对齐原版: 区域扫描图/生命/属性/作业包摘要)。
	// 皮肤 9-slice 后边框厚度固定, 内边距在边框+角饰(~40px)基础上留呼吸空间。
	UBorder* LeftPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	LeftPanel->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.05f, 0.92f));
	LeftPanel->SetPadding(FMargin(30.f, 34.f, 30.f, 28.f));
	USizeBox* LeftWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LeftWidth->SetWidthOverride(360.f);
	LeftWidth->SetContent(MakeSkinnedPanel(LeftPanel, TEXT("/Game/Graytail/UI/hud/ui_panel_left"), FVector2D(684.f, 580.f), 40.f));
	if (UHorizontalBoxSlot* LeftSlot = MainRow->AddChildToHorizontalBox(LeftWidth))
	{
		LeftSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		LeftSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// 中央列: 房间在上(Fill), 雷险标牌/键位栏依次排在正下方。
	UVerticalBox* CenterCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UHorizontalBoxSlot* CenterSlot = MainRow->AddChildToHorizontalBox(CenterCol))
	{
		CenterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// 房间视图居中自适应放大(内部逻辑坐标固定 560, ScaleBox 只缩放显示)。
	RoomView = CreateWidget<UGT_RoomViewWidget>(this, UGT_RoomViewWidget::StaticClass());
	if (RoomView)
	{
		RoomView->OnRoomChanged.BindUObject(this, &UGT_GameHudWidget::HandleRoomChanged);
		RoomView->OnCombatStateChanged.BindUObject(this, &UGT_GameHudWidget::RefreshPanels);
		RoomView->OnSearchRequested.BindUObject(this, &UGT_GameHudWidget::OnSearch);
		RoomView->OnMapRequested.BindUObject(this, &UGT_GameHudWidget::OpenMapOverlay);
		RoomView->OnExtractRequested.BindUObject(this, &UGT_GameHudWidget::OnExtract);
		RoomView->OnEventRequested.BindUObject(this, &UGT_GameHudWidget::OpenEventPanel);
		RoomView->OnConsumableRequested.BindUObject(this, &UGT_GameHudWidget::OnUseConsumable);
		RoomView->OnItemSlotRequested.BindUObject(this, &UGT_GameHudWidget::SelectConsumableSlot);

		UScaleBox* RoomScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		RoomScale->SetStretch(EStretch::ScaleToFit);
		USizeBox* RoomSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RoomSize->SetWidthOverride(560.f);
		RoomSize->SetHeightOverride(560.f);
		RoomSize->AddChild(RoomView);
		RoomScale->AddChild(RoomSize);

		// 房间叠加层: 房间铺底 + 居中播报 toast 悬浮其上。
		// toast 挂这里(而非整屏)是为了落在地图区中央 —— 左侧 360px 信息面板会让整屏中心明显偏左于地图。
		UOverlay* RoomStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		if (UOverlaySlot* RoomScaleSlot = RoomStack->AddChildToOverlay(RoomScale))
		{
			RoomScaleSlot->SetHorizontalAlignment(HAlign_Fill);
			RoomScaleSlot->SetVerticalAlignment(VAlign_Fill);
		}
		{
			UBorder* ToastBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			FSlateBrush ToastBrush;
			ToastBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			ToastBrush.TintColor = FSlateColor(FLinearColor(FColor(18, 22, 32, 235)));
			ToastBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			ToastBrush.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
			ToastBrush.OutlineSettings.Color = FSlateColor(FLinearColor(FColor(120, 150, 200)));
			ToastBrush.OutlineSettings.Width = 1.f;
			ToastBg->SetBrush(ToastBrush);
			ToastBg->SetPadding(FMargin(22.f, 11.f));
			ToastText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			ToastText->SetFont(GT_UIStyle::Font(18));
			ToastText->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(255, 226, 150))));
			ToastText->SetJustification(ETextJustify::Center);
			ToastBg->SetContent(ToastText);
			ToastBg->SetVisibility(ESlateVisibility::Collapsed);
			ToastRoot = ToastBg;
			if (UOverlaySlot* ToastSlot = RoomStack->AddChildToOverlay(ToastBg))
			{
				ToastSlot->SetHorizontalAlignment(HAlign_Center);
				ToastSlot->SetVerticalAlignment(VAlign_Top);
				ToastSlot->SetPadding(FMargin(0.f, 28.f, 0.f, 0.f));
			}
		}
		if (UVerticalBoxSlot* RoomSlot = CenterCol->AddChildToVerticalBox(RoomStack))
		{
			RoomSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			RoomSlot->SetHorizontalAlignment(HAlign_Fill);
			RoomSlot->SetVerticalAlignment(VAlign_Fill);
			RoomSlot->SetPadding(FMargin(16.f, 12.f, 16.f, 4.f));
		}
	}

	// 雷险标牌: 房间正下方(从房间画布挪出, 不再遮挡地图/门)。
	{
		UBorder* RiskBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		if (UTexture2D* TagTexture = LoadUiTexture(TEXT("/Game/Graytail/UI/hud/ui_mine_risk_tag")))
		{
			FSlateBrush TagBrush;
			TagBrush.SetResourceObject(TagTexture);
			TagBrush.DrawAs = ESlateBrushDrawType::Image;
			RiskBorder->SetBrush(TagBrush);
		}
		// 左留空避开底图自带图标; 上下不对称内边距校正中文字形基线(按 PIE 截图逐像素校准)。
		RiskBorder->SetPadding(FMargin(59.f, 3.f, 12.f, 2.f));
		RiskBorder->SetVerticalAlignment(VAlign_Center);
		MineRiskText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		MineRiskText->SetFont(GT_UIStyle::Font(16));
		MineRiskText->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(232, 222, 198))));
		RiskBorder->SetContent(MineRiskText);

		USizeBox* RiskSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RiskSize->SetWidthOverride(220.f);
		RiskSize->SetHeightOverride(44.f);
		RiskSize->SetContent(RiskBorder);
		MineRiskRoot = RiskSize;
		MineRiskRoot->SetVisibility(ESlateVisibility::Hidden);
		if (UVerticalBoxSlot* RiskSlot = CenterCol->AddChildToVerticalBox(RiskSize))
		{
			RiskSlot->SetHorizontalAlignment(HAlign_Center);
			RiskSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		}
	}

	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	LeftPanel->SetContent(Panel);

	// 左面板背景图 ~68% 处有分隔线分上下两块: 上半大块放信息, 下半小块放道具面板。
	// 两个 Fill 子区(权重 2.1 : 1.0 ≈ 68% : 32%)切分; 上区内容自顶向下排, 道具面板钉在下区顶部(分隔线下沿, 即用户要的位置)。
	UVerticalBox* UpperBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* US = Panel->AddChildToVerticalBox(UpperBox)) { FSlateChildSize Sz(ESlateSizeRule::Fill); Sz.Value = 2.1f; US->SetSize(Sz); }
	UVerticalBox* LowerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* LS = Panel->AddChildToVerticalBox(LowerBox)) { FSlateChildSize Sz(ESlateSizeRule::Fill); Sz.Value = 1.0f; LS->SetSize(Sz); }

	UTextBlock* MapTitle = MakePanelText(UpperBox, 15, FLinearColor(0.85f, 0.88f, 0.95f, 1.f));
	MapTitle->SetText(FText::FromString(TEXT("区域扫描图")));

	MiniMapGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	MiniMapGrid->SetSlotPadding(FMargin(1.f));   // 格间细分隔线(随缩放放大)。
	// 等比放大填满面板宽(对齐原版): ScaleBox(ScaleToFit) 把方形网格整体缩放, 列宽行高同步、格子保持正方;
	// 外层 SizeBox 给一个 >= 内容宽的方形高度(300), 让 ScaleToFit 以宽度为准撑满到框边。
	// (不能直接让 UniformGridPanel 在 Fill 容器里拉伸 —— 那样只拉宽列、格子变扁、还出空列。)
	UScaleBox* MapScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	MapScale->SetStretch(EStretch::ScaleToFit);
	MapScale->SetContent(MiniMapGrid);
	USizeBox* MapArea = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MapArea->SetHeightOverride(300.f);
	MapArea->SetContent(MapScale);
	if (UVerticalBoxSlot* GridSlot = UpperBox->AddChildToVerticalBox(MapArea))
	{
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	UTextBlock* Legend = MakePanelText(UpperBox, 10, FLinearColor(0.55f, 0.60f, 0.70f, 1.f));
	Legend->SetText(FText::FromString(TEXT("数字 = 周围8格雷险 · 蓝点 = 可点击回传")));

	// 生命条(原版红条)。
	UTextBlock* HpTitle = MakePanelText(UpperBox, 13, FLinearColor(0.9f, 0.45f, 0.40f, 1.f));
	HpTitle->SetText(FText::FromString(TEXT("生命")));
	HpBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	HpBar->SetFillColorAndOpacity(FLinearColor(0.70f, 0.02f, 0.02f, 1.f));   // 鲜红(降 G/B 去粉调)
	if (UVerticalBoxSlot* HpSlot = UpperBox->AddChildToVerticalBox(HpBar))
	{
		HpSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 2.f));
	}
	HpText = MakePanelText(UpperBox, 12, FLinearColor(0.9f, 0.9f, 0.9f, 1.f));

	// 属性行分色(对齐原版面板配色)。
	PowerText = MakePanelText(UpperBox, 13, FLinearColor(0.92f, 0.93f, 0.95f, 1.f));
	PendingText = MakePanelText(UpperBox, 13, FLinearColor(FColor(255, 226, 120)));
	SafeText = MakePanelText(UpperBox, 13, FLinearColor(FColor(120, 220, 170)));
	PartsText = MakePanelText(UpperBox, 13, FLinearColor(FColor(130, 200, 255)));
	SearchedText = MakePanelText(UpperBox, 11, FLinearColor(0.55f, 0.60f, 0.70f, 1.f));
	StateText = MakePanelText(UpperBox, 13, FLinearColor(0.95f, 0.85f, 0.5f, 1.f));

	// 作业包摘要前空开一两行, 与上面的属性信息分隔(别和上面挤在一起)。
	{
		USpacer* BagGap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		BagGap->SetSize(FVector2D(1.f, 16.f));
		UpperBox->AddChildToVerticalBox(BagGap);
	}

	UTextBlock* BagTitle = MakePanelText(UpperBox, 13, FLinearColor(0.65f, 0.75f, 0.95f, 1.f));
	BagTitle->SetText(FText::FromString(TEXT("作业包摘要")));

	ItemsList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UpperBox->AddChildToVerticalBox(ItemsList);

	// 动作按钮已移除: F=搜索/攻击, E=撤离, 重开走局终弹窗。
	LogText = MakePanelText(UpperBox, 11, FLinearColor(0.6f, 0.65f, 0.72f, 1.f));
	LogText->SetAutoWrapText(true);

	// 道具选择面板放在下半块里; 顶部留一点间距往下挪, 不贴分隔线(用户反馈钉在分隔线正下沿偏上)。PIE 可调。
	{
		USpacer* LowerTopGap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		LowerTopGap->SetSize(FVector2D(1.f, 48.f));
		LowerBox->AddChildToVerticalBox(LowerTopGap);
	}
	UTextBlock* BagItemTitle = MakePanelText(LowerBox, 13, FLinearColor(0.65f, 0.75f, 0.95f, 1.f));
	BagItemTitle->SetText(FText::FromString(TEXT("道具 (数字键选 · Q 使用)")));
	ConsumableList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	LowerBox->AddChildToVerticalBox(ConsumableList);

	// 第 3 层: 右上协议面板(占位, 数值待协议系统迁入)。
	UBorder* ProtocolPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ProtocolPanel->SetBrushColor(FLinearColor(0.05f, 0.03f, 0.03f, 0.9f));
	ProtocolPanel->SetPadding(FMargin(12.f, 8.f));
	ProtocolText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ProtocolText->SetFont(GT_UIStyle::Font(16));
	ProtocolText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.55f, 0.45f, 1.f)));
	ProtocolText->SetText(FText::FromString(TEXT("协议 5")));
	ProtocolPanel->SetContent(ProtocolText);
	// 协议面板保持整图拉伸画法(竖版贴图压扁成小匾, 原版认可的效果; 9-slice 反而会摊开成大竖框)。
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
			KeyText->SetFont(GT_UIStyle::Font(10));
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
		ActionText->SetFont(GT_UIStyle::Font(13));
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
	AddKeyHint(TEXT("/Game/Graytail/UI/keys/ui_key_t"), TEXT("T"), TEXT("事件"));
	AddKeyHint(TEXT("/Game/Graytail/UI/keys/ui_key_q"), TEXT("Q"), TEXT("用道具"));
	// 底栏背景用无分隔的连续金属条(ui_bar_blank_dark): 原版 ui_bottom_bar 是死画的 4 格,
	// 与 6 个键位项数量不匹配会错位, 换成连续条让键帽均分对齐。
	if (UVerticalBoxSlot* HotbarSlot = CenterCol->AddChildToVerticalBox(MakeSkinnedPanel(Hotbar, TEXT("/Game/Graytail/UI/common/ui_bar_blank_dark"))))
	{
		HotbarSlot->SetHorizontalAlignment(HAlign_Center);
		HotbarSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
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

	// 第 6.5 层(模态): 事件交互面板(旅商/赌徒/祭坛/机关), 默认收起。
	EventPanel = CreateWidget<UGT_EventPanelWidget>(this, UGT_EventPanelWidget::StaticClass());
	if (EventPanel)
	{
		EventPanel->OnClosed.BindUObject(this, &UGT_GameHudWidget::HandleEventPanelClosed);
		// 每次选项执行后只刷侧栏数据, 不抢面板焦点。
		EventPanel->OnStateChanged.BindUObject(this, &UGT_GameHudWidget::RefreshPanels);
		EventPanel->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* EventSlot = Screen->AddChildToOverlay(EventPanel))
		{
			EventSlot->SetHorizontalAlignment(HAlign_Fill);
			EventSlot->SetVerticalAlignment(VAlign_Fill);
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
		RunEndTitle->SetFont(GT_UIStyle::Font(24));
		RunEndTitle->SetJustification(ETextJustify::Center);
		EndColumn->AddChildToVerticalBox(RunEndTitle);

		RunEndBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		RunEndBody->SetFont(GT_UIStyle::Font(13));
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

		// 返回菜单: 重选难度(打通 菜单→局内→局终→菜单 循环)。
		UButton* MenuButton = MakeButton(ButtonRow, TEXT("返回菜单"));
		if (UHorizontalBoxSlot* MenuButtonSlot = Cast<UHorizontalBoxSlot>(MenuButton->Slot))
		{
			MenuButtonSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		}
		MenuButton->OnClicked.AddDynamic(this, &UGT_GameHudWidget::OnReturnToMenu);

		RunEndRoot = EndRoot;
		RunEndRoot->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* EndSlot = Screen->AddChildToOverlay(EndRoot))
		{
			EndSlot->SetHorizontalAlignment(HAlign_Fill);
			EndSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// 第 8 层(最顶): 主菜单(整图背景全覆盖)。无局时由 NativeConstruct 打开。
	MainMenu = CreateWidget<UGT_MainMenuWidget>(this, UGT_MainMenuWidget::StaticClass());
	if (MainMenu)
	{
		MainMenu->OnStartRequested.BindUObject(this, &UGT_GameHudWidget::HandleMenuStartRequested);
		MainMenu->OnDeployRequested.BindUObject(this, &UGT_GameHudWidget::HandleDeployRequested);
		MainMenu->OnSettingsRequested.BindUObject(this, &UGT_GameHudWidget::HandleSettingsRequested);
		MainMenu->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* MenuSlot = Screen->AddChildToOverlay(MainMenu))
		{
			MenuSlot->SetHorizontalAlignment(HAlign_Fill);
			MenuSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// 第 8.5 层: 局外部署终端(主菜单之上、教程弹窗之下)。由主菜单「装备天赋」打开。
	DeployTerminal = CreateWidget<UGT_DeployTerminalWidget>(this, UGT_DeployTerminalWidget::StaticClass());
	if (DeployTerminal)
	{
		DeployTerminal->OnBackRequested.BindUObject(this, &UGT_GameHudWidget::HandleDeployBack);
		DeployTerminal->OnDepartRequested.BindUObject(this, &UGT_GameHudWidget::HandleDeployDepart);
		DeployTerminal->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* DeploySlot = Screen->AddChildToOverlay(DeployTerminal))
		{
			DeploySlot->SetHorizontalAlignment(HAlign_Fill);
			DeploySlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// 第 8.6 层: 设置面板(主菜单之上)。由主菜单「设置」打开, 含作弊模式总开关。
	SettingsPanel = CreateWidget<UGT_SettingsWidget>(this, UGT_SettingsWidget::StaticClass());
	if (SettingsPanel)
	{
		SettingsPanel->OnBackRequested.BindUObject(this, &UGT_GameHudWidget::HandleSettingsBack);
		SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* SettingsSlot = Screen->AddChildToOverlay(SettingsPanel))
		{
			SettingsSlot->SetHorizontalAlignment(HAlign_Fill);
			SettingsSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// 第 9 层(最顶): 新手教程教学弹窗(blocking 模态需盖住房间/底栏并抢焦点)。
	TutorialPopup = CreateWidget<UGT_TutorialPopupWidget>(this, UGT_TutorialPopupWidget::StaticClass());
	if (TutorialPopup)
	{
		TutorialPopup->OnConfirmed.BindUObject(this, &UGT_GameHudWidget::TutorialConfirm);
		if (UOverlaySlot* TutorialSlot = Screen->AddChildToOverlay(TutorialPopup))
		{
			TutorialSlot->SetHorizontalAlignment(HAlign_Fill);
			TutorialSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// 第 10 层(最顶): 局内暂停菜单(ESC / =)。盖住一切并抢焦点暂停移动。
	PauseMenu = CreateWidget<UGT_PauseMenuWidget>(this, UGT_PauseMenuWidget::StaticClass());
	if (PauseMenu)
	{
		PauseMenu->OnResume.BindUObject(this, &UGT_GameHudWidget::HandlePauseResume);
		PauseMenu->OnReturnToTitle.BindUObject(this, &UGT_GameHudWidget::HandlePauseReturnToTitle);
		PauseMenu->OnQuitGame.BindUObject(this, &UGT_GameHudWidget::HandlePauseQuitGame);
		PauseMenu->OnOpenCheatPanel.BindUObject(this, &UGT_GameHudWidget::HandleOpenCheatPanel);
		PauseMenu->OnCheatApplied.BindUObject(this, &UGT_GameHudWidget::RefreshAll);
		PauseMenu->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* PauseSlot = Screen->AddChildToOverlay(PauseMenu))
		{
			PauseSlot->SetHorizontalAlignment(HAlign_Fill);
			PauseSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

}

UTextBlock* UGT_GameHudWidget::MakePanelText(UVerticalBox* Panel, int32 FontSize, const FLinearColor& Color)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(GT_UIStyle::Font(FontSize));
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
	Text->SetFont(GT_UIStyle::Font(12));
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
	RefreshMineRiskTag();
	RefreshItemsList();
	RefreshConsumableList();

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
	else if (Reason == FName(TEXT("ProtocolDrain")))
	{
		ReasonLine = TEXT("信号持续高压侵蚀, 血量耗尽, 作业体信号丢失。");
	}
	else if (Reason == FName(TEXT("CombatDeath")))
	{
		ReasonLine = TEXT("作业体损毁于异常体交战。");
	}
	else if (Reason == FName(TEXT("Event")))
	{
		ReasonLine = TEXT("事件导致血量归零, 作业体信号丢失。");
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

void UGT_GameHudWidget::RefreshMineRiskTag()
{
	if (!MineRiskRoot || !MineRiskText)
	{
		return;
	}

	// 当前格已扫描才亮牌(对齐原版: 数字 = 周围8格雷险)。
	bool bShow = false;
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	FGT_DebugRunSnapshot Snapshot;
	TArray<FGT_MiniMapCellViewData> Cells;
	int32 Width = 0;
	int32 Height = 0;
	if (Debug && Debug->GetDebugRunSnapshot(Snapshot)
		&& Debug->GetDebugMiniMapViewData(Cells, Width, Height)
		&& Width > 0 && Cells.IsValidIndex(Snapshot.PlayerY * Width + Snapshot.PlayerX))
	{
		const FGT_MiniMapCellViewData& Cell = Cells[Snapshot.PlayerY * Width + Snapshot.PlayerX];
		if (Cell.bScanned)
		{
			MineRiskText->SetText(FText::FromString(FString::Printf(TEXT("周围雷险: %d"), Cell.DisplayedNumber)));
			bShow = true;
		}
	}
	// Hidden 而非 Collapsed: 占位不变, 显隐切换时房间缩放不跳动。
	MineRiskRoot->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
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

	// 邻域感知天赋: 玩家相邻 8 格描黄框(进房高亮邻域威胁, 对齐 Lua mapHighlight)。
	const UGT_RunContext* HudRunContext = GetRunContext();
	const bool bMapHighlightActive = HudRunContext && HudRunContext->IsLoadoutMapHighlightActive();

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
			// 大图插的旗同步到小地图(旗标数据在 MapOverlay, 纯 UI 标注)。
			const bool bFlagged = !bKnown && MapOverlay && MapOverlay->IsCellFlagged(X, Y);

			USizeBox* CellSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			CellSize->SetWidthOverride(GTMiniMapCellSize);
			CellSize->SetHeightOverride(GTMiniMapCellSize);

			// 底色对齐 MapOverlay: 隐藏(70,75,95) / 插旗(160,50,50) / 雷(220,40,40) / 已知(40,45,60)。
			UBorder* CellBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			FLinearColor BgColor = FLinearColor(FColor(40, 45, 60));
			if (bFlagged)
			{
				BgColor = FLinearColor(FColor(160, 50, 50));
			}
			else if (!bKnown)
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

			// 邻域感知天赋: 玩家相邻 8 格(非玩家格)描黄框, 高亮邻域威胁。
			if (bMapHighlightActive && !bPlayerHere
				&& FMath::Abs(X - Snapshot.PlayerX) <= 1 && FMath::Abs(Y - Snapshot.PlayerY) <= 1)
			{
				UBorder* NeighborHl = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				FSlateBrush HlBrush;
				HlBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
				HlBrush.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
				HlBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
				HlBrush.OutlineSettings.CornerRadii = FVector4(3.f, 3.f, 3.f, 3.f);
				HlBrush.OutlineSettings.Color = FSlateColor(FLinearColor(FColor(255, 220, 80)));
				HlBrush.OutlineSettings.Width = 2.f;
				NeighborHl->SetBrush(HlBrush);
				if (UOverlaySlot* HlSlot = CellOverlay->AddChildToOverlay(NeighborHl))
				{
					HlSlot->SetHorizontalAlignment(HAlign_Fill);
					HlSlot->SetVerticalAlignment(VAlign_Fill);
				}
			}

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
				// 未知格: ? 砖块贴图; 插过旗的盖旗子图。
				AddCellIcon(LoadUiTexture(TEXT("/Game/Graytail/UI/Icons64/01_weizhi_ge")), 0.95f);
				if (bFlagged)
				{
					AddCellIcon(LoadUiTexture(TEXT("/Game/Graytail/UI/Icons64/04_biaoji_qi")), 0.85f);
				}
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
						BadgeText->SetFont(GT_UIStyle::Font(8));
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
						NumberText->SetFont(GT_UIStyle::Font(13));
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
		EmptyText->SetFont(GT_UIStyle::Font(11));
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
			MoreText->SetFont(GT_UIStyle::Font(11));
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
		ItemText->SetFont(GT_UIStyle::Font(12));
		ItemText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.9f, 1.f)));
		ItemText->SetText(FText::FromString(FString::Printf(TEXT(" %s x%d"),
			Def ? *Def->DisplayName : *Stack.ItemId.ToString(),
			Stack.Count)));
		ItemRow->AddChild(ItemText);
	}
}

TArray<FName> UGT_GameHudWidget::GetUsableConsumableIds() const
{
	TArray<FName> Ids;
	const UGT_RunContext* RunContext = GetRunContext();
	if (!RunContext)
	{
		return Ids;
	}
	for (const FGT_ItemStack& Stack : RunContext->GetRunInventory().CarriedItems)
	{
		if (Stack.Count <= 0) { continue; }
		const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(Stack.ItemId);
		if (Def && Def->Kind == EGT_ItemKind::Consumable) { Ids.Add(Stack.ItemId); }
	}
	return Ids;
}

void UGT_GameHudWidget::SelectConsumableSlot(int32 InSlot)
{
	SelectedConsumableSlot = FMath::Max(1, InSlot);
	RefreshConsumableList();
}

void UGT_GameHudWidget::RefreshConsumableList()
{
	if (!ConsumableList)
	{
		return;
	}
	ConsumableList->ClearChildren();

	const TArray<FName> Ids = GetUsableConsumableIds();
	if (Ids.Num() == 0)
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Empty->SetFont(GT_UIStyle::Font(11));
		Empty->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.54f, 0.62f, 1.f)));
		Empty->SetText(FText::FromString(TEXT("(无可用道具)")));
		ConsumableList->AddChildToVerticalBox(Empty);
		return;
	}

	SelectedConsumableSlot = FMath::Clamp(SelectedConsumableSlot, 1, Ids.Num());
	const UGT_RunContext* RunContext = GetRunContext();
	for (int32 i = 0; i < Ids.Num(); ++i)
	{
		const FName Id = Ids[i];
		const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(Id);
		const int32 Count = RunContext ? RunContext->GetRunInventory().GetItemCount(Id) : 0;
		const bool bSel = (i + 1 == SelectedConsumableSlot);

		UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Row->SetFont(GT_UIStyle::Font(12));
		Row->SetColorAndOpacity(FSlateColor(bSel
			? FLinearColor(FColor(255, 226, 120))            // 选中: 亮金
			: FLinearColor(0.78f, 0.80f, 0.86f, 1.f)));
		Row->SetText(FText::FromString(FString::Printf(TEXT("%s【%d】%s x%d"),
			bSel ? TEXT("▶") : TEXT("  "),
			i + 1,
			Def ? *Def->DisplayName : *Id.ToString(),
			Count)));
		ConsumableList->AddChildToVerticalBox(Row);
	}
}

UTexture2D* UGT_GameHudWidget::GetItemIcon(FName ItemId)
{
	// 逐物品图标映射在 GT_ItemCatalog(与战利品弹窗共用), 纹理缓存在 LoadUiTexture 里。
	return LoadUiTexture(GT_ItemCatalog::GetItemIconAssetPath(ItemId));
}

UWidget* UGT_GameHudWidget::MakeSkinnedPanel(UBorder* Panel, const FString& AssetPath,
	const FVector2D& TextureSize, float FramePx)
{
	UTexture2D* Texture = LoadUiTexture(AssetPath);
	if (!Texture)
	{
		// 资产缺失: 保留面板自身的纯色底(对齐 Lua drawPanel 的 fallback)。
		return Panel;
	}

	// 皮肤直接作为面板的背景刷: 背景刷只负责绘制、不参与尺寸计算,
	// 不会出现叠 Image 层时贴图原始尺寸撑大面板的问题。
	FSlateBrush SkinBrush;
	SkinBrush.SetResourceObject(Texture);
	if (FramePx > 0.f && TextureSize.X > 0.f && TextureSize.Y > 0.f)
	{
		// 9-slice: 四角按原始像素绘制, 只拉伸中段 — 面板与贴图长宽比差异大也不变形。
		SkinBrush.ImageSize = TextureSize;
		SkinBrush.DrawAs = ESlateBrushDrawType::Box;
		SkinBrush.Margin = FMargin(FramePx / TextureSize.X, FramePx / TextureSize.Y);
	}
	else
	{
		SkinBrush.DrawAs = ESlateBrushDrawType::Image;
	}
	Panel->SetBrush(SkinBrush);
	Panel->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.85f));
	return Panel;
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

FReply UGT_GameHudWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	// HUD 根不持键盘焦点(gt.HUD 的 SetInputMode 会把焦点定到根):
	// 暂停菜单优先(模态, 持焦点暂停移动); 其次 blocking 教程弹窗;
	// 再次菜单开着转交菜单(Enter/W/S 选难度), 否则转交房间视图(WASD)。
	if (PauseMenu && PauseMenu->IsOpen())
	{
		return FReply::Handled().SetUserFocus(PauseMenu->TakeWidget(), EFocusCause::SetDirectly);
	}
	if (TutorialPopup && TutorialPopup->IsBlockingActive())
	{
		return FReply::Handled().SetUserFocus(TutorialPopup->TakeWidget(), EFocusCause::SetDirectly);
	}
	if (DeployTerminal && DeployTerminal->IsOpen())
	{
		return FReply::Handled().SetUserFocus(DeployTerminal->TakeWidget(), EFocusCause::SetDirectly);
	}
	if (SettingsPanel && SettingsPanel->IsOpen())
	{
		return FReply::Handled().SetUserFocus(SettingsPanel->TakeWidget(), EFocusCause::SetDirectly);
	}
	if (MainMenu && MainMenu->IsOpen())
	{
		return FReply::Handled().SetUserFocus(MainMenu->TakeWidget(), EFocusCause::SetDirectly);
	}
	if (RoomView)
	{
		return FReply::Handled().SetUserFocus(RoomView->TakeWidget(), EFocusCause::SetDirectly);
	}
	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

FReply UGT_GameHudWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// 焦点在弹窗/按钮上时 WASD 事件冒泡到这里, 转发给房间视图记账,
	// 保证持键状态与物理按键一致(关弹窗后按住的键立刻续走, 不卡键)。
	const FKey Key = InKeyEvent.GetKey();
	// ESC / = : 切换局内暂停菜单。= 是给 PIE 用的备用键(PIE 里 ESC 被编辑器抢去停 play)。
	// 菜单打开时由 PauseMenu 自己吞 ESC/=(关闭), 不会冒泡到这里; 这里只负责"关闭→打开"。
	if (Key == EKeys::Escape || Key == EKeys::Equals)
	{
		TogglePauseMenu();
		return FReply::Handled();
	}
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
	// 逻辑修改：不仅要战斗激活，还必须当前脚下站着的是怪物房，F 键才作为“攻击”
	FGT_DebugRunSnapshot Probe;
	if (Debug->GetDebugRunSnapshot(Probe) && Probe.bCombatActive && Probe.CurrentRoomBaseType == EGT_RoomBaseType::Combat)
	{
		// 实时战斗: F = 挥砍, 受挥砍冷却门控(多刀削怪血, 不再一击必杀)。冷却未到则忽略本次。
		if (RoomView && !RoomView->TryConsumePlayerAttack())
		{
			return;
		}
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

void UGT_GameHudWidget::OpenEventPanel()
{
	// 是否在事件房由内核裁决(Open 内部查菜单, 不可用就不弹)。
	if (EventPanel)
	{
		EventPanel->Open();
	}
}

void UGT_GameHudWidget::HandleEventPanelClosed()
{
	// 事件可能改了金币/血量/背包/协议压力, 甚至终结本局; 整体刷新并把焦点还给房间。
	RefreshAll();
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

void UGT_GameHudWidget::OnUseConsumable()
{
	// Q = 使用左下道具栏【选中】的道具(数字键 1-9 切换选中)。满血/无库存由内核拒绝。
	// 显式选中=玩家清楚要用哪件, 不会误用硬币; 用后面板物品数变化即反馈。
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	if (!Debug)
	{
		return;
	}
	const TArray<FName> Ids = GetUsableConsumableIds();
	if (Ids.Num() == 0)
	{
		return;   // 背包无可用道具, 静默
	}
	const int32 Index = FMath::Clamp(SelectedConsumableSlot - 1, 0, Ids.Num() - 1);
	const FName PickId = Ids[Index];

	FGT_DebugRunSnapshot Snapshot;
	const bool bUsed = Debug->DebugUseConsumable(PickId, Snapshot);
	RefreshAll();

	// 用成功的反馈: 止血贴播绿光; 幸运硬币无粒子/无显眼数值变化, 读 outcome 给居中 toast 播报结果。
	if (bUsed)
	{
		if (PickId == FName(TEXT("emergency_bandage")))
		{
			if (RoomView) { RoomView->PlayHealGlow(); }
		}
		else if (PickId == FName(TEXT("lucky_coin")))
		{
			const UGT_RunContext* RC = GetRunContext();
			const FName Status = RC ? RC->GetLastConsumableOutcome().Status : NAME_None;
			if (Status == FName(TEXT("lucky_gold")))       { ShowToast(TEXT("幸运硬币 · 正面! +30 结算币")); }
			else if (Status == FName(TEXT("lucky_reveal"))) { ShowToast(TEXT("幸运硬币 · 反面! 揭示了相邻区域")); }
			else                                            { ShowToast(TEXT("幸运硬币 · 已使用")); }
		}
	}
}

void UGT_GameHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (ToastTimer > 0.f && ToastRoot)
	{
		ToastTimer -= InDeltaTime;
		if (ToastTimer <= 0.f)
		{
			ToastRoot->SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (ToastTimer < 0.5f)
		{
			ToastRoot->SetRenderOpacity(ToastTimer / 0.5f);   // 末 0.5s 线性淡出
		}
	}
}

void UGT_GameHudWidget::ShowToast(const FString& Message, float Duration)
{
	if (!ToastText || !ToastRoot) { return; }
	ToastText->SetText(FText::FromString(Message));
	ToastRoot->SetRenderOpacity(1.f);
	ToastRoot->SetVisibility(ESlateVisibility::HitTestInvisible);   // 显示但不吃点击
	ToastTimer = Duration;
}

void UGT_GameHudWidget::ShowCompassHintIfNeeded()
{
	const UGT_RunContext* RC = GetRunContext();
	if (!RC || !RC->IsLoadoutExitHintActive()) { return; }
	int32 PX = 0, PY = 0;
	if (!RC->TryGetPlayerPosition(PX, PY)) { return; }
	const TArray<FIntPoint> Exits = RC->GetExitCells();
	if (Exits.Num() == 0) { return; }
	// 各撤离信标 -> 四斜向箭头(只给象限, 不暴露"正东/正北"那种精确同行同列, 弱化罗盘强度)。
	// 屏幕 Y 向下: dy<=0 归北侧、dx>=0 归东侧(正交方向并入相邻象限)。
	TArray<FString> Arrows;
	for (const FIntPoint& E : Exits)
	{
		const bool bNorth = (E.Y - PY) <= 0;   // 同行(=)时归北侧
		const bool bEast = (E.X - PX) >= 0;    // 同列(=)时归东侧
		const TCHAR* Arrow = bNorth ? (bEast ? TEXT("↗") : TEXT("↖")) : (bEast ? TEXT("↘") : TEXT("↙"));
		Arrows.AddUnique(FString(Arrow));
	}
	// 罗盘提示显示久一点(5s); 普通 toast 仍是默认 2.2s。
	ShowToast(FString::Printf(TEXT("罗盘 · 撤离信标 %s"), *FString::Join(Arrows, TEXT("  "))), 5.f);
}

void UGT_GameHudWidget::OnNewRun()
{
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	FGT_DebugRunSnapshot Snapshot;
	if (Debug)
	{
		// 每局随机种子(对齐原版); 局内随机仍由该种子完全确定, 复现指定地图用 gt.StartStd。
		// 难度 = 主菜单最近一次选择("重新出发"沿用同难度)。
		Debug->DebugStartStandardRun(FMath::RandRange(1, MAX_int32 - 1), LastDifficulty, Snapshot);
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

	// 罗盘: 开局播报撤离信标方位(带罗盘才显示)。
	ShowCompassHintIfNeeded();

	// 教程局: 重置教学状态, 在出生格弹开场说明(blocking, 会接管焦点暂停移动直到确认)。
	TutorialReset();
	if (bTutorialActive)
	{
		TutorialEnterRoom(Snapshot.PlayerX, Snapshot.PlayerY);
	}
}

void UGT_GameHudWidget::HandleDeployRequested()
{
	if (MainMenu) { MainMenu->Close(); }
	if (DeployTerminal) { DeployTerminal->Open(); }
}

void UGT_GameHudWidget::HandleDeployBack()
{
	if (DeployTerminal) { DeployTerminal->Close(); }
	if (MainMenu) { MainMenu->Open(); }
}

void UGT_GameHudWidget::HandleSettingsRequested()
{
	if (MainMenu) { MainMenu->Close(); }
	if (SettingsPanel) { SettingsPanel->Open(); }
}

void UGT_GameHudWidget::HandleSettingsBack()
{
	if (SettingsPanel) { SettingsPanel->Close(); }
	if (MainMenu) { MainMenu->Open(); }
}

void UGT_GameHudWidget::HandleDeployDepart()
{
	if (DeployTerminal) { DeployTerminal->Close(); }
	if (MainMenu) { MainMenu->OpenToDepart(); }
}

void UGT_GameHudWidget::OnReturnToMenu()
{
	// 只做导航: 收起局终弹窗回主菜单重选难度。bRunEndShown 保持 true,
	// 防止 RefreshRunEndPanel 在局态仍为 Failed/Succeeded 时把弹窗又弹回来。
	if (RunEndRoot)
	{
		RunEndRoot->SetVisibility(ESlateVisibility::Collapsed);
	}
	// 回菜单时收起残留教学弹窗并停用教程驱动(防提示条盖住菜单)。
	bTutorialActive = false;
	TutorialActivePopupId = NAME_None;
	if (TutorialPopup)
	{
		TutorialPopup->Hide();
	}
	if (MainMenu)
	{
		MainMenu->Open();
	}
}

void UGT_GameHudWidget::TogglePauseMenu()
{
	if (!PauseMenu)
	{
		return;
	}
	if (PauseMenu->IsOpen())
	{
		HandlePauseResume();
		return;
	}
	// 仅局内、且无其它顶层界面占据时才弹(主菜单/部署终端/blocking 教程模态各有自己的焦点与按键)。
	const UGT_RunContext* RunContext = GetRunContext();
	const bool bRunActive = RunContext && RunContext->GetRunState() == EGT_RunState::Running;
	const bool bBlockedByOther =
		(MainMenu && MainMenu->IsOpen())
		|| (DeployTerminal && DeployTerminal->IsOpen())
		|| (TutorialPopup && TutorialPopup->IsBlockingActive());
	if (!bRunActive || bBlockedByOther)
	{
		return;
	}
	// 作弊面板入口仅在作弊模式总开关开启时显示(标题「设置」里切换)。
	const UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	PauseMenu->Open(Debug && Debug->IsCheatModeEnabled());
}

void UGT_GameHudWidget::HandlePauseResume()
{
	if (PauseMenu)
	{
		PauseMenu->Close();
	}
	// 焦点还房间视图, 暂停期间松开/按住的 WASD 下一次 keydown 会重新登记(不卡键)。
	if (RoomView)
	{
		RoomView->SetKeyboardFocus();
	}
}

void UGT_GameHudWidget::HandlePauseReturnToTitle()
{
	// 放弃本局: 不发 RunFailed/RunSucceeded 事件 → 不触发结算(待结算金币丢失)。
	// 直接回主菜单, 下次开局 StartNewRun 会重置本局状态。
	if (PauseMenu)
	{
		PauseMenu->Close();
	}
	OnReturnToMenu();
}

void UGT_GameHudWidget::HandlePauseQuitGame()
{
	if (PauseMenu)
	{
		PauseMenu->Close();
	}
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UGT_GameHudWidget::HandleOpenCheatPanel()
{
	// 增量2: 打开作弊面板。当前作弊入口未启用(Open(false) 不显示该按钮), 不会触发。
}

void UGT_GameHudWidget::HandleRoomChanged()
{
	RefreshPanels();

	if (!bTutorialActive)
	{
		return;
	}
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	FGT_DebugRunSnapshot Snapshot;
	if (Debug && Debug->GetDebugRunSnapshot(Snapshot))
	{
		TutorialEnterRoom(Snapshot.PlayerX, Snapshot.PlayerY);
	}
}

void UGT_GameHudWidget::TutorialReset()
{
	bTutorialActive = LastDifficulty == EGT_Difficulty::Tutorial;
	TutorialCurrentRoom = FIntPoint(-1, -1);
	TutorialActivePopupId = NAME_None;
	TutorialShownOnce.Reset();
	if (TutorialPopup)
	{
		TutorialPopup->Hide();
	}
}

void UGT_GameHudWidget::TutorialEnterRoom(int32 X, int32 Y)
{
	if (!bTutorialActive || !TutorialPopup)
	{
		return;
	}

	const FIntPoint Key(X, Y);
	if (Key == TutorialCurrentRoom)
	{
		return;   // 同格不重弹(对齐 Lua currentRoomKey 去重)。
	}

	// 离开旧房: 关掉 roomScoped(非blocking)提示条。blocking 弹窗占焦点时玩家无法移动,
	// 不会走到这里换房, 故无需为其特判。
	if (!TutorialActivePopupId.IsNone())
	{
		const FGT_TutorialPopup* Active = GT_TutorialContent::FindPopupById(TutorialActivePopupId);
		if (Active && Active->bRoomScoped)
		{
			TutorialPopup->Hide();
			TutorialActivePopupId = NAME_None;
		}
	}

	TutorialCurrentRoom = Key;

	const FGT_TutorialPopup* Popup = GT_TutorialContent::FindPopupForCell(X, Y);
	if (!Popup)
	{
		return;
	}

	// 事件格: 占位映射统一是"狐狸旅商", 这里查当前格真实事件类型(旅商/赌徒/祭坛/机关),
	// 把弹窗换成贴合该房间的描述(用户反馈: UI 写旅商但房间其实是赌徒)。
	if (Popup->Id == FName(TEXT("event_rule")))
	{
		UGT_DebugSubsystem* Debug = GetDebugSubsystem();
		FGT_EventMenuView EventMenu;
		if (Debug && Debug->DebugGetEventMenu(EventMenu) && EventMenu.bAvailable)
		{
			if (const FGT_TutorialPopup* KindPopup = GT_TutorialContent::FindEventPopupForKind(EventMenu.Kind))
			{
				Popup = KindPopup;
			}
		}
	}

	if (Popup->bOnce && TutorialShownOnce.Contains(Popup->Id))
	{
		return;   // once 弹窗确认过就不再弹。
	}

	TutorialActivePopupId = Popup->Id;
	TutorialPopup->ShowPopup(*Popup);   // blocking 形态内部会抢焦点暂停移动。
}

void UGT_GameHudWidget::TutorialConfirm()
{
	if (!TutorialPopup)
	{
		return;
	}
	if (!TutorialActivePopupId.IsNone())
	{
		const FGT_TutorialPopup* Active = GT_TutorialContent::FindPopupById(TutorialActivePopupId);
		if (Active && Active->bOnce)
		{
			TutorialShownOnce.Add(Active->Id);
		}
	}
	TutorialActivePopupId = NAME_None;
	TutorialPopup->Hide();

	// 还焦点给房间视图, 玩家可继续移动(对齐 Lua ConfirmPopup 解锁输入)。
	if (RoomView)
	{
		RoomView->SetFocus();
	}
}

void UGT_GameHudWidget::HandleMenuStartRequested(EGT_Difficulty Difficulty)
{
	// 难度确认后直接开局; 将来部署界面(局外组)插在这一步之前, 由团队定。
	LastDifficulty = Difficulty;
	if (MainMenu)
	{
		MainMenu->Close();
	}
	OnNewRun();
}
