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
	const FLinearColor GTColGold(0.86f, 0.73f, 0.36f);
	const FLinearColor GTColWhite(0.93f, 0.93f, 0.93f);
	const FLinearColor GTColDim(0.62f, 0.63f, 0.66f);
	const FLinearColor GTColGreen(0.52f, 0.90f, 0.55f);
	const FLinearColor GTColCardBg(0.11f, 0.12f, 0.15f, 0.96f);
	const FLinearColor GTColCardEquipped(0.10f, 0.19f, 0.13f, 0.97f);

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
	if (Id == FName(TEXT("armor")))    { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_armor")); }
	if (Id == FName(TEXT("backpack"))) { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_backpack")); }
	if (Id == FName(TEXT("compass")))  { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_compass")); }
	if (Id == FName(TEXT("medkit")))   { return Self->LoadUiTex(TEXT("/Game/Graytail/Items/Consumable/item_consumable_medkit")); }
	// whetstone / insulated_gloves 暂无专属图(待补 AI 生成)。
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
		Target->SetBrushColor(FLinearColor(0.09f, 0.10f, 0.13f, 0.96f));
		return;
	}
	FSlateBrush Brush;
	Brush.SetResourceObject(Tex);
	Brush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
	Brush.DrawAs = (MarginFrac > 0.f) ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
	Brush.Margin = FMargin(MarginFrac);
	Target->SetBrush(Brush);
}

UButton* UGT_DeployTerminalWidget::MakeNavButton(UHorizontalBox* Row, const FString& Label, const FString& TexPath)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>();
	UTexture2D* Tex = LoadUiTex(TexPath);
	if (Tex)
	{
		// 用导航贴图(标签已烤在图里), 归一到 44 高。
		const float TexW = FMath::Max(1, Tex->GetSizeX());
		const float TexH = FMath::Max(1, Tex->GetSizeY());
		const FVector2D Size(TexW * (44.f / TexH), 44.f);
		FButtonStyle Style = Btn->GetStyle();
		FSlateBrush B;
		B.SetResourceObject(Tex);
		B.ImageSize = Size;
		B.DrawAs = ESlateBrushDrawType::Image;
		Style.Normal = B;
		Style.Hovered = B;
		Style.Pressed = B;
		Btn->SetStyle(Style);
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
		HSlot->SetPadding(FMargin(3.f, 0.f));
		HSlot->SetVerticalAlignment(VAlign_Center);
	}
	return Btn;
}

UButton* UGT_DeployTerminalWidget::MakeFooterButton(UHorizontalBox* Row, const FString& Label, const FString& AssetPath)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>();
	if (UTexture2D* Tex = LoadUiTex(AssetPath))
	{
		const float TexW = FMath::Max(1, Tex->GetSizeX());
		const float TexH = FMath::Max(1, Tex->GetSizeY());
		const FVector2D Size(TexW * (52.f / TexH), 52.f);
		FButtonStyle Style = Btn->GetStyle();
		FSlateBrush B;
		B.SetResourceObject(Tex);
		B.ImageSize = Size;
		B.DrawAs = ESlateBrushDrawType::Image;
		Style.Normal = B;
		Style.Hovered = B;
		Style.Pressed = B;
		Btn->SetStyle(Style);
	}
	UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
	Txt->SetText(FText::FromString(Label));
	Txt->SetFont(GTFont(19));
	Txt->SetColorAndOpacity(FSlateColor(GTColGold));
	Btn->SetContent(Txt);
	if (UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(Btn))
	{
		HSlot->SetPadding(FMargin(6.f, 0.f));
		HSlot->SetVerticalAlignment(VAlign_Center);
	}
	return Btn;
}

void UGT_DeployTerminalWidget::BuildWidgetTree()
{
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DeployRoot"));
	Root->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.07f, 0.99f));
	Root->SetPadding(FMargin(40.f, 30.f));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->SetContent(Main);

	// --- 顶栏: 标题 + 弹簧 + 金币 ---
	UHorizontalBox* TopBar = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(TopBar)) { S->SetPadding(FMargin(0, 0, 0, 14)); }
	TitleText = WidgetTree->ConstructWidget<UTextBlock>();
	TitleText->SetText(FText::FromString(TEXT("灰尾回收 · 部署终端")));
	TitleText->SetFont(GTFont(26));
	TitleText->SetColorAndOpacity(FSlateColor(GTColGold));
	if (UHorizontalBoxSlot* S = TopBar->AddChildToHorizontalBox(TitleText)) { S->SetVerticalAlignment(VAlign_Center); }
	USpacer* TopSpacer = WidgetTree->ConstructWidget<USpacer>();
	if (UHorizontalBoxSlot* S = TopBar->AddChildToHorizontalBox(TopSpacer)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
	if (UTexture2D* GoldIcon = LoadUiTex(TEXT("/Game/Graytail/UI/common/ui_icon_account_gold")))
	{
		UImage* Img = WidgetTree->ConstructWidget<UImage>();
		Img->SetBrushFromTexture(GoldIcon);
		Img->SetDesiredSizeOverride(FVector2D(28.f, 28.f));
		if (UHorizontalBoxSlot* S = TopBar->AddChildToHorizontalBox(Img)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0, 0, 7, 0)); }
	}
	GoldText = WidgetTree->ConstructWidget<UTextBlock>();
	GoldText->SetFont(GTFont(22));
	GoldText->SetColorAndOpacity(FSlateColor(GTColGold));
	if (UHorizontalBoxSlot* S = TopBar->AddChildToHorizontalBox(GoldText)) { S->SetVerticalAlignment(VAlign_Center); }

	// --- 导航栏(贴图按钮, 标签烤在图里) ---
	UHorizontalBox* Nav = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(Nav)) { S->SetPadding(FMargin(0, 0, 0, 10)); }
	MakeNavButton(Nav, TEXT("申领"), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_requisition"))
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavRequisition);
	MakeNavButton(Nav, TEXT("作业装备"), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_loadout"))
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavLoadout);
	MakeNavButton(Nav, TEXT("仓库"), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_warehouse"))
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavWarehouse);
	MakeNavButton(Nav, TEXT("天赋"), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_talent_selected"))
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavTalent);
	MakeNavButton(Nav, TEXT("抢救记录"), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_recovery"))
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavRecovery);

	HintText = WidgetTree->ConstructWidget<UTextBlock>();
	HintText->SetFont(GTFont(14));
	HintText->SetColorAndOpacity(FSlateColor(GTColDim));
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(HintText)) { S->SetPadding(FMargin(2, 0, 0, 8)); }

	// --- 主体: 带框内容区(卡片网格滚动) + 右侧带框摘要 ---
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(Body)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }

	UBorder* ContentFrame = WidgetTree->ConstructWidget<UBorder>();
	Apply9Slice(ContentFrame, TEXT("/Game/Graytail/UI/deploy/ui_panel_deploy_main_blank"), 0.30f);
	ContentFrame->SetPadding(FMargin(16.f));
	if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(ContentFrame)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetPadding(FMargin(0, 0, 14, 0)); }
	ContentScroll = WidgetTree->ConstructWidget<UScrollBox>();
	ContentFrame->SetContent(ContentScroll);
	ContentWrap = WidgetTree->ConstructWidget<UWrapBox>();
	ContentScroll->AddChild(ContentWrap);

	USizeBox* SummarySize = WidgetTree->ConstructWidget<USizeBox>();
	SummarySize->SetWidthOverride(300.f);
	Body->AddChildToHorizontalBox(SummarySize);
	UBorder* SummaryFrame = WidgetTree->ConstructWidget<UBorder>();
	Apply9Slice(SummaryFrame, TEXT("/Game/Graytail/UI/deploy/ui_panel_deploy_summary_blank"), 0.30f);
	SummaryFrame->SetPadding(FMargin(18.f));
	SummarySize->SetContent(SummaryFrame);
	SummaryBox = WidgetTree->ConstructWidget<UVerticalBox>();
	SummaryFrame->SetContent(SummaryBox);

	// --- 底栏: 返回 + 弹簧 + 出发探索 ---
	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(Footer)) { S->SetPadding(FMargin(0, 14, 0, 0)); }
	MakeFooterButton(Footer, TEXT("返回主界面"), TEXT("/Game/Graytail/UI/deploy/ui_button_back_main"))
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnBackClicked);
	USpacer* FootSpacer = WidgetTree->ConstructWidget<USpacer>();
	if (UHorizontalBoxSlot* S = Footer->AddChildToHorizontalBox(FootSpacer)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
	MakeFooterButton(Footer, TEXT("出发探索 ▶"), TEXT("/Game/Graytail/UI/deploy/ui_button_confirm_deploy_large"))
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnDepartClicked);
}

void UGT_DeployTerminalWidget::AddItemCard(int32 Index, UTexture2D* Icon, const FString& Name, const FString& TypeLine,
	const FString& Effect, const FString& InfoLine, const FString& ActionLabel, bool bActionEnabled, bool bHighlight)
{
	if (!ContentWrap) { return; }

	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
	CardSize->SetWidthOverride(338.f);
	CardSize->SetHeightOverride(150.f);
	if (UWrapBoxSlot* WSlot = ContentWrap->AddChildToWrapBox(CardSize)) { WSlot->SetPadding(FMargin(6.f)); }

	UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
	Card->SetBrushColor(bHighlight ? GTColCardEquipped : GTColCardBg);
	Card->SetPadding(FMargin(12.f));
	CardSize->SetContent(Card);

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
	Card->SetContent(Col);

	// 头部: 图标 + (名称 / 类型)
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>();
	Col->AddChildToVerticalBox(Head);
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>();
	IconBox->SetWidthOverride(44.f);
	IconBox->SetHeightOverride(44.f);
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
	NameTxt->SetFont(GTFont(19));
	NameTxt->SetColorAndOpacity(FSlateColor(bHighlight ? GTColGreen : GTColWhite));
	NameCol->AddChildToVerticalBox(NameTxt);
	UTextBlock* TypeTxt = WidgetTree->ConstructWidget<UTextBlock>();
	TypeTxt->SetText(FText::FromString(TypeLine));
	TypeTxt->SetFont(GTFont(12));
	TypeTxt->SetColorAndOpacity(FSlateColor(GTColDim));
	NameCol->AddChildToVerticalBox(TypeTxt);

	// 效果描述
	UTextBlock* EffTxt = WidgetTree->ConstructWidget<UTextBlock>();
	EffTxt->SetText(FText::FromString(Effect));
	EffTxt->SetFont(GTFont(14));
	EffTxt->SetColorAndOpacity(FSlateColor(GTColWhite));
	EffTxt->SetAutoWrapText(true);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(EffTxt)) { S->SetPadding(FMargin(0, 8, 0, 0)); }

	// 底部: 拥有/价格行 + 动作按钮
	UHorizontalBox* Foot = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Foot)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetVerticalAlignment(VAlign_Bottom); }
	UTextBlock* InfoTxt = WidgetTree->ConstructWidget<UTextBlock>();
	InfoTxt->SetText(FText::FromString(InfoLine));
	InfoTxt->SetFont(GTFont(13));
	InfoTxt->SetColorAndOpacity(FSlateColor(GTColDim));
	if (UHorizontalBoxSlot* S = Foot->AddChildToHorizontalBox(InfoTxt)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetVerticalAlignment(VAlign_Center); }

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
	if (HintText) { HintText->SetText(FText::GetEmpty()); }

	UGT_MetaProgressSubsystem* Meta = GetMeta();
	if (!Meta) { return; }
	const int32 Gold = Meta->GetGold();

	if (CurrentSection == ESection::Requisition)
	{
		for (const FGT_EquipDef& Def : GT_MetaCatalog::GetEquipDefs())
		{
			const bool bOwned = Meta->OwnsItem(Def.Id);
			const bool bAfford = Gold >= Def.Price;
			const FString Info = bOwned ? TEXT("已拥有 x1")
				: FString::Printf(TEXT("价格 %d 结算币"), Def.Price);
			AddItemCard(CurrentRows.Num(), IconForEquip(Def.Id), Def.DisplayName, TEXT("作业装备 · 后勤"),
				EquipEffectText(Def), Info, bOwned ? TEXT("已拥有") : TEXT("申领"),
				!bOwned && bAfford, false);
			CurrentRows.Add({ Def.Id, false });
		}
		for (const FGT_ConsumableDef& Def : GT_MetaCatalog::GetConsumableDefs())
		{
			const bool bAfford = Gold >= Def.Price;
			const int32 Have = Meta->GetConsumableCount(Def.Id);
			AddItemCard(CurrentRows.Num(), IconForConsumable(Def.Id), Def.DisplayName, TEXT("作业消耗品 · 后勤"),
				FString::Printf(TEXT("局内使用: 回复 %d 生命"), Def.Heal),
				FString::Printf(TEXT("持有 %d · 价格 %d 结算币"), Have, Def.Price),
				TEXT("申领"), bAfford, false);
			CurrentRows.Add({ Def.Id, true });
		}
	}
	else if (CurrentSection == ESection::Loadout)
	{
		bool bAny = false;
		for (const FGT_EquipDef& Def : GT_MetaCatalog::GetEquipDefs())
		{
			if (!Meta->OwnsItem(Def.Id)) { continue; }
			bAny = true;
			const bool bEquipped = Meta->IsEquipped(Def.Id);
			AddItemCard(CurrentRows.Num(), IconForEquip(Def.Id), Def.DisplayName, TEXT("作业装备 · 后勤"),
				EquipEffectText(Def), bEquipped ? TEXT("已装备") : TEXT("可装备"),
				bEquipped ? TEXT("卸下") : TEXT("装备"), true, bEquipped);
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
				FString::Printf(TEXT("库存 %d · 已带入 %d"), Stock, Carry),
				FString::Printf(TEXT("带入 %d"), Carry), true, Carry > 0);
			CurrentRows.Add({ Def.Id, true });
		}
		if (!bAny && HintText)
		{
			HintText->SetText(FText::FromString(TEXT("尚无可配置的装备/消耗品 —— 先到「申领」购买。")));
		}
	}
	else if (HintText)
	{
		HintText->SetText(FText::FromString(TEXT("该模块将在 Phase 2 开放(仓库 / 天赋 / 抢救记录)。")));
	}
}

void UGT_DeployTerminalWidget::RefreshGold()
{
	if (UGT_MetaProgressSubsystem* Meta = GetMeta())
	{
		if (GoldText) { GoldText->SetText(FText::FromString(FString::Printf(TEXT("结算币 %d"), Meta->GetGold()))); }
	}
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
	AddLine(TEXT("── 已装备 ──"), 13, GTColDim);
	const TArray<FName>& Equipped = Meta->GetEquippedItems();
	if (Equipped.Num() == 0) { AddLine(TEXT("(未配置)"), 15, GTColDim); }
	for (const FName& Id : Equipped)
	{
		const FGT_EquipDef* Def = GT_MetaCatalog::FindEquip(Id);
		AddLine(FString::Printf(TEXT("· %s"), Def ? Def->DisplayName : *Id.ToString()), 15, GTColWhite);
	}

	AddLine(TEXT("── 本局加成 ──"), 13, GTColDim);
	const FGT_EquipBonus B = Meta->GetEquipBonus();
	const FGT_TalentEffects T = Meta->GetTalentEffects();
	bool bAnyBonus = false;
	if (B.BonusHP > 0) { AddLine(FString::Printf(TEXT("生命 +%d"), B.BonusHP), 15, GTColGreen); bAnyBonus = true; }
	if (B.BonusPower > 0) { AddLine(FString::Printf(TEXT("战斗力 +%d"), B.BonusPower), 15, GTColGreen); bAnyBonus = true; }
	const int32 MineRed = B.MineDmgReduce + T.MineDmgReduce;
	if (MineRed > 0) { AddLine(FString::Printf(TEXT("雷险伤害 -%d"), MineRed), 15, GTColGreen); bAnyBonus = true; }
	if (B.bMineImmunity) { AddLine(TEXT("首次踩雷免疫"), 15, GTColGreen); bAnyBonus = true; }
	if (B.SearchBonus > 0) { AddLine(FString::Printf(TEXT("搜索奖励 +%d%%"), B.SearchBonus), 15, GTColGreen); bAnyBonus = true; }
	if (B.bShowExitHint) { AddLine(TEXT("撤离信标提示"), 15, GTColGreen); bAnyBonus = true; }
	if (!bAnyBonus) { AddLine(TEXT("(无)"), 15, GTColDim); }

	AddLine(TEXT("── 带入消耗品 ──"), 13, GTColDim);
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
	RefreshGold();
	RebuildContent();
	RefreshSummary();
}

void UGT_DeployTerminalWidget::ShowSection(ESection Section)
{
	CurrentSection = Section;
	RebuildContent();
	RefreshSummary();
}

void UGT_DeployTerminalWidget::HandleRowClicked(int32 Index)
{
	if (!CurrentRows.IsValidIndex(Index)) { return; }
	UGT_MetaProgressSubsystem* Meta = GetMeta();
	if (!Meta) { return; }
	const FRowRef Ref = CurrentRows[Index];

	if (CurrentSection == ESection::Requisition)
	{
		FName Err;
		if (Ref.bConsumable) { Meta->BuyConsumable(Ref.Id, 1, Err); }
		else { Meta->BuyItem(Ref.Id, Err); }
	}
	else if (CurrentSection == ESection::Loadout)
	{
		if (Ref.bConsumable)
		{
			const int32 Cur = Meta->GetLoadout().FindRef(Ref.Id);
			const int32 Stock = Meta->GetConsumableCount(Ref.Id);
			const FGT_ConsumableDef* Def = GT_MetaCatalog::FindConsumable(Ref.Id);
			int32 Cap = Stock;
			if (Def && Def->MaxCarry > 0) { Cap = FMath::Min(Cap, Def->MaxCarry); }
			const int32 Next = (Cur >= Cap) ? 0 : Cur + 1;
			Meta->SetLoadoutConsumable(Ref.Id, Next);
		}
		else
		{
			FName Err;
			Meta->ToggleEquip(Ref.Id, Err);
		}
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
	CurrentSection = ESection::Requisition;
	RefreshAll();
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
