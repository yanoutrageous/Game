#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GT_SettingsWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;

class UGT_DebugSubsystem;

// 标题界面「设置」面板(纯 C++ UMG)。当前只有一个「作弊模式」总开关,
// 开启后局内 ESC 暂停菜单才显示「作弊面板」入口。后续真实设置(画面/音量)可往这里加。
UCLASS()
class GRAYTAIL_API UGT_SettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void Open();
	void Close();
	bool IsOpen() const;

	TDelegate<void()> OnBackRequested;   // 返回主菜单(HUD 接)

private:
	void BuildWidgetTree();
	void RefreshCheatLabel();
	UGT_DebugSubsystem* GetDebugSubsystem() const;

	UFUNCTION() void HandleToggleCheat();
	UFUNCTION() void HandleBack();

	UPROPERTY(Transient) UCanvasPanel* Root = nullptr;
	UPROPERTY(Transient) UTextBlock* CheatToggleText = nullptr;
};
