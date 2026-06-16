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
	const FLinearColor GTColGold(0.85f, 0.72f, 0.34f);
	const FLinearColor GTColWhite(0.92f, 0.92f, 0.92f);
	const FLinearColor GTColDim(0.60f, 0.60f, 0.62f);
	const FLinearColor GTColGreen(0.50f, 0.88f, 0.52f);
	const FLinearColor GTColRed(0.90f, 0.42f, 0.40f);

	FSlateFontInfo GTFont(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle("Regular", Size);
	}

	// 装备效果短文案(从目录字段拼, 每件装备恰好一项)。
	FString EquipEffectText(const FGT_EquipDef& Def)
	{
		if (Def.BonusHP > 0) { return FString::Printf(TEXT("最大生命 +%d"), Def.BonusHP); }
		if (Def.BonusPower > 0) { return FString::Printf(TEXT("战斗力 +%d"), Def.BonusPower); }
		if (Def.bMineImmunity) { return TEXT("首次踩雷免疫"); }
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
	// whetstone / insulated_gloves 暂无专属图(待补), 返回空只显文字。
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

UButton* UGT_DeployTerminalWidget::MakeNavButton(UHorizontalBox* Row, const FString& Label)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>();
	UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
	Txt->SetText(FText::FromString(Label));
	Txt->SetFont(GTFont(15));
	Txt->SetColorAndOpacity(FSlateColor(GTColWhite));
	Btn->SetContent(Txt);
	if (UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(Btn))
	{
		HSlot->SetPadding(FMargin(2.f, 0.f));
	}
	return Btn;
}

UButton* UGT_DeployTerminalWidget::MakeFooterButton(UHorizontalBox* Row, const FString& Label, const FString& AssetPath)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>();
	if (UTexture2D* Tex = LoadUiTex(AssetPath))
	{
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.SetResourceObject(Tex);
		Style.Hovered.SetResourceObject(Tex);
		Style.Pressed.SetResourceObject(Tex);
		Btn->SetStyle(Style);
	}
	UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
	Txt->SetText(FText::FromString(Label));
	Txt->SetFont(GTFont(18));
	Txt->SetColorAndOpacity(FSlateColor(GTColGold));
	Btn->SetContent(Txt);
	if (UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(Btn))
	{
		HSlot->SetPadding(FMargin(6.f, 0.f));
	}
	return Btn;
}

void UGT_DeployTerminalWidget::BuildWidgetTree()
{
	// 根 = 暗底 Border 罩满(P1 用纯色终端底, 视觉打磨待 PIE 反馈)。
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DeployRoot"));
	Root->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.98f));
	Root->SetPadding(FMargin(36.f, 28.f));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->SetContent(Main);

	// --- 顶栏: 标题 + 弹簧 + 金币 ---
	UHorizontalBox* TopBar = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(TopBar)) { S->SetPadding(FMargin(0, 0, 0, 12)); }
	TitleText = WidgetTree->ConstructWidget<UTextBlock>();
	TitleText->SetText(FText::FromString(TEXT("灰尾回收 · 部署终端")));
	TitleText->SetFont(GTFont(24));
	TitleText->SetColorAndOpacity(FSlateColor(GTColGold));
	TopBar->AddChildToHorizontalBox(TitleText);
	USpacer* TopSpacer = WidgetTree->ConstructWidget<USpacer>();
	if (UHorizontalBoxSlot* S = TopBar->AddChildToHorizontalBox(TopSpacer)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
	if (UTexture2D* GoldIcon = LoadUiTex(TEXT("/Game/Graytail/UI/common/ui_icon_account_gold")))
	{
		UImage* Img = WidgetTree->ConstructWidget<UImage>();
		Img->SetBrushFromTexture(GoldIcon);
		Img->SetDesiredSizeOverride(FVector2D(26.f, 26.f));
		if (UHorizontalBoxSlot* S = TopBar->AddChildToHorizontalBox(Img)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0, 0, 6, 0)); }
	}
	GoldText = WidgetTree->ConstructWidget<UTextBlock>();
	GoldText->SetFont(GTFont(20));
	GoldText->SetColorAndOpacity(FSlateColor(GTColGold));
	if (UHorizontalBoxSlot* S = TopBar->AddChildToHorizontalBox(GoldText)) { S->SetVerticalAlignment(VAlign_Center); }

	// --- 导航栏 ---
	UHorizontalBox* Nav = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(Nav)) { S->SetPadding(FMargin(0, 0, 0, 6)); }
	MakeNavButton(Nav, TEXT("申领"))->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavRequisition);
	MakeNavButton(Nav, TEXT("作业装备"))->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavLoadout);
	MakeNavButton(Nav, TEXT("仓库"))->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavWarehouse);
	MakeNavButton(Nav, TEXT("天赋"))->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavTalent);
	MakeNavButton(Nav, TEXT("抢救记录"))->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavRecovery);

	HintText = WidgetTree->ConstructWidget<UTextBlock>();
	HintText->SetFont(GTFont(13));
	HintText->SetColorAndOpacity(FSlateColor(GTColDim));
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(HintText)) { S->SetPadding(FMargin(2, 0, 0, 8)); }

	// --- 主体: 内容滚动列表(填充) + 右侧摘要(固定宽) ---
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(Body)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }

	ContentScroll = WidgetTree->ConstructWidget<UScrollBox>();
	if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(ContentScroll)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetPadding(FMargin(0, 0, 12, 0)); }
	ContentList = WidgetTree->ConstructWidget<UVerticalBox>();
	ContentScroll->AddChild(ContentList);

	USizeBox* SummarySize = WidgetTree->ConstructWidget<USizeBox>();
	SummarySize->SetWidthOverride(280.f);
	Body->AddChildToHorizontalBox(SummarySize);
	UBorder* SummaryBorder = WidgetTree->ConstructWidget<UBorder>();
	SummaryBorder->SetBrushColor(FLinearColor(0.10f, 0.11f, 0.14f, 0.95f));
	SummaryBorder->SetPadding(FMargin(14.f));
	SummarySize->SetContent(SummaryBorder);
	SummaryBox = WidgetTree->ConstructWidget<UVerticalBox>();
	SummaryBorder->SetContent(SummaryBox);

	// --- 底栏: 返回 + 弹簧 + 出发探索 ---
	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(Footer)) { S->SetPadding(FMargin(0, 12, 0, 0)); }
	MakeFooterButton(Footer, TEXT("返回"), TEXT("/Game/Graytail/UI/deploy/ui_button_back_main"))
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnBackClicked);
	USpacer* FootSpacer = WidgetTree->ConstructWidget<USpacer>();
	if (UHorizontalBoxSlot* S = Footer->AddChildToHorizontalBox(FootSpacer)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
	MakeFooterButton(Footer, TEXT("出发探索 ▶"), TEXT("/Game/Graytail/UI/deploy/ui_button_confirm_deploy_large"))
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnDepartClicked);
}

void UGT_DeployTerminalWidget::AddItemRow(int32 Index, UTexture2D* Icon, const FString& Name, const FString& Effect,
	const FString& Right, bool bEnabled, bool bHighlight)
{
	if (!ContentList) { return; }

	UGT_IndexedButton* Btn = WidgetTree->ConstructWidget<UGT_IndexedButton>();
	Btn->Index = Index;
	Btn->SetIsEnabled(bEnabled);
	Btn->OnIndexClicked.AddDynamic(this, &UGT_DeployTerminalWidget::HandleRowClicked);

	UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>();
	Btn->SetContent(RowBox);

	// 图标(无图则留白占位)
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>();
	IconBox->SetWidthOverride(40.f);
	IconBox->SetHeightOverride(40.f);
	if (Icon)
	{
		UImage* Img = WidgetTree->ConstructWidget<UImage>();
		Img->SetBrushFromTexture(Icon);
		IconBox->SetContent(Img);
	}
	if (UHorizontalBoxSlot* S = RowBox->AddChildToHorizontalBox(IconBox)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0, 0, 10, 0)); }

	// 名称 + 效果
	UVerticalBox* NameCol = WidgetTree->ConstructWidget<UVerticalBox>();
	UTextBlock* NameTxt = WidgetTree->ConstructWidget<UTextBlock>();
	NameTxt->SetText(FText::FromString(bHighlight ? (TEXT("● ") + Name) : Name));
	NameTxt->SetFont(GTFont(17));
	NameTxt->SetColorAndOpacity(FSlateColor(bHighlight ? GTColGreen : GTColWhite));
	NameCol->AddChildToVerticalBox(NameTxt);
	UTextBlock* EffTxt = WidgetTree->ConstructWidget<UTextBlock>();
	EffTxt->SetText(FText::FromString(Effect));
	EffTxt->SetFont(GTFont(12));
	EffTxt->SetColorAndOpacity(FSlateColor(GTColDim));
	NameCol->AddChildToVerticalBox(EffTxt);
	if (UHorizontalBoxSlot* S = RowBox->AddChildToHorizontalBox(NameCol)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetVerticalAlignment(VAlign_Center); }

	// 右侧价/状态
	UTextBlock* RightTxt = WidgetTree->ConstructWidget<UTextBlock>();
	RightTxt->SetText(FText::FromString(Right));
	RightTxt->SetFont(GTFont(16));
	RightTxt->SetColorAndOpacity(FSlateColor(bEnabled ? GTColGold : GTColDim));
	if (UHorizontalBoxSlot* S = RowBox->AddChildToHorizontalBox(RightTxt)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(10, 0, 6, 0)); }

	if (UVerticalBoxSlot* S = ContentList->AddChildToVerticalBox(Btn)) { S->SetPadding(FMargin(0, 3)); }
}

void UGT_DeployTerminalWidget::RebuildContent()
{
	if (!ContentList) { return; }
	ContentList->ClearChildren();
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
			const FString Right = bOwned ? TEXT("已拥有") : FString::Printf(TEXT("%d"), Def.Price);
			AddItemRow(CurrentRows.Num(), IconForEquip(Def.Id), Def.DisplayName, EquipEffectText(Def),
				Right, !bOwned && bAfford, false);
			CurrentRows.Add({ Def.Id, false });
		}
		for (const FGT_ConsumableDef& Def : GT_MetaCatalog::GetConsumableDefs())
		{
			const bool bAfford = Gold >= Def.Price;
			const int32 Have = Meta->GetConsumableCount(Def.Id);
			const FString Eff = FString::Printf(TEXT("回复 %d 生命 · 持有 %d"), Def.Heal, Have);
			AddItemRow(CurrentRows.Num(), IconForConsumable(Def.Id), Def.DisplayName, Eff,
				FString::Printf(TEXT("%d"), Def.Price), bAfford, false);
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
			AddItemRow(CurrentRows.Num(), IconForEquip(Def.Id), Def.DisplayName, EquipEffectText(Def),
				bEquipped ? TEXT("已装备") : TEXT("点击装备"), true, bEquipped);
			CurrentRows.Add({ Def.Id, false });
		}
		for (const FGT_ConsumableDef& Def : GT_MetaCatalog::GetConsumableDefs())
		{
			const int32 Stock = Meta->GetConsumableCount(Def.Id);
			if (Stock <= 0) { continue; }
			bAny = true;
			const int32 Carry = Meta->GetLoadout().FindRef(Def.Id);
			const FString Eff = FString::Printf(TEXT("回复 %d 生命 · 库存 %d"), Def.Heal, Stock);
			AddItemRow(CurrentRows.Num(), IconForConsumable(Def.Id), Def.DisplayName, Eff,
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
		if (UVerticalBoxSlot* S = SummaryBox->AddChildToVerticalBox(T)) { S->SetPadding(FMargin(0, 2)); }
	};

	AddLine(TEXT("作业摘要"), 18, GTColGold);
	AddLine(TEXT("── 已装备 ──"), 13, GTColDim);
	const TArray<FName>& Equipped = Meta->GetEquippedItems();
	if (Equipped.Num() == 0) { AddLine(TEXT("(未配置)"), 14, GTColDim); }
	for (const FName& Id : Equipped)
	{
		const FGT_EquipDef* Def = GT_MetaCatalog::FindEquip(Id);
		AddLine(FString::Printf(TEXT("· %s"), Def ? Def->DisplayName : *Id.ToString()), 14, GTColWhite);
	}

	AddLine(TEXT("── 本局加成 ──"), 13, GTColDim);
	const FGT_EquipBonus B = Meta->GetEquipBonus();
	const FGT_TalentEffects T = Meta->GetTalentEffects();
	bool bAnyBonus = false;
	if (B.BonusHP > 0) { AddLine(FString::Printf(TEXT("生命 +%d"), B.BonusHP), 14, GTColGreen); bAnyBonus = true; }
	if (B.BonusPower > 0) { AddLine(FString::Printf(TEXT("战斗力 +%d"), B.BonusPower), 14, GTColGreen); bAnyBonus = true; }
	const int32 MineRed = B.MineDmgReduce + T.MineDmgReduce;
	if (MineRed > 0) { AddLine(FString::Printf(TEXT("雷险伤害 -%d"), MineRed), 14, GTColGreen); bAnyBonus = true; }
	if (B.bMineImmunity) { AddLine(TEXT("首次踩雷免疫"), 14, GTColGreen); bAnyBonus = true; }
	if (B.SearchBonus > 0) { AddLine(FString::Printf(TEXT("搜索奖励 +%d%%"), B.SearchBonus), 14, GTColGreen); bAnyBonus = true; }
	if (B.bShowExitHint) { AddLine(TEXT("撤离信标提示"), 14, GTColGreen); bAnyBonus = true; }
	if (!bAnyBonus) { AddLine(TEXT("(无)"), 14, GTColDim); }

	AddLine(TEXT("── 带入消耗品 ──"), 13, GTColDim);
	const TMap<FName, int32>& Loadout = Meta->GetLoadout();
	if (Loadout.Num() == 0) { AddLine(TEXT("(无)"), 14, GTColDim); }
	for (const TPair<FName, int32>& Pair : Loadout)
	{
		const FGT_ConsumableDef* Def = GT_MetaCatalog::FindConsumable(Pair.Key);
		AddLine(FString::Printf(TEXT("· %s ×%d"), Def ? Def->DisplayName : *Pair.Key.ToString(), Pair.Value), 14, GTColWhite);
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
			const int32 Next = (Cur >= Cap) ? 0 : Cur + 1;  // 点击循环 0..Cap
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
