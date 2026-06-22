#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Domains/Map/GT_MapTypes.h"
#include "GT_MainMenuWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UTextBlock;
class UTexture2D;

// 主菜单(对齐 Lua 原版: 整图背景 + 画面元素上的点击热区), 三页:
//   主页   = menu_bg(文字烤在图里): 出发探索/新手教程/装备天赋/设置 四热区,
//            后三项尚未开放(装备/天赋 = 局外组部署终端的预留入口);
//   区域页 = main_menu_bg_no_text: 木牌"选择区域", 中型 10×10 / 大型 13×13 / 返回;
//   难度页 = 同背景: 木牌"选择难度", 三档随所选区域切换 + 返回。
// 难度阶梯按 难度判断.md §16(共 6 档, 不含教学):
//   中型: 简单=Easy(3撤离,§16.3) 标准=Standard(2撤离,§16.4) 困难=Hard(1撤离,§16.5)
//   大型: 普通=Veteran(4撤离,§16.6) 困难=Elite(2撤离,§16.7) 地狱=Nightmare(1撤离,§16.8)
// 热区/文字用百分比锚点钉在木板上(随整图拉伸对位), 倾斜用 shear 错切
// (字直行斜, 贴合美术里烤字的透视画法; 旋转会把字也转歪)。
// 表现层薄壳: 只回调 OnStartRequested(难度), 开局由 HUD 走命令管线。
UCLASS()
class GRAYTAIL_API UGT_MainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

	void Open();
	void Close();
	bool IsOpen() const;
	// 从部署终端「出发探索」回来: 打开并直接进区域选择页。
	void OpenToDepart();

	// 选难度开局(HUD 负责发命令); 参数 = 内核难度档。
	TDelegate<void(EGT_Difficulty)> OnStartRequested;

	// 「装备天赋」入口: 打开局外部署终端(HUD 接)。
	TDelegate<void()> OnDeployRequested;

	// 「设置」入口: 打开设置面板(含作弊模式总开关; HUD 接)。
	TDelegate<void()> OnSettingsRequested;

private:
	// 页面: 主页(四入口) / 区域页(地图大小) / 难度页(三档, 随区域切换)。
	enum class EPage : uint8 { Main, Size, Difficulty };

	void BuildWidgetTree();
	UCanvasPanel* BuildPageCanvas(class UOverlay* Root, const FString& BgAssetPath);
	// 左上吊牌标题(自绘页专用), 返回牌面 Border(校准时改锚点/角度)。
	UBorder* AddPageTitle(UCanvasPanel* Canvas, const FString& TitleText);
	// 在木板位置放一块热区 Border(shear 错切成贴板的平行四边形);
	// LabelText 为空 = 文字已烤在背景图里, 只做高亮。
	UBorder* MakePlank(UCanvasPanel* Canvas, const FAnchors& Anchors, float ShearAngle, const FString& LabelText, TArray<UTextBlock*>& OutLabels);
	UTextBlock* MakeHintLine(UCanvasPanel* Canvas, const FAnchors& Anchors, int32 FontSize);
	UTexture2D* LoadUiTexture(const FString& AssetPath);

	void ShowPage(EPage Page);
	void SetSelection(int32 NewIndex);
	void RefreshOptionStyles();
	void ConfirmSelection();
	// 鼠标位置命中当前页哪块木板(没有返回 INDEX_NONE)。
	int32 HitTestOptions(const FVector2D& ScreenPosition) const;
	int32 CurrentOptionCount() const;
	const TArray<UBorder*>& CurrentPlanks() const;
	// 当前选中项的底部提示文字。
	FString CurrentHint(int32 Index) const;

	// ---- F10 校准模式: 现场用键盘对齐热区, 数值存 GameUserSettings.ini(本机) ----
	// 主页校准主页门, 难度页校准城堡门+木牌(区域页与难度页共用城堡锚点, 自动跟齐)。
	void ToggleTuneMode();
	bool HandleTuneKey(const FKeyEvent& InKeyEvent);
	// 当前页 TuneIndex 指向的 锚点/错切 指针(木牌返回 nullptr 错切, 用 CastleTitleAngle)。
	// 区域页板 0/1/2 映射到共用城堡板 0/1/3, 改的就是城堡数据(难度页自动跟齐)。
	FAnchors* TuneAnchorPtr();
	FVector2D* TuneShearPtr();
	int32 TunePlankCount() const;
	bool PageHasTitle() const { return CurrentPage != EPage::Main; }
	// 把工作副本(默认值/校准值)套到所有槽位与变换上。
	void ApplyTunables();
	void LoadTunablesFromConfig();
	void SaveTunablesToConfig() const;
	void DumpTunablesToLog() const;
	void RefreshTuneInfo();

	UPROPERTY(Transient) UCanvasPanel* PageMain = nullptr;
	UPROPERTY(Transient) UCanvasPanel* PageSize = nullptr;
	UPROPERTY(Transient) UCanvasPanel* PageDifficulty = nullptr;
	// 木板热区(透明 Border, 选中时罩暖光; 命中测试用其几何)。
	UPROPERTY(Transient) TArray<UBorder*> MainPlanks;
	UPROPERTY(Transient) TArray<UBorder*> SizePlanks;
	UPROPERTY(Transient) TArray<UBorder*> DiffPlanks;
	// 自绘文字(主页文字在背景图里, 无对应项; 难度页前三块随区域换文案)。
	UPROPERTY(Transient) TArray<UTextBlock*> SizeLabels;
	UPROPERTY(Transient) TArray<UTextBlock*> DiffLabels;
	// 底部提示行(每页一条): 当前选中项的说明 / 未开放提示。
	UPROPERTY(Transient) UTextBlock* HintMain = nullptr;
	UPROPERTY(Transient) UTextBlock* HintSize = nullptr;
	UPROPERTY(Transient) UTextBlock* HintDiff = nullptr;

	// UI 贴图资产缓存(key = /Game 包路径, 防 GC)。
	UPROPERTY(Transient) TMap<FString, UTexture2D*> UiTextureCache;

	EPage CurrentPage = EPage::Main;
	int32 SelectedIndex = 0;
	// 区域页选了大型(13×13)? 决定难度页的三档文案与映射。
	bool bLargeMap = false;

	// ---- 校准模式状态 ----
	// 槽位缓存(校准时改锚点用)。区域页板映射城堡板 {0,1,3}。
	UPROPERTY(Transient) TArray<class UCanvasPanelSlot*> MainPlankSlots;
	UPROPERTY(Transient) TArray<class UCanvasPanelSlot*> SizePlankSlots;
	UPROPERTY(Transient) TArray<class UCanvasPanelSlot*> DiffPlankSlots;
	UPROPERTY(Transient) class UCanvasPanelSlot* SizeTitleSlot = nullptr;
	UPROPERTY(Transient) class UCanvasPanelSlot* DiffTitleSlot = nullptr;
	UPROPERTY(Transient) UBorder* SizeTitlePlate = nullptr;
	UPROPERTY(Transient) UBorder* DiffTitlePlate = nullptr;
	// 校准信息面板(左上角, 仅校准模式可见)。
	UPROPERTY(Transient) UTextBlock* TuneInfoText = nullptr;

	// 锚点/错切工作副本: 默认 = 代码常量, 启动时被本机 ini 覆盖, F10 退出时回存。
	// 每块板各一份(不再整门共用), Shear=(横X, 纵Y)度: Y=左右高低, X=上下边水平错开(门倾)。
	TArray<FAnchors> MainAnchors;
	TArray<FAnchors> CastleAnchors;
	TArray<FVector2D> MainShears;
	TArray<FVector2D> CastleShears;
	FAnchors CastleTitleAnchors;
	float CastleTitleAngle = 6.5f;

	bool bTuneMode = false;
	// 0..3 = 当前门的四块板; 4 = 木牌标题(仅难度页)。
	int32 TuneIndex = 0;
	// 视口宽高比修正系数: 非 16:9 视口下背景被非等比拉伸, 斜度按比例跟随。
	float AspectShearScale = 1.f;
};
