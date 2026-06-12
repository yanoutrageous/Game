#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GT_MapOverlayWidget.generated.h"

class UBorder;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UUniformGridPanel;
class UGT_DebugSubsystem;

// 全屏区域扫描图(对齐 Lua MapOverlay.lua): 暗色遮罩 + 居中放大格子图。
// 左键未知格 = 插旗/取消(纯 UI 标注, 不进内核); 左键已探索安全格 = 回传并自动关图;
// 右键/ESC/M/点图外 = 关闭。入口: HUD 小地图点击或房间视图 M 键。
UCLASS()
class GRAYTAIL_API UGT_MapOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

	void Open();
	void Close();
	// 新一局开始时清掉上局的旗标。
	void ResetFlags();
	void RefreshGrid();

	// 某格是否被玩家插旗(左上小地图同步显示用; 旗标是 UI 标注, 内核不感知)。
	bool IsCellFlagged(int32 X, int32 Y) const { return FlaggedCells.Contains(FIntPoint(X, Y)); }

	// 关闭后回调(HUD 整体刷新并还焦点给房间视图; 回传也走这条路)。
	FSimpleDelegate OnClosed;

private:
	void BuildWidgetTree();
	UGT_DebugSubsystem* GetDebugSubsystem() const;
	UTexture2D* LoadUiTexture(const FString& AssetPath);

	UPROPERTY(Transient) USizeBox* GridSizeBox = nullptr;
	UPROPERTY(Transient) UUniformGridPanel* MapGrid = nullptr;

	// UI 贴图资产缓存(key = /Game 包路径, 防 GC)。
	UPROPERTY(Transient) TMap<FString, UTexture2D*> UiTextureCache;

	// 标雷旗(玩家自己的判断标注, 内核不感知)。
	TSet<FIntPoint> FlaggedCells;
};
