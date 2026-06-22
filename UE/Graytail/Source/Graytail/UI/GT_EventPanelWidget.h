#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Domains/Events/GT_EventTypes.h"
#include "GT_EventPanelWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;
class UGT_DebugSubsystem;
class UGT_RunContext;

// 事件交互面板(对齐 Lua DrawEventPanel): 旅商/赌徒/祭坛/机关的统一选项弹窗。
// W/S 选择, Enter/T 确认, Esc/右键 关闭; 鼠标点选项直接执行。
// 表现层薄壳: 菜单/结果全部来自内核(DebugGetEventMenu / ChooseEventOption 命令)。
UCLASS()
class GRAYTAIL_API UGT_EventPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

	// 打开面板(查询玩家当前格事件菜单)。非事件房/BasicDebug 模式返回 false 不显示。
	bool Open();
	void Close();
	bool IsOpen() const;

	// 关闭时通知 HUD(刷新全部并把焦点还给房间)。
	FSimpleDelegate OnClosed;

	// 每次选项执行后通知 HUD 刷新侧栏(金币/血量/背包已变, 但面板保持焦点)。
	FSimpleDelegate OnStateChanged;

private:
	void BuildWidgetTree();
	UGT_DebugSubsystem* GetDebugSubsystem() const;
	const UGT_RunContext* GetRunContext() const;
	// 重新查询菜单并重建选项行(保留底部消息)。菜单不可用时直接关闭。
	void RefreshMenu();
	void RebuildOptionRows();
	void MoveSelection(int32 Delta);
	void ConfirmSelection();

	UPROPERTY(Transient) UBorder* FrameBorder = nullptr;
	UPROPERTY(Transient) UTextBlock* TitleText = nullptr;
	UPROPERTY(Transient) UTextBlock* DescText = nullptr;
	UPROPERTY(Transient) UVerticalBox* OptionsBox = nullptr;
	UPROPERTY(Transient) UTextBlock* MessageText = nullptr;
	UPROPERTY(Transient) TArray<UBorder*> OptionRows;

	FGT_EventMenuView Menu;
	int32 SelectedIndex = 0;
};
