#include "UI/GT_TutorialPopupWidget.h"

#include "UI/GT_TutorialContent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	const FLinearColor GTTutorialBackdrop(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor GTTutorialCardBg(FColor(28, 32, 44, 248));
	const FLinearColor GTTutorialCardEdge(FColor(120, 150, 200, 255));
	const FLinearColor GTTutorialTitle(FColor(255, 226, 150));
	const FLinearColor GTTutorialBody(FColor(222, 228, 240));
	const FLinearColor GTTutorialHint(FColor(150, 190, 235, 235));

	constexpr float GTTutorialCardWidth = 520.f;
}

TSharedRef<SWidget> UGT_TutorialPopupWidget::RebuildWidget()
{
	if (!Root)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UGT_TutorialPopupWidget::BuildWidgetTree()
{
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	// 暗化背景(仅 blocking 形态可见, 吃点击)。
	Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Backdrop->SetBrushColor(GTTutorialBackdrop);
	if (UCanvasPanelSlot* BackdropSlot = Cast<UCanvasPanelSlot>(Root->AddChild(Backdrop)))
	{
		BackdropSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackdropSlot->SetOffsets(FMargin(0.f));
	}
	Backdrop->SetVisibility(ESlateVisibility::Collapsed);

	// 卡片: 固定宽度, 高度随内容。外层 Border 上色描边 + 内层 Border 底色。
	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CardSize->SetWidthOverride(GTTutorialCardWidth);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Root->AddChild(CardSize)))
	{
		CardSlot = CanvasSlot;
		CardSlot->SetAutoSize(true);
	}

	UBorder* CardEdge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CardEdge->SetBrushColor(GTTutorialCardEdge);
	CardEdge->SetPadding(FMargin(2.f));
	CardSize->SetContent(CardEdge);

	Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card->SetBrushColor(GTTutorialCardBg);
	Card->SetPadding(FMargin(20.f, 16.f, 20.f, 16.f));
	CardEdge->SetContent(Card);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Column);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 18));
	TitleText->SetColorAndOpacity(FSlateColor(GTTutorialTitle));
	Column->AddChildToVerticalBox(TitleText);

	BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BodyText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 13));
	BodyText->SetColorAndOpacity(FSlateColor(GTTutorialBody));
	BodyText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* BodySlot = Column->AddChildToVerticalBox(BodyText))
	{
		BodySlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	}

	ConfirmHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ConfirmHint->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 11));
	ConfirmHint->SetColorAndOpacity(FSlateColor(GTTutorialHint));
	if (UVerticalBoxSlot* HintSlot = Column->AddChildToVerticalBox(ConfirmHint))
	{
		HintSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UGT_TutorialPopupWidget::ShowPopup(const FGT_TutorialPopup& Popup)
{
	if (!Root)
	{
		return;
	}

	TitleText->SetText(FText::FromString(Popup.Title));
	BodyText->SetText(FText::FromString(Popup.Body));
	bBlockingActive = Popup.bBlocking;

	if (bBlockingActive)
	{
		// 模态: 暗化背景 + 居中 + 不透明 + 抢焦点(暂停 RoomView 移动)。
		Backdrop->SetVisibility(ESlateVisibility::Visible);
		ConfirmHint->SetVisibility(ESlateVisibility::HitTestInvisible);
		ConfirmHint->SetText(FText::FromString(FString::Printf(
			TEXT("%s    (Enter / 空格 / 点击 继续)"),
			Popup.ConfirmText.IsEmpty() ? TEXT("继续") : *Popup.ConfirmText)));
		if (CardSlot)
		{
			CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CardSlot->SetPosition(FVector2D::ZeroVector);
		}
		SetRenderOpacity(1.f);
		SetVisibility(ESlateVisibility::Visible);
		SetKeyboardFocus();
	}
	else
	{
		// 提示条: 对齐房间中心、贴底上方、80% 不透明、点击关闭; 不抢键盘焦点(WASD 照常), 离房由 HUD 调 Hide()。
		Backdrop->SetVisibility(ESlateVisibility::Collapsed);
		ConfirmHint->SetVisibility(ESlateVisibility::HitTestInvisible);
		ConfirmHint->SetText(FText::FromString(TEXT("(点击关闭)")));
		if (CardSlot)
		{
			// 横向对齐房间中心: 屏幕中心 + 左面板半宽(HUD 左面板固定 360 → +180), 与"周围雷险"标牌同一横轴。
			// 纵向钉在底部上方 118px(在雷险牌+底部键位栏之上), 卡片向上生长, 再高也不遮挡周围雷险。
			CardSlot->SetAnchors(FAnchors(0.5f, 1.f));
			CardSlot->SetAlignment(FVector2D(0.5f, 1.f));
			CardSlot->SetPosition(FVector2D(180.f, -118.f));
		}
		SetRenderOpacity(0.8f);
		SetVisibility(ESlateVisibility::Visible);   // 可被点击关闭; 不抢焦点(NativeSupportsKeyboardFocus=false)。
	}
}

void UGT_TutorialPopupWidget::Hide()
{
	bBlockingActive = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGT_TutorialPopupWidget::Confirm()
{
	// blocking=确认继续, 非blocking=点击关闭; 都交给 HUD 统一处理(记 once / Hide / 还焦点)。
	OnConfirmed.ExecuteIfBound();
}

FReply UGT_TutorialPopupWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bBlockingActive)
	{
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::F || Key == EKeys::Escape)
		{
			Confirm();
		}
		// 模态期间吞掉所有按键, 防 WASD 漏到游戏层。
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UGT_TutorialPopupWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 点击弹窗即关闭(blocking 与非blocking 都支持; 非blocking 不持焦点, 点击不会打断 WASD)。
	if (GetVisibility() != ESlateVisibility::Collapsed)
	{
		Confirm();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
