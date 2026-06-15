#include "UI/GT_MainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Events.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"
#include "UObject/Package.h"

namespace
{
	// 木板锚点 = 背景图(1672×941)里木板的像素范围换算成百分比,
	// 背景全屏拉伸时锚点同步拉伸, 热区/文字始终钉在木板上(同 Lua 热区思路)。
	// 像素范围用 1:1 纵向条带+亮度边界实测(两扇门都左高右低, 纵向错切 +4°;
	// 门体还整体左倾 → 板的竖边也是斜的: 横向错切(上下边水平错开)另算一轴,
	// 每块板的 x 范围再按门倾逐块左移)。
	// 用 shear 而不是旋转: 美术烤字是"字直行斜"的透视画法。
	// Shear = (横X, 纵Y) 单位度: Y=左右高低, X=上下边水平错开(斜体方向)。
	// 双轴默认值在头文件成员初始化(MainShear/CastleShear); 这俩只是构建期占位的纵向值。
	constexpr float GTMainShear = 4.f;
	constexpr float GTCastleShear = 4.f;

	struct FGTMenuOptionDef
	{
		// 自绘文字; 空串 = 文字已烤在背景图里, 只做热区+高亮。
		const TCHAR* Label;
		const TCHAR* Hint;
		FAnchors Anchors;
	};

	// 主页(menu_bg, 文字烤图): 出发探索 / 新手教程 / 装备天赋 / 设置。
	// 锚点与下方错切默认值均为豪豪 2026-06-14 F10 校准模式实测值。
	const FGTMenuOptionDef GTMainOptions[] =
	{
		{ TEXT(""), TEXT("进入回收区域作业, 先选区域规模再选难度"),
		  FAnchors(0.6993f, 0.3262f, 0.8866f, 0.4307f) },
		{ TEXT(""), TEXT("新手教程: 固定 5×5 安全演练区, 跟着提示学会避雷与撤离"),
		  FAnchors(0.6825f, 0.4662f, 0.8728f, 0.5667f) },
		{ TEXT(""), TEXT("装备/天赋: 部署终端尚未开放(局外系统)"),
		  FAnchors(0.6730f, 0.6017f, 0.8574f, 0.7045f) },
		{ TEXT(""), TEXT("设置: 尚未开放"),
		  FAnchors(0.6620f, 0.7371f, 0.8454f, 0.8093f) },
	};
	// 主页入口: 出发探索(0) / 新手教程(1) 已开放; 装备天赋(2) / 设置(3) = 局外组部署终端预留位, 尚未开放。
	constexpr int32 GTMainOption_Depart = 0;
	constexpr int32 GTMainOption_Tutorial = 1;

	// 每块板独立的错切默认值 (横X=上下边错开, 纵Y=左右高低), F10 校准实测。
	const FVector2D GTMainShearDefaults[] =
	{
		FVector2D(-4.00f, 5.00f), FVector2D(-4.00f, 7.50f), FVector2D(-11.75f, 9.75f), FVector2D(-13.00f, 12.25f),
	};

	// 城堡门四块板的锚点(区域页/难度页共用; 门左倾, 下面的板逐块左移)。
	const FAnchors GTCastlePlanks[] =
	{
		FAnchors(0.7127f, 0.3489f, 0.8960f, 0.4532f),
		FAnchors(0.6998f, 0.4901f, 0.8851f, 0.6034f),
		FAnchors(0.6828f, 0.6336f, 0.8721f, 0.7387f),
		FAnchors(0.6779f, 0.7703f, 0.8532f, 0.8591f),
	};
	const FVector2D GTCastleShearDefaults[] =
	{
		FVector2D(-10.00f, 5.00f), FVector2D(-10.00f, 6.75f), FVector2D(-10.00f, 10.75f), FVector2D(-13.00f, 13.00f),
	};

	// 区域页: 中型/大型 在板 1-2, 返回固定在最下板(板 3 留空)。
	const FGTMenuOptionDef GTSizeOptions[] =
	{
		{ TEXT("中型区域"), TEXT("中型回收区: 10×10 · 难度 简单/标准/困难"), GTCastlePlanks[0] },
		{ TEXT("大型区域"), TEXT("大型回收区: 13×13 · 难度 普通/困难/地狱"), GTCastlePlanks[1] },
		{ TEXT("返回"),     TEXT("返回主菜单"),                              GTCastlePlanks[3] },
	};
	constexpr int32 GTSizeOption_Back = 2;

	// 难度阶梯(难度判断.md §16, 共 6 档, 不含 7×7 教学):
	struct FGTDiffDef
	{
		const TCHAR* Label;
		const TCHAR* Hint;
		EGT_Difficulty Difficulty;
	};
	const FGTDiffDef GTDiff10[] =
	{
		{ TEXT("简单作业"), TEXT("简单: 10×10 · 撤离信标 ×3 · 保留找点体验, 尾部风险低(§16.3)"), EGT_Difficulty::Easy },
		{ TEXT("标准作业"), TEXT("标准: 10×10 · 撤离信标 ×2 · Demo 基准, 标准偏难(§16.4)"),     EGT_Difficulty::Standard },
		{ TEXT("困难作业"), TEXT("困难: 10×10 · 撤离信标 ×1 · 强制探索更长(§16.5)"),             EGT_Difficulty::Hard },
	};
	const FGTDiffDef GTDiff13[] =
	{
		{ TEXT("普通作业"), TEXT("普通: 13×13 · 撤离信标 ×4 · 节奏近中图标准, 信息更复杂(§16.6)"), EGT_Difficulty::Veteran },
		{ TEXT("困难作业"), TEXT("困难: 13×13 · 撤离信标 ×2 · 真正的大图困难(§16.7)"),             EGT_Difficulty::Elite },
		{ TEXT("地狱作业"), TEXT("地狱: 13×13 · 撤离信标 ×1 · 高失败率挑战(§16.8)"),               EGT_Difficulty::Nightmare },
	};
	constexpr int32 GTDiffOption_Back = 3;

	const FLinearColor GTMenuLabelSelected(FColor(255, 236, 170));
	const FLinearColor GTMenuLabelNormal(FColor(206, 176, 116));
	// 选中木板罩一层极淡暖光(透明 Border 调 alpha), 错切后即贴板的平行四边形。
	const FLinearColor GTMenuPlankGlow(1.f, 0.92f, 0.6f, 0.13f);
	const FLinearColor GTMenuPlankClear(1.f, 1.f, 1.f, 0.f);
	const FLinearColor GTMenuHintNormal(FColor(225, 210, 175, 235));
	const FLinearColor GTMenuHintLocked(FColor(235, 150, 120, 245));

	// 错切变换(平移/缩放/旋转都不动): Y 轴错切=横线倾斜(竖线仍竖直),
	// X 轴错切=竖线倾斜(上下边水平错开, 斜体方向)。
	FWidgetTransform MakeShearTransform(const FVector2D& ShearAnglesDeg)
	{
		return FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, ShearAnglesDeg, 0.f);
	}

	// 基准斜度是在图像素空间量的; 非 16:9 视口下背景被非等比拉伸,
	// 屏幕有效斜度: 纵向 = atan(tan(基准)×系数), 横向 = atan(tan(基准)÷系数)。
	FVector2D ToEffectiveShearDeg(const FVector2D& BaseDeg, float AspectScale)
	{
		const float SafeScale = FMath::Max(AspectScale, 0.01f);
		return FVector2D(
			FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(BaseDeg.X)) / SafeScale)),
			FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(BaseDeg.Y)) * SafeScale)));
	}

	// 校准数值的本机存档(GameUserSettings.ini, 不进仓库)。
	const TCHAR* GTTuneSection = TEXT("GraytailMenuTune");

	FString AnchorsToString(const FAnchors& Anchors)
	{
		return FString::Printf(TEXT("%.4f,%.4f,%.4f,%.4f"),
			Anchors.Minimum.X, Anchors.Minimum.Y, Anchors.Maximum.X, Anchors.Maximum.Y);
	}

	bool AnchorsFromString(const FString& Text, FAnchors& OutAnchors)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() != 4)
		{
			return false;
		}
		OutAnchors.Minimum.X = FCString::Atof(*Parts[0]);
		OutAnchors.Minimum.Y = FCString::Atof(*Parts[1]);
		OutAnchors.Maximum.X = FCString::Atof(*Parts[2]);
		OutAnchors.Maximum.Y = FCString::Atof(*Parts[3]);
		return true;
	}

	// 区域页三块板对应城堡门的第 0/1/3 块(板 2 留空)。
	const int32 GTSizeToCastlePlank[] = { 0, 1, 3 };
}

TSharedRef<SWidget> UGT_MainMenuWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UGT_MainMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 非 16:9 视口(常见于 PIE 内嵌视口)下背景被非等比拉伸,
	// 木板的屏幕斜度随之变化, 错切角按当前宽高比实时修正。
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	if (LocalSize.X > 1.f && LocalSize.Y > 1.f)
	{
		const float NewScale = (1672.f / 941.f) * (LocalSize.Y / LocalSize.X);
		if (!FMath::IsNearlyEqual(NewScale, AspectShearScale, 0.005f))
		{
			AspectShearScale = NewScale;
			ApplyTunables();
		}
	}
}

void UGT_MainMenuWidget::BuildWidgetTree()
{
	// 三页全屏 Canvas 叠在 Overlay 里切换显隐。百分比锚点需要 Canvas;
	// HUD 走真实布局容器, 菜单要素必须钉在美术图案上, 场景不同工具不同。
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	// ---- 主页: 文字烤在图里, 只放热区. ----
	PageMain = BuildPageCanvas(Root, TEXT("/Game/Graytail/Sprites/Misc/menu_bg"));
	MainPlanks.Reset();
	MainPlankSlots.Reset();
	MainAnchors.Reset();
	for (const FGTMenuOptionDef& Option : GTMainOptions)
	{
		TArray<UTextBlock*> Unused;
		UBorder* Plank = MakePlank(PageMain, Option.Anchors, GTMainShear, Option.Label, Unused);
		MainPlanks.Add(Plank);
		MainPlankSlots.Add(Cast<UCanvasPanelSlot>(Plank->Slot));
		MainAnchors.Add(Option.Anchors);
		const int32 ShearIdx = FMath::Min(MainShears.Num(), (int32)UE_ARRAY_COUNT(GTMainShearDefaults) - 1);
		MainShears.Add(GTMainShearDefaults[ShearIdx]);
	}
	HintMain = MakeHintLine(PageMain, FAnchors(0.06f, 0.90f, 0.62f, 0.952f), 20);
	{
		UTextBlock* Footer = MakeHintLine(PageMain, FAnchors(0.06f, 0.955f, 0.62f, 0.995f), 14);
		Footer->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.68f, 0.58f, 0.85f)));
		Footer->SetText(FText::FromString(TEXT("W/S 选择 · Enter 确认 · 鼠标点击直接进入")));
	}

	// 城堡门锚点/错切工作副本(区域页/难度页共用)。
	CastleAnchors.Reset();
	CastleShears.Reset();
	for (const FAnchors& PlankAnchors : GTCastlePlanks)
	{
		CastleAnchors.Add(PlankAnchors);
		const int32 ShearIdx = FMath::Min(CastleShears.Num(), (int32)UE_ARRAY_COUNT(GTCastleShearDefaults) - 1);
		CastleShears.Add(GTCastleShearDefaults[ShearIdx]);
	}

	// ---- 区域页: 无字版背景, 选地图规模. ----
	PageSize = BuildPageCanvas(Root, TEXT("/Game/Graytail/UI/main_menu/main_menu_bg_no_text"));
	SizeTitlePlate = AddPageTitle(PageSize, TEXT("选择区域"));
	SizeTitleSlot = Cast<UCanvasPanelSlot>(SizeTitlePlate->Slot);
	SizePlanks.Reset();
	SizePlankSlots.Reset();
	SizeLabels.Reset();
	for (const FGTMenuOptionDef& Option : GTSizeOptions)
	{
		UBorder* Plank = MakePlank(PageSize, Option.Anchors, GTCastleShear, Option.Label, SizeLabels);
		SizePlanks.Add(Plank);
		SizePlankSlots.Add(Cast<UCanvasPanelSlot>(Plank->Slot));
	}
	HintSize = MakeHintLine(PageSize, FAnchors(0.10f, 0.90f, 0.66f, 0.952f), 20);
	{
		UTextBlock* Footer = MakeHintLine(PageSize, FAnchors(0.10f, 0.955f, 0.66f, 0.995f), 14);
		Footer->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.68f, 0.58f, 0.85f)));
		Footer->SetText(FText::FromString(TEXT("W/S 选择 · Enter 确认 · Esc 返回")));
	}
	PageSize->SetVisibility(ESlateVisibility::Collapsed);

	// ---- 难度页: 同背景, 三档文案随所选区域在 ShowPage 时填充. ----
	PageDifficulty = BuildPageCanvas(Root, TEXT("/Game/Graytail/UI/main_menu/main_menu_bg_no_text"));
	DiffTitlePlate = AddPageTitle(PageDifficulty, TEXT("选择难度"));
	DiffTitleSlot = Cast<UCanvasPanelSlot>(DiffTitlePlate->Slot);
	DiffPlanks.Reset();
	DiffPlankSlots.Reset();
	DiffLabels.Reset();
	for (int32 Index = 0; Index < (int32)UE_ARRAY_COUNT(GTCastlePlanks); ++Index)
	{
		// 占位文案, 进页时按区域刷新; 最下板固定"返回"。
		const FString Label = Index == GTDiffOption_Back ? TEXT("返回") : TEXT("-");
		UBorder* Plank = MakePlank(PageDifficulty, GTCastlePlanks[Index], GTCastleShear, Label, DiffLabels);
		DiffPlanks.Add(Plank);
		DiffPlankSlots.Add(Cast<UCanvasPanelSlot>(Plank->Slot));
	}
	HintDiff = MakeHintLine(PageDifficulty, FAnchors(0.10f, 0.90f, 0.66f, 0.952f), 20);
	{
		UTextBlock* Footer = MakeHintLine(PageDifficulty, FAnchors(0.10f, 0.955f, 0.66f, 0.995f), 14);
		Footer->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.68f, 0.58f, 0.85f)));
		Footer->SetText(FText::FromString(TEXT("W/S 选择 · Enter 确认 · Esc 返回")));
	}
	PageDifficulty->SetVisibility(ESlateVisibility::Collapsed);

	// ---- 校准信息面板(最顶层, 仅 F10 校准模式可见). ----
	{
		TuneInfoText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TuneInfoText->SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 13));
		TuneInfoText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.85f, 1.f)));

		UBorder* InfoBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		InfoBox->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.72f));
		InfoBox->SetPadding(FMargin(10.f, 8.f));
		InfoBox->SetContent(TuneInfoText);
		InfoBox->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* InfoSlot = Root->AddChildToOverlay(InfoBox))
		{
			InfoSlot->SetHorizontalAlignment(HAlign_Left);
			InfoSlot->SetVerticalAlignment(VAlign_Top);
			InfoSlot->SetPadding(FMargin(14.f, 12.f, 0.f, 0.f));
		}
	}

	// 木牌默认锚点(AddPageTitle 里的常量), 之后可能被本机校准值覆盖。
	CastleTitleAnchors = FAnchors(0.0572f, 0.1713f, 0.4400f, 0.3626f);

	// 本机校准值(若有)覆盖默认, 再统一套到槽位/变换上。
	LoadTunablesFromConfig();
	ApplyTunables();
}

UCanvasPanel* UGT_MainMenuWidget::BuildPageCanvas(UOverlay* Root, const FString& BgAssetPath)
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	if (UOverlaySlot* CanvasSlot = Root->AddChildToOverlay(Canvas))
	{
		CanvasSlot->SetHorizontalAlignment(HAlign_Fill);
		CanvasSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// 第 0 层: 纯色垫底(美术缺失时菜单仍可读)。
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Backdrop->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.04f, 1.f));
	if (UCanvasPanelSlot* BackdropSlot = Canvas->AddChildToCanvas(Backdrop))
	{
		BackdropSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackdropSlot->SetOffsets(FMargin(0.f));
	}

	// 第 1 层: 整图背景全屏拉伸(对齐 Lua 原版菜单)。
	if (UTexture2D* BgTexture = LoadUiTexture(BgAssetPath))
	{
		UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Background->SetBrushFromTexture(BgTexture);
		if (UCanvasPanelSlot* BgSlot = Canvas->AddChildToCanvas(Background))
		{
			BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			BgSlot->SetOffsets(FMargin(0.f));
		}
	}
	return Canvas;
}

UBorder* UGT_MainMenuWidget::AddPageTitle(UCanvasPanel* Canvas, const FString& TitleText)
{
	// 左上吊牌(图内 84..754, 98..363 像素); 牌身整体是转过去的, 标题用旋转贴牌面。
	UBorder* TitlePlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	TitlePlate->SetBrushColor(GTMenuPlankClear);
	TitlePlate->SetHorizontalAlignment(HAlign_Center);
	TitlePlate->SetVerticalAlignment(VAlign_Center);
	TitlePlate->SetRenderTransformAngle(6.5f);
	if (UCanvasPanelSlot* PlateSlot = Canvas->AddChildToCanvas(TitlePlate))
	{
		PlateSlot->SetAnchors(FAnchors(0.0572f, 0.1713f, 0.4400f, 0.3626f));
		PlateSlot->SetOffsets(FMargin(0.f));
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 96));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(236, 205, 130))));
	Title->SetShadowOffset(FVector2D(3.f, 3.f));
	Title->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	Title->SetText(FText::FromString(TitleText));
	TitlePlate->SetContent(Title);
	return TitlePlate;
}

UBorder* UGT_MainMenuWidget::MakePlank(UCanvasPanel* Canvas, const FAnchors& Anchors, float ShearAngle, const FString& LabelText, TArray<UTextBlock*>& OutLabels)
{
	UBorder* Plank = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Plank->SetBrushColor(GTMenuPlankClear);
	Plank->SetHorizontalAlignment(HAlign_Center);
	Plank->SetVerticalAlignment(VAlign_Center);
	// 构建期先给纵向斜度占位, 构建完 ApplyTunables 会用完整的双轴错切覆盖。
	Plank->SetRenderTransform(MakeShearTransform(FVector2D(0.f, ShearAngle)));
	if (UCanvasPanelSlot* PlankSlot = Canvas->AddChildToCanvas(Plank))
	{
		PlankSlot->SetAnchors(Anchors);
		PlankSlot->SetOffsets(FMargin(0.f));
	}

	if (!LabelText.IsEmpty())
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 52));
		Label->SetShadowOffset(FVector2D(2.f, 2.f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f));
		Label->SetText(FText::FromString(LabelText));
		Plank->SetContent(Label);
		OutLabels.Add(Label);
	}
	return Plank;
}

UTextBlock* UGT_MainMenuWidget::MakeHintLine(UCanvasPanel* Canvas, const FAnchors& Anchors, int32 FontSize)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FontSize));
	Text->SetColorAndOpacity(FSlateColor(GTMenuHintNormal));
	Text->SetShadowOffset(FVector2D(2.f, 2.f));
	Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Text->SetJustification(ETextJustify::Center);

	UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Box->SetBrushColor(GTMenuPlankClear);
	Box->SetHorizontalAlignment(HAlign_Center);
	Box->SetVerticalAlignment(VAlign_Center);
	Box->SetContent(Text);
	if (UCanvasPanelSlot* BoxSlot = Canvas->AddChildToCanvas(Box))
	{
		BoxSlot->SetAnchors(Anchors);
		BoxSlot->SetOffsets(FMargin(0.f));
	}
	return Text;
}

UTexture2D* UGT_MainMenuWidget::LoadUiTexture(const FString& AssetPath)
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
		UE_LOG(LogTemp, Warning, TEXT("GT_MainMenuWidget: missing texture asset %s"), *AssetPath);
	}
	UiTextureCache.Add(AssetPath, Texture);
	return Texture;
}

void UGT_MainMenuWidget::Open()
{
	ShowPage(EPage::Main);
	SetVisibility(ESlateVisibility::Visible);
	SetFocus();
}

void UGT_MainMenuWidget::Close()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UGT_MainMenuWidget::IsOpen() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

void UGT_MainMenuWidget::ShowPage(EPage Page)
{
	CurrentPage = Page;
	SelectedIndex = 0;

	// 难度页文案随所选区域刷新(返回板固定)。
	if (Page == EPage::Difficulty)
	{
		const FGTDiffDef* Table = bLargeMap ? GTDiff13 : GTDiff10;
		for (int32 Index = 0; Index < 3 && Index < DiffLabels.Num(); ++Index)
		{
			if (DiffLabels[Index])
			{
				DiffLabels[Index]->SetText(FText::FromString(Table[Index].Label));
			}
		}
	}

	auto SetPageVisible = [Page](UCanvasPanel* Canvas, EPage Self)
	{
		if (Canvas)
		{
			Canvas->SetVisibility(Page == Self ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	};
	SetPageVisible(PageMain, EPage::Main);
	SetPageVisible(PageSize, EPage::Size);
	SetPageVisible(PageDifficulty, EPage::Difficulty);
	RefreshOptionStyles();
}

int32 UGT_MainMenuWidget::CurrentOptionCount() const
{
	switch (CurrentPage)
	{
	case EPage::Main:       return UE_ARRAY_COUNT(GTMainOptions);
	case EPage::Size:       return UE_ARRAY_COUNT(GTSizeOptions);
	case EPage::Difficulty: return UE_ARRAY_COUNT(GTCastlePlanks);
	}
	return 0;
}

const TArray<UBorder*>& UGT_MainMenuWidget::CurrentPlanks() const
{
	switch (CurrentPage)
	{
	case EPage::Size:       return SizePlanks;
	case EPage::Difficulty: return DiffPlanks;
	default:                return MainPlanks;
	}
}

FString UGT_MainMenuWidget::CurrentHint(int32 Index) const
{
	switch (CurrentPage)
	{
	case EPage::Main:
		return GTMainOptions[Index].Hint;
	case EPage::Size:
		return GTSizeOptions[Index].Hint;
	case EPage::Difficulty:
		if (Index == GTDiffOption_Back)
		{
			return TEXT("返回区域选择");
		}
		return (bLargeMap ? GTDiff13 : GTDiff10)[Index].Hint;
	}
	return FString();
}

void UGT_MainMenuWidget::SetSelection(int32 NewIndex)
{
	const int32 Count = CurrentOptionCount();
	// 循环选择(对齐事件面板的 W/S 包绕)。
	SelectedIndex = (NewIndex % Count + Count) % Count;
	RefreshOptionStyles();
}

void UGT_MainMenuWidget::RefreshOptionStyles()
{
	const TArray<UBorder*>& Planks = CurrentPlanks();
	for (int32 Index = 0; Index < Planks.Num(); ++Index)
	{
		if (Planks[Index])
		{
			Planks[Index]->SetBrushColor(Index == SelectedIndex ? GTMenuPlankGlow : GTMenuPlankClear);
		}
	}
	// 自绘文字跟选中态变色(主页文字在图里, 只有暖光)。
	const TArray<UTextBlock*>* Labels = nullptr;
	if (CurrentPage == EPage::Size)
	{
		Labels = &SizeLabels;
	}
	else if (CurrentPage == EPage::Difficulty)
	{
		Labels = &DiffLabels;
	}
	if (Labels)
	{
		for (int32 Index = 0; Index < Labels->Num(); ++Index)
		{
			if ((*Labels)[Index])
			{
				(*Labels)[Index]->SetColorAndOpacity(FSlateColor(Index == SelectedIndex ? GTMenuLabelSelected : GTMenuLabelNormal));
			}
		}
	}

	UTextBlock* Hint = CurrentPage == EPage::Main ? HintMain : CurrentPage == EPage::Size ? HintSize : HintDiff;
	if (Hint && SelectedIndex >= 0 && SelectedIndex < CurrentOptionCount())
	{
		Hint->SetColorAndOpacity(FSlateColor(GTMenuHintNormal));
		Hint->SetText(FText::FromString(CurrentHint(SelectedIndex)));
	}
}

void UGT_MainMenuWidget::ConfirmSelection()
{
	if (SelectedIndex < 0 || SelectedIndex >= CurrentOptionCount())
	{
		return;
	}

	switch (CurrentPage)
	{
	case EPage::Main:
		if (SelectedIndex == GTMainOption_Depart)
		{
			// 将来部署界面(局外组)插在出发流程里, 位置由团队定。
			ShowPage(EPage::Size);
			return;
		}
		if (SelectedIndex == GTMainOption_Tutorial)
		{
			// 新手教程: 跳过区域/难度选择, 直接开固定 5×5 教学局(Tutorial 档自带手摆布局)。
			OnStartRequested.ExecuteIfBound(EGT_Difficulty::Tutorial);
			return;
		}
		// 其余入口未开放: 提示行变暖红反馈。
		if (HintMain)
		{
			HintMain->SetColorAndOpacity(FSlateColor(GTMenuHintLocked));
			HintMain->SetText(FText::FromString(FString::Printf(TEXT("【尚未开放】%s"), GTMainOptions[SelectedIndex].Hint)));
		}
		return;

	case EPage::Size:
		if (SelectedIndex == GTSizeOption_Back)
		{
			ShowPage(EPage::Main);
			return;
		}
		bLargeMap = SelectedIndex == 1;
		ShowPage(EPage::Difficulty);
		return;

	case EPage::Difficulty:
		if (SelectedIndex == GTDiffOption_Back)
		{
			ShowPage(EPage::Size);
			return;
		}
		OnStartRequested.ExecuteIfBound((bLargeMap ? GTDiff13 : GTDiff10)[SelectedIndex].Difficulty);
		return;
	}
}

int32 UGT_MainMenuWidget::HitTestOptions(const FVector2D& ScreenPosition) const
{
	const TArray<UBorder*>& Planks = CurrentPlanks();
	const int32 Count = CurrentOptionCount();
	for (int32 Index = 0; Index < Planks.Num() && Index < Count; ++Index)
	{
		if (Planks[Index] && Planks[Index]->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FReply UGT_MainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::F10)
	{
		ToggleTuneMode();
		return FReply::Handled();
	}
	if (bTuneMode)
	{
		HandleTuneKey(InKeyEvent);
		return FReply::Handled();
	}
	if (Key == EKeys::W || Key == EKeys::Up)
	{
		SetSelection(SelectedIndex - 1);
		return FReply::Handled();
	}
	if (Key == EKeys::S || Key == EKeys::Down)
	{
		SetSelection(SelectedIndex + 1);
		return FReply::Handled();
	}
	if (Key == EKeys::Enter || Key == EKeys::F || Key == EKeys::SpaceBar)
	{
		ConfirmSelection();
		return FReply::Handled();
	}
	if (Key == EKeys::Escape)
	{
		if (CurrentPage == EPage::Difficulty)
		{
			ShowPage(EPage::Size);
		}
		else if (CurrentPage == EPage::Size)
		{
			ShowPage(EPage::Main);
		}
		return FReply::Handled();
	}
	// 菜单是顶层界面, 吞掉其余按键防止穿透到下层 HUD/房间。
	return FReply::Handled();
}

FReply UGT_MainMenuWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 校准模式下鼠标不动选择(避免悬停盖掉键盘选的元素高亮)。
	if (bTuneMode)
	{
		return FReply::Handled();
	}
	// 悬停即选中(同步键盘光标, 底部提示跟着换)。
	const int32 HitIndex = HitTestOptions(InMouseEvent.GetScreenSpacePosition());
	if (HitIndex != INDEX_NONE && HitIndex != SelectedIndex)
	{
		SetSelection(HitIndex);
	}
	return FReply::Handled();
}

FReply UGT_MainMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bTuneMode)
	{
		SetFocus();
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const int32 HitIndex = HitTestOptions(InMouseEvent.GetScreenSpacePosition());
		if (HitIndex != INDEX_NONE)
		{
			SetSelection(HitIndex);
			ConfirmSelection();
		}
	}
	SetFocus();
	return FReply::Handled();
}

// ---------------- F10 校准模式 ----------------
// 远程量图始终有误差, 最终对齐交给屏幕前的人眼:
// 进模式后键盘直接推板子, 退出时存本机 ini 并把数值打到日志(烤回代码用)。

void UGT_MainMenuWidget::ToggleTuneMode()
{
	// 三页都能校准: 区域页改的是共用城堡板(0/1/3), 难度页自动跟齐。
	bTuneMode = !bTuneMode;
	if (bTuneMode)
	{
		TuneIndex = 0;
		RefreshTuneInfo();
	}
	else
	{
		SaveTunablesToConfig();
		DumpTunablesToLog();
		RefreshOptionStyles();
		if (SizeTitlePlate)
		{
			SizeTitlePlate->SetBrushColor(GTMenuPlankClear);
		}
		if (DiffTitlePlate)
		{
			DiffTitlePlate->SetBrushColor(GTMenuPlankClear);
		}
	}
	if (TuneInfoText && TuneInfoText->GetParent())
	{
		TuneInfoText->GetParent()->SetVisibility(bTuneMode ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

bool UGT_MainMenuWidget::HandleTuneKey(const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Escape)
	{
		ToggleTuneMode();
		return true;
	}
	if (Key == EKeys::P)
	{
		DumpTunablesToLog();
		return true;
	}
	// 选板/木牌: 1-3 通用; 4 仅当本页有 4 块板; 0 选木牌(主页无木牌)。
	if (Key == EKeys::One)   { TuneIndex = 0; }
	else if (Key == EKeys::Two)   { TuneIndex = 1; }
	else if (Key == EKeys::Three) { TuneIndex = 2; }
	else if (Key == EKeys::Four && TunePlankCount() >= 4)  { TuneIndex = 3; }
	else if (Key == EKeys::Zero && PageHasTitle()) { TuneIndex = 4; }
	else if (Key == EKeys::Q || Key == EKeys::E)
	{
		// Q/E: 纵向错切(左右高低); 选中木牌时调旋转角。每块板独立。
		const float Delta = Key == EKeys::Q ? -0.25f : 0.25f;
		if (TuneIndex == 4)
		{
			CastleTitleAngle += Delta * 2.f;
		}
		else if (FVector2D* Shear = TuneShearPtr())
		{
			Shear->Y += Delta;
		}
	}
	else if (Key == EKeys::A || Key == EKeys::D)
	{
		// A/D: 横向错切(上下边水平错开, 门倾方向); 每块板独立, 木牌不适用。
		const float Delta = Key == EKeys::A ? -0.25f : 0.25f;
		if (FVector2D* Shear = TuneShearPtr())
		{
			Shear->X += Delta;
		}
	}
	else if (Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::Left || Key == EKeys::Right)
	{
		const float Step = InKeyEvent.IsControlDown() ? 0.0005f : 0.002f;
		const float DX = Key == EKeys::Right ? Step : Key == EKeys::Left ? -Step : 0.f;
		const float DY = Key == EKeys::Down ? Step : Key == EKeys::Up ? -Step : 0.f;

		if (FAnchors* Target = TuneAnchorPtr())
		{
			if (InKeyEvent.IsShiftDown())
			{
				// Shift = 只动右/下边(调尺寸)。
				Target->Maximum.X += DX;
				Target->Maximum.Y += DY;
			}
			else
			{
				Target->Minimum.X += DX; Target->Maximum.X += DX;
				Target->Minimum.Y += DY; Target->Maximum.Y += DY;
			}
		}
	}

	ApplyTunables();
	RefreshTuneInfo();
	return true;
}

int32 UGT_MainMenuWidget::TunePlankCount() const
{
	switch (CurrentPage)
	{
	case EPage::Main: return MainPlanks.Num();
	case EPage::Size: return SizePlanks.Num();
	case EPage::Difficulty: return DiffPlanks.Num();
	}
	return 0;
}

FAnchors* UGT_MainMenuWidget::TuneAnchorPtr()
{
	if (TuneIndex == 4)
	{
		return PageHasTitle() ? &CastleTitleAnchors : nullptr;
	}
	if (CurrentPage == EPage::Main)
	{
		return MainAnchors.IsValidIndex(TuneIndex) ? &MainAnchors[TuneIndex] : nullptr;
	}
	// 区域页板 0/1/2 → 城堡板 0/1/3; 难度页板序即城堡板序。
	int32 CastleIndex = TuneIndex;
	if (CurrentPage == EPage::Size)
	{
		CastleIndex = (TuneIndex >= 0 && TuneIndex < (int32)UE_ARRAY_COUNT(GTSizeToCastlePlank)) ? GTSizeToCastlePlank[TuneIndex] : INDEX_NONE;
	}
	return CastleAnchors.IsValidIndex(CastleIndex) ? &CastleAnchors[CastleIndex] : nullptr;
}

FVector2D* UGT_MainMenuWidget::TuneShearPtr()
{
	if (TuneIndex == 4)
	{
		return nullptr; // 木牌用旋转角, 无错切。
	}
	if (CurrentPage == EPage::Main)
	{
		return MainShears.IsValidIndex(TuneIndex) ? &MainShears[TuneIndex] : nullptr;
	}
	int32 CastleIndex = TuneIndex;
	if (CurrentPage == EPage::Size)
	{
		CastleIndex = (TuneIndex >= 0 && TuneIndex < (int32)UE_ARRAY_COUNT(GTSizeToCastlePlank)) ? GTSizeToCastlePlank[TuneIndex] : INDEX_NONE;
	}
	return CastleShears.IsValidIndex(CastleIndex) ? &CastleShears[CastleIndex] : nullptr;
}

void UGT_MainMenuWidget::ApplyTunables()
{
	for (int32 Index = 0; Index < MainPlankSlots.Num(); ++Index)
	{
		if (MainPlankSlots[Index] && MainAnchors.IsValidIndex(Index))
		{
			MainPlankSlots[Index]->SetAnchors(MainAnchors[Index]);
			MainPlankSlots[Index]->SetOffsets(FMargin(0.f));
		}
		if (MainPlanks.IsValidIndex(Index) && MainPlanks[Index] && MainShears.IsValidIndex(Index))
		{
			MainPlanks[Index]->SetRenderTransform(MakeShearTransform(ToEffectiveShearDeg(MainShears[Index], AspectShearScale)));
		}
	}
	for (int32 Index = 0; Index < SizePlankSlots.Num() && Index < (int32)UE_ARRAY_COUNT(GTSizeToCastlePlank); ++Index)
	{
		const int32 CastleIndex = GTSizeToCastlePlank[Index];
		if (SizePlankSlots[Index] && CastleAnchors.IsValidIndex(CastleIndex))
		{
			SizePlankSlots[Index]->SetAnchors(CastleAnchors[CastleIndex]);
			SizePlankSlots[Index]->SetOffsets(FMargin(0.f));
		}
		if (SizePlanks.IsValidIndex(Index) && SizePlanks[Index] && CastleShears.IsValidIndex(CastleIndex))
		{
			SizePlanks[Index]->SetRenderTransform(MakeShearTransform(ToEffectiveShearDeg(CastleShears[CastleIndex], AspectShearScale)));
		}
	}
	for (int32 Index = 0; Index < DiffPlankSlots.Num(); ++Index)
	{
		if (DiffPlankSlots[Index] && CastleAnchors.IsValidIndex(Index))
		{
			DiffPlankSlots[Index]->SetAnchors(CastleAnchors[Index]);
			DiffPlankSlots[Index]->SetOffsets(FMargin(0.f));
		}
		if (DiffPlanks.IsValidIndex(Index) && DiffPlanks[Index] && CastleShears.IsValidIndex(Index))
		{
			DiffPlanks[Index]->SetRenderTransform(MakeShearTransform(ToEffectiveShearDeg(CastleShears[Index], AspectShearScale)));
		}
	}
	if (SizeTitleSlot)
	{
		SizeTitleSlot->SetAnchors(CastleTitleAnchors);
		SizeTitleSlot->SetOffsets(FMargin(0.f));
	}
	if (DiffTitleSlot)
	{
		DiffTitleSlot->SetAnchors(CastleTitleAnchors);
		DiffTitleSlot->SetOffsets(FMargin(0.f));
	}
	if (SizeTitlePlate)
	{
		SizeTitlePlate->SetRenderTransformAngle(CastleTitleAngle);
	}
	if (DiffTitlePlate)
	{
		DiffTitlePlate->SetRenderTransformAngle(CastleTitleAngle);
	}
}

void UGT_MainMenuWidget::LoadTunablesFromConfig()
{
	if (!GConfig)
	{
		return;
	}
	FString Value;
	for (int32 Index = 0; Index < MainAnchors.Num(); ++Index)
	{
		if (GConfig->GetString(GTTuneSection, *FString::Printf(TEXT("MainPlank%d"), Index), Value, GGameUserSettingsIni))
		{
			AnchorsFromString(Value, MainAnchors[Index]);
		}
	}
	for (int32 Index = 0; Index < CastleAnchors.Num(); ++Index)
	{
		if (GConfig->GetString(GTTuneSection, *FString::Printf(TEXT("CastlePlank%d"), Index), Value, GGameUserSettingsIni))
		{
			AnchorsFromString(Value, CastleAnchors[Index]);
		}
	}
	if (GConfig->GetString(GTTuneSection, TEXT("CastleTitle"), Value, GGameUserSettingsIni))
	{
		AnchorsFromString(Value, CastleTitleAnchors);
	}
	float FloatValue = 0.f;
	for (int32 Index = 0; Index < MainShears.Num(); ++Index)
	{
		if (GConfig->GetFloat(GTTuneSection, *FString::Printf(TEXT("MainShear%dX"), Index), FloatValue, GGameUserSettingsIni)) { MainShears[Index].X = FloatValue; }
		if (GConfig->GetFloat(GTTuneSection, *FString::Printf(TEXT("MainShear%dY"), Index), FloatValue, GGameUserSettingsIni)) { MainShears[Index].Y = FloatValue; }
	}
	for (int32 Index = 0; Index < CastleShears.Num(); ++Index)
	{
		if (GConfig->GetFloat(GTTuneSection, *FString::Printf(TEXT("CastleShear%dX"), Index), FloatValue, GGameUserSettingsIni)) { CastleShears[Index].X = FloatValue; }
		if (GConfig->GetFloat(GTTuneSection, *FString::Printf(TEXT("CastleShear%dY"), Index), FloatValue, GGameUserSettingsIni)) { CastleShears[Index].Y = FloatValue; }
	}
	GConfig->GetFloat(GTTuneSection, TEXT("CastleTitleAngle"), CastleTitleAngle, GGameUserSettingsIni);
}

void UGT_MainMenuWidget::SaveTunablesToConfig() const
{
	if (!GConfig)
	{
		return;
	}
	for (int32 Index = 0; Index < MainAnchors.Num(); ++Index)
	{
		GConfig->SetString(GTTuneSection, *FString::Printf(TEXT("MainPlank%d"), Index), *AnchorsToString(MainAnchors[Index]), GGameUserSettingsIni);
	}
	for (int32 Index = 0; Index < CastleAnchors.Num(); ++Index)
	{
		GConfig->SetString(GTTuneSection, *FString::Printf(TEXT("CastlePlank%d"), Index), *AnchorsToString(CastleAnchors[Index]), GGameUserSettingsIni);
	}
	GConfig->SetString(GTTuneSection, TEXT("CastleTitle"), *AnchorsToString(CastleTitleAnchors), GGameUserSettingsIni);
	for (int32 Index = 0; Index < MainShears.Num(); ++Index)
	{
		GConfig->SetFloat(GTTuneSection, *FString::Printf(TEXT("MainShear%dX"), Index), MainShears[Index].X, GGameUserSettingsIni);
		GConfig->SetFloat(GTTuneSection, *FString::Printf(TEXT("MainShear%dY"), Index), MainShears[Index].Y, GGameUserSettingsIni);
	}
	for (int32 Index = 0; Index < CastleShears.Num(); ++Index)
	{
		GConfig->SetFloat(GTTuneSection, *FString::Printf(TEXT("CastleShear%dX"), Index), CastleShears[Index].X, GGameUserSettingsIni);
		GConfig->SetFloat(GTTuneSection, *FString::Printf(TEXT("CastleShear%dY"), Index), CastleShears[Index].Y, GGameUserSettingsIni);
	}
	GConfig->SetFloat(GTTuneSection, TEXT("CastleTitleAngle"), CastleTitleAngle, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	UE_LOG(LogTemp, Display, TEXT("GT_MenuTune: saved to %s [%s]"), *GGameUserSettingsIni, GTTuneSection);
}

void UGT_MainMenuWidget::DumpTunablesToLog() const
{
	UE_LOG(LogTemp, Display, TEXT("GT_MenuTune: ---- current values (code-ready) ----"));
	for (int32 Index = 0; Index < MainAnchors.Num(); ++Index)
	{
		UE_LOG(LogTemp, Display, TEXT("GT_MenuTune: MainPlank%d  FAnchors(%.4ff, %.4ff, %.4ff, %.4ff)"),
			Index, MainAnchors[Index].Minimum.X, MainAnchors[Index].Minimum.Y, MainAnchors[Index].Maximum.X, MainAnchors[Index].Maximum.Y);
	}
	for (int32 Index = 0; Index < CastleAnchors.Num(); ++Index)
	{
		UE_LOG(LogTemp, Display, TEXT("GT_MenuTune: CastlePlank%d FAnchors(%.4ff, %.4ff, %.4ff, %.4ff)"),
			Index, CastleAnchors[Index].Minimum.X, CastleAnchors[Index].Minimum.Y, CastleAnchors[Index].Maximum.X, CastleAnchors[Index].Maximum.Y);
	}
	UE_LOG(LogTemp, Display, TEXT("GT_MenuTune: CastleTitle  FAnchors(%.4ff, %.4ff, %.4ff, %.4ff)  Angle=%.2f"),
		CastleTitleAnchors.Minimum.X, CastleTitleAnchors.Minimum.Y, CastleTitleAnchors.Maximum.X, CastleTitleAnchors.Maximum.Y, CastleTitleAngle);
	// 错切每块板独立, 烤回代码时填进对应板的 FVector2D(横X, 纵Y)。
	for (int32 Index = 0; Index < MainShears.Num(); ++Index)
	{
		UE_LOG(LogTemp, Display, TEXT("GT_MenuTune: MainShear%d   FVector2D(%.2ff, %.2ff)  (横X, 纵Y)"), Index, MainShears[Index].X, MainShears[Index].Y);
	}
	for (int32 Index = 0; Index < CastleShears.Num(); ++Index)
	{
		UE_LOG(LogTemp, Display, TEXT("GT_MenuTune: CastleShear%d FVector2D(%.2ff, %.2ff)  (横X, 纵Y)"), Index, CastleShears[Index].X, CastleShears[Index].Y);
	}
}

void UGT_MainMenuWidget::RefreshTuneInfo()
{
	// 高亮当前校准元素(红), 其余板淡黄示位。
	const FLinearColor TuneSelected(1.f, 0.25f, 0.2f, 0.4f);
	const FLinearColor TuneFaint(1.f, 0.92f, 0.6f, 0.18f);
	const TArray<UBorder*>& Planks = CurrentPlanks();
	for (int32 Index = 0; Index < Planks.Num(); ++Index)
	{
		if (Planks[Index])
		{
			Planks[Index]->SetBrushColor(Index == TuneIndex ? TuneSelected : TuneFaint);
		}
	}
	// 当前页的木牌(区域页/难度页各有一块)。
	UBorder* TitlePlate = CurrentPage == EPage::Size ? SizeTitlePlate : CurrentPage == EPage::Difficulty ? DiffTitlePlate : nullptr;
	if (TitlePlate)
	{
		TitlePlate->SetBrushColor(TuneIndex == 4 ? TuneSelected : GTMenuPlankClear);
	}

	if (!TuneInfoText)
	{
		return;
	}
	const TCHAR* PageName = CurrentPage == EPage::Main ? TEXT("主页门")
		: CurrentPage == EPage::Size ? TEXT("城堡门-区域页(难度页自动跟齐)")
		: TEXT("城堡门-难度页(区域页自动跟齐)");
	FString ElementName;
	FAnchors Current = FAnchors();
	FString AngleLine;
	if (TuneIndex == 4)
	{
		ElementName = TEXT("木牌标题");
		Current = CastleTitleAnchors;
		AngleLine = FString::Printf(TEXT("旋转=%.2f°"), CastleTitleAngle);
	}
	else
	{
		ElementName = FString::Printf(TEXT("第 %d 块板"), TuneIndex + 1);
		if (const FAnchors* Anchor = TuneAnchorPtr())
		{
			Current = *Anchor;
		}
		const FVector2D Shear = TuneShearPtr() ? *TuneShearPtr() : FVector2D::ZeroVector;
		AngleLine = FString::Printf(TEXT("纵斜(Q/E)=%.2f°  横错(A/D)=%.2f°"), Shear.Y, Shear.X);
	}
	TuneInfoText->SetText(FText::FromString(FString::Printf(
		TEXT("[校准模式] %s · %s (本块独立)\n")
		TEXT("锚点 L=%.4f T=%.4f R=%.4f B=%.4f  %s\n")
		TEXT("%s · 方向键移动 · Shift+方向键调右/下边 · Ctrl 微调\n")
		TEXT("Q/E 左右高低 · A/D 上下边错开 · P 打印数值 · F10/Esc 保存退出"),
		PageName,
		*ElementName,
		Current.Minimum.X, Current.Minimum.Y, Current.Maximum.X, Current.Maximum.Y,
		*AngleLine,
		PageHasTitle() ? TEXT("1-3 选板 · 0 选木牌") : TEXT("1-4 选板"))));
}
