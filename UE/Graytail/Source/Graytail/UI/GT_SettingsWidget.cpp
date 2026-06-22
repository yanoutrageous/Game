#include "UI/GT_SettingsWidget.h"

#include "Debug/GT_DebugSubsystem.h"
#include "UI/GT_UIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

namespace
{
	const FLinearColor GTSetBackdrop(0.f, 0.f, 0.f, 0.72f);
	const FLinearColor GTSetCardBg(FColor(26, 30, 40, 250));
	const FLinearColor GTSetCardEdge(FColor(120, 150, 200, 255));
	const FLinearColor GTSetTitle(FColor(255, 226, 150));
	const FLinearColor GTSetText(FColor(222, 228, 240));
	const FLinearColor GTSetOn(FColor(150, 225, 160));
	const FLinearColor GTSetOff(FColor(170, 178, 190));

	constexpr float GTSetCardWidth = 380.f;
}

UGT_DebugSubsystem* UGT_SettingsWidget::GetDebugSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UGT_DebugSubsystem>() : nullptr;
}

TSharedRef<SWidget> UGT_SettingsWidget::RebuildWidget()
{
	if (!Root)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UGT_SettingsWidget::BuildWidgetTree()
{
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Backdrop->SetBrushColor(GTSetBackdrop);
	if (UCanvasPanelSlot* BackdropSlot = Cast<UCanvasPanelSlot>(Root->AddChild(Backdrop)))
	{
		BackdropSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackdropSlot->SetOffsets(FMargin(0.f));
	}

	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CardSize->SetWidthOverride(GTSetCardWidth);
	if (UCanvasPanelSlot* CardSlot = Cast<UCanvasPanelSlot>(Root->AddChild(CardSize)))
	{
		CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CardSlot->SetAutoSize(true);
	}

	UBorder* CardEdge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CardEdge->SetBrushColor(GTSetCardEdge);
	CardEdge->SetPadding(FMargin(2.f));
	CardSize->SetContent(CardEdge);

	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card->SetBrushColor(GTSetCardBg);
	Card->SetPadding(FMargin(24.f, 20.f, 24.f, 20.f));
	CardEdge->SetContent(Card);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Column);

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetFont(GT_UIStyle::Font(20));
	TitleText->SetColorAndOpacity(FSlateColor(GTSetTitle));
	TitleText->SetJustification(ETextJustify::Center);
	TitleText->SetText(FText::FromString(TEXT("设置")));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	// 作弊模式开关(单按钮, 文字反映当前状态)。
	UButton* CheatButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CheatButton->SetStyle(GT_UIStyle::DarkButton());
	CheatToggleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CheatToggleText->SetFont(GT_UIStyle::Font(16));
	CheatToggleText->SetJustification(ETextJustify::Center);
	CheatButton->AddChild(CheatToggleText);
	CheatButton->OnClicked.AddDynamic(this, &UGT_SettingsWidget::HandleToggleCheat);
	if (UVerticalBoxSlot* CheatSlot = Column->AddChildToVerticalBox(CheatButton))
	{
		CheatSlot->SetPadding(FMargin(0.f, 5.f, 0.f, 5.f));
		CheatSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	UTextBlock* HintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	HintText->SetFont(GT_UIStyle::Font(11));
	HintText->SetColorAndOpacity(FSlateColor(GTSetOff));
	HintText->SetJustification(ETextJustify::Center);
	HintText->SetAutoWrapText(true);
	HintText->SetText(FText::FromString(TEXT("开启后, 局内按 ESC / = 暂停菜单里会出现「作弊面板」。")));
	if (UVerticalBoxSlot* HintSlot = Column->AddChildToVerticalBox(HintText))
	{
		HintSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 12.f));
	}

	UButton* BackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	BackButton->SetStyle(GT_UIStyle::DarkButton());
	UTextBlock* BackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BackText->SetFont(GT_UIStyle::Font(16));
	BackText->SetColorAndOpacity(FSlateColor(GTSetText));
	BackText->SetJustification(ETextJustify::Center);
	BackText->SetText(FText::FromString(TEXT("返回")));
	BackButton->AddChild(BackText);
	BackButton->OnClicked.AddDynamic(this, &UGT_SettingsWidget::HandleBack);
	if (UVerticalBoxSlot* BackSlot = Column->AddChildToVerticalBox(BackButton))
	{
		BackSlot->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
		BackSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UGT_SettingsWidget::RefreshCheatLabel()
{
	if (!CheatToggleText)
	{
		return;
	}
	const UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	const bool bOn = Debug && Debug->IsCheatModeEnabled();
	CheatToggleText->SetText(FText::FromString(bOn ? TEXT("作弊模式: 开") : TEXT("作弊模式: 关")));
	CheatToggleText->SetColorAndOpacity(FSlateColor(bOn ? GTSetOn : GTSetOff));
}

void UGT_SettingsWidget::Open()
{
	if (!Root)
	{
		return;
	}
	RefreshCheatLabel();
	SetVisibility(ESlateVisibility::Visible);
	SetKeyboardFocus();
}

void UGT_SettingsWidget::Close()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UGT_SettingsWidget::IsOpen() const
{
	return GetVisibility() != ESlateVisibility::Collapsed;
}

void UGT_SettingsWidget::HandleToggleCheat()
{
	if (UGT_DebugSubsystem* Debug = GetDebugSubsystem())
	{
		Debug->SetCheatModeEnabled(!Debug->IsCheatModeEnabled());
	}
	RefreshCheatLabel();
}

void UGT_SettingsWidget::HandleBack()
{
	OnBackRequested.ExecuteIfBound();
}

FReply UGT_SettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		HandleBack();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
