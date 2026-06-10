#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GT_GameHudWidget.generated.h"

class UTextBlock;
class UButton;
class UHorizontalBox;
class UUniformGridPanel;
class UTexture2D;
class UGT_DebugSubsystem;
class UGT_RunContext;

// 最小可玩 HUD: 纯 C++ 搭 UMG 树(无蓝图资产)。
// 薄壳原则: 动作走 DebugSubsystem 现成入口, 显示读 RunContext 只读状态。
// 入口: 控制台 gt.HUD。
UCLASS()
class GRAYTAIL_API UGT_GameHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 纯 C++ 搭树必须在 RebuildWidget(Slate 构建前)完成;
	// NativeConstruct 时 Slate 树已生成, 再设 RootWidget 不会显示。
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildWidgetTree();
	UGT_DebugSubsystem* GetDebugSubsystem() const;
	const UGT_RunContext* GetRunContext() const;
	void RefreshAll();
	void RefreshStatusLine();
	void RefreshMiniMapGrid();
	void RefreshItemsRow();
	UTexture2D* GetItemIcon(FName ItemId);
	UButton* MakeButton(UHorizontalBox* Row, const FString& Label);

	UFUNCTION() void OnMoveUp();
	UFUNCTION() void OnMoveDown();
	UFUNCTION() void OnMoveLeft();
	UFUNCTION() void OnMoveRight();
	UFUNCTION() void OnSearch();
	UFUNCTION() void OnAttack();
	UFUNCTION() void OnExtract();
	UFUNCTION() void OnNewRun();

	void TryMove(int32 DeltaX, int32 DeltaY);

	UPROPERTY(Transient) UTextBlock* StatusText = nullptr;
	UPROPERTY(Transient) UUniformGridPanel* MiniMapGrid = nullptr;
	UPROPERTY(Transient) UHorizontalBox* ItemsRow = nullptr;
	UPROPERTY(Transient) UTextBlock* LogText = nullptr;

	// 运行时加载的物品图标缓存(防 GC)。
	UPROPERTY(Transient) TMap<FName, UTexture2D*> IconCache;
};
