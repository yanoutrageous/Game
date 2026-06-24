#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GT_SettingsWidget.generated.h"

class UButton;
class UCanvasPanel;
class USlider;
class UTextBlock;
class UVerticalBox;

class UGT_DebugSubsystem;
class UGT_SettingsSubsystem;

// 标题界面「设置」面板(纯 C++ UMG): 显示(模式/分辨率/垂同, 走 UGameUserSettings)
// + 音频(BGM 主音量, 走 UGT_SettingsSubsystem) + 作弊模式开关。
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
	UGT_DebugSubsystem* GetDebugSubsystem() const;
	UGT_SettingsSubsystem* GetSettingsSubsystem() const;

	void RefreshAll();
	void RefreshCheatLabel();
	void RefreshDisplayLabels();
	void RefreshMusicLabel();
	void ApplyDisplaySettings();

	UFUNCTION() void HandleWindowModePrev();
	UFUNCTION() void HandleWindowModeNext();
	UFUNCTION() void HandleResolutionPrev();
	UFUNCTION() void HandleResolutionNext();
	UFUNCTION() void HandleVSyncToggle();
	UFUNCTION() void HandleMusicVolumeChanged(float Value);
	UFUNCTION() void HandleToggleCheat();
	UFUNCTION() void HandleBack();

	UPROPERTY(Transient) UCanvasPanel* Root = nullptr;
	UPROPERTY(Transient) UTextBlock* WindowModeText = nullptr;
	UPROPERTY(Transient) UTextBlock* ResolutionText = nullptr;
	UPROPERTY(Transient) UTextBlock* VSyncText = nullptr;
	UPROPERTY(Transient) USlider* MusicSlider = nullptr;
	UPROPERTY(Transient) UTextBlock* MusicPercentText = nullptr;
	UPROPERTY(Transient) UTextBlock* CheatToggleText = nullptr;

	// 显示设置的当前选择(Open 时从 UGameUserSettings 读入)。
	int32 WindowModeIndex = 0;   // 0 全屏 / 1 无边框窗口 / 2 窗口
	int32 ResolutionIndex = 0;
	bool bVSyncOn = true;
	TArray<FIntPoint> Resolutions;
};
