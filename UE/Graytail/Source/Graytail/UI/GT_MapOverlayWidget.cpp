#include "UI/GT_MapOverlayWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/GT_RunContext.h"
#include "Core/GT_RunSubsystem.h"
#include "Debug/GT_DebugSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Events.h"
#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"
#include "UI/GT_UIStyle.h"

namespace
{
	// 对齐 Lua MapOverlay.ComputeLayout: 1080p 下 10x10/13x13 都收敛到 54。
	constexpr float GTOverlayCellSize = 54.f;

	// 经典扫雷数字配色(对齐 Lua MapOverlay.NUMBER_COLORS; 0 = 暗灰, 用户要求 0 也显示)。
	FLinearColor GTOverlayNumberColor(int32 Number)
	{
		switch (Number)
		{
		case 1: return FLinearColor(FColor(60, 100, 220));
		case 2: return FLinearColor(FColor(40, 160, 40));
		case 3: return FLinearColor(FColor(220, 40, 40));
		case 4: return FLinearColor(FColor(120, 40, 180));
		case 5: return FLinearColor(FColor(160, 80, 20));
		case 6: return FLinearColor(FColor(40, 160, 160));
		case 7: return FLinearColor(FColor(60, 60, 60));
		case 8: return FLinearColor(FColor(120, 120, 120));
		default: return FLinearColor(0.55f, 0.55f, 0.60f, 1.f);
		}
	}
}

TSharedRef<SWidget> UGT_MapOverlayWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UGT_MapOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UGT_MapOverlayWidget::BuildWidgetTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	// 暗色遮罩(对齐 Lua nvgRGBA(0,0,0,180))。
	UBorder* Mask = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Mask->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	if (UOverlaySlot* MaskSlot = Root->AddChildToOverlay(Mask))
	{
		MaskSlot->SetHorizontalAlignment(HAlign_Fill);
		MaskSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// 居中竖排: 标题 / 格子图 / 底部提示。整列铺满屏幕, 让格子区拿到确定的可用空间,
	// ScaleBox 才能按可用空间等比缩放(防止大图被小视口非等比压扁)。
	UVerticalBox* CenterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UOverlaySlot* CenterSlot = Root->AddChildToOverlay(CenterBox))
	{
		CenterSlot->SetHorizontalAlignment(HAlign_Fill);
		CenterSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetFont(GT_UIStyle::Font(18));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.94f)));
	Title->SetJustification(ETextJustify::Center);
	Title->SetText(FText::FromString(TEXT("区域扫描图 (点击格子标记雷险/回传)")));
	if (UVerticalBoxSlot* TitleSlot = CenterBox->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// GridSizeBox 固定 (列×54)×(行×54) 正方形比例; 外包 ScaleBox(ScaleToFit) 等比缩放,
	// 任意视口下都不变形(13×13 大图在小视口里整体缩小, cell 始终正方形)。
	GridSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MapGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	GridSizeBox->SetContent(MapGrid);
	UScaleBox* GridScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	GridScale->SetStretch(EStretch::ScaleToFit);
	GridScale->SetContent(GridSizeBox);
	if (UVerticalBoxSlot* GridSlot = CenterBox->AddChildToVerticalBox(GridScale))
	{
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
		GridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// 操作被拒提示(战斗禁回传等), 默认隐藏占位(不顶动布局)。
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	StatusText->SetFont(GT_UIStyle::Font(13));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(255, 110, 100, 230))));
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetVisibility(ESlateVisibility::Hidden);
	if (UVerticalBoxSlot* StatusSlot = CenterBox->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
	}

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Hint->SetFont(GT_UIStyle::Font(13));
	Hint->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(180, 200, 220, 200))));
	Hint->SetJustification(ETextJustify::Center);
	Hint->SetText(FText::FromString(TEXT("左键: 未知格标记雷险/取消  |  已探索格回传  |  ESC/右键关闭")));
	if (UVerticalBoxSlot* HintSlot = CenterBox->AddChildToVerticalBox(Hint))
	{
		HintSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		HintSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

UGT_DebugSubsystem* UGT_MapOverlayWidget::GetDebugSubsystem() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UGT_DebugSubsystem>() : nullptr;
}

const UGT_RunContext* UGT_MapOverlayWidget::GetRunContext() const
{
	const UGT_RunSubsystem* RunSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGT_RunSubsystem>() : nullptr;
	return RunSubsystem ? RunSubsystem->GetCurrentRunContext() : nullptr;
}

UTexture2D* UGT_MapOverlayWidget::LoadUiTexture(const FString& AssetPath)
{
	if (UTexture2D** Cached = UiTextureCache.Find(AssetPath))
	{
		return *Cached;
	}

	const FString ObjectPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("GT_MapOverlayWidget: missing texture asset %s"), *AssetPath);
	}
	UiTextureCache.Add(AssetPath, Texture);
	return Texture;
}

void UGT_MapOverlayWidget::Open()
{
	if (StatusText)
	{
		StatusText->SetVisibility(ESlateVisibility::Hidden);
	}
	RefreshGrid();
	SetVisibility(ESlateVisibility::Visible);
	SetFocus();
}

void UGT_MapOverlayWidget::Close()
{
	SetVisibility(ESlateVisibility::Collapsed);
	if (OnClosed.IsBound())
	{
		OnClosed.Execute();
	}
}

void UGT_MapOverlayWidget::ResetFlags()
{
	FlaggedCells.Reset();
	if (GetVisibility() != ESlateVisibility::Collapsed)
	{
		RefreshGrid();
	}
}

void UGT_MapOverlayWidget::RefreshGrid()
{
	if (!MapGrid)
	{
		return;
	}
	MapGrid->ClearChildren();

	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	TArray<FGT_MiniMapCellViewData> Cells;
	int32 Width = 0;
	int32 Height = 0;
	if (!Debug || !Debug->GetDebugMiniMapViewData(Cells, Width, Height) || Width <= 0 || Height <= 0)
	{
		return;
	}

	FGT_DebugRunSnapshot Snapshot;
	Debug->GetDebugRunSnapshot(Snapshot);

	// 邻域感知天赋: 门控读取 + 算"相邻是否藏雷"(任一相邻未知格是雷 -> 相邻未知格全红, 否则全黄)。
	const UGT_RunContext* OvRunContext = GetRunContext();
	const bool bMapHl = OvRunContext && OvRunContext->IsLoadoutMapHighlightActive();
	bool bHiddenMineNear = false;
	if (bMapHl)
	{
		for (int32 NdY = -1; NdY <= 1 && !bHiddenMineNear; ++NdY)
		{
			for (int32 NdX = -1; NdX <= 1 && !bHiddenMineNear; ++NdX)
			{
				if (NdX == 0 && NdY == 0) { continue; }
				const int32 Nx = Snapshot.PlayerX + NdX, Ny = Snapshot.PlayerY + NdY;
				if (Nx < 0 || Ny < 0 || Nx >= Width || Ny >= Height) { continue; }
				const FGT_MiniMapCellViewData& NCell = Cells[Ny * Width + Nx];
				if (NCell.bExplored || NCell.bVisible) { continue; }   // 已知格不算"未探出"
				FGT_TruthCell NTruth;
				if (OvRunContext->GetTruthCellSnapshot(Nx, Ny, NTruth) && NTruth.bHasMine)
				{
					bHiddenMineNear = true;
				}
			}
		}
	}

	GridSizeBox->SetWidthOverride(Width * GTOverlayCellSize);
	GridSizeBox->SetHeightOverride(Height * GTOverlayCellSize);

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const FGT_MiniMapCellViewData& Cell = Cells[Y * Width + X];
			const bool bPlayerHere = X == Snapshot.PlayerX && Y == Snapshot.PlayerY;
			const bool bKnown = Cell.bExplored || Cell.bVisible;
			const FString Icon = Cell.VisibleRoomIcon.ToString();
			const bool bMineCell = bKnown && Icon == TEXT("Mine");
			const bool bExitCell = bKnown && Icon == TEXT("Exit");
			const bool bFlagged = !bKnown && FlaggedCells.Contains(FIntPoint(X, Y));

			USizeBox* CellSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			CellSize->SetWidthOverride(GTOverlayCellSize);
			CellSize->SetHeightOverride(GTOverlayCellSize);

			// 外框(对齐 Lua 0.5px 描边; 撤离点 = 醒目黄框)。
			UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Frame->SetBrushColor(bExitCell
				? FLinearColor(FColor(255, 220, 70))
				: FLinearColor(FColor(50, 55, 70, 200)));
			Frame->SetPadding(FMargin(bExitCell ? 2.f : 1.f));
			CellSize->SetContent(Frame);

			// 底色(对齐 Lua: 隐藏/插旗/雷/已知)。
			UBorder* CellBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			FLinearColor BgColor = FLinearColor(FColor(40, 45, 60));
			if (bFlagged)
			{
				BgColor = FLinearColor(FColor(160, 50, 50));
			}
			else if (!bKnown)
			{
				BgColor = FLinearColor(FColor(70, 75, 95));
			}
			else if (bMineCell)
			{
				BgColor = FLinearColor(FColor(220, 40, 40));
			}
			CellBg->SetBrushColor(BgColor);
			CellBg->SetPadding(FMargin(0.f));
			Frame->SetContent(CellBg);

			UOverlay* CellOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
			CellBg->SetContent(CellOverlay);

			auto AddCellIcon = [this, CellOverlay](UTexture2D* Texture, float Scale, float Opacity = 1.f)
			{
				if (!Texture)
				{
					return;
				}
				UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				IconImage->SetBrushFromTexture(Texture);
				IconImage->SetDesiredSizeOverride(FVector2D(GTOverlayCellSize * Scale, GTOverlayCellSize * Scale));
				IconImage->SetOpacity(Opacity);
				if (UOverlaySlot* IconSlot = CellOverlay->AddChildToOverlay(IconImage))
				{
					IconSlot->SetHorizontalAlignment(HAlign_Center);
					IconSlot->SetVerticalAlignment(VAlign_Center);
				}
			};

			bool bSpecialIcon = false;
			if (!bKnown)
			{
				AddCellIcon(LoadUiTexture(TEXT("/Game/Graytail/UI/Icons64/01_weizhi_ge")), 0.7f, 0.8f);
				if (bFlagged)
				{
					AddCellIcon(LoadUiTexture(TEXT("/Game/Graytail/UI/Icons64/04_biaoji_qi")), 0.7f);
				}
			}
			else
			{
				// 房型图标(对齐 Lua drawRoomIcon + 撤离/雷)。
				const TCHAR* IconPath = nullptr;
				if (Icon == TEXT("Chest")) { IconPath = TEXT("/Game/Graytail/UI/Icons64/07_baoxiang_icon"); }
				else if (Icon == TEXT("Combat")) { IconPath = TEXT("/Game/Graytail/UI/Icons64/06_guaiwu_icon"); }
				else if (Icon == TEXT("Event")) { IconPath = TEXT("/Game/Graytail/UI/Icons64/03_saomiao_ge"); }
				else if (Icon == TEXT("Exit")) { IconPath = TEXT("/Game/Graytail/UI/Icons64/08_cheli_icon"); }
				else if (Icon == TEXT("Mine")) { IconPath = TEXT("/Game/Graytail/UI/Icons64/05_dici_xianjing_icon"); }
				bSpecialIcon = IconPath != nullptr;
				if (bSpecialIcon)
				{
					AddCellIcon(LoadUiTexture(IconPath), 0.7f);
				}

				// 玩家头像画在数字之前(角标压在头像上层, 防止挡住本格雷数)。
				if (bPlayerHere)
				{
					AddCellIcon(LoadUiTexture(TEXT("/Game/Graytail/UI/Icons64/00_wanjia_dingwei")), 0.8f);
				}

				if (Cell.bScanned && !bMineCell)
				{
					// 玩家所在格: 数字改右下角标(含 0), 不被头像遮挡(用户要求: 开图标雷要看得到本格雷数)。
					const bool bBadgeNumber = (bSpecialIcon && Cell.DisplayedNumber > 0) || (bPlayerHere && !bSpecialIcon);
					if (bBadgeNumber)
					{
						// 右下角数字角标(对齐 drawNumberBadge)。
						UBorder* Badge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
						Badge->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.06f, 0.88f));
						Badge->SetPadding(FMargin(3.f, 1.f));
						UTextBlock* BadgeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
						BadgeText->SetFont(GT_UIStyle::Font(12));
						BadgeText->SetText(FText::FromString(FString::FromInt(Cell.DisplayedNumber)));
						BadgeText->SetColorAndOpacity(FSlateColor(GTOverlayNumberColor(Cell.DisplayedNumber)));
						Badge->SetContent(BadgeText);
						if (UOverlaySlot* BadgeSlot = CellOverlay->AddChildToOverlay(Badge))
						{
							BadgeSlot->SetHorizontalAlignment(HAlign_Right);
							BadgeSlot->SetVerticalAlignment(VAlign_Bottom);
						}
					}
					else if (!bSpecialIcon && !bPlayerHere && Cell.DisplayedNumber >= 1 && Cell.DisplayedNumber <= 8)
					{
						AddCellIcon(LoadUiTexture(FString::Printf(
							TEXT("/Game/Graytail/UI/Icons64/1%d_shuzi_%d"), Cell.DisplayedNumber, Cell.DisplayedNumber)), 0.7f);
					}
					else if (!bSpecialIcon && !bPlayerHere)
					{
						// 0: 文本数字(无 0 图标, 用户要求 0 也显示)。
						UTextBlock* NumberText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
						NumberText->SetFont(GT_UIStyle::Font(26));
						NumberText->SetText(FText::FromString(FString::FromInt(Cell.DisplayedNumber)));
						NumberText->SetColorAndOpacity(FSlateColor(GTOverlayNumberColor(Cell.DisplayedNumber)));
						if (UOverlaySlot* NumberSlot = CellOverlay->AddChildToOverlay(NumberText))
						{
							NumberSlot->SetHorizontalAlignment(HAlign_Center);
							NumberSlot->SetVerticalAlignment(VAlign_Center);
						}
					}
				}
			}

			// 可回传小蓝点(右下, 对齐 Lua explored 圆点)。
			if (Cell.bExplored && bKnown && !bMineCell && !bFlagged)
			{
				USizeBox* DotSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				DotSize->SetWidthOverride(6.f);
				DotSize->SetHeightOverride(6.f);
				UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				Dot->SetBrushColor(FLinearColor(FColor(100, 200, 255, 200)));
				DotSize->SetContent(Dot);
				if (UOverlaySlot* DotSlot = CellOverlay->AddChildToOverlay(DotSize))
				{
					DotSlot->SetHorizontalAlignment(HAlign_Left);
					DotSlot->SetVerticalAlignment(VAlign_Bottom);
					DotSlot->SetPadding(FMargin(3.f));
				}
			}

			// 邻域感知天赋: 相邻未知格整体染色(画最上层, 盖过 ? 砖块) —— 有相邻未探出雷=全红, 否则全黄。
			// 不暴露具体哪格雷, 仅未探索 ? 格生效。
			if (bMapHl && !bPlayerHere && !bKnown
				&& FMath::Abs(X - Snapshot.PlayerX) <= 1 && FMath::Abs(Y - Snapshot.PlayerY) <= 1)
			{
				UBorder* NeighborHl = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				FSlateBrush HlBrush;
				HlBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
				HlBrush.TintColor = bHiddenMineNear
					? FSlateColor(FLinearColor(1.f, 0.22f, 0.22f, 0.32f))   // 有雷: 红内填
					: FSlateColor(FLinearColor(1.f, 0.86f, 0.32f, 0.16f));  // 安全: 淡黄内填
				HlBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
				HlBrush.OutlineSettings.CornerRadii = FVector4(4.f, 4.f, 4.f, 4.f);
				HlBrush.OutlineSettings.Color = bHiddenMineNear
					? FSlateColor(FLinearColor(FColor(255, 70, 70)))
					: FSlateColor(FLinearColor(FColor(255, 220, 80)));
				HlBrush.OutlineSettings.Width = bHiddenMineNear ? 4.f : 3.f;
				NeighborHl->SetBrush(HlBrush);
				if (UOverlaySlot* HlSlot = CellOverlay->AddChildToOverlay(NeighborHl))
				{
					HlSlot->SetHorizontalAlignment(HAlign_Fill);
					HlSlot->SetVerticalAlignment(VAlign_Fill);
				}
			}

			MapGrid->AddChildToUniformGrid(CellSize, Y, X);
		}
	}
}

FReply UGT_MapOverlayWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 右键 = 关闭(对齐 Lua MOUSEB_RIGHT)。
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Close();
		return FReply::Handled();
	}

	UGT_DebugSubsystem* Debug = GetDebugSubsystem();
	if (!MapGrid || !Debug)
	{
		Close();
		return FReply::Handled();
	}

	const FGeometry GridGeometry = MapGrid->GetCachedGeometry();
	const FVector2D LocalPos = GridGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D GridSize = GridGeometry.GetLocalSize();
	if (LocalPos.X < 0.f || LocalPos.Y < 0.f || LocalPos.X >= GridSize.X || LocalPos.Y >= GridSize.Y)
	{
		// 点图外 = 关闭(对齐 Lua)。
		Close();
		return FReply::Handled();
	}

	TArray<FGT_MiniMapCellViewData> Cells;
	int32 Width = 0;
	int32 Height = 0;
	if (!Debug->GetDebugMiniMapViewData(Cells, Width, Height) || Width <= 0 || Height <= 0)
	{
		return FReply::Handled();
	}

	const int32 CellX = FMath::Clamp(static_cast<int32>(LocalPos.X / (GridSize.X / Width)), 0, Width - 1);
	const int32 CellY = FMath::Clamp(static_cast<int32>(LocalPos.Y / (GridSize.Y / Height)), 0, Height - 1);
	const FGT_MiniMapCellViewData& Cell = Cells[CellY * Width + CellX];
	const bool bKnown = Cell.bExplored || Cell.bVisible;

	if (!bKnown)
	{
		// 未知格: 插旗/取消(对齐 Lua hidden/flagged -> onFlag)。
		const FIntPoint Coord(CellX, CellY);
		if (FlaggedCells.Contains(Coord))
		{
			FlaggedCells.Remove(Coord);
		}
		else
		{
			FlaggedCells.Add(Coord);
		}
		RefreshGrid();
	}
	else if (Cell.bExplored && Cell.VisibleRoomIcon != FName(TEXT("Mine")))
	{
		// 战斗激活时禁止回传(对齐 game-design 11.3 交互限制)。
		FGT_DebugRunSnapshot Snapshot;
		if (Debug->GetDebugRunSnapshot(Snapshot) && Snapshot.bCombatActive)
		{
			if (StatusText)
			{
				StatusText->SetText(FText::FromString(TEXT("异常体活动中，无法回传")));
				StatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			return FReply::Handled();
		}

		// 已探索安全格: 回传并关图(OnClosed 里 HUD 会整体刷新 + 还焦点)。
		Debug->DebugTeleport(CellX, CellY, Snapshot);
		Close();
	}
	return FReply::Handled();
}

FReply UGT_MapOverlayWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 快速连点(插旗->秒取消)会被 Slate 判成双击事件, 当普通点击处理, 否则第二下"没反应"。
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UGT_MapOverlayWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::M)
	{
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
