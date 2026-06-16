#include "UI/GT_DeployTerminalWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"

#include "Domains/Meta/GT_MetaCatalog.h"
#include "Domains/Meta/GT_MetaProgressSubsystem.h"
#include "Domains/Meta/GT_MetaTypes.h"
#include "UI/GT_IndexedButton.h"

namespace
{
	// 线性色: 渲染转 sRGB 会变亮, 故"深"填充用很小的线性值(否则显成中灰)。
	const FLinearColor GTColGold(0.95f, 0.72f, 0.30f);
	const FLinearColor GTColWhite(0.90f, 0.90f, 0.92f);
	const FLinearColor GTColDim(0.34f, 0.37f, 0.45f);
	const FLinearColor GTColBg(0.010f, 0.014f, 0.024f, 1.0f);     // 根背景 深灰蓝
	const FLinearColor GTColPanel(0.020f, 0.027f, 0.044f, 1.0f);  // 面板/药丸 深灰蓝
	const FLinearColor GTColCard(0.032f, 0.042f, 0.064f, 1.0f);   // 卡片 灰蓝
	const FLinearColor GTColCardEq(0.058f, 0.044f, 0.017f, 1.0f); // 已装备卡片(暗金调)
	const FLinearColor GTColLine(0.11f, 0.082f, 0.030f, 1.0f);    // 暗金描边
	const FLinearColor GTColLineEq(0.55f, 0.34f, 0.085f, 1.0f);   // 高亮金描边

	FSlateFontInfo GTFont(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle("Regular", Size);
	}

	FString EquipEffectText(const FGT_EquipDef& Def)
	{
		if (Def.BonusHP > 0) { return FString::Printf(TEXT("最大生命 +%d"), Def.BonusHP); }
		if (Def.BonusPower > 0) { return FString::Printf(TEXT("战斗力 +%d"), Def.BonusPower); }
		if (Def.bMineImmunity) { return TEXT("首次踩雷免疫伤害"); }
		if (Def.MineDmgReduce > 0) { return FString::Printf(TEXT("雷险伤害 -%d"), Def.MineDmgReduce); }
		if (Def.bShowExitHint) { return TEXT("撤离信标方向提示"); }
		if (Def.SearchBonus > 0) { return FString::Printf(TEXT("搜索奖励 +%d%%"), Def.SearchBonus); }
		return TEXT("");
	}
}

TSharedRef<SWidget> UGT_DeployTerminalWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

UGT_MetaProgressSubsystem* UGT_DeployTerminalWidget::GetMeta() const
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UGT_MetaProgressSubsystem>();
		}
	}
	return nullptr;
}

UTexture2D* UGT_DeployTerminalWidget::LoadUiTex(const FString& AssetPath)
{
	if (AssetPath.IsEmpty()) { return nullptr; }
	if (UTexture2D** Cached = TexCache.Find(AssetPath))
	{
		return *Cached;
	}
	const FString ObjectPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *ObjectPath);
	TexCache.Add(AssetPath, Tex);
	return Tex;
}

UTexture2D* UGT_DeployTerminalWidget::IconForEquip(FName Id) const
{
	UGT_DeployTerminalWidget* Self = const_cast<UGT_DeployTerminalWidget*>(this);
	if (Id == FName(TEXT("armor")))            { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_armor")); }
	if (Id == FName(TEXT("backpack")))         { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_backpack")); }
	if (Id == FName(TEXT("compass")))          { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_compass")); }
	if (Id == FName(TEXT("medkit")))           { return Self->LoadUiTex(TEXT("/Game/Graytail/Items/Consumable/item_consumable_medkit")); }
	if (Id == FName(TEXT("whetstone")))        { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_whetstone")); }
	if (Id == FName(TEXT("insulated_gloves"))) { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_insulated_gloves")); }
	return nullptr;
}

UTexture2D* UGT_DeployTerminalWidget::IconForConsumable(FName Id) const
{
	UGT_DeployTerminalWidget* Self = const_cast<UGT_DeployTerminalWidget*>(this);
	if (Id == FName(TEXT("emergency_bandage")))
	{
		return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_bandage"));
	}
	return nullptr;
}

void UGT_DeployTerminalWidget::Apply9Slice(UBorder* Target, const FString& TexPath, float MarginFrac)
{
	if (!Target) { return; }
	UTexture2D* Tex = LoadUiTex(TexPath);
	if (!Tex)
	{
		Target->SetBrushColor(GTColPanel);
		return;
	}
	FSlateBrush Brush;
	Brush.SetResourceObject(Tex);
	Brush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.Margin = FMargin(MarginFrac);
	Target->SetBrush(Brush);
}

// 贴图按钮(标签烤在图里): 原生尺寸 1:1, 不拉伸不挤压。
UButton* UGT_DeployTerminalWidget::MakeTexButton(UHorizontalBox* Row, const FString& Label, const FString& TexPath, float Pad)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>();
	// 透明按钮底(去掉默认灰框), 只显贴图; 贴图放进 UImage 当内容, 按比例定尺寸不拉伸。
	FButtonStyle Style = Btn->GetStyle();
	FSlateBrush Empty;
	Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Normal = Empty;
	Style.Hovered = Empty;
	Style.Pressed = Empty;
	Style.Disabled = Empty;
	Btn->SetStyle(Style);

	UTexture2D* Tex = LoadUiTex(TexPath);
	if (Tex)
	{
		// 按贴图原生尺寸显示(最忠实、最醒目); SetBrushFromTexture + SizeBox(等比不挤压)。
		const float W = Tex->GetSizeX();
		const float H = Tex->GetSizeY();
		UImage* Img = WidgetTree->ConstructWidget<UImage>();
		Img->SetBrushFromTexture(Tex);
		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>();
		IconBox->SetWidthOverride(W);
		IconBox->SetHeightOverride(H);
		IconBox->SetContent(Img);
		Btn->SetContent(IconBox);
	}
	else
	{
		UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
		Txt->SetText(FText::FromString(Label));
		Txt->SetFont(GTFont(15));
		Txt->SetColorAndOpacity(FSlateColor(GTColWhite));
		Btn->SetContent(Txt);
	}
	if (UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(Btn))
	{
		HSlot->SetPadding(FMargin(Pad, 0.f));
		HSlot->SetVerticalAlignment(VAlign_Center);
	}
	return Btn;
}

void UGT_DeployTerminalWidget::AddFilterPill(UHorizontalBox* Row, const FString& Label, bool bSelected)
{
	UBorder* Pill = WidgetTree->ConstructWidget<UBorder>();
	Pill->SetBrushColor(bSelected ? GTColLine : GTColPanel);
	Pill->SetPadding(FMargin(13.f, 4.f));
	UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
	Txt->SetText(FText::FromString(Label));
	Txt->SetFont(GTFont(13));
	Txt->SetColorAndOpacity(FSlateColor(bSelected ? GTColGold : GTColDim));
	Pill->SetContent(Txt);
	if (UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(Pill))
	{
		HSlot->SetPadding(FMargin(0.f, 0.f, 7.f, 0.f));
		HSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UGT_DeployTerminalWidget::BuildWidgetTree()
{
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DeployRoot"));
	Root->SetBrushColor(GTColBg);
	Root->SetPadding(FMargin(34.f, 22.f));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->SetContent(Main);

	// === 顶栏: 左 返回主界面 + 右 五个导航页签(贴图自带标签) ===
	UHorizontalBox* TopBar = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(TopBar)) { S->SetPadding(FMargin(0, 0, 0, 6)); }
	MakeTexButton(TopBar, TEXT("返回主界面"), TEXT("/Game/Graytail/UI/deploy/ui_button_back_main"), 0.f)
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnBackClicked);
	USpacer* TopSpacer = WidgetTree->ConstructWidget<USpacer>();
	if (UHorizontalBoxSlot* S = TopBar->AddChildToHorizontalBox(TopSpacer)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
	MakeTexButton(TopBar, TEXT("后勤仓库"), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_warehouse"), 4.f)
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavWarehouse);
	MakeTexButton(TopBar, TEXT("后勤申领"), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_requisition"), 4.f)
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavRequisition);
	MakeTexButton(TopBar, TEXT("出勤配置"), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_loadout"), 4.f)
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavLoadout);
	MakeTexButton(TopBar, TEXT("回收资历"), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_recovery"), 4.f)
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavRecovery);
	MakeTexButton(TopBar, TEXT("天赋"), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_talent_selected"), 4.f)
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavTalent);

	// === 面包屑 ===
	BreadcrumbText = WidgetTree->ConstructWidget<UTextBlock>();
	BreadcrumbText->SetFont(GTFont(13));
	BreadcrumbText->SetColorAndOpacity(FSlateColor(GTColDim));
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(BreadcrumbText)) { S->SetPadding(FMargin(4, 2, 0, 8)); }

	// === 主体: 左主面板(terminal_main 金边框) + 右出勤摘要(summary 框) ===
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(Body)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }

	UBorder* MainPanel = WidgetTree->ConstructWidget<UBorder>();
	Apply9Slice(MainPanel, TEXT("/Game/Graytail/UI/common/ui_panel_terminal_main"), 0.07f);
	MainPanel->SetPadding(FMargin(28.f, 24.f));
	if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(MainPanel)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetPadding(FMargin(0, 0, 14, 0)); }
	UVerticalBox* MainCol = WidgetTree->ConstructWidget<UVerticalBox>();
	MainPanel->SetContent(MainCol);

	// -- 标题行: 章节名 + 账户 --
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = MainCol->AddChildToVerticalBox(HeaderRow)) { S->SetPadding(FMargin(0, 0, 0, 10)); }
	SectionTitleText = WidgetTree->ConstructWidget<UTextBlock>();
	SectionTitleText->SetFont(GTFont(24));
	SectionTitleText->SetColorAndOpacity(FSlateColor(GTColGold));
	if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(SectionTitleText)) { S->SetVerticalAlignment(VAlign_Center); }
	USpacer* HeadSpacer = WidgetTree->ConstructWidget<USpacer>();
	if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(HeadSpacer)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
	if (UTexture2D* GoldIcon = LoadUiTex(TEXT("/Game/Graytail/UI/common/ui_icon_account_gold")))
	{
		UImage* Img = WidgetTree->ConstructWidget<UImage>();
		Img->SetBrushFromTexture(GoldIcon);
		Img->SetDesiredSizeOverride(FVector2D(24.f, 24.f));
		if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(Img)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0, 0, 6, 0)); }
	}
	AccountText = WidgetTree->ConstructWidget<UTextBlock>();
	AccountText->SetFont(GTFont(16));
	AccountText->SetColorAndOpacity(FSlateColor(GTColWhite));
	if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(AccountText)) { S->SetVerticalAlignment(VAlign_Center); }

	// -- 筛选条(纯视觉) --
	UHorizontalBox* FilterRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = MainCol->AddChildToVerticalBox(FilterRow)) { S->SetPadding(FMargin(0, 0, 0, 12)); }
	AddFilterPill(FilterRow, TEXT("全部"), true);
	AddFilterPill(FilterRow, TEXT("作业装备"), false);
	AddFilterPill(FilterRow, TEXT("消耗"), false);

	// -- 卡片网格(滚动) --
	ContentScroll = WidgetTree->ConstructWidget<UScrollBox>();
	if (UVerticalBoxSlot* S = MainCol->AddChildToVerticalBox(ContentScroll)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
	ContentWrap = WidgetTree->ConstructWidget<UWrapBox>();
	ContentScroll->AddChild(ContentWrap);

	// -- 底部当前选中条 --
	DetailText = WidgetTree->ConstructWidget<UTextBlock>();
	DetailText->SetFont(GTFont(13));
	DetailText->SetColorAndOpacity(FSlateColor(GTColDim));
	DetailText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* S = MainCol->AddChildToVerticalBox(DetailText)) { S->SetPadding(FMargin(2, 10, 0, 0)); }

	// 右侧 出勤摘要
	USizeBox* SummarySize = WidgetTree->ConstructWidget<USizeBox>();
	SummarySize->SetWidthOverride(300.f);
	Body->AddChildToHorizontalBox(SummarySize);
	UBorder* SummaryPanel = WidgetTree->ConstructWidget<UBorder>();
	Apply9Slice(SummaryPanel, TEXT("/Game/Graytail/UI/deploy/ui_panel_deploy_summary_blank"), 0.12f);
	SummaryPanel->SetPadding(FMargin(20.f, 18.f));
	SummarySize->SetContent(SummaryPanel);
	SummaryBox = WidgetTree->ConstructWidget<UVerticalBox>();
	SummaryPanel->SetContent(SummaryBox);

	// === 底栏: 右下 确认出发 ===
	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(Footer)) { S->SetPadding(FMargin(0, 12, 0, 0)); }
	USpacer* FootSpacer = WidgetTree->ConstructWidget<USpacer>();
	if (UHorizontalBoxSlot* S = Footer->AddChildToHorizontalBox(FootSpacer)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
	MakeTexButton(Footer, TEXT("确认出发"), TEXT("/Game/Graytail/UI/deploy/ui_button_confirm_deploy_large"), 0.f)
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnDepartClicked);
}

void UGT_DeployTerminalWidget::AddItemCard(int32 Index, UTexture2D* Icon, const FString& Name, const FString& TypeLine,
	const FString& Effect, const FString& InfoLine, const FString& StatusLine, const FString& ActionLabel,
	bool bActionEnabled, bool bHighlight)
{
	if (!ContentWrap) { return; }

	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
	CardSize->SetWidthOverride(330.f);
	CardSize->SetHeightOverride(158.f);
	if (UWrapBoxSlot* WSlot = ContentWrap->AddChildToWrapBox(CardSize)) { WSlot->SetPadding(FMargin(6.f)); }

	UBorder* Outer = WidgetTree->ConstructWidget<UBorder>();
	Outer->SetBrushColor(bHighlight ? GTColLineEq : GTColLine);
	Outer->SetPadding(FMargin(bHighlight ? 2.f : 1.f));
	CardSize->SetContent(Outer);
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
	Card->SetBrushColor(bHighlight ? GTColCardEq : GTColCard);
	Card->SetPadding(FMargin(12.f));
	Outer->SetContent(Card);

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
	Card->SetContent(Col);

	// 头部: 图标 + 名称/类型
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>();
	Col->AddChildToVerticalBox(Head);
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>();
	IconBox->SetWidthOverride(42.f);
	IconBox->SetHeightOverride(42.f);
	if (Icon)
	{
		UImage* Img = WidgetTree->ConstructWidget<UImage>();
		Img->SetBrushFromTexture(Icon);
		IconBox->SetContent(Img);
	}
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(IconBox)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0, 0, 10, 0)); }
	UVerticalBox* NameCol = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(NameCol)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetVerticalAlignment(VAlign_Center); }
	UTextBlock* NameTxt = WidgetTree->ConstructWidget<UTextBlock>();
	NameTxt->SetText(FText::FromString(Name));
	NameTxt->SetFont(GTFont(18));
	NameTxt->SetColorAndOpacity(FSlateColor(bHighlight ? GTColGold : GTColWhite));
	NameCol->AddChildToVerticalBox(NameTxt);
	UTextBlock* TypeTxt = WidgetTree->ConstructWidget<UTextBlock>();
	TypeTxt->SetText(FText::FromString(TypeLine));
	TypeTxt->SetFont(GTFont(11));
	TypeTxt->SetColorAndOpacity(FSlateColor(GTColDim));
	NameCol->AddChildToVerticalBox(TypeTxt);

	// 效果描述
	UTextBlock* EffTxt = WidgetTree->ConstructWidget<UTextBlock>();
	EffTxt->SetText(FText::FromString(Effect));
	EffTxt->SetFont(GTFont(13));
	EffTxt->SetColorAndOpacity(FSlateColor(GTColWhite));
	EffTxt->SetAutoWrapText(true);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(EffTxt)) { S->SetPadding(FMargin(0, 7, 0, 0)); }

	// 拥有/价格行
	UTextBlock* InfoTxt = WidgetTree->ConstructWidget<UTextBlock>();
	InfoTxt->SetText(FText::FromString(InfoLine));
	InfoTxt->SetFont(GTFont(12));
	InfoTxt->SetColorAndOpacity(FSlateColor(GTColDim));
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(InfoTxt)) { S->SetPadding(FMargin(0, 5, 0, 0)); }

	// 底部: 状态(左) + 动作按钮(右)
	UHorizontalBox* Foot = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Foot)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetVerticalAlignment(VAlign_Bottom); }
	UTextBlock* StatusTxt = WidgetTree->ConstructWidget<UTextBlock>();
	StatusTxt->SetText(FText::FromString(StatusLine));
	StatusTxt->SetFont(GTFont(13));
	StatusTxt->SetColorAndOpacity(FSlateColor(bHighlight ? GTColGold : GTColDim));
	if (UHorizontalBoxSlot* S = Foot->AddChildToHorizontalBox(StatusTxt)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetVerticalAlignment(VAlign_Center); }

	UGT_IndexedButton* ActBtn = WidgetTree->ConstructWidget<UGT_IndexedButton>();
	ActBtn->Index = Index;
	ActBtn->SetIsEnabled(bActionEnabled);
	ActBtn->OnIndexClicked.AddDynamic(this, &UGT_DeployTerminalWidget::HandleRowClicked);
	UTextBlock* ActTxt = WidgetTree->ConstructWidget<UTextBlock>();
	ActTxt->SetText(FText::FromString(ActionLabel));
	ActTxt->SetFont(GTFont(15));
	ActTxt->SetColorAndOpacity(FSlateColor(bActionEnabled ? GTColGold : GTColDim));
	ActBtn->SetContent(ActTxt);
	if (UHorizontalBoxSlot* S = Foot->AddChildToHorizontalBox(ActBtn)) { S->SetVerticalAlignment(VAlign_Center); }
}

void UGT_DeployTerminalWidget::RebuildContent()
{
	if (!ContentWrap) { return; }
	ContentWrap->ClearChildren();
	CurrentRows.Reset();

	UGT_MetaProgressSubsystem* Meta = GetMeta();
	if (!Meta) { return; }
	const int32 Gold = Meta->GetGold();
	const int32 EquippedNum = Meta->GetEquippedItems().Num();

	if (CurrentSection == ESection::Requisition)
	{
		if (DetailText) { DetailText->SetText(FText::FromString(TEXT("点击卡片右侧按钮: 未拥有→申领, 已拥有→装备/卸下(最多 2 件)。"))); }
		for (const FGT_EquipDef& Def : GT_MetaCatalog::GetEquipDefs())
		{
			const bool bOwned = Meta->OwnsItem(Def.Id);
			const bool bEq = Meta->IsEquipped(Def.Id);
			const bool bAfford = Gold >= Def.Price;
			const FString Info = bOwned ? TEXT("拥有 x1") : FString::Printf(TEXT("价格 %d 结算币"), Def.Price);
			const FString Status = !bOwned ? TEXT("未拥有") : (bEq ? TEXT("已装备") : TEXT("已拥有"));
			const FString Action = !bOwned ? TEXT("申领") : (bEq ? TEXT("卸下") : TEXT("装备"));
			const bool bEnabled = !bOwned ? bAfford : (bEq ? true : EquippedNum < GT_MetaCatalog::MaxEquipped);
			AddItemCard(CurrentRows.Num(), IconForEquip(Def.Id), Def.DisplayName, TEXT("作业装备 · 后勤"),
				EquipEffectText(Def), Info, Status, Action, bEnabled, bEq);
			CurrentRows.Add({ Def.Id, false });
		}
		for (const FGT_ConsumableDef& Def : GT_MetaCatalog::GetConsumableDefs())
		{
			const bool bAfford = Gold >= Def.Price;
			const int32 Have = Meta->GetConsumableCount(Def.Id);
			AddItemCard(CurrentRows.Num(), IconForConsumable(Def.Id), Def.DisplayName, TEXT("作业消耗品 · 后勤"),
				FString::Printf(TEXT("局内使用: 回复 %d 生命"), Def.Heal),
				FString::Printf(TEXT("价格 %d 结算币"), Def.Price), FString::Printf(TEXT("持有 %d"), Have),
				TEXT("申领"), bAfford, false);
			CurrentRows.Add({ Def.Id, true });
		}
	}
	else if (CurrentSection == ESection::Loadout)
	{
		if (DetailText) { DetailText->SetText(FText::FromString(TEXT("装备: 点击装/卸(最多 2 件)。消耗品: 点击循环设置带入数量。"))); }
		bool bAny = false;
		for (const FGT_EquipDef& Def : GT_MetaCatalog::GetEquipDefs())
		{
			if (!Meta->OwnsItem(Def.Id)) { continue; }
			bAny = true;
			const bool bEq = Meta->IsEquipped(Def.Id);
			AddItemCard(CurrentRows.Num(), IconForEquip(Def.Id), Def.DisplayName, TEXT("作业装备 · 后勤"),
				EquipEffectText(Def), TEXT("拥有 x1"), bEq ? TEXT("已装备") : TEXT("已拥有"),
				bEq ? TEXT("卸下") : TEXT("装备"), bEq ? true : EquippedNum < GT_MetaCatalog::MaxEquipped, bEq);
			CurrentRows.Add({ Def.Id, false });
		}
		for (const FGT_ConsumableDef& Def : GT_MetaCatalog::GetConsumableDefs())
		{
			const int32 Stock = Meta->GetConsumableCount(Def.Id);
			if (Stock <= 0) { continue; }
			bAny = true;
			const int32 Carry = Meta->GetLoadout().FindRef(Def.Id);
			AddItemCard(CurrentRows.Num(), IconForConsumable(Def.Id), Def.DisplayName, TEXT("作业消耗品 · 后勤"),
				FString::Printf(TEXT("局内使用: 回复 %d 生命"), Def.Heal),
				FString::Printf(TEXT("库存 %d"), Stock), FString::Printf(TEXT("已带入 %d"), Carry),
				FString::Printf(TEXT("带入 %d"), Carry), true, Carry > 0);
			CurrentRows.Add({ Def.Id, true });
		}
		if (!bAny && DetailText)
		{
			DetailText->SetText(FText::FromString(TEXT("尚无可配置的装备/消耗品 —— 先到「后勤申领」购买。")));
		}
	}
	else if (DetailText)
	{
		DetailText->SetText(FText::FromString(TEXT("该模块将在 Phase 2 开放(后勤仓库 / 回收资历 / 天赋)。")));
	}
}

void UGT_DeployTerminalWidget::RefreshAccount()
{
	UGT_MetaProgressSubsystem* Meta = GetMeta();
	if (!Meta) { return; }
	const int32 Items = GT_MetaCatalog::GetEquipDefs().Num() + GT_MetaCatalog::GetConsumableDefs().Num();
	if (AccountText) { AccountText->SetText(FText::FromString(FString::Printf(TEXT("结算币 %d · %d 项"), Meta->GetGold(), Items))); }
}

void UGT_DeployTerminalWidget::RefreshSummary()
{
	if (!SummaryBox) { return; }
	SummaryBox->ClearChildren();
	UGT_MetaProgressSubsystem* Meta = GetMeta();
	if (!Meta) { return; }

	auto AddLine = [this](const FString& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
		T->SetText(FText::FromString(Text));
		T->SetFont(GTFont(Size));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = SummaryBox->AddChildToVerticalBox(T)) { S->SetPadding(FMargin(0, 3)); }
	};

	AddLine(TEXT("出勤摘要"), 20, GTColGold);
	AddLine(TEXT("── 已装备 ──"), 12, GTColDim);
	const TArray<FName>& Equipped = Meta->GetEquippedItems();
	if (Equipped.Num() == 0) { AddLine(TEXT("(未配置)"), 15, GTColDim); }
	for (const FName& Id : Equipped)
	{
		const FGT_EquipDef* Def = GT_MetaCatalog::FindEquip(Id);
		AddLine(FString::Printf(TEXT("· %s"), Def ? Def->DisplayName : *Id.ToString()), 15, GTColWhite);
	}

	AddLine(TEXT("── 本局加成 ──"), 12, GTColDim);
	const FGT_EquipBonus B = Meta->GetEquipBonus();
	const FGT_TalentEffects T = Meta->GetTalentEffects();
	bool bAnyBonus = false;
	if (B.BonusHP > 0) { AddLine(FString::Printf(TEXT("生命 +%d"), B.BonusHP), 15, GTColGold); bAnyBonus = true; }
	if (B.BonusPower > 0) { AddLine(FString::Printf(TEXT("战斗力 +%d"), B.BonusPower), 15, GTColGold); bAnyBonus = true; }
	const int32 MineRed = B.MineDmgReduce + T.MineDmgReduce;
	if (MineRed > 0) { AddLine(FString::Printf(TEXT("雷险伤害 -%d"), MineRed), 15, GTColGold); bAnyBonus = true; }
	if (B.bMineImmunity) { AddLine(TEXT("首次踩雷免疫"), 15, GTColGold); bAnyBonus = true; }
	if (B.SearchBonus > 0) { AddLine(FString::Printf(TEXT("搜索奖励 +%d%%"), B.SearchBonus), 15, GTColGold); bAnyBonus = true; }
	if (B.bShowExitHint) { AddLine(TEXT("撤离信标提示"), 15, GTColGold); bAnyBonus = true; }
	if (!bAnyBonus) { AddLine(TEXT("(无)"), 15, GTColDim); }

	AddLine(TEXT("── 带入消耗品 ──"), 12, GTColDim);
	const TMap<FName, int32>& Loadout = Meta->GetLoadout();
	if (Loadout.Num() == 0) { AddLine(TEXT("(无)"), 15, GTColDim); }
	for (const TPair<FName, int32>& Pair : Loadout)
	{
		const FGT_ConsumableDef* Def = GT_MetaCatalog::FindConsumable(Pair.Key);
		AddLine(FString::Printf(TEXT("· %s ×%d"), Def ? Def->DisplayName : *Pair.Key.ToString(), Pair.Value), 15, GTColWhite);
	}
}

void UGT_DeployTerminalWidget::RefreshAll()
{
	RefreshAccount();
	RebuildContent();
	RefreshSummary();
}

void UGT_DeployTerminalWidget::ShowSection(ESection Section)
{
	CurrentSection = Section;
	FString Name;
	switch (Section)
	{
	case ESection::Requisition: Name = TEXT("后勤申领"); break;
	case ESection::Loadout:     Name = TEXT("出勤配置"); break;
	case ESection::Warehouse:   Name = TEXT("后勤仓库"); break;
	case ESection::Talent:      Name = TEXT("天赋"); break;
	case ESection::Recovery:    Name = TEXT("回收资历"); break;
	}
	if (SectionTitleText) { SectionTitleText->SetText(FText::FromString(Name)); }
	if (BreadcrumbText) { BreadcrumbText->SetText(FText::FromString(TEXT("当前页签 / ") + Name)); }
	RefreshAccount();
	RebuildContent();
	RefreshSummary();
}

void UGT_DeployTerminalWidget::HandleRowClicked(int32 Index)
{
	if (!CurrentRows.IsValidIndex(Index)) { return; }
	UGT_MetaProgressSubsystem* Meta = GetMeta();
	if (!Meta) { return; }
	const FRowRef Ref = CurrentRows[Index];
	FName Err;

	if (Ref.bConsumable && CurrentSection == ESection::Loadout)
	{
		const int32 Cur = Meta->GetLoadout().FindRef(Ref.Id);
		const int32 Stock = Meta->GetConsumableCount(Ref.Id);
		const FGT_ConsumableDef* Def = GT_MetaCatalog::FindConsumable(Ref.Id);
		int32 Cap = Stock;
		if (Def && Def->MaxCarry > 0) { Cap = FMath::Min(Cap, Def->MaxCarry); }
		const int32 Next = (Cur >= Cap) ? 0 : Cur + 1;
		Meta->SetLoadoutConsumable(Ref.Id, Next);
	}
	else if (Ref.bConsumable)
	{
		Meta->BuyConsumable(Ref.Id, 1, Err);   // 申领页买消耗品
	}
	else
	{
		// 装备: 未拥有→购买, 已拥有→装/卸(申领页与出勤配置页一致)。
		if (!Meta->OwnsItem(Ref.Id)) { Meta->BuyItem(Ref.Id, Err); }
		else { Meta->ToggleEquip(Ref.Id, Err); }
	}
	RefreshAll();
}

void UGT_DeployTerminalWidget::OnNavRequisition() { ShowSection(ESection::Requisition); }
void UGT_DeployTerminalWidget::OnNavLoadout() { ShowSection(ESection::Loadout); }
void UGT_DeployTerminalWidget::OnNavWarehouse() { ShowSection(ESection::Warehouse); }
void UGT_DeployTerminalWidget::OnNavTalent() { ShowSection(ESection::Talent); }
void UGT_DeployTerminalWidget::OnNavRecovery() { ShowSection(ESection::Recovery); }

void UGT_DeployTerminalWidget::OnBackClicked() { OnBackRequested.ExecuteIfBound(); }
void UGT_DeployTerminalWidget::OnDepartClicked() { OnDepartRequested.ExecuteIfBound(); }

void UGT_DeployTerminalWidget::Open()
{
	SetVisibility(ESlateVisibility::Visible);
	ShowSection(ESection::Requisition);
	SetKeyboardFocus();
}

void UGT_DeployTerminalWidget::Close()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UGT_DeployTerminalWidget::IsOpen() const
{
	return GetVisibility() != ESlateVisibility::Collapsed;
}

FReply UGT_DeployTerminalWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnBackRequested.ExecuteIfBound();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
