#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GT_DeployTerminalWidget.generated.h"

class UTextBlock;
class UScrollBox;
class UVerticalBox;
class UHorizontalBox;
class UWrapBox;
class UBorder;
class UButton;
class UTexture2D;
class UGT_MetaProgressSubsystem;

// 局外部署终端(S5 元终端 UI, Phase 1): 严格照原版构图——
// 顶栏(左 返回主界面 + 右 五个导航页签) / 面包屑 / 主面板(terminal_main 金边框:
// 标题+账户 / 筛选条 / 卡片网格 / 底部当前选中条) / 右侧出勤摘要(summary 框) / 右下 确认出发。
// 纯 C++ UMG, 读写 S1 UGT_MetaProgressSubsystem。Phase 2 补 仓库/天赋/抢救页 + 筛选功能。
UCLASS()
class GRAYTAIL_API UGT_DeployTerminalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

	void Open();
	void Close();
	bool IsOpen() const;

	TDelegate<void()> OnBackRequested;
	TDelegate<void()> OnDepartRequested;

private:
	enum class ESection : uint8 { Requisition, Loadout, Warehouse, Talent, Recovery };

	enum class ERowKind : uint8 { Equip, Consumable, Talent, Warehouse };

	struct FRowRef
	{
		FName Id;
		ERowKind Kind = ERowKind::Equip;
	};

	void BuildWidgetTree();
	void ShowSection(ESection Section);
	void RebuildContent();
	void RefreshAccount();
	void RefreshSummary();
	void RefreshAll();

	UGT_MetaProgressSubsystem* GetMeta() const;
	UTexture2D* LoadUiTex(const FString& AssetPath);
	UTexture2D* IconForEquip(FName Id) const;
	UTexture2D* IconForConsumable(FName Id) const;
	UTexture2D* IconForTalent(FName Id) const;
	UTexture2D* IconForWarehouse(FName Id) const;

	// 回收资历页(纯统计, 非卡片): 在内容区放一块宽统计面板。
	void AddRecoveryPanel();

	// 卡片: 图标 + 名称 + 类型 + 效果 + flavor 描述 + 拥有/价格行 + (状态左 / 动作按钮右)。圆角 RoundedBox。
	void AddItemCard(int32 Index, UTexture2D* Icon, const FString& Name, const FString& TypeLine,
		const FString& Effect, const FString& Flavor, const FString& InfoLine, const FString& StatusLine,
		const FString& ActionLabel, bool bActionEnabled, bool bHighlight);
	// 顶栏导航/底栏按钮: 用 deploy 贴图(标签烤在图里), 显式传原生尺寸 1:1 不拉伸。
	// (这些按钮图带 mipmap+流送, 运行时 Tex->GetSizeX() 早期返回 32 占位, 故尺寸不取运行时值, 用实测原生像素。)
	// OutHighlight 非空时: 把按钮外包一层圆角 Border 当"当前页签"高亮框, 通过它回传(导航用)。
	UButton* MakeTexButton(UHorizontalBox* Row, const FString& Label, const FString& TexPath, float NativeW, float NativeH, float Pad, UBorder** OutHighlight = nullptr);
	// 导航高亮框状态: 0=清除 1=选中(金边) 2=悬停(灰框)。
	void SetNavHighlight(UBorder* Target, uint8 State);
	// 重建当前页签的筛选药丸(分类, 可点击筛选)。
	void RebuildFilterPills();
	// 加一颗可点的筛选药丸(Index = FilterPillKeys 下标)。
	void AddFilterPill(int32 Index, const FString& Label, bool bSelected);
	// 当前筛选是否放行某物品(类型键 type_equip/type_consumable/type_recovered + 档位键 tier_*)。
	bool PassesFilter(FName TypeKey, FName TierKey) const;
	// 把贴图设成 Border 的 9-slice 金边框(MarginFrac = 边框占贴图比例)。
	void Apply9Slice(UBorder* Target, const FString& TexPath, float MarginFrac);

	UFUNCTION() void OnNavRequisition();
	UFUNCTION() void OnNavLoadout();
	UFUNCTION() void OnNavWarehouse();
	UFUNCTION() void OnNavTalent();
	UFUNCTION() void OnNavRecovery();
	UFUNCTION() void OnBackClicked();
	UFUNCTION() void OnDepartClicked();
	UFUNCTION() void HandleRowClicked(int32 Index);
	UFUNCTION() void HandleFilterClicked(int32 Index);

	UPROPERTY(Transient) UTextBlock* SectionTitleText = nullptr;
	UPROPERTY(Transient) UHorizontalBox* FilterRow = nullptr;
	UPROPERTY(Transient) UTextBlock* AccountText = nullptr;
	// 五个导航页签的高亮框/按钮(与 NavSections 平行), 选中套金边、悬停套灰框。
	UPROPERTY(Transient) TArray<UBorder*> NavHighlights;
	UPROPERTY(Transient) TArray<UButton*> NavButtons;
	TArray<ESection> NavSections;
	TArray<uint8> NavVisualState;        // 上一帧各页签高亮态(变化才重设刷, 免抖动)
	// 当前筛选键 + 当前页签药丸键表(与药丸下标对应)。
	FName CurrentFilter = FName(TEXT("all"));
	TArray<FName> FilterPillKeys;
	UPROPERTY(Transient) UScrollBox* ContentScroll = nullptr;
	UPROPERTY(Transient) UWrapBox* ContentWrap = nullptr;
	UPROPERTY(Transient) UVerticalBox* SummaryBox = nullptr;
	UPROPERTY(Transient) UTextBlock* DetailText = nullptr;

	UPROPERTY(Transient) TMap<FString, UTexture2D*> TexCache;

	TArray<FRowRef> CurrentRows;
	ESection CurrentSection = ESection::Requisition;
};
