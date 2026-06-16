#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Domains/Map/GT_MapTypes.h"
#include "GT_GameHudWidget.generated.h"

class UBorder;
class UTextBlock;
class UButton;
class UHorizontalBox;
class UVerticalBox;
class UProgressBar;
class UUniformGridPanel;
class UTexture2D;
class UGT_DebugSubsystem;
class UGT_RunContext;
class UGT_RoomViewWidget;
class UGT_MapOverlayWidget;
class UGT_LootResultWidget;
class UGT_EventPanelWidget;
class UGT_MainMenuWidget;
class UGT_DeployTerminalWidget;
class UGT_TutorialPopupWidget;

// 主游戏界面(对齐 Lua 原版构图): 房间视图铺满全屏为背景,
// 左侧信息面板/右上协议面板/底部快捷键栏全部悬浮其上。
// 纯 C++ 搭 UMG 树(无蓝图资产), 入口: 控制台 gt.HUD。
UCLASS()
class GRAYTAIL_API UGT_GameHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

private:
	void BuildWidgetTree();
	UGT_DebugSubsystem* GetDebugSubsystem() const;
	const UGT_RunContext* GetRunContext() const;
	void RefreshAll();
	void RefreshPanels();
	void RefreshStatusPanel();
	void RefreshMiniMapGrid();
	void RefreshMineRiskTag();
	void RefreshItemsList();
	void RefreshRunEndPanel();
	UTexture2D* GetItemIcon(FName ItemId);
	UTexture2D* LoadUiTexture(const FString& AssetPath);
	// 把原版边框贴图设为面板背景刷(背景刷不参与尺寸计算, 不会撑大面板)。
	// 给 TextureSize+FramePx 时按 9-slice 绘制(四角原始像素, 只拉中段), 否则整图拉伸。
	UWidget* MakeSkinnedPanel(UBorder* Panel, const FString& AssetPath,
		const FVector2D& TextureSize = FVector2D::ZeroVector, float FramePx = 0.f);
	UButton* MakeButton(UHorizontalBox* Row, const FString& Label);
	UTextBlock* MakePanelText(UVerticalBox* Panel, int32 FontSize, const FLinearColor& Color);

	UFUNCTION() void OnSearch();
	UFUNCTION() void OnExtract();
	UFUNCTION() void OnNewRun();
	UFUNCTION() void OnReturnToMenu();

	// 主菜单回调: 选难度开局。
	void HandleMenuStartRequested(EGT_Difficulty Difficulty);

	// 部署终端(局外)导航: 装备天赋入口打开 / 返回主菜单 / 出发探索(转难度选择)。
	void HandleDeployRequested();
	void HandleDeployBack();
	void HandleDeployDepart();

	// 过门换房后的统一回调: 刷新信息面板 + 驱动新手教程弹窗(若在教程局)。
	void HandleRoomChanged();

	// ---- 新手教程教学弹窗驱动(对齐 Lua Tutorial: 坐标→弹窗, blocking 锁操作, once 去重) ----
	// 纯表现层: 不动内核, 只按玩家所在格弹文案。仅 LastDifficulty==Tutorial 的局生效。
	void TutorialReset();                       // 开局重置: 按当前难度决定是否激活, 清已展示集合。
	void TutorialEnterRoom(int32 X, int32 Y);   // 进入新房: 关旧提示, 弹该格教学弹窗。
	void TutorialConfirm();                      // blocking 弹窗确认: 记 once、关弹窗、还焦点给房间。

	void OpenMapOverlay();
	void HandleMapOverlayClosed();
	void HandleLootResultClosed();
	void OpenEventPanel();
	void HandleEventPanelClosed();
	// Q 键使用消耗品(默认止血贴): 发命令走真规则, 刷新血条/背包。
	void OnUseConsumable();

	UPROPERTY(Transient) UGT_RoomViewWidget* RoomView = nullptr;
	UPROPERTY(Transient) UGT_MapOverlayWidget* MapOverlay = nullptr;
	UPROPERTY(Transient) UGT_LootResultWidget* LootResult = nullptr;
	UPROPERTY(Transient) UGT_EventPanelWidget* EventPanel = nullptr;
	UPROPERTY(Transient) UUniformGridPanel* MiniMapGrid = nullptr;
	// 周围雷险标牌(房间正下方, 中央列内; 未扫描时 Hidden 占位防房间缩放跳动)。
	UPROPERTY(Transient) UWidget* MineRiskRoot = nullptr;
	UPROPERTY(Transient) UTextBlock* MineRiskText = nullptr;
	UPROPERTY(Transient) UProgressBar* HpBar = nullptr;
	UPROPERTY(Transient) UTextBlock* HpText = nullptr;
	// 属性行按原版分色: 战斗力白 / 待结算金 / 已锁定绿 / 回收物青 / 已搜索灰。
	UPROPERTY(Transient) UTextBlock* PowerText = nullptr;
	UPROPERTY(Transient) UTextBlock* PendingText = nullptr;
	UPROPERTY(Transient) UTextBlock* SafeText = nullptr;
	UPROPERTY(Transient) UTextBlock* PartsText = nullptr;
	UPROPERTY(Transient) UTextBlock* SearchedText = nullptr;
	UPROPERTY(Transient) UTextBlock* StateText = nullptr;
	UPROPERTY(Transient) UVerticalBox* ItemsList = nullptr;
	UPROPERTY(Transient) UTextBlock* LogText = nullptr;
	UPROPERTY(Transient) UTextBlock* ProtocolText = nullptr;

	// 局终结算弹窗(死亡/撤离后弹出, 含 重新出发/返回菜单 按钮)。
	UPROPERTY(Transient) UWidget* RunEndRoot = nullptr;
	UPROPERTY(Transient) UBorder* RunEndFrame = nullptr;
	UPROPERTY(Transient) UTextBlock* RunEndTitle = nullptr;
	UPROPERTY(Transient) UTextBlock* RunEndBody = nullptr;
	UPROPERTY(Transient) UButton* RunEndButton = nullptr;
	bool bRunEndShown = false;

	// 主菜单(最顶层): 无局时显示, 选难度后开局; "重新出发"沿用上次选的难度。
	UPROPERTY(Transient) UGT_MainMenuWidget* MainMenu = nullptr;
	// 局外部署终端(装备天赋入口打开; 在主菜单之上)。
	UPROPERTY(Transient) UGT_DeployTerminalWidget* DeployTerminal = nullptr;
	EGT_Difficulty LastDifficulty = EGT_Difficulty::Standard;

	// 新手教程教学弹窗(最顶层): blocking 模态抢焦点暂停移动, 非blocking 顶部提示条。
	UPROPERTY(Transient) UGT_TutorialPopupWidget* TutorialPopup = nullptr;
	bool bTutorialActive = false;                 // 本局是否教程局(LastDifficulty==Tutorial)。
	FIntPoint TutorialCurrentRoom = FIntPoint(-1, -1);  // 当前所在教程格(去重, 同格不重弹)。
	FName TutorialActivePopupId = NAME_None;       // 正在显示的弹窗 Id(NAME_None=无)。
	TSet<FName> TutorialShownOnce;                 // once 弹窗已展示集合(确认后记入, 不再弹)。

	// UI 贴图资产缓存(key = /Game 包路径, 防 GC)。
	UPROPERTY(Transient) TMap<FString, UTexture2D*> UiTextureCache;
};
