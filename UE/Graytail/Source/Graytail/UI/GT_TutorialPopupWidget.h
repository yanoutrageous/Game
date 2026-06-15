#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GT_TutorialPopupWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UBorder;
class UTextBlock;
struct FGT_TutorialPopup;

// 新手教程教学弹窗(对齐 Lua Tutorial 弹窗层)。一个控件两种形态:
//   blocking  = 居中模态: 暗化背景 + 抢键盘焦点(借此暂停 RoomView 移动) + 必须确认才关。
//   非blocking = 顶部提示条: HitTestInvisible 不抢焦点不挡操作, 离开房间由 HUD 调 Hide()。
// 纯表现层: 不读写内核状态, 只显示文案 + 回调确认。
UCLASS()
class GRAYTAIL_API UGT_TutorialPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	// 仅 blocking 形态需要键盘焦点(借此暂停移动); 非blocking 提示条不抢焦点, 玩家可继续 WASD。
	virtual bool NativeSupportsKeyboardFocus() const override { return bBlockingActive; }

	// 显示一条弹窗(按 bBlocking 切换模态/提示条形态)。blocking 形态会抢焦点。
	void ShowPopup(const FGT_TutorialPopup& Popup);
	void Hide();

	// 当前是否有 blocking 弹窗在显示(HUD 焦点重定向时用: 别把焦点从模态弹窗抢走)。
	bool IsBlockingActive() const { return bBlockingActive; }

	// blocking 弹窗确认时触发(HUD 据此还焦点给 RoomView 续走)。
	FSimpleDelegate OnConfirmed;

private:
	void BuildWidgetTree();
	void Confirm();

	UPROPERTY(Transient) UCanvasPanel* Root = nullptr;
	UPROPERTY(Transient) UBorder* Backdrop = nullptr;
	UPROPERTY(Transient) UBorder* Card = nullptr;
	UPROPERTY(Transient) UCanvasPanelSlot* CardSlot = nullptr;
	UPROPERTY(Transient) UTextBlock* TitleText = nullptr;
	UPROPERTY(Transient) UTextBlock* BodyText = nullptr;
	UPROPERTY(Transient) UTextBlock* ConfirmHint = nullptr;

	bool bBlockingActive = false;
};
