#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GT_PauseMenuWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;
class UVerticalBox;

// 局内暂停菜单(ESC / = 打开): 继续 / [作弊面板] / 返回标题 / 退出游戏。
// 纯 C++ UMG。模态: 显示时抢键盘焦点 → RoomView 失焦 → 移动暂停(复用既有焦点模型)。
// 返回标题走二次确认(放弃本局, 不结算)。作弊面板入口仅在开启作弊模式时显示。
UCLASS()
class GRAYTAIL_API UGT_PauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// bShowCheatEntry = 是否显示「作弊面板」入口(由 HUD 按作弊模式总开关传入)。
	void Open(bool bShowCheatEntry);
	void Close();
	bool IsOpen() const;

	TDelegate<void()> OnResume;          // 继续(关菜单还焦点)
	TDelegate<void()> OnReturnToTitle;   // 放弃本局回标题(已二次确认)
	TDelegate<void()> OnQuitGame;        // 退出游戏
	TDelegate<void()> OnOpenCheatPanel;  // 打开作弊面板(HUD 接)

private:
	void BuildWidgetTree();
	UButton* AddMenuButton(UVerticalBox* Box, const FString& Label, const FLinearColor& Tint);
	void ShowConfirmReturn(bool bShow);

	UFUNCTION() void HandleResume();
	UFUNCTION() void HandleCheat();
	UFUNCTION() void HandleReturnClicked();   // 第一步: 展开确认
	UFUNCTION() void HandleReturnConfirm();   // 第二步: 真放弃
	UFUNCTION() void HandleReturnCancel();
	UFUNCTION() void HandleQuit();

	UPROPERTY(Transient) UCanvasPanel* Root = nullptr;
	UPROPERTY(Transient) UBorder* Backdrop = nullptr;
	UPROPERTY(Transient) UVerticalBox* MainButtons = nullptr;
	UPROPERTY(Transient) UVerticalBox* ConfirmBox = nullptr;
	UPROPERTY(Transient) UButton* CheatButton = nullptr;

	bool bCheatEntryVisible = false;
};
