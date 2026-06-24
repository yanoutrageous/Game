#include "UI/GT_EventPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/GT_RunContext.h"
#include "Core/GT_RunSubsystem.h"
#include "Debug/GT_DebugSubsystem.h"
#include "Engine/GameInstance.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Events.h"
#include "Styling/CoreStyle.h"
#include "UI/GT_UIStyle.h"

namespace
{
	const FName GTEventPanelOption_Leave(TEXT("leave"));
	const FLinearColor GTEventRowSelected(0.24f, 0.19f, 0.10f, 0.96f);   // 选中行加深加饱和, 让文字更跳
	const FLinearColor GTEventRowNormal(0.10f, 0.095f, 0.085f, 0.82f);
}

TSharedRef<SWidget> UGT_EventPanelWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UGT_EventPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// 模态弹窗"粘焦点": 执行选项后 OnStateChanged 触发 HUD RefreshAll 会把焦点抢回房间,
	// 面板还开着却收不到键(F/Enter/Esc 关不掉); 只要可见就每帧兜底夺回焦点(兑现 OnStateChanged 注释承诺)。
	if (GetVisibility() == ESlateVisibility::Visible && !HasKeyboardFocus())
	{
		SetFocus();
	}
}

void UGT_EventPanelWidget::BuildWidgetTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	// 半透明遮罩(模态), 点击空白处关闭。
	UBorder* Mask = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Mask->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.62f));
	if (UOverlaySlot* MaskSlot = Root->AddChildToOverlay(Mask))
	{
		MaskSlot->SetHorizontalAlignment(HAlign_Fill);
		MaskSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// 居中面板: 暗金描边 + 深底(对齐 Lua DrawEventPanel 的事件面板配色)。
	FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	FrameBorder->SetBrushColor(FLinearColor(FColor(150, 125, 80)));   // fallback 纯色(贴图缺失时保留)
	FrameBorder->SetPadding(FMargin(2.f));
	GT_UIStyle::SkinPanel9(FrameBorder, GT_UIStyle::PanelDialogSkin());   // 金属框换皮
	FrameBorder->SetBrushColor(FLinearColor(FColor(205, 178, 125)));  // 暖黄铜 tint: 保留"事件"暖框识别
	if (UOverlaySlot* FrameSlot = Root->AddChildToOverlay(FrameBorder))
	{
		FrameSlot->SetHorizontalAlignment(HAlign_Center);
		FrameSlot->SetVerticalAlignment(VAlign_Center);
	}

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Background->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));   // 透明: 透出金属框贴图
	Background->SetPadding(FMargin(28.f, 28.f, 28.f, 48.f));        // 底部加大避开框内阴影
	FrameBorder->SetContent(Background);

	USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WidthBox->SetWidthOverride(540.f);
	Background->SetContent(WidthBox);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	WidthBox->SetContent(Column);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetFont(GT_UIStyle::Font(20));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(240, 210, 140))));
	Column->AddChildToVerticalBox(TitleText);

	DescText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	DescText->SetFont(GT_UIStyle::Font(13));
	DescText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.76f, 0.70f, 1.f)));
	DescText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* DescSlot = Column->AddChildToVerticalBox(DescText))
	{
		DescSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}

	OptionsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* OptionsSlot = Column->AddChildToVerticalBox(OptionsBox))
	{
		OptionsSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
	}

	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MessageText->SetFont(GT_UIStyle::Font(13));
	MessageText->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(255, 186, 150))));
	MessageText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* MessageSlot = Column->AddChildToVerticalBox(MessageText))
	{
		MessageSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	}

	UTextBlock* Footer = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Footer->SetFont(GT_UIStyle::Font(11));
	Footer->SetColorAndOpacity(FSlateColor(FLinearColor(0.52f, 0.50f, 0.46f, 1.f)));
	Footer->SetText(FText::FromString(TEXT("W/S 选择 · Enter/T 确认 · Esc/右键 离开")));
	if (UVerticalBoxSlot* FooterSlot = Column->AddChildToVerticalBox(Footer))
	{
		FooterSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}
}

UGT_DebugSubsystem* UGT_EventPanelWidget::GetDebugSubsystem() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UGT_DebugSubsystem>() : nullptr;
}

const UGT_RunContext* UGT_EventPanelWidget::GetRunContext() const
{
	const UGT_RunSubsystem* RunSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGT_RunSubsystem>() : nullptr;
	return RunSubsystem ? RunSubsystem->GetCurrentRunContext() : nullptr;
}

bool UGT_EventPanelWidget::Open()
{
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	if (!Debug || !Debug->DebugGetEventMenu(Menu) || !Menu.bAvailable)
	{
		return false;
	}

	SelectedIndex = 0;
	MessageText->SetText(FText::GetEmpty());
	TitleText->SetText(FText::FromString(Menu.Title));
	DescText->SetText(FText::FromString(Menu.Description));
	RebuildOptionRows();
	SetVisibility(ESlateVisibility::Visible);
	SetFocus();
	return true;
}

void UGT_EventPanelWidget::Close()
{
	if (!IsOpen())
	{
		return;
	}
	SetVisibility(ESlateVisibility::Collapsed);
	if (OnClosed.IsBound())
	{
		OnClosed.Execute();
	}
}

bool UGT_EventPanelWidget::IsOpen() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

void UGT_EventPanelWidget::RefreshMenu()
{
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	if (!Debug || !Debug->DebugGetEventMenu(Menu) || !Menu.bAvailable)
	{
		Close();
		return;
	}
	SelectedIndex = FMath::Clamp(SelectedIndex, 0, FMath::Max(0, Menu.Options.Num() - 1));
	TitleText->SetText(FText::FromString(Menu.Title));
	DescText->SetText(FText::FromString(Menu.Description));
	RebuildOptionRows();
}

void UGT_EventPanelWidget::RebuildOptionRows()
{
	OptionsBox->ClearChildren();
	OptionRows.Reset();

	for (int32 Index = 0; Index < Menu.Options.Num(); ++Index)
	{
		const FGT_EventOptionView& Option = Menu.Options[Index];
		const bool bSelected = Index == SelectedIndex;

		UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Row->SetBrushColor(bSelected ? GTEventRowSelected : GTEventRowNormal);
		Row->SetPadding(FMargin(12.f, 8.f));
		if (UVerticalBoxSlot* RowSlot = OptionsBox->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
		OptionRows.Add(Row);

		UVerticalBox* RowColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Row->SetContent(RowColumn);

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetFont(GT_UIStyle::Font(15));
		Label->SetColorAndOpacity(FSlateColor(!Option.bEnabled
			? FLinearColor(FColor(182, 176, 162))
			: bSelected ? FLinearColor(FColor(255, 235, 170)) : FLinearColor(FColor(222, 218, 204))));
		Label->SetText(FText::FromString(Option.Label));
		RowColumn->AddChildToVerticalBox(Label);

		// 第二行: 可用选项给 代价/收益/风险 摘要, 不可用选项给原因。
		FString DetailLine;
		FLinearColor DetailColor(0.70f, 0.67f, 0.60f, 1.f);
		if (!Option.bEnabled && !Option.DisabledReason.IsEmpty())
		{
			DetailLine = Option.DisabledReason;
			DetailColor = FLinearColor(FColor(240, 162, 150));
		}
		else if (Option.CostText != TEXT("无") || Option.RewardText != TEXT("无") || Option.RiskText != TEXT("无"))
		{
			DetailLine = FString::Printf(TEXT("代价 %s · 收益 %s · 风险 %s"),
				*Option.CostText, *Option.RewardText, *Option.RiskText);
		}
		if (!DetailLine.IsEmpty())
		{
			UTextBlock* Detail = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Detail->SetFont(GT_UIStyle::Font(11));
			Detail->SetColorAndOpacity(FSlateColor(DetailColor));
			Detail->SetAutoWrapText(true);
			Detail->SetText(FText::FromString(DetailLine));
			if (UVerticalBoxSlot* DetailSlot = RowColumn->AddChildToVerticalBox(Detail))
			{
				DetailSlot->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
			}
		}
	}
}

void UGT_EventPanelWidget::MoveSelection(int32 Delta)
{
	const int32 Count = Menu.Options.Num();
	if (Count <= 0)
	{
		return;
	}
	// 循环选择(对齐 Lua 的 W/S 包绕)。
	SelectedIndex = (SelectedIndex + Delta % Count + Count) % Count;
	RebuildOptionRows();
}

void UGT_EventPanelWidget::ConfirmSelection()
{
	if (!Menu.Options.IsValidIndex(SelectedIndex))
	{
		return;
	}
	const FGT_EventOptionView Option = Menu.Options[SelectedIndex];

	if (!Option.bEnabled)
	{
		MessageText->SetText(FText::FromString(Option.DisabledReason.IsEmpty() ? TEXT("条件不足。") : Option.DisabledReason));
		return;
	}
	if (Option.OptionId == GTEventPanelOption_Leave)
	{
		Close();
		return;
	}

	// 执行走命令管线; 结果明细从内核 LastEventOutcome 读取。
	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	if (!Debug)
	{
		return;
	}
	FGT_DebugRunSnapshot Snapshot;
	Debug->DebugChooseEventOption(Option.OptionId, Snapshot);

	const UGT_RunContext* RunContext = GetRunContext();
	const FGT_EventOutcome Outcome = RunContext ? RunContext->GetLastEventOutcome() : FGT_EventOutcome();
	MessageText->SetText(FText::FromString(Outcome.Message));

	if (OnStateChanged.IsBound())
	{
		OnStateChanged.Execute();
	}

	// 局已结束(机关致死/压力满载) -> 关面板让位给局终结算。
	if (!RunContext || !RunContext->IsRunActive())
	{
		Close();
		return;
	}
	// 不在执行后立即关面板(原来 bClosePanel 类事件=机关/赌徒/旅商 会同帧关闭, 玩家看不到结果文案)。
	// 改为统一保留面板 + 按最新状态重建菜单: 已完成事件(机关/赌徒)菜单会变成只剩"关闭",
	// MessageText 显示判定结果(机关成功/失控、赌徒输赢), 玩家看清后再按键/Esc 关闭。祭坛连续献祭沿用此路径。
	RefreshMenu();
}

FReply UGT_EventPanelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::W || Key == EKeys::Up)
	{
		MoveSelection(-1);
		return FReply::Handled();
	}
	if (Key == EKeys::S || Key == EKeys::Down)
	{
		MoveSelection(1);
		return FReply::Handled();
	}
	if (Key == EKeys::Enter || Key == EKeys::T || Key == EKeys::F || Key == EKeys::SpaceBar)
	{
		ConfirmSelection();
		return FReply::Handled();
	}
	if (Key == EKeys::Escape)
	{
		Close();
		return FReply::Handled();
	}
	// 吞掉其余按键, 防止移动等输入穿透到房间。
	return FReply::Handled();
}

FReply UGT_EventPanelWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Close();
		return FReply::Handled();
	}

	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();

	// 点选项行 = 选中并执行(对齐原版鼠标操作)。
	for (int32 Index = 0; Index < OptionRows.Num(); ++Index)
	{
		if (OptionRows[Index] && OptionRows[Index]->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			SelectedIndex = Index;
			RebuildOptionRows();
			ConfirmSelection();
			return FReply::Handled();
		}
	}

	// 点面板外 = 关闭; 点面板内空白 = 拿回焦点。
	if (FrameBorder && !FrameBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition))
	{
		Close();
		return FReply::Handled();
	}
	SetFocus();
	return FReply::Handled();
}
