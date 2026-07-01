#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GT_PauseMenuWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;
class UVerticalBox;
#if !UE_BUILD_SHIPPING
class UGT_IndexedButton;
class UGT_DebugSubsystem;
#endif

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
	void ShowActionError(const FString& Message);

	TDelegate<void()> OnResume;          // 继续(关菜单还焦点)
	TDelegate<void()> OnReturnToTitle;   // 放弃本局回标题(已二次确认)
	TDelegate<void()> OnQuitGame;        // 退出游戏
#if !UE_BUILD_SHIPPING
	TDelegate<void()> OnOpenCheatPanel;  // (预留)由 HUD 处理的作弊入口; 当前作弊面板内置于本控件
	TDelegate<void()> OnCheatApplied;    // 作弊动作后请 HUD 整体刷新(金币/血量/背包/房间)
	TDelegate<void(const FString&)> OnToast;   // 作弊提示(如"没有可传送的怪物房了") -> HUD 居中 toast
#endif

private:
	void BuildWidgetTree();
#if !UE_BUILD_SHIPPING
	void BuildCheatBox(UVerticalBox* Column);
#endif
	UButton* AddMenuButton(UVerticalBox* Box, const FString& Label, const FLinearColor& Tint);
#if !UE_BUILD_SHIPPING
	// 作弊按钮: 带 Index, 点击统一进 HandleCheatIndex; 返回其文字块(无敌按钮要动态改标签)。
	UTextBlock* AddCheatButton(UVerticalBox* Box, const FString& Label, int32 Index, const FLinearColor& Tint);
#endif
	void ShowConfirmReturn(bool bShow);
#if !UE_BUILD_SHIPPING
	void ShowCheatView(bool bShow);
	void RefreshGodLabel();
	UGT_DebugSubsystem* GetDebug() const;
	bool ReadGodMode() const;
#endif

	UFUNCTION() void HandleResume();
	UFUNCTION() void HandleCheat();
	UFUNCTION() void HandleCheatBack();
	UFUNCTION() void HandleCheatIndex(int32 Index);
	UFUNCTION() void HandleReturnClicked();   // 第一步: 展开确认
	UFUNCTION() void HandleReturnConfirm();   // 第二步: 真放弃
	UFUNCTION() void HandleReturnCancel();
	UFUNCTION() void HandleQuit();

	UPROPERTY(Transient) UCanvasPanel* Root = nullptr;
	UPROPERTY(Transient) UBorder* Backdrop = nullptr;
	UPROPERTY(Transient) UVerticalBox* MainButtons = nullptr;
	UPROPERTY(Transient) UVerticalBox* ConfirmBox = nullptr;
	UPROPERTY(Transient) UButton* ResumeButton = nullptr;
	UPROPERTY(Transient) UTextBlock* ActionErrorText = nullptr;
	UPROPERTY(Transient) UVerticalBox* CheatBox = nullptr;
	UPROPERTY(Transient) UButton* CheatButton = nullptr;
	UPROPERTY(Transient) UTextBlock* CheatGodText = nullptr;

#if !UE_BUILD_SHIPPING
	bool bCheatEntryVisible = false;
#endif
};
