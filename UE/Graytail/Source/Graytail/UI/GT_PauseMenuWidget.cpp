#include "UI/GT_PauseMenuWidget.h"

#include "UI/GT_UIStyle.h"
#include "UI/GT_IndexedButton.h"
#include "Core/GT_RunContext.h"
#include "Core/GT_RunSubsystem.h"
#include "Debug/GT_DebugSubsystem.h"
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
	const FLinearColor GTPauseBackdrop(0.f, 0.f, 0.f, 0.72f);
	const FLinearColor GTPauseCardBg(FColor(26, 30, 40, 250));
	const FLinearColor GTPauseCardEdge(FColor(120, 150, 200, 255));
	const FLinearColor GTPauseTitle(FColor(255, 226, 150));
	const FLinearColor GTPauseBtnText(FColor(222, 228, 240));
	const FLinearColor GTPauseDanger(FColor(240, 150, 130));
	const FLinearColor GTPauseWarn(FColor(245, 200, 120));

	constexpr float GTPauseCardWidth = 360.f;
}

TSharedRef<SWidget> UGT_PauseMenuWidget::RebuildWidget()
{
	if (!Root)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

UButton* UGT_PauseMenuWidget::AddMenuButton(UVerticalBox* Box, const FString& Label, const FLinearColor& Tint)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(GT_UIStyle::Font(16));
	Text->SetColorAndOpacity(FSlateColor(Tint));
	Text->SetJustification(ETextJustify::Center);
	Text->SetText(FText::FromString(Label));
	Button->AddChild(Text);
	if (UVerticalBoxSlot* BtnSlot = Box->AddChildToVerticalBox(Button))
	{
		BtnSlot->SetPadding(FMargin(0.f, 5.f, 0.f, 5.f));
		BtnSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	return Button;
}

void UGT_PauseMenuWidget::BuildWidgetTree()
{
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	// 暗化背景(吃点击, 防穿透到房间)。
	Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Backdrop->SetBrushColor(GTPauseBackdrop);
	if (UCanvasPanelSlot* BackdropSlot = Cast<UCanvasPanelSlot>(Root->AddChild(Backdrop)))
	{
		BackdropSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackdropSlot->SetOffsets(FMargin(0.f));
	}

	// 居中卡片: 描边 + 底色。
	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CardSize->SetWidthOverride(GTPauseCardWidth);
	if (UCanvasPanelSlot* CardSlot = Cast<UCanvasPanelSlot>(Root->AddChild(CardSize)))
	{
		CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CardSlot->SetAutoSize(true);
	}

	UBorder* CardEdge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CardEdge->SetBrushColor(GTPauseCardEdge);
	CardEdge->SetPadding(FMargin(2.f));
	CardSize->SetContent(CardEdge);

	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card->SetBrushColor(GTPauseCardBg);
	Card->SetPadding(FMargin(24.f, 20.f, 24.f, 20.f));
	CardEdge->SetContent(Card);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Column);

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetFont(GT_UIStyle::Font(20));
	TitleText->SetColorAndOpacity(FSlateColor(GTPauseTitle));
	TitleText->SetJustification(ETextJustify::Center);
	TitleText->SetText(FText::FromString(TEXT("暂停")));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	// 主按钮组。
	MainButtons = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Column->AddChildToVerticalBox(MainButtons);

	UButton* ResumeButton = AddMenuButton(MainButtons, TEXT("继续"), GTPauseBtnText);
	ResumeButton->OnClicked.AddDynamic(this, &UGT_PauseMenuWidget::HandleResume);

	CheatButton = AddMenuButton(MainButtons, TEXT("作弊面板"), GTPauseWarn);
	CheatButton->OnClicked.AddDynamic(this, &UGT_PauseMenuWidget::HandleCheat);

	UButton* ReturnButton = AddMenuButton(MainButtons, TEXT("返回标题"), GTPauseBtnText);
	ReturnButton->OnClicked.AddDynamic(this, &UGT_PauseMenuWidget::HandleReturnClicked);

	UButton* QuitButton = AddMenuButton(MainButtons, TEXT("退出游戏"), GTPauseBtnText);
	QuitButton->OnClicked.AddDynamic(this, &UGT_PauseMenuWidget::HandleQuit);

	// 返回标题的二次确认组(默认隐藏)。
	ConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Column->AddChildToVerticalBox(ConfirmBox);

	UTextBlock* ConfirmText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ConfirmText->SetFont(GT_UIStyle::Font(13));
	ConfirmText->SetColorAndOpacity(FSlateColor(GTPauseDanger));
	ConfirmText->SetJustification(ETextJustify::Center);
	ConfirmText->SetAutoWrapText(true);
	ConfirmText->SetText(FText::FromString(TEXT("确认放弃本局? 待结算金币将丢失, 不结算。")));
	if (UVerticalBoxSlot* ConfirmTextSlot = ConfirmBox->AddChildToVerticalBox(ConfirmText))
	{
		ConfirmTextSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	UButton* ConfirmYes = AddMenuButton(ConfirmBox, TEXT("确认放弃"), GTPauseDanger);
	ConfirmYes->OnClicked.AddDynamic(this, &UGT_PauseMenuWidget::HandleReturnConfirm);

	UButton* ConfirmNo = AddMenuButton(ConfirmBox, TEXT("取消"), GTPauseBtnText);
	ConfirmNo->OnClicked.AddDynamic(this, &UGT_PauseMenuWidget::HandleReturnCancel);

	ConfirmBox->SetVisibility(ESlateVisibility::Collapsed);

	// 作弊面板子视图(作弊模式开启时由「作弊面板」按钮切入)。
	BuildCheatBox(Column);

	SetVisibility(ESlateVisibility::Collapsed);
}

UTextBlock* UGT_PauseMenuWidget::AddCheatButton(UVerticalBox* Box, const FString& Label, int32 Index, const FLinearColor& Tint)
{
	UGT_IndexedButton* Button = WidgetTree->ConstructWidget<UGT_IndexedButton>(UGT_IndexedButton::StaticClass());
	Button->Index = Index;
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(GT_UIStyle::Font(14));
	Text->SetColorAndOpacity(FSlateColor(Tint));
	Text->SetJustification(ETextJustify::Center);
	Text->SetText(FText::FromString(Label));
	Button->AddChild(Text);
	Button->OnIndexClicked.AddDynamic(this, &UGT_PauseMenuWidget::HandleCheatIndex);
	if (UVerticalBoxSlot* BtnSlot = Box->AddChildToVerticalBox(Button))
	{
		BtnSlot->SetPadding(FMargin(0.f, 3.f, 0.f, 3.f));
		BtnSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	return Text;
}

void UGT_PauseMenuWidget::BuildCheatBox(UVerticalBox* Column)
{
	CheatBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Column->AddChildToVerticalBox(CheatBox);

	// 索引约定: 0=无敌切换; 1..7=进房型; 8=+金币; 9=给止血贴; 10=满血; 11=残血。
	CheatGodText = AddCheatButton(CheatBox, TEXT("无敌: 关"), 0, GTPauseWarn);
	AddCheatButton(CheatBox, TEXT("进宝箱房"), 1, GTPauseBtnText);
	AddCheatButton(CheatBox, TEXT("进怪物房"), 2, GTPauseBtnText);
	AddCheatButton(CheatBox, TEXT("进旅商房"), 3, GTPauseBtnText);
	AddCheatButton(CheatBox, TEXT("进赌徒房"), 4, GTPauseBtnText);
	AddCheatButton(CheatBox, TEXT("进祭坛房"), 5, GTPauseBtnText);
	AddCheatButton(CheatBox, TEXT("进机关房"), 6, GTPauseBtnText);
	AddCheatButton(CheatBox, TEXT("去撤离点"), 7, GTPauseBtnText);
	AddCheatButton(CheatBox, TEXT("+100 金币"), 8, GTPauseWarn);
	AddCheatButton(CheatBox, TEXT("给止血贴 x1"), 9, GTPauseWarn);
	AddCheatButton(CheatBox, TEXT("满血"), 10, GTPauseWarn);
	AddCheatButton(CheatBox, TEXT("残血(1)"), 11, GTPauseWarn);

	UButton* CheatBack = AddMenuButton(CheatBox, TEXT("返回"), GTPauseBtnText);
	CheatBack->OnClicked.AddDynamic(this, &UGT_PauseMenuWidget::HandleCheatBack);

	CheatBox->SetVisibility(ESlateVisibility::Collapsed);
}

UGT_DebugSubsystem* UGT_PauseMenuWidget::GetDebug() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UGT_DebugSubsystem>() : nullptr;
}

bool UGT_PauseMenuWidget::ReadGodMode() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UGT_RunSubsystem* RunSubsystem = GameInstance ? GameInstance->GetSubsystem<UGT_RunSubsystem>() : nullptr;
	const UGT_RunContext* RunContext = RunSubsystem ? RunSubsystem->GetCurrentRunContext() : nullptr;
	return RunContext && RunContext->IsCheatGodMode();
}

void UGT_PauseMenuWidget::RefreshGodLabel()
{
	if (!CheatGodText)
	{
		return;
	}
	const bool bOn = ReadGodMode();
	CheatGodText->SetText(FText::FromString(bOn ? TEXT("无敌: 开") : TEXT("无敌: 关")));
	CheatGodText->SetColorAndOpacity(FSlateColor(bOn ? FLinearColor(FColor(150, 225, 160)) : GTPauseBtnText));
}

void UGT_PauseMenuWidget::ShowConfirmReturn(bool bShow)
{
	if (MainButtons)
	{
		MainButtons->SetVisibility(bShow ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (ConfirmBox)
	{
		ConfirmBox->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (CheatBox)
	{
		CheatBox->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UGT_PauseMenuWidget::ShowCheatView(bool bShow)
{
	if (MainButtons)
	{
		MainButtons->SetVisibility(bShow ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (ConfirmBox)
	{
		ConfirmBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (CheatBox)
	{
		CheatBox->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UGT_PauseMenuWidget::Open(bool bShowCheatEntry)
{
	if (!Root)
	{
		return;
	}
	bCheatEntryVisible = bShowCheatEntry;
	if (CheatButton)
	{
		CheatButton->SetVisibility(bShowCheatEntry ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	ShowConfirmReturn(false);
	SetVisibility(ESlateVisibility::Visible);
	SetKeyboardFocus();
}

void UGT_PauseMenuWidget::Close()
{
	ShowConfirmReturn(false);
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UGT_PauseMenuWidget::IsOpen() const
{
	return GetVisibility() != ESlateVisibility::Collapsed;
}

void UGT_PauseMenuWidget::HandleResume()
{
	OnResume.ExecuteIfBound();
}

void UGT_PauseMenuWidget::HandleCheat()
{
	// 作弊面板内置于本控件: 切到作弊子视图(同步无敌按钮标签)。
	RefreshGodLabel();
	ShowCheatView(true);
}

void UGT_PauseMenuWidget::HandleCheatBack()
{
	ShowConfirmReturn(false);   // 回暂停主视图
}

void UGT_PauseMenuWidget::HandleCheatIndex(int32 Index)
{
	UGT_DebugSubsystem* Debug = GetDebug();
	if (!Debug)
	{
		return;
	}
	FGT_DebugRunSnapshot Snapshot;
	bool bCloseAfter = false;
	switch (Index)
	{
	case 0:  Debug->DebugSetGodMode(!ReadGodMode(), Snapshot); RefreshGodLabel(); break;
	case 1:  Debug->DebugGotoRoomType(TEXT("chest"),  Snapshot); bCloseAfter = true; break;
	case 2:  Debug->DebugGotoRoomType(TEXT("combat"), Snapshot); bCloseAfter = true; break;
	case 3:  Debug->DebugGotoRoomType(TEXT("trader"), Snapshot); bCloseAfter = true; break;
	case 4:  Debug->DebugGotoRoomType(TEXT("dice"),   Snapshot); bCloseAfter = true; break;
	case 5:  Debug->DebugGotoRoomType(TEXT("altar"),  Snapshot); bCloseAfter = true; break;
	case 6:  Debug->DebugGotoRoomType(TEXT("trap"),   Snapshot); bCloseAfter = true; break;
	case 7:  Debug->DebugGotoRoomType(TEXT("exit"),   Snapshot); bCloseAfter = true; break;
	case 8:  Debug->DebugAddGold(100, Snapshot); break;
	case 9:  Debug->DebugGiveItem(FName(TEXT("emergency_bandage")), 1, Snapshot); break;
	case 10: Debug->DebugSetHp(9999, Snapshot); break;
	case 11: Debug->DebugSetHp(1, Snapshot); break;
	default: return;
	}
	OnCheatApplied.ExecuteIfBound();   // 请 HUD 整体刷新(金币/血量/背包/房间)
	if (bCloseAfter)
	{
		OnResume.ExecuteIfBound();      // 进房型后关菜单, 直接看到房间
	}
}

void UGT_PauseMenuWidget::HandleReturnClicked()
{
	ShowConfirmReturn(true);
}

void UGT_PauseMenuWidget::HandleReturnConfirm()
{
	OnReturnToTitle.ExecuteIfBound();
}

void UGT_PauseMenuWidget::HandleReturnCancel()
{
	ShowConfirmReturn(false);
}

void UGT_PauseMenuWidget::HandleQuit()
{
	OnQuitGame.ExecuteIfBound();
}

FReply UGT_PauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	// ESC / = : 确认视图下当作取消, 否则当作继续(关菜单)。吞键防漏到游戏层。
	if (Key == EKeys::Escape || Key == EKeys::Equals)
	{
		if (ConfirmBox && ConfirmBox->GetVisibility() != ESlateVisibility::Collapsed)
		{
			ShowConfirmReturn(false);
		}
		else
		{
			HandleResume();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
