#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GT_GameHudWidget.generated.h"

class UTextBlock;
class UButton;
class UHorizontalBox;
class UVerticalBox;
class UProgressBar;
class UUniformGridPanel;
class UTexture2D;
class UGT_DebugSubsystem;
class UGT_RunContext;
class UGT_RoomViewWidget;
class UGT_MapOverlayWidget;
class UGT_LootResultWidget;

// 主游戏界面(对齐 Lua 原版构图): 房间视图铺满全屏为背景,
// 左侧信息面板/右上协议面板/底部快捷键栏全部悬浮其上。
// 纯 C++ 搭 UMG 树(无蓝图资产), 入口: 控制台 gt.HUD。
UCLASS()
class GRAYTAIL_API UGT_GameHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void BuildWidgetTree();
	UGT_DebugSubsystem* GetDebugSubsystem() const;
	const UGT_RunContext* GetRunContext() const;
	void RefreshAll();
	void RefreshPanels();
	void RefreshStatusPanel();
	void RefreshMiniMapGrid();
	void RefreshItemsList();
	UTexture2D* GetItemIcon(FName ItemId);
	UTexture2D* LoadUiTexture(const FString& AssetPath);
	UButton* MakeButton(UHorizontalBox* Row, const FString& Label);
	UTextBlock* MakePanelText(UVerticalBox* Panel, int32 FontSize, const FLinearColor& Color);

	UFUNCTION() void OnSearch();
	UFUNCTION() void OnAttack();
	UFUNCTION() void OnExtract();
	UFUNCTION() void OnNewRun();

	void OpenMapOverlay();
	void HandleMapOverlayClosed();
	void HandleLootResultClosed();

	UPROPERTY(Transient) UGT_RoomViewWidget* RoomView = nullptr;
	UPROPERTY(Transient) UGT_MapOverlayWidget* MapOverlay = nullptr;
	UPROPERTY(Transient) UGT_LootResultWidget* LootResult = nullptr;
	UPROPERTY(Transient) UUniformGridPanel* MiniMapGrid = nullptr;
	UPROPERTY(Transient) UProgressBar* HpBar = nullptr;
	UPROPERTY(Transient) UTextBlock* HpText = nullptr;
	UPROPERTY(Transient) UTextBlock* StatsText = nullptr;
	UPROPERTY(Transient) UTextBlock* StateText = nullptr;
	UPROPERTY(Transient) UVerticalBox* ItemsList = nullptr;
	UPROPERTY(Transient) UTextBlock* LogText = nullptr;
	UPROPERTY(Transient) UTextBlock* ProtocolText = nullptr;
	UPROPERTY(Transient) UButton* SearchButton = nullptr;

	// UI 贴图资产缓存(key = /Game 包路径, 防 GC)。
	UPROPERTY(Transient) TMap<FString, UTexture2D*> UiTextureCache;
};
