#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GT_GameHudWidget.generated.h"

class UBorder;
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
	void RefreshRunEndPanel();
	UTexture2D* GetItemIcon(FName ItemId);
	UTexture2D* LoadUiTexture(const FString& AssetPath);
	// 蓝底科技风面板: 深蓝底 + 原版边框贴图(冷色调降透明度)叠层, 返回挂到屏幕上的包装件。
	UWidget* MakeSkinnedPanel(UBorder* Panel, const FString& AssetPath);
	UButton* MakeButton(UHorizontalBox* Row, const FString& Label);
	UTextBlock* MakePanelText(UVerticalBox* Panel, int32 FontSize, const FLinearColor& Color);

	UFUNCTION() void OnSearch();
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
	// 属性行按原版分色: 战斗力白 / 待结算金 / 已锁定绿 / 回收物青 / 已搜索灰。
	UPROPERTY(Transient) UTextBlock* PowerText = nullptr;
	UPROPERTY(Transient) UTextBlock* PendingText = nullptr;
	UPROPERTY(Transient) UTextBlock* SafeText = nullptr;
	UPROPERTY(Transient) UTextBlock* PartsText = nullptr;
	UPROPERTY(Transient) UTextBlock* SearchedText = nullptr;
	UPROPERTY(Transient) UTextBlock* StateText = nullptr;
	UPROPERTY(Transient) UVerticalBox* ItemsList = nullptr;
	UPROPERTY(Transient) UTextBlock* LogText = nullptr;
	UPROPERTY(Transient) UTextBlock* ProtocolText = nullptr;

	// 局终结算弹窗(死亡/撤离后弹出, 含重新出发按钮; NewRun 按钮已移除)。
	UPROPERTY(Transient) UWidget* RunEndRoot = nullptr;
	UPROPERTY(Transient) UBorder* RunEndFrame = nullptr;
	UPROPERTY(Transient) UTextBlock* RunEndTitle = nullptr;
	UPROPERTY(Transient) UTextBlock* RunEndBody = nullptr;
	UPROPERTY(Transient) UButton* RunEndButton = nullptr;
	bool bRunEndShown = false;

	// UI 贴图资产缓存(key = /Game 包路径, 防 GC)。
	UPROPERTY(Transient) TMap<FString, UTexture2D*> UiTextureCache;
};
