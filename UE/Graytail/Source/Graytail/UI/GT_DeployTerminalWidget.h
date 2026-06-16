#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GT_DeployTerminalWidget.generated.h"

class UTextBlock;
class UScrollBox;
class UVerticalBox;
class UHorizontalBox;
class UBorder;
class UButton;
class UTexture2D;
class UGT_MetaProgressSubsystem;

// 局外部署终端(S5 元终端 UI, Phase 1): 终端外壳 + 导航 + 申领(商店) + 作业装备(装备/loadout)
// + 金币栏 + 作业摘要。纯 C++ UMG(RebuildWidget 搭树), 读写 S1 UGT_MetaProgressSubsystem。
// 入口由主菜单「装备天赋」触发, HUD 持有并开关。Phase 2 补 仓库/天赋/抢救页。
UCLASS()
class GRAYTAIL_API UGT_DeployTerminalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

	void Open();
	void Close();
	bool IsOpen() const;

	// 返回主菜单 / 出发探索(开局选难度流程), 由 HUD 接。
	TDelegate<void()> OnBackRequested;
	TDelegate<void()> OnDepartRequested;

private:
	enum class ESection : uint8 { Requisition, Loadout, Warehouse, Talent, Recovery };

	// 当前列表每行指向的物品(HandleRowClicked 按 Index 查)。
	struct FRowRef
	{
		FName Id;
		bool bConsumable = false;
	};

	void BuildWidgetTree();
	void ShowSection(ESection Section);
	void RebuildContent();
	void RefreshGold();
	void RefreshSummary();
	void RefreshAll();

	UGT_MetaProgressSubsystem* GetMeta() const;
	UTexture2D* LoadUiTex(const FString& AssetPath);
	UTexture2D* IconForEquip(FName Id) const;
	UTexture2D* IconForConsumable(FName Id) const;

	// 往内容列表加一行(图标 + 名称/效果 + 右侧价或状态 + 是否可点/是否高亮)。
	void AddItemRow(int32 Index, UTexture2D* Icon, const FString& Name, const FString& Effect,
		const FString& Right, bool bEnabled, bool bHighlight);
	UButton* MakeFooterButton(UHorizontalBox* Row, const FString& Label, const FString& AssetPath);
	UButton* MakeNavButton(UHorizontalBox* Row, const FString& Label);

	UFUNCTION() void OnNavRequisition();
	UFUNCTION() void OnNavLoadout();
	UFUNCTION() void OnNavWarehouse();
	UFUNCTION() void OnNavTalent();
	UFUNCTION() void OnNavRecovery();
	UFUNCTION() void OnBackClicked();
	UFUNCTION() void OnDepartClicked();
	UFUNCTION() void HandleRowClicked(int32 Index);

	UPROPERTY(Transient) UTextBlock* GoldText = nullptr;
	UPROPERTY(Transient) UTextBlock* TitleText = nullptr;
	UPROPERTY(Transient) UScrollBox* ContentScroll = nullptr;
	UPROPERTY(Transient) UVerticalBox* ContentList = nullptr;
	UPROPERTY(Transient) UVerticalBox* SummaryBox = nullptr;
	UPROPERTY(Transient) UTextBlock* HintText = nullptr;

	UPROPERTY(Transient) TMap<FString, UTexture2D*> TexCache;

	TArray<FRowRef> CurrentRows;
	ESection CurrentSection = ESection::Requisition;
};
