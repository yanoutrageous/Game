#include "UI/GT_LootResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Domains/Inventory/GT_ItemCatalog.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Events.h"
#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"
#include "UI/GT_UIStyle.h"

namespace
{
	// 对齐 Lua ITEM_RARITY_COLORS。
	FLinearColor GTRarityColor(FName Rarity)
	{
		if (Rarity == FName(TEXT("uncommon"))) { return FLinearColor(FColor(120, 220, 170)); }
		if (Rarity == FName(TEXT("rare"))) { return FLinearColor(FColor(115, 180, 255)); }
		return FLinearColor(FColor(180, 200, 210)); // common
	}

	// 对齐 Lua ITEM_DEFS 的 typeName / rarityName 文案。
	const TCHAR* GTKindLabel(EGT_ItemKind Kind)
	{
		return Kind == EGT_ItemKind::Consumable ? TEXT("作业消耗品") : TEXT("异常回收物");
	}

	const TCHAR* GTRarityLabel(FName Rarity)
	{
		return Rarity == FName(TEXT("common")) ? TEXT("一般") : TEXT("稀有");
	}
}

TSharedRef<SWidget> UGT_LootResultWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UGT_LootResultWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UGT_LootResultWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// 模态弹窗"粘焦点": 只要可见就保持键盘焦点。搜索后 RefreshAll 会把焦点抢回房间,
	// 同帧 SetFocus 偶发不生效, 导致 F/Enter/Esc 关不掉; 这里每帧兜底夺回。
	if (GetVisibility() == ESlateVisibility::Visible && !HasKeyboardFocus())
	{
		SetFocus();
	}
}

void UGT_LootResultWidget::BuildWidgetTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	// 暗色遮罩(对齐 Lua nvgRGBA(0,0,0,150))。
	UBorder* Mask = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Mask->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.59f));
	if (UOverlaySlot* MaskSlot = Root->AddChildToOverlay(Mask))
	{
		MaskSlot->SetHorizontalAlignment(HAlign_Fill);
		MaskSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// 居中面板: 外层描边(宝箱金框/普通青框, Open 时着色) + 内层深底。
	PanelFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PanelFrame->SetPadding(FMargin(2.f));
	if (UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelFrame))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
	}

	UBorder* PanelBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PanelBg->SetBrushColor(FLinearColor(FColor(17, 23, 32, 242)));
	PanelBg->SetPadding(FMargin(26.f, 20.f));
	PanelFrame->SetContent(PanelBg);

	USizeBox* PanelWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelWidth->SetWidthOverride(640.f);
	PanelBg->SetContent(PanelWidth);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PanelWidth->SetContent(Column);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetFont(GT_UIStyle::Font(22));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(242, 246, 235))));
	Column->AddChildToVerticalBox(TitleText);

	SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SubtitleText->SetFont(GT_UIStyle::Font(12));
	SubtitleText->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(180, 205, 210, 230))));
	if (UVerticalBoxSlot* SubtitleSlot = Column->AddChildToVerticalBox(SubtitleText))
	{
		SubtitleSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}

	SummaryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SummaryText->SetFont(GT_UIStyle::Font(13));
	SummaryText->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(255, 226, 120, 245))));
	if (UVerticalBoxSlot* SummarySlot = Column->AddChildToVerticalBox(SummaryText))
	{
		SummarySlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}

	ItemsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* ItemsSlot = Column->AddChildToVerticalBox(ItemsBox))
	{
		ItemsSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
	}

	UTextBlock* Footer = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Footer->SetFont(GT_UIStyle::Font(11));
	Footer->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(155, 170, 180, 220))));
	Footer->SetJustification(ETextJustify::Right);
	Footer->SetText(FText::FromString(TEXT("Enter / F / Esc 确认放入临时回收包")));
	if (UVerticalBoxSlot* FooterSlot = Column->AddChildToVerticalBox(Footer))
	{
		FooterSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
		FooterSlot->SetHorizontalAlignment(HAlign_Right);
	}
}

UTexture2D* UGT_LootResultWidget::LoadUiTexture(const FString& AssetPath)
{
	if (UTexture2D** Cached = UiTextureCache.Find(AssetPath))
	{
		return *Cached;
	}

	const FString ObjectPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("GT_LootResultWidget: missing texture asset %s"), *AssetPath);
	}
	UiTextureCache.Add(AssetPath, Texture);
	return Texture;
}

void UGT_LootResultWidget::Open(const FGT_SearchOutcome& Outcome)
{
	RebuildContent(Outcome);
	SetVisibility(ESlateVisibility::Visible);
	SetFocus();
}

void UGT_LootResultWidget::Close()
{
	SetVisibility(ESlateVisibility::Collapsed);
	if (OnClosed.IsBound())
	{
		OnClosed.Execute();
	}
}

void UGT_LootResultWidget::RebuildContent(const FGT_SearchOutcome& Outcome)
{
	const FGT_SearchReward& Reward = Outcome.Reward;

	// 标题/副标题/描边色按是否宝箱切换(对齐 OpenLootResultPanel)。
	TitleText->SetText(FText::FromString(Reward.bIsChest ? TEXT("未登记物资箱") : TEXT("搜索结果")));
	SubtitleText->SetText(FText::FromString(Reward.bIsChest
		? TEXT("高价值物资已放入临时回收包")
		: TEXT("可回收物已放入临时回收包")));
	SubtitleText->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(180, 205, 210, 230))));
	PanelFrame->SetBrushColor(Reward.bIsChest
		? FLinearColor(FColor(240, 190, 90, 220))
		: FLinearColor(FColor(90, 170, 190, 210)));

	const int32 ItemValue = GT_ItemCatalog::GetCarriedItemsValue(Reward.Items);
	SummaryText->SetText(FText::FromString(FString::Printf(
		TEXT("结算币 +%d    回收物 %d 件    估值 +%d"), Reward.Gold, Reward.Parts, ItemValue)));

	ItemsBox->ClearChildren();
	if (Reward.Items.Num() == 0)
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Empty->SetFont(GT_UIStyle::Font(14));
		Empty->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(180, 190, 200, 220))));
		Empty->SetText(FText::FromString(TEXT("未发现可携带回收物。")));
		ItemsBox->AddChildToVerticalBox(Empty);
		return;
	}

	for (const FGT_ItemStack& Stack : Reward.Items)
	{
		AddItemCard(Stack);
	}
}

void UGT_LootResultWidget::AddItemCard(const FGT_ItemStack& Stack)
{
	const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(Stack.ItemId);
	if (!Def)
	{
		return;
	}
	const FLinearColor RarityColor = GTRarityColor(Def->Rarity);

	// 卡片: 稀有度描边 + 深底(对齐 drawLootItemCard)。
	UBorder* CardFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CardFrame->SetBrushColor(FLinearColor(RarityColor.R, RarityColor.G, RarityColor.B, 0.75f));
	CardFrame->SetPadding(FMargin(1.5f));
	if (UVerticalBoxSlot* CardSlot = ItemsBox->AddChildToVerticalBox(CardFrame))
	{
		CardSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}

	UBorder* CardBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CardBg->SetBrushColor(FLinearColor(FColor(24, 31, 42, 230)));
	CardBg->SetPadding(FMargin(14.f, 14.f));
	CardFrame->SetContent(CardBg);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	CardBg->SetContent(Row);

	// 图标: 稀有度淡色底 + 物品图(逐物品映射在 GT_ItemCatalog, 与 HUD 共用)。
	USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconSize->SetWidthOverride(56.f);
	IconSize->SetHeightOverride(56.f);
	UBorder* IconBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	IconBg->SetBrushColor(FLinearColor(RarityColor.R, RarityColor.G, RarityColor.B, 0.19f));
	IconBg->SetPadding(FMargin(4.f));
	IconSize->SetContent(IconBg);
	if (UTexture2D* IconTexture = LoadUiTexture(GT_ItemCatalog::GetItemIconAssetPath(Stack.ItemId)))
	{
		UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		IconImage->SetBrushFromTexture(IconTexture);
		IconBg->SetContent(IconImage);
	}
	if (UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconSize))
	{
		IconSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		IconSlot->SetVerticalAlignment(VAlign_Center);
	}

	// 中列: 名称 xN / 类别·稀有度 / 效果 / 描述。
	UVerticalBox* TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(TextColumn))
	{
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	NameText->SetFont(GT_UIStyle::Font(16));
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(240, 245, 235))));
	NameText->SetText(FText::FromString(Stack.Count > 1
		? FString::Printf(TEXT("%s x%d"), *Def->DisplayName, Stack.Count)
		: Def->DisplayName));
	TextColumn->AddChildToVerticalBox(NameText);

	UTextBlock* KindText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	KindText->SetFont(GT_UIStyle::Font(11));
	KindText->SetColorAndOpacity(FSlateColor(RarityColor));
	KindText->SetText(FText::FromString(FString::Printf(
		TEXT("%s · %s"), GTKindLabel(Def->Kind), GTRarityLabel(Def->Rarity))));
	if (UVerticalBoxSlot* KindSlot = TextColumn->AddChildToVerticalBox(KindText))
	{
		KindSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	if (!Def->EffectText.IsEmpty())
	{
		UTextBlock* EffectTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		EffectTextBlock->SetFont(GT_UIStyle::Font(11));
		EffectTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(215, 230, 170, 230))));
		EffectTextBlock->SetText(FText::FromString(Def->EffectText));
		if (UVerticalBoxSlot* EffectSlot = TextColumn->AddChildToVerticalBox(EffectTextBlock))
		{
			EffectSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
	}

	UTextBlock* DescText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	DescText->SetFont(GT_UIStyle::Font(10));
	DescText->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(175, 188, 196, 220))));
	DescText->SetAutoWrapText(true);
	DescText->SetText(FText::FromString(Def->Description));
	if (UVerticalBoxSlot* DescSlot = TextColumn->AddChildToVerticalBox(DescText))
	{
		DescSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	// 右上: 估值。
	UTextBlock* ValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ValueText->SetFont(GT_UIStyle::Font(11));
	ValueText->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(255, 220, 120, 230))));
	ValueText->SetText(FText::FromString(FString::Printf(TEXT("估值 %d"), Def->Value * FMath::Max(1, Stack.Count))));
	if (UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(ValueText))
	{
		ValueSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		ValueSlot->SetVerticalAlignment(VAlign_Top);
	}
}

FReply UGT_LootResultWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 点击任意处 = 确认关闭。
	Close();
	return FReply::Handled();
}

FReply UGT_LootResultWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter || Key == EKeys::F || Key == EKeys::Escape)
	{
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
