#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Domains/Inventory/GT_InventoryTypes.h"
#include "GT_LootResultWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;
class UTexture2D;

// 搜索/开箱结果弹窗(对齐 Lua main.lua DrawLootResultPanel + drawLootItemCard):
// 标题/副标题按是否宝箱切换, 汇总行(结算币/回收物/估值), 物品卡片列表
// (稀有度描边与配色, 图标/名称/类别/效果/描述/估值)。
// Enter/F/Esc 或点击任意处关闭。入口: HUD 搜索命令被接受后 Open。
UCLASS()
class GRAYTAIL_API UGT_LootResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

	void Open(const FGT_SearchOutcome& Outcome);
	void Close();

	// 关闭后回调(HUD 整体刷新并还焦点给房间视图)。
	FSimpleDelegate OnClosed;

private:
	void BuildWidgetTree();
	void RebuildContent(const FGT_SearchOutcome& Outcome);
	void AddItemCard(const FGT_ItemStack& Stack);
	UTexture2D* LoadUiTexture(const FString& AssetPath);

	UPROPERTY(Transient) UBorder* PanelFrame = nullptr;
	UPROPERTY(Transient) UTextBlock* TitleText = nullptr;
	UPROPERTY(Transient) UTextBlock* SubtitleText = nullptr;
	UPROPERTY(Transient) UTextBlock* SummaryText = nullptr;
	UPROPERTY(Transient) UVerticalBox* ItemsBox = nullptr;

	// UI 贴图资产缓存(key = /Game 包路径, 防 GC)。
	UPROPERTY(Transient) TMap<FString, UTexture2D*> UiTextureCache;
};
