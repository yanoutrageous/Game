#include "UI/GT_SettingsWidget.h"

#include "Core/GT_SettingsSubsystem.h"
#include "Debug/GT_DebugSubsystem.h"
#include "UI/GT_UIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameUserSettings.h"

namespace
{
	const FLinearColor GTSetBackdrop(0.f, 0.f, 0.f, 0.72f);
	const FLinearColor GTSetCardBg(FColor(26, 30, 40, 250));
	const FLinearColor GTSetCardEdge(FColor(120, 150, 200, 255));
	const FLinearColor GTSetTitle(FColor(255, 226, 150));
	const FLinearColor GTSetHeader(FColor(150, 185, 235));
	const FLinearColor GTSetText(FColor(222, 228, 240));
	const FLinearColor GTSetValue(FColor(255, 226, 150));
	const FLinearColor GTSetOn(FColor(150, 225, 160));
	const FLinearColor GTSetOff(FColor(170, 178, 190));

	constexpr float GTSetCardWidth = 430.f;

	const TCHAR* GTWindowModeNames[] = { TEXT("全屏"), TEXT("无边框窗口"), TEXT("窗口") };

	// ── 行构件(纯构造期助手, 取 WidgetTree 直接建) ──
	UButton* MakeStepButton(UWidgetTree* Tree, const FString& Glyph)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetStyle(GT_UIStyle::DarkButton());
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetFont(GT_UIStyle::Font(15));
		Text->SetColorAndOpacity(FSlateColor(GTSetText));
		Text->SetJustification(ETextJustify::Center);
		Text->SetText(FText::FromString(Glyph));
		Button->AddChild(Text);
		return Button;
	}

	// [标签 .... ◀ 值 ▶] 一行; 返回 prev/next 按钮与值文本供调用方绑定/刷新。
	void AddCyclerRow(UWidgetTree* Tree, UVerticalBox* Column, const FString& Label,
		UButton*& OutPrev, UTextBlock*& OutValue, UButton*& OutNext)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UTextBlock* LabelText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		LabelText->SetFont(GT_UIStyle::Font(14));
		LabelText->SetColorAndOpacity(FSlateColor(GTSetText));
		LabelText->SetText(FText::FromString(Label));
		if (UHorizontalBoxSlot* LS = Row->AddChildToHorizontalBox(LabelText))
		{
			LS->SetVerticalAlignment(VAlign_Center);
			LS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		OutPrev = MakeStepButton(Tree, TEXT("<"));
		if (UHorizontalBoxSlot* PS = Row->AddChildToHorizontalBox(OutPrev)) { PS->SetVerticalAlignment(VAlign_Center); }

		OutValue = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		OutValue->SetFont(GT_UIStyle::Font(14));
		OutValue->SetColorAndOpacity(FSlateColor(GTSetValue));
		OutValue->SetJustification(ETextJustify::Center);
		USizeBox* ValBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ValBox->SetWidthOverride(130.f);
		ValBox->SetContent(OutValue);
		if (UHorizontalBoxSlot* VS = Row->AddChildToHorizontalBox(ValBox)) { VS->SetVerticalAlignment(VAlign_Center); }

		OutNext = MakeStepButton(Tree, TEXT(">"));
		if (UHorizontalBoxSlot* NS = Row->AddChildToHorizontalBox(OutNext)) { NS->SetVerticalAlignment(VAlign_Center); }

		if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 4.f));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	// 整宽状态按钮(文字反映开关状态), 返回按钮 + 文本。
	UButton* AddStateButton(UWidgetTree* Tree, UVerticalBox* Column, UTextBlock*& OutText)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetStyle(GT_UIStyle::DarkButton());
		OutText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		OutText->SetFont(GT_UIStyle::Font(15));
		OutText->SetJustification(ETextJustify::Center);
		Button->AddChild(OutText);
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Button))
		{
			Slot->SetPadding(FMargin(0.f, 4.f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
		return Button;
	}

	void AddSectionHeader(UWidgetTree* Tree, UVerticalBox* Column, const FString& Title)
	{
		UTextBlock* Header = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Header->SetFont(GT_UIStyle::Font(13));
		Header->SetColorAndOpacity(FSlateColor(GTSetHeader));
		Header->SetText(FText::FromString(Title));
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Header))
		{
			Slot->SetPadding(FMargin(0.f, 12.f, 0.f, 2.f));
		}
	}

	// [标签 ──滑条── 百分比] 一行; 返回滑条(调用方绑 OnValueChanged), 百分比文本经 out 回传。
	USlider* AddSliderRow(UWidgetTree* Tree, UVerticalBox* Column, const FString& Label, UTextBlock*& OutPercent)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UTextBlock* LabelText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		LabelText->SetFont(GT_UIStyle::Font(14));
		LabelText->SetColorAndOpacity(FSlateColor(GTSetText));
		LabelText->SetText(FText::FromString(Label));
		if (UHorizontalBoxSlot* LS = Row->AddChildToHorizontalBox(LabelText)) { LS->SetVerticalAlignment(VAlign_Center); }

		USlider* Slider = Tree->ConstructWidget<USlider>(USlider::StaticClass());
		Slider->SetMinValue(0.f);
		Slider->SetMaxValue(1.f);
		Slider->SetStepSize(0.05f);
		Slider->SetSliderBarColor(FLinearColor(0.30f, 0.34f, 0.42f));
		Slider->SetSliderHandleColor(FLinearColor(0.55f, 0.85f, 1.0f));
		if (UHorizontalBoxSlot* SS = Row->AddChildToHorizontalBox(Slider))
		{
			SS->SetVerticalAlignment(VAlign_Center);
			SS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SS->SetPadding(FMargin(10.f, 0.f));
		}

		OutPercent = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		OutPercent->SetFont(GT_UIStyle::Font(14));
		OutPercent->SetColorAndOpacity(FSlateColor(GTSetValue));
		OutPercent->SetJustification(ETextJustify::Right);
		USizeBox* PctBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PctBox->SetWidthOverride(52.f);
		PctBox->SetContent(OutPercent);
		if (UHorizontalBoxSlot* PS = Row->AddChildToHorizontalBox(PctBox)) { PS->SetVerticalAlignment(VAlign_Center); }

		if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 6.f));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		return Slider;
	}
}

UGT_DebugSubsystem* UGT_SettingsWidget::GetDebugSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UGT_DebugSubsystem>() : nullptr;
}

UGT_SettingsSubsystem* UGT_SettingsWidget::GetSettingsSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UGT_SettingsSubsystem>() : nullptr;
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
	Resolutions = { FIntPoint(1280, 720), FIntPoint(1366, 768), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440) };

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
	CardSize->SetMaxDesiredHeight(620.f);
	if (UCanvasPanelSlot* CardSlot = Cast<UCanvasPanelSlot>(Root->AddChild(CardSize)))
	{
		CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CardSlot->SetAutoSize(true);
	}

	UBorder* CardEdge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CardEdge->SetBrushColor(GTSetCardEdge);   // fallback 纯色(贴图缺失时保留)
	CardEdge->SetPadding(FMargin(2.f));
	GT_UIStyle::SkinPanel9(CardEdge, GT_UIStyle::PanelDialogSkin());   // 组员金属边框换皮
	CardSize->SetContent(CardEdge);

	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));   // 透明: 让 CardEdge 的边框贴图透出
	Card->SetPadding(FMargin(28.f, 24.f, 28.f, 24.f));        // 内缩避开金属框
	CardEdge->SetContent(Card);

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Card->SetContent(Scroll);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Column);

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetFont(GT_UIStyle::Font(20));
	TitleText->SetColorAndOpacity(FSlateColor(GTSetTitle));
	TitleText->SetJustification(ETextJustify::Center);
	TitleText->SetText(FText::FromString(TEXT("设置")));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	// ── 显示 ──
	AddSectionHeader(WidgetTree, Column, TEXT("显示"));
	{
		UButton* Prev = nullptr; UButton* Next = nullptr;
		AddCyclerRow(WidgetTree, Column, TEXT("显示模式"), Prev, WindowModeText, Next);
		Prev->OnClicked.AddDynamic(this, &UGT_SettingsWidget::HandleWindowModePrev);
		Next->OnClicked.AddDynamic(this, &UGT_SettingsWidget::HandleWindowModeNext);
	}
	{
		UButton* Prev = nullptr; UButton* Next = nullptr;
		AddCyclerRow(WidgetTree, Column, TEXT("分辨率"), Prev, ResolutionText, Next);
		Prev->OnClicked.AddDynamic(this, &UGT_SettingsWidget::HandleResolutionPrev);
		Next->OnClicked.AddDynamic(this, &UGT_SettingsWidget::HandleResolutionNext);
	}
	{
		UButton* VSyncButton = AddStateButton(WidgetTree, Column, VSyncText);
		VSyncButton->OnClicked.AddDynamic(this, &UGT_SettingsWidget::HandleVSyncToggle);
	}

	// ── 音频 ──
	AddSectionHeader(WidgetTree, Column, TEXT("音频"));
	MusicSlider = AddSliderRow(WidgetTree, Column, TEXT("主音量(BGM)"), MusicPercentText);
	MusicSlider->OnValueChanged.AddDynamic(this, &UGT_SettingsWidget::HandleMusicVolumeChanged);
	SfxSlider = AddSliderRow(WidgetTree, Column, TEXT("音效音量"), SfxPercentText);
	SfxSlider->OnValueChanged.AddDynamic(this, &UGT_SettingsWidget::HandleSfxVolumeChanged);

	// ── 其他(作弊) ──
	AddSectionHeader(WidgetTree, Column, TEXT("其他"));
	{
		UButton* CheatButton = AddStateButton(WidgetTree, Column, CheatToggleText);
		CheatButton->OnClicked.AddDynamic(this, &UGT_SettingsWidget::HandleToggleCheat);
	}
	UTextBlock* HintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	HintText->SetFont(GT_UIStyle::Font(11));
	HintText->SetColorAndOpacity(FSlateColor(GTSetOff));
	HintText->SetJustification(ETextJustify::Center);
	HintText->SetAutoWrapText(true);
	HintText->SetText(FText::FromString(TEXT("作弊开启后, 局内按 ESC / = 暂停菜单里会出现「作弊面板」。")));
	if (UVerticalBoxSlot* HintSlot = Column->AddChildToVerticalBox(HintText))
	{
		HintSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 10.f));
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
		BackSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		BackSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UGT_SettingsWidget::RefreshAll()
{
	RefreshDisplayLabels();
	RefreshMusicLabel();
	RefreshSfxLabel();
	RefreshCheatLabel();
}

void UGT_SettingsWidget::RefreshDisplayLabels()
{
	if (WindowModeText)
	{
		const int32 Idx = FMath::Clamp(WindowModeIndex, 0, 2);
		WindowModeText->SetText(FText::FromString(GTWindowModeNames[Idx]));
	}
	if (ResolutionText && Resolutions.IsValidIndex(ResolutionIndex))
	{
		const FIntPoint R = Resolutions[ResolutionIndex];
		ResolutionText->SetText(FText::FromString(FString::Printf(TEXT("%d×%d"), R.X, R.Y)));
	}
	if (VSyncText)
	{
		VSyncText->SetText(FText::FromString(bVSyncOn ? TEXT("垂直同步: 开") : TEXT("垂直同步: 关")));
		VSyncText->SetColorAndOpacity(FSlateColor(bVSyncOn ? GTSetOn : GTSetOff));
	}
}

void UGT_SettingsWidget::RefreshMusicLabel()
{
	if (!MusicPercentText)
	{
		return;
	}
	const UGT_SettingsSubsystem* Settings = GetSettingsSubsystem();
	const float Vol = Settings ? Settings->GetMusicVolume() : 0.5f;
	MusicPercentText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Vol * 100.f))));
}

void UGT_SettingsWidget::RefreshSfxLabel()
{
	if (!SfxPercentText)
	{
		return;
	}
	const UGT_SettingsSubsystem* Settings = GetSettingsSubsystem();
	const float Vol = Settings ? Settings->GetSfxVolume() : 0.7f;
	SfxPercentText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Vol * 100.f))));
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

void UGT_SettingsWidget::ApplyDisplaySettings()
{
	UGameUserSettings* GS = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!GS)
	{
		return;
	}
	EWindowMode::Type Mode = EWindowMode::Fullscreen;
	switch (WindowModeIndex)
	{
	case 1: Mode = EWindowMode::WindowedFullscreen; break;
	case 2: Mode = EWindowMode::Windowed; break;
	default: Mode = EWindowMode::Fullscreen; break;
	}
	GS->SetFullscreenMode(Mode);
	if (Resolutions.IsValidIndex(ResolutionIndex))
	{
		GS->SetScreenResolution(Resolutions[ResolutionIndex]);
	}
	GS->SetVSyncEnabled(bVSyncOn);
	GS->ApplyResolutionSettings(false);
	GS->SaveSettings();
}

void UGT_SettingsWidget::Open()
{
	if (!Root)
	{
		return;
	}
	// 读当前显示设置。
	if (UGameUserSettings* GS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		switch (GS->GetFullscreenMode())
		{
		case EWindowMode::WindowedFullscreen: WindowModeIndex = 1; break;
		case EWindowMode::Windowed: WindowModeIndex = 2; break;
		default: WindowModeIndex = 0; break;
		}
		bVSyncOn = GS->IsVSyncEnabled();
		ResolutionIndex = Resolutions.IndexOfByKey(GS->GetScreenResolution());
		if (ResolutionIndex == INDEX_NONE)
		{
			ResolutionIndex = Resolutions.IndexOfByKey(FIntPoint(1920, 1080));
			if (ResolutionIndex == INDEX_NONE) { ResolutionIndex = Resolutions.Num() / 2; }
		}
	}
	// 读当前音量到滑条。
	if (const UGT_SettingsSubsystem* Settings = GetSettingsSubsystem())
	{
		if (MusicSlider) { MusicSlider->SetValue(Settings->GetMusicVolume()); }
		if (SfxSlider) { SfxSlider->SetValue(Settings->GetSfxVolume()); }
	}
	RefreshAll();
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

void UGT_SettingsWidget::HandleWindowModePrev()
{
	WindowModeIndex = (WindowModeIndex + 2) % 3;
	ApplyDisplaySettings();
	RefreshDisplayLabels();
}

void UGT_SettingsWidget::HandleWindowModeNext()
{
	WindowModeIndex = (WindowModeIndex + 1) % 3;
	ApplyDisplaySettings();
	RefreshDisplayLabels();
}

void UGT_SettingsWidget::HandleResolutionPrev()
{
	if (Resolutions.Num() == 0) { return; }
	ResolutionIndex = (ResolutionIndex + Resolutions.Num() - 1) % Resolutions.Num();
	ApplyDisplaySettings();
	RefreshDisplayLabels();
}

void UGT_SettingsWidget::HandleResolutionNext()
{
	if (Resolutions.Num() == 0) { return; }
	ResolutionIndex = (ResolutionIndex + 1) % Resolutions.Num();
	ApplyDisplaySettings();
	RefreshDisplayLabels();
}

void UGT_SettingsWidget::HandleVSyncToggle()
{
	bVSyncOn = !bVSyncOn;
	ApplyDisplaySettings();
	RefreshDisplayLabels();
}

void UGT_SettingsWidget::HandleMusicVolumeChanged(float Value)
{
	if (UGT_SettingsSubsystem* Settings = GetSettingsSubsystem())
	{
		Settings->SetMusicVolume(Value);
	}
	RefreshMusicLabel();
}

void UGT_SettingsWidget::HandleSfxVolumeChanged(float Value)
{
	if (UGT_SettingsSubsystem* Settings = GetSettingsSubsystem())
	{
		Settings->SetSfxVolume(Value);
	}
	RefreshSfxLabel();
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
