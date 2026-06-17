#include "UI/GT_DeployTerminalWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"

#include "Domains/Meta/GT_MetaCatalog.h"
#include "Domains/Meta/GT_MetaProgressSubsystem.h"
#include "Domains/Meta/GT_MetaTypes.h"
#include "Domains/Inventory/GT_ItemCatalog.h"
#include "Domains/Inventory/GT_InventoryTypes.h"
#include "UI/GT_IndexedButton.h"

namespace
{
	// 线性色: 渲染转 sRGB 会变亮, 故"深"填充用很小的线性值(否则显成中灰)。
	const FLinearColor GTColGold(0.95f, 0.72f, 0.30f);
	const FLinearColor GTColName(FColor(236, 232, 222));          // 物品名 暖白(最亮一层)
	const FLinearColor GTColWhite(0.90f, 0.90f, 0.92f);
	const FLinearColor GTColEffect(FColor(196, 206, 224));        // 效果行 冷白(中亮)
	const FLinearColor GTColDim(0.34f, 0.37f, 0.45f);
	const FLinearColor GTColFaint(0.23f, 0.25f, 0.31f);          // flavor 最暗一层
	const FLinearColor GTColBg(0.010f, 0.014f, 0.024f, 1.0f);     // 根背景 深灰蓝
	const FLinearColor GTColPanel(0.020f, 0.027f, 0.044f, 1.0f);  // 面板/药丸 深灰蓝
	const FLinearColor GTColCard(FColor(21, 29, 40));             // 卡片底(原版色, 比背景稍浅一点)
	const FLinearColor GTColCardEq(0.058f, 0.044f, 0.017f, 1.0f); // 已装备卡片(暗金调)
	const FLinearColor GTColLine(0.11f, 0.082f, 0.030f, 1.0f);    // 暗金描边
	const FLinearColor GTColLineEq(0.55f, 0.34f, 0.085f, 1.0f);   // 高亮金描边
	const FLinearColor GTColNavSel(FColor(255, 205, 90));         // 当前页签高亮框 亮金
	const FLinearColor GTColNavSelFill(0.95f, 0.72f, 0.30f, 0.12f); // 当前页签淡金底光
	const FLinearColor GTColNavHover(FColor(150, 156, 170));      // 悬停预选框 灰
	const FLinearColor GTColNavHoverFill(0.5f, 0.52f, 0.56f, 0.09f); // 悬停淡灰底光
	// 动作按钮(申领/出售/装备/解锁): 蓝底白字。
	const FLinearColor GTColBtn(FColor(90, 150, 200));
	const FLinearColor GTColBtnHover(FColor(120, 180, 225));
	const FLinearColor GTColBtnPress(FColor(70, 120, 165));
	const FLinearColor GTColBtnDisabled(FColor(52, 58, 70));
	const FLinearColor GTColBtnText(FColor(245, 248, 252));
	const FLinearColor GTColBtnTextDisabled(0.45f, 0.48f, 0.55f);

	// 物品稀有度(FName)-> 中文档位键。装备在 UI 层按 id 派生。
	FName RarityTierKey(FName Rarity)
	{
		const FString R = Rarity.ToString().ToLower();
		if (R == TEXT("rare")) { return FName(TEXT("tier_rare")); }
		if (R == TEXT("precious") || R == TEXT("epic")) { return FName(TEXT("tier_precious")); }
		if (R == TEXT("abnormal")) { return FName(TEXT("tier_abnormal")); }
		return FName(TEXT("tier_common")); // common / uncommon / low / 其它
	}

	// 6 件装备的稀有度(UI 派生, 申领页按价位粗分)。
	FName EquipTierKey(FName Id)
	{
		if (Id == FName(TEXT("armor")) || Id == FName(TEXT("whetstone"))) { return FName(TEXT("tier_common")); }
		return FName(TEXT("tier_rare")); // medkit/insulated_gloves/compass/backpack
	}

	FSlateFontInfo GTFont(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle("Regular", Size);
	}

	FString EquipEffectText(const FGT_EquipDef& Def)
	{
		if (Def.BonusHP > 0) { return FString::Printf(TEXT("最大生命 +%d"), Def.BonusHP); }
		if (Def.BonusPower > 0) { return FString::Printf(TEXT("战斗力 +%d"), Def.BonusPower); }
		if (Def.bMineImmunity) { return TEXT("首次踩雷免疫伤害"); }
		if (Def.MineDmgReduce > 0) { return FString::Printf(TEXT("雷险伤害 -%d"), Def.MineDmgReduce); }
		if (Def.bShowExitHint) { return TEXT("撤离信标方向提示"); }
		if (Def.SearchBonus > 0) { return FString::Printf(TEXT("搜索奖励 +%d%%"), Def.SearchBonus); }
		// S6 触发型装备(效果已实现, 走 EGT_ItemTrigger, 不是静态加成, 故单列描述)。
		switch (Def.Trigger)
		{
		case EGT_ItemTrigger::KillPowerStack:
			return FString::Printf(TEXT("击败异常体后 战斗力 +%d(至多叠 %d 层)"), Def.TriggerAmount, Def.TriggerCap);
		case EGT_ItemTrigger::ProtocolHeal:
			return FString::Printf(TEXT("协议每升一级 回复 %d 生命(至多 %d 次)"), Def.TriggerAmount, Def.TriggerCap);
		case EGT_ItemTrigger::SettleGoldBonus:
			return FString::Printf(TEXT("撤离结算 金币 +%d%%"), Def.TriggerAmount);
		case EGT_ItemTrigger::ChestBonusLoot:
			return FString::Printf(TEXT("进入宝箱房 额外掉落 %d 件低价值回收物(每房一次)"), Def.TriggerAmount);
		default:
			break;
		}
		return TEXT("");
	}

	FString TalentEffectText(const FGT_TalentDef& Def)
	{
		if (Def.bMapHighlight) { return TEXT("小地图邻域威胁高亮"); }
		if (Def.MineDmgReduce > 0) { return FString::Printf(TEXT("雷险伤害 -%d"), Def.MineDmgReduce); }
		if (Def.MonsterFleeBonus > 0) { return TEXT("怪物面对持有者时更易犹疑"); }
		if (Def.FailureGoldBonus > 0) { return FString::Printf(TEXT("撤离失败保留 +%d 金"), Def.FailureGoldBonus); }
		if (Def.TradePrice > 0) { return FString::Printf(TEXT("旅商收购价 +%d%%"), Def.TradePrice); }
		return TEXT("");
	}

	// 消耗品效果文案(按物品真效果, 非一律"回复 N 生命"——幸运硬币 Heal=0 不该显示"回复0生命")。
	FString ConsumableEffectText(const FGT_ConsumableDef& Def)
	{
		if (Def.Id == FName(TEXT("lucky_coin"))) { return TEXT("局内使用: 50% 得 30 结算币 / 50% 揭示相邻未知房"); }
		if (Def.Heal > 0) { return FString::Printf(TEXT("局内使用: 回复 %d 生命"), Def.Heal); }
		return TEXT("局内使用");
	}

	// 仓库条目来源 -> 中文类型行。
	FString WarehouseSourceLabel(FName Source)
	{
		if (Source == FName(TEXT("recovered"))) { return TEXT("回收物 · 可出售"); }
		if (Source == FName(TEXT("equipment"))) { return TEXT("作业装备"); }
		if (Source == FName(TEXT("consumable"))) { return TEXT("作业消耗品"); }
		return TEXT("库存物资");
	}

	// CanSellItem 的 OutReason -> 状态行文案。
	FString SellReasonLabel(FName Reason)
	{
		if (Reason == FName(TEXT("unique"))) { return TEXT("独有 · 不可售"); }
		if (Reason == FName(TEXT("not_sellable"))) { return TEXT("非回收物 · 不可售"); }
		if (Reason == FName(TEXT("equipped"))) { return TEXT("装备中 · 不可售"); }
		if (Reason == FName(TEXT("no_value"))) { return TEXT("无价值"); }
		return TEXT("不可出售");
	}

	// flavor 文本(照原版风格补写, 无对应原文的自拟)。
	FString EquipFlavor(FName Id)
	{
		if (Id == FName(TEXT("armor")))            { return TEXT("复合纤维背心, 挡得住流弹与近身啃咬, 是老兵进区的底气。"); }
		if (Id == FName(TEXT("whetstone")))        { return TEXT("粗粒油石, 临行前给刀刃补一道锋, 出手更狠更利落。"); }
		if (Id == FName(TEXT("medkit")))           { return TEXT("塞满止血凝胶与镇定剂, 关键时刻扛下踩空的第一脚雷。"); }
		if (Id == FName(TEXT("insulated_gloves"))) { return TEXT("厚橡胶绝缘手套, 削弱雷管回路的瞬时电流, 少挨点伤。"); }
		if (Id == FName(TEXT("compass")))          { return TEXT("磁针锁定撤离信标, 再慌乱也能摸到回家的方向。"); }
		if (Id == FName(TEXT("backpack")))         { return TEXT("加装侧袋与挂点, 翻找废墟时总能多摸出点值钱货。"); }
		return TEXT("制式作业装备, 后勤按需配发。");
	}

	FString ConsumableFlavor(FName Id)
	{
		if (Id == FName(TEXT("emergency_bandage"))) { return TEXT("单兵速效止血贴, 局内随用随贴, 回点血续命。"); }
		return TEXT("一次性作业消耗品, 进区前可备入背包。");
	}

	FString TalentFlavor(FName Id)
	{
		if (Id == FName(TEXT("talent_map")))     { return TEXT("长期作业练出的直觉, 危险还没靠近就先在小地图上发亮。"); }
		if (Id == FName(TEXT("talent_mine")))    { return TEXT("皮糙肉厚, 常年踩线练出的抗性, 雷险伤害都打了折。"); }
		if (Id == FName(TEXT("talent_monster"))) { return TEXT("气场摄人, 游荡的怪物总要多犹豫两秒才敢扑上来。"); }
		if (Id == FName(TEXT("talent_extract"))) { return TEXT("和后勤私下签的条款, 撤离失败也能多保住一笔结算币。"); }
		if (Id == FName(TEXT("talent_event")))   { return TEXT("舌灿莲花, 跟黑市贩子谈价从不吃亏, 出售更值钱。"); }
		return TEXT("长期作业沉淀下来的资历。");
	}
}

TSharedRef<SWidget> UGT_DeployTerminalWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

UGT_MetaProgressSubsystem* UGT_DeployTerminalWidget::GetMeta() const
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UGT_MetaProgressSubsystem>();
		}
	}
	return nullptr;
}

UTexture2D* UGT_DeployTerminalWidget::LoadUiTex(const FString& AssetPath)
{
	if (AssetPath.IsEmpty()) { return nullptr; }
	if (UTexture2D** Cached = TexCache.Find(AssetPath))
	{
		return *Cached;
	}
	const FString ObjectPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *ObjectPath);
	TexCache.Add(AssetPath, Tex);
	return Tex;
}

UTexture2D* UGT_DeployTerminalWidget::IconForEquip(FName Id) const
{
	UGT_DeployTerminalWidget* Self = const_cast<UGT_DeployTerminalWidget*>(this);
	if (Id == FName(TEXT("armor")))            { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_armor")); }
	if (Id == FName(TEXT("backpack")))         { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_backpack")); }
	if (Id == FName(TEXT("compass")))          { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_compass")); }
	if (Id == FName(TEXT("medkit")))           { return Self->LoadUiTex(TEXT("/Game/Graytail/Items/Consumable/item_consumable_medkit")); }
	if (Id == FName(TEXT("whetstone")))        { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_whetstone")); }
	if (Id == FName(TEXT("insulated_gloves"))) { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_insulated_gloves")); }
	// S6 局外组新装备图标(_import_loadout_icons.py 从 assets/item_loadout 导入)。
	if (Id == FName(TEXT("anomaly_fang")))     { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_anomaly_fang")); }
	if (Id == FName(TEXT("lockdown_crystal"))) { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_lockdown_crystal")); }
	if (Id == FName(TEXT("company_badge")))    { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_company_badge")); }
	if (Id == FName(TEXT("salvage_magnet")))   { return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_salvage_magnet")); }
	return nullptr;
}

UTexture2D* UGT_DeployTerminalWidget::IconForConsumable(FName Id) const
{
	UGT_DeployTerminalWidget* Self = const_cast<UGT_DeployTerminalWidget*>(this);
	if (Id == FName(TEXT("emergency_bandage")))
	{
		return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_bandage"));
	}
	// S6 幸运硬币图标。
	if (Id == FName(TEXT("lucky_coin")))
	{
		return Self->LoadUiTex(TEXT("/Game/Graytail/UI/deploy/ui_icon_lucky_coin"));
	}
	return nullptr;
}

UTexture2D* UGT_DeployTerminalWidget::IconForTalent(FName Id) const
{
	UGT_DeployTerminalWidget* Self = const_cast<UGT_DeployTerminalWidget*>(this);
	// id "talent_map" -> deploy/ui_icon_talent_map(切自 菜单图标 sheet)。
	FString Key = Id.ToString();
	Key.RemoveFromStart(TEXT("talent_"));
	return Self->LoadUiTex(FString::Printf(TEXT("/Game/Graytail/UI/deploy/ui_icon_talent_%s"), *Key));
}

UTexture2D* UGT_DeployTerminalWidget::IconForWarehouse(FName Id) const
{
	UGT_DeployTerminalWidget* Self = const_cast<UGT_DeployTerminalWidget*>(this);
	// 装备/消耗品类回收物先用部署终端自己的图标; 回收物走物品目录的图标路径。
	if (UTexture2D* Equip = IconForEquip(Id)) { return Equip; }
	if (UTexture2D* Cons = IconForConsumable(Id)) { return Cons; }
	const FString Path = GT_ItemCatalog::GetItemIconAssetPath(Id);
	return Path.IsEmpty() ? nullptr : Self->LoadUiTex(Path);
}

void UGT_DeployTerminalWidget::Apply9Slice(UBorder* Target, const FString& TexPath, float MarginFrac)
{
	if (!Target) { return; }
	UTexture2D* Tex = LoadUiTex(TexPath);
	if (!Tex)
	{
		Target->SetBrushColor(GTColPanel);
		return;
	}
	FSlateBrush Brush;
	Brush.SetResourceObject(Tex);
	Brush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.Margin = FMargin(MarginFrac);
	Target->SetBrush(Brush);
}

// 贴图按钮(标签烤在图里): 原生尺寸 1:1, 不拉伸不挤压。
// 关键: SizeBox 套在 UButton **外层**(同卡片图标那条已验证尺寸路径),
// 贴图设成按钮的 style 背景刷而非 content —— SButton 把背景刷铺满整块几何 = 1:1,
// 绕开 "UButton 包 SizeBox 时 content 尺寸不上传 → 被挤成 30-40px" 的坑。
UButton* UGT_DeployTerminalWidget::MakeTexButton(UHorizontalBox* Row, const FString& Label, const FString& TexPath, float NativeW, float NativeH, float Pad, UBorder** OutHighlight)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>();
	UTexture2D* Tex = LoadUiTex(TexPath);

	FButtonStyle Style = Btn->GetStyle();
	FSlateBrush TexBrush;
	if (Tex)
	{
		TexBrush.SetResourceObject(Tex);
		TexBrush.ImageSize = FVector2D(NativeW, NativeH);   // 用实测原生尺寸, 不取运行时 GetSizeX(早期=32)
		TexBrush.DrawAs = ESlateBrushDrawType::Image;       // 整刷铺满按钮几何
	}
	else
	{
		TexBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	}
	Style.Normal = TexBrush;
	Style.Pressed = TexBrush;
	Style.Disabled = TexBrush;
	// 悬停极淡提亮(无独立贴图时给点反馈); 按下略压暗。
	FSlateBrush HoverBrush = TexBrush;
	HoverBrush.TintColor = FSlateColor(FLinearColor(1.12f, 1.12f, 1.12f, 1.f));
	Style.Hovered = HoverBrush;
	Style.Pressed.TintColor = FSlateColor(FLinearColor(0.82f, 0.82f, 0.82f, 1.f));
	// content padding 清零(免得在背景刷外再加边距把外层 SizeBox 撑大)。
	Style.NormalPadding = FMargin(0.f);
	Style.PressedPadding = FMargin(0.f);
	Btn->SetStyle(Style);

	// 缺贴图时退化为文字按钮(放进按钮当 content); 有贴图时背景刷已自绘, 无需 content。
	if (!Tex)
	{
		UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
		Txt->SetText(FText::FromString(Label));
		Txt->SetFont(GTFont(15));
		Txt->SetColorAndOpacity(FSlateColor(GTColWhite));
		Btn->SetContent(Txt);
	}

	// SizeBox 在 UButton 外层强制原生尺寸 —— 与卡片图标同一条尺寸路径, 稳。
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(NativeW);
	Box->SetHeightOverride(NativeH);
	Box->SetContent(Btn);

	// 导航按钮再外包一层圆角高亮框(默认透明, 选中时画金边); 其余(返回/确认)直接放。
	UWidget* RowChild = Box;
	if (OutHighlight)
	{
		UBorder* Hl = WidgetTree->ConstructWidget<UBorder>();
		Hl->SetPadding(FMargin(4.f, 3.f));
		Hl->SetContent(Box);
		SetNavHighlight(Hl, 0);
		*OutHighlight = Hl;
		RowChild = Hl;
	}

	if (UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(RowChild))
	{
		HSlot->SetPadding(FMargin(Pad, 0.f));
		HSlot->SetVerticalAlignment(VAlign_Center);
	}
	return Btn;
}

void UGT_DeployTerminalWidget::SetNavHighlight(UBorder* Target, uint8 State)
{
	if (!Target) { return; }
	FSlateBrush Br;
	Br.DrawAs = ESlateBrushDrawType::RoundedBox;
	Br.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Br.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
	if (State == 1) // 选中: 金边
	{
		Br.TintColor = FSlateColor(GTColNavSelFill);
		Br.OutlineSettings.Color = FSlateColor(GTColNavSel);
		Br.OutlineSettings.Width = 2.f;
	}
	else if (State == 2) // 悬停: 灰预选框
	{
		Br.TintColor = FSlateColor(GTColNavHoverFill);
		Br.OutlineSettings.Color = FSlateColor(GTColNavHover);
		Br.OutlineSettings.Width = 2.f;
	}
	else // 清除
	{
		Br.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		Br.OutlineSettings.Color = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		Br.OutlineSettings.Width = 0.f;
	}
	Target->SetBrush(Br);
}

void UGT_DeployTerminalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 导航页签视觉态: 选中(1) > 悬停(2) > 清除(0); 仅在变化时重设刷, 免每帧抖动。
	if (NavVisualState.Num() != NavHighlights.Num())
	{
		NavVisualState.Init(255, NavHighlights.Num());
	}
	for (int32 i = 0; i < NavHighlights.Num(); ++i)
	{
		const bool bSel = NavSections.IsValidIndex(i) && NavSections[i] == CurrentSection;
		const bool bHov = NavButtons.IsValidIndex(i) && NavButtons[i] && NavButtons[i]->IsHovered();
		const uint8 Want = bSel ? 1 : (bHov ? 2 : 0);
		if (NavVisualState[i] != Want)
		{
			NavVisualState[i] = Want;
			SetNavHighlight(NavHighlights[i], Want);
		}
	}
}

namespace
{
	// 各页签的筛选分类(键, 中文标签)。键: all / type_* / tier_*。
	struct FFilterPill { const TCHAR* Key; const TCHAR* Label; };
	const FFilterPill GTReqPills[] = {
		{ TEXT("all"), TEXT("全部") }, { TEXT("tier_common"), TEXT("一般") }, { TEXT("tier_rare"), TEXT("稀有") },
		{ TEXT("type_equip"), TEXT("作业装备") }, { TEXT("type_consumable"), TEXT("消耗") },
	};
	const FFilterPill GTWarehousePills[] = {
		{ TEXT("all"), TEXT("全部") }, { TEXT("tier_common"), TEXT("一般") }, { TEXT("tier_rare"), TEXT("稀有") },
		{ TEXT("tier_precious"), TEXT("史诗") }, { TEXT("tier_abnormal"), TEXT("异常") },
		{ TEXT("type_equip"), TEXT("作业装备") }, { TEXT("type_consumable"), TEXT("消耗") }, { TEXT("type_recovered"), TEXT("回收") },
	};
}

// 可点击筛选药丸: 选中态金底金字, 否则深底灰字。
void UGT_DeployTerminalWidget::AddFilterPill(int32 Index, const FString& Label, bool bSelected)
{
	if (!FilterRow) { return; }
	UGT_IndexedButton* Pill = WidgetTree->ConstructWidget<UGT_IndexedButton>();
	Pill->Index = Index;
	Pill->OnIndexClicked.AddDynamic(this, &UGT_DeployTerminalWidget::HandleFilterClicked);

	FButtonStyle Style = Pill->GetStyle();
	auto MakePillBrush = [](const FLinearColor& Fill, const FLinearColor& Outline)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Fill);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		B.OutlineSettings.CornerRadii = FVector4(7.f, 7.f, 7.f, 7.f);
		B.OutlineSettings.Color = FSlateColor(Outline);
		B.OutlineSettings.Width = 1.f;
		return B;
	};
	Style.Normal = MakePillBrush(bSelected ? GTColNavSelFill : GTColPanel, bSelected ? GTColNavSel : GTColLine);
	Style.Hovered = MakePillBrush(bSelected ? GTColNavSelFill : GTColCard, bSelected ? GTColNavSel : GTColNavHover);
	Style.Pressed = Style.Normal;
	Style.NormalPadding = FMargin(12.f, 4.f);
	Style.PressedPadding = FMargin(12.f, 4.f);
	Pill->SetStyle(Style);

	UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
	Txt->SetText(FText::FromString(Label));
	Txt->SetFont(GTFont(13));
	Txt->SetColorAndOpacity(FSlateColor(bSelected ? GTColGold : GTColDim));
	Pill->SetContent(Txt);
	if (UHorizontalBoxSlot* HSlot = FilterRow->AddChildToHorizontalBox(Pill))
	{
		HSlot->SetPadding(FMargin(0.f, 0.f, 7.f, 0.f));
		HSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UGT_DeployTerminalWidget::RebuildFilterPills()
{
	if (!FilterRow) { return; }
	FilterRow->ClearChildren();
	FilterPillKeys.Reset();

	const FFilterPill* Pills = nullptr;
	int32 Count = 0;
	if (CurrentSection == ESection::Warehouse) { Pills = GTWarehousePills; Count = UE_ARRAY_COUNT(GTWarehousePills); }
	else if (CurrentSection == ESection::Requisition || CurrentSection == ESection::Loadout) { Pills = GTReqPills; Count = UE_ARRAY_COUNT(GTReqPills); }
	for (int32 i = 0; i < Count; ++i)
	{
		const FName Key(Pills[i].Key);
		FilterPillKeys.Add(Key);
		AddFilterPill(i, Pills[i].Label, Key == CurrentFilter);
	}
}

bool UGT_DeployTerminalWidget::PassesFilter(FName TypeKey, FName TierKey) const
{
	if (CurrentFilter == FName(TEXT("all"))) { return true; }
	const FString F = CurrentFilter.ToString();
	if (F.StartsWith(TEXT("type_"))) { return CurrentFilter == TypeKey; }
	if (F.StartsWith(TEXT("tier_"))) { return CurrentFilter == TierKey; }
	return true;
}

void UGT_DeployTerminalWidget::HandleFilterClicked(int32 Index)
{
	if (!FilterPillKeys.IsValidIndex(Index)) { return; }
	CurrentFilter = FilterPillKeys[Index];
	RebuildFilterPills();
	RebuildContent();
}

void UGT_DeployTerminalWidget::BuildWidgetTree()
{
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DeployRoot"));
	Root->SetBrushColor(GTColBg);
	Root->SetPadding(FMargin(34.f, 22.f));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->SetContent(Main);

	// === 顶栏: 左 返回主界面 + 右 五个导航页签(贴图自带标签) ===
	UHorizontalBox* TopBar = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(TopBar)) { S->SetPadding(FMargin(0, 0, 0, 6)); }
	MakeTexButton(TopBar, TEXT("返回主界面"), TEXT("/Game/Graytail/UI/deploy/ui_button_back_main"), 194.f, 56.f, 0.f)
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnBackClicked);
	USpacer* TopSpacer = WidgetTree->ConstructWidget<USpacer>();
	if (UHorizontalBoxSlot* S = TopBar->AddChildToHorizontalBox(TopSpacer)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }

	// 导航页签: 各自带高亮框(选中金边/悬停灰框); NavButtons/NavHighlights/NavSections 平行登记。
	// (AddDynamic 需字面函数名, 不能用函数指针变量, 故逐个展开。)
	NavButtons.Reset();
	NavHighlights.Reset();
	NavSections.Reset();
	NavVisualState.Reset();
	UBorder* Hl = nullptr;
	UButton* NavB = nullptr;
	NavB = MakeTexButton(TopBar, FString(), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_warehouse"), 154.f, 56.f, 4.f, &Hl);
	NavB->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavWarehouse);
	NavButtons.Add(NavB); NavHighlights.Add(Hl); NavSections.Add(ESection::Warehouse);
	NavB = MakeTexButton(TopBar, FString(), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_requisition"), 148.f, 56.f, 4.f, &Hl);
	NavB->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavRequisition);
	NavButtons.Add(NavB); NavHighlights.Add(Hl); NavSections.Add(ESection::Requisition);
	NavB = MakeTexButton(TopBar, FString(), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_loadout"), 141.f, 56.f, 4.f, &Hl);
	NavB->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavLoadout);
	NavButtons.Add(NavB); NavHighlights.Add(Hl); NavSections.Add(ESection::Loadout);
	NavB = MakeTexButton(TopBar, FString(), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_recovery"), 170.f, 60.f, 4.f, &Hl);
	NavB->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavRecovery);
	NavButtons.Add(NavB); NavHighlights.Add(Hl); NavSections.Add(ESection::Recovery);
	NavB = MakeTexButton(TopBar, FString(), TEXT("/Game/Graytail/UI/deploy/ui_button_nav_talent_selected"), 228.f, 61.f, 4.f, &Hl);
	NavB->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnNavTalent);
	NavButtons.Add(NavB); NavHighlights.Add(Hl); NavSections.Add(ESection::Talent);

	// === 主体: 左主面板(terminal_main 金边框) + 右出勤摘要(summary 框) ===
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(Body)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }

	UBorder* MainPanel = WidgetTree->ConstructWidget<UBorder>();
	Apply9Slice(MainPanel, TEXT("/Game/Graytail/UI/common/ui_panel_terminal_main"), 0.07f);
	// 内边距加大, 让标题/卡片/底部说明都缩在金边框内, 不压到贴图的边框与底部分隔条。
	MainPanel->SetPadding(FMargin(44.f, 34.f, 44.f, 54.f));
	if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(MainPanel)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetPadding(FMargin(0, 0, 14, 0)); }
	UVerticalBox* MainCol = WidgetTree->ConstructWidget<UVerticalBox>();
	MainPanel->SetContent(MainCol);

	// -- 标题行: 章节名 + 账户 --
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = MainCol->AddChildToVerticalBox(HeaderRow)) { S->SetPadding(FMargin(0, 0, 0, 10)); }
	SectionTitleText = WidgetTree->ConstructWidget<UTextBlock>();
	SectionTitleText->SetFont(GTFont(24));
	SectionTitleText->SetColorAndOpacity(FSlateColor(GTColGold));
	if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(SectionTitleText)) { S->SetVerticalAlignment(VAlign_Center); }
	USpacer* HeadSpacer = WidgetTree->ConstructWidget<USpacer>();
	if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(HeadSpacer)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
	if (UTexture2D* GoldIcon = LoadUiTex(TEXT("/Game/Graytail/UI/common/ui_icon_account_gold")))
	{
		UImage* Img = WidgetTree->ConstructWidget<UImage>();
		Img->SetBrushFromTexture(GoldIcon);
		Img->SetDesiredSizeOverride(FVector2D(24.f, 24.f));
		if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(Img)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0, 0, 6, 0)); }
	}
	AccountText = WidgetTree->ConstructWidget<UTextBlock>();
	AccountText->SetFont(GTFont(16));
	AccountText->SetColorAndOpacity(FSlateColor(GTColWhite));
	if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(AccountText)) { S->SetVerticalAlignment(VAlign_Center); }

	// -- 筛选条(分类, 可点击筛选; 申领/仓库各一套, ShowSection 里重建) --
	FilterRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = MainCol->AddChildToVerticalBox(FilterRow)) { S->SetPadding(FMargin(0, 0, 0, 12)); }

	// -- 卡片网格(滚动): 卡片只铺在背景图上半大块内, 超出由 ScrollBox 滚动(不向下溢出盖住下半块) --
	ContentScroll = WidgetTree->ConstructWidget<UScrollBox>();
	if (UVerticalBoxSlot* S = MainCol->AddChildToVerticalBox(ContentScroll)) { FSlateChildSize Sz(ESlateSizeRule::Fill); Sz.Value = 1.0f; S->SetSize(Sz); }
	ContentWrap = WidgetTree->ConstructWidget<UWrapBox>();
	ContentScroll->AddChild(ContentWrap);

	// -- 底部当前选中条 --
	DetailText = WidgetTree->ConstructWidget<UTextBlock>();
	DetailText->SetFont(GTFont(13));
	DetailText->SetColorAndOpacity(FSlateColor(GTColDim));
	DetailText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* S = MainCol->AddChildToVerticalBox(DetailText)) { S->SetPadding(FMargin(2, 10, 0, 0)); }

	// -- 底部留空区: 背景图 ~76% 处有分隔线分上下两块, 这块 Fill Spacer 占住下半小块使其留空,
	//    把内容区(卡片+选中条)压在上半大块内。与 ContentScroll 同为 Fill, 按权重 1.0 : 0.33 ≈ 76% : 24% 分配剩余高度。PIE 可调。
	USpacer* BottomReserve = WidgetTree->ConstructWidget<USpacer>();
	if (UVerticalBoxSlot* S = MainCol->AddChildToVerticalBox(BottomReserve)) { FSlateChildSize Sz(ESlateSizeRule::Fill); Sz.Value = 0.33f; S->SetSize(Sz); }

	// 右侧 出勤摘要
	USizeBox* SummarySize = WidgetTree->ConstructWidget<USizeBox>();
	SummarySize->SetWidthOverride(300.f);
	Body->AddChildToHorizontalBox(SummarySize);
	UBorder* SummaryPanel = WidgetTree->ConstructWidget<UBorder>();
	Apply9Slice(SummaryPanel, TEXT("/Game/Graytail/UI/deploy/ui_panel_deploy_summary_blank"), 0.12f);
	SummaryPanel->SetPadding(FMargin(20.f, 18.f));
	SummarySize->SetContent(SummaryPanel);
	SummaryBox = WidgetTree->ConstructWidget<UVerticalBox>();
	SummaryPanel->SetContent(SummaryBox);

	// === 底栏: 右下 确认出发 ===
	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Main->AddChildToVerticalBox(Footer)) { S->SetPadding(FMargin(0, 12, 0, 0)); }
	USpacer* FootSpacer = WidgetTree->ConstructWidget<USpacer>();
	if (UHorizontalBoxSlot* S = Footer->AddChildToHorizontalBox(FootSpacer)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
	MakeTexButton(Footer, TEXT("确认出发"), TEXT("/Game/Graytail/UI/deploy/ui_button_confirm_deploy_large"), 289.f, 98.f, 0.f)
		->OnClicked.AddDynamic(this, &UGT_DeployTerminalWidget::OnDepartClicked);
}

void UGT_DeployTerminalWidget::AddItemCard(int32 Index, UTexture2D* Icon, const FString& Name, const FString& TypeLine,
	const FString& Effect, const FString& Flavor, const FString& InfoLine, const FString& StatusLine, const FString& ActionLabel,
	bool bActionEnabled, bool bHighlight)
{
	if (!ContentWrap) { return; }

	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
	CardSize->SetWidthOverride(330.f);
	CardSize->SetHeightOverride(196.f);
	if (UWrapBoxSlot* WSlot = ContentWrap->AddChildToWrapBox(CardSize)) { WSlot->SetPadding(FMargin(6.f)); }

	// 圆角卡片(RoundedBox): 底色 (21,29,40), 描边暗金/高亮金。
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
	FSlateBrush CardBrush;
	CardBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	CardBrush.TintColor = FSlateColor(bHighlight ? GTColCardEq : GTColCard);
	CardBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	CardBrush.OutlineSettings.CornerRadii = FVector4(10.f, 10.f, 10.f, 10.f);
	CardBrush.OutlineSettings.Color = FSlateColor(bHighlight ? GTColLineEq : GTColLine);
	CardBrush.OutlineSettings.Width = bHighlight ? 2.f : 1.f;
	Card->SetBrush(CardBrush);
	Card->SetPadding(FMargin(13.f, 11.f));
	CardSize->SetContent(Card);

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
	Card->SetContent(Col);

	// 头部: 图标 + 名称/类型
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>();
	Col->AddChildToVerticalBox(Head);
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>();
	IconBox->SetWidthOverride(42.f);
	IconBox->SetHeightOverride(42.f);
	if (Icon)
	{
		UImage* Img = WidgetTree->ConstructWidget<UImage>();
		Img->SetBrushFromTexture(Icon);
		IconBox->SetContent(Img);
	}
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(IconBox)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0, 0, 10, 0)); }
	UVerticalBox* NameCol = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(NameCol)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetVerticalAlignment(VAlign_Center); }
	UTextBlock* NameTxt = WidgetTree->ConstructWidget<UTextBlock>();
	NameTxt->SetText(FText::FromString(Name));
	NameTxt->SetFont(GTFont(18));
	NameTxt->SetColorAndOpacity(FSlateColor(bHighlight ? GTColGold : GTColName));
	NameCol->AddChildToVerticalBox(NameTxt);
	UTextBlock* TypeTxt = WidgetTree->ConstructWidget<UTextBlock>();
	TypeTxt->SetText(FText::FromString(TypeLine));
	TypeTxt->SetFont(GTFont(11));
	TypeTxt->SetColorAndOpacity(FSlateColor(GTColDim));
	NameCol->AddChildToVerticalBox(TypeTxt);

	// 效果描述
	UTextBlock* EffTxt = WidgetTree->ConstructWidget<UTextBlock>();
	EffTxt->SetText(FText::FromString(Effect));
	EffTxt->SetFont(GTFont(13));
	EffTxt->SetColorAndOpacity(FSlateColor(GTColEffect));
	EffTxt->SetAutoWrapText(true);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(EffTxt)) { S->SetPadding(FMargin(0, 7, 0, 0)); }

	// flavor 描述(暗色小字, 可换行)
	if (!Flavor.IsEmpty())
	{
		UTextBlock* FlavorTxt = WidgetTree->ConstructWidget<UTextBlock>();
		FlavorTxt->SetText(FText::FromString(Flavor));
		FlavorTxt->SetFont(GTFont(11));
		FlavorTxt->SetColorAndOpacity(FSlateColor(GTColFaint));
		FlavorTxt->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(FlavorTxt)) { S->SetPadding(FMargin(0, 4, 0, 0)); }
	}

	// 拥有/价格行
	UTextBlock* InfoTxt = WidgetTree->ConstructWidget<UTextBlock>();
	InfoTxt->SetText(FText::FromString(InfoLine));
	InfoTxt->SetFont(GTFont(12));
	InfoTxt->SetColorAndOpacity(FSlateColor(GTColDim));
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(InfoTxt)) { S->SetPadding(FMargin(0, 5, 0, 0)); }

	// 底部: 状态(左) + 动作按钮(右)
	UHorizontalBox* Foot = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Foot)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetVerticalAlignment(VAlign_Bottom); }
	UTextBlock* StatusTxt = WidgetTree->ConstructWidget<UTextBlock>();
	StatusTxt->SetText(FText::FromString(StatusLine));
	StatusTxt->SetFont(GTFont(13));
	StatusTxt->SetColorAndOpacity(FSlateColor(bHighlight ? GTColGold : GTColDim));
	if (UHorizontalBoxSlot* S = Foot->AddChildToHorizontalBox(StatusTxt)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetVerticalAlignment(VAlign_Center); }

	UGT_IndexedButton* ActBtn = WidgetTree->ConstructWidget<UGT_IndexedButton>();
	ActBtn->Index = Index;
	ActBtn->SetIsEnabled(bActionEnabled);
	ActBtn->OnIndexClicked.AddDynamic(this, &UGT_DeployTerminalWidget::HandleRowClicked);
	// 蓝底白字圆角按钮(原版风格); 不可用时灰底暗字。
	{
		FButtonStyle BS = ActBtn->GetStyle();
		auto BtnBrush = [](const FLinearColor& Fill)
		{
			FSlateBrush B;
			B.DrawAs = ESlateBrushDrawType::RoundedBox;
			B.TintColor = FSlateColor(Fill);
			B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			B.OutlineSettings.CornerRadii = FVector4(6.f, 6.f, 6.f, 6.f);
			return B;
		};
		BS.Normal = BtnBrush(GTColBtn);
		BS.Hovered = BtnBrush(GTColBtnHover);
		BS.Pressed = BtnBrush(GTColBtnPress);
		BS.Disabled = BtnBrush(GTColBtnDisabled);
		BS.NormalPadding = FMargin(15.f, 5.f);
		BS.PressedPadding = FMargin(15.f, 5.f);
		ActBtn->SetStyle(BS);
	}
	UTextBlock* ActTxt = WidgetTree->ConstructWidget<UTextBlock>();
	ActTxt->SetText(FText::FromString(ActionLabel));
	ActTxt->SetFont(GTFont(15));
	ActTxt->SetColorAndOpacity(FSlateColor(bActionEnabled ? GTColBtnText : GTColBtnTextDisabled));
	ActBtn->SetContent(ActTxt);
	if (UHorizontalBoxSlot* S = Foot->AddChildToHorizontalBox(ActBtn)) { S->SetVerticalAlignment(VAlign_Center); }
}

void UGT_DeployTerminalWidget::RebuildContent()
{
	if (!ContentWrap) { return; }
	ContentWrap->ClearChildren();
	CurrentRows.Reset();

	UGT_MetaProgressSubsystem* Meta = GetMeta();
	if (!Meta) { return; }
	const int32 Gold = Meta->GetGold();
	const int32 EquippedNum = Meta->GetEquippedItems().Num();

	if (CurrentSection == ESection::Requisition)
	{
		if (DetailText) { DetailText->SetText(FText::FromString(TEXT("点击卡片右侧按钮: 未拥有→申领, 已拥有→装备/卸下(最多 2 件)。"))); }
		for (const FGT_EquipDef& Def : GT_MetaCatalog::GetEquipDefs())
		{
			if (!PassesFilter(FName(TEXT("type_equip")), EquipTierKey(Def.Id))) { continue; }
			const bool bOwned = Meta->OwnsItem(Def.Id);
			const bool bEq = Meta->IsEquipped(Def.Id);
			const bool bAfford = Gold >= Def.Price;
			const FString Info = bOwned ? TEXT("拥有 x1") : FString::Printf(TEXT("价格 %d 结算币"), Def.Price);
			const FString Status = !bOwned ? TEXT("未拥有") : (bEq ? TEXT("已装备") : TEXT("已拥有"));
			const FString Action = !bOwned ? TEXT("申领") : (bEq ? TEXT("卸下") : TEXT("装备"));
			const bool bEnabled = !bOwned ? bAfford : (bEq ? true : EquippedNum < GT_MetaCatalog::MaxEquipped);
			AddItemCard(CurrentRows.Num(), IconForEquip(Def.Id), Def.DisplayName, TEXT("作业装备 · 后勤"),
				EquipEffectText(Def), EquipFlavor(Def.Id), Info, Status, Action, bEnabled, bEq);
			CurrentRows.Add({ Def.Id, ERowKind::Equip });
		}
		for (const FGT_ConsumableDef& Def : GT_MetaCatalog::GetConsumableDefs())
		{
			if (!PassesFilter(FName(TEXT("type_consumable")), FName(TEXT("tier_common")))) { continue; }
			const bool bAfford = Gold >= Def.Price;
			const int32 Have = Meta->GetConsumableCount(Def.Id);
			AddItemCard(CurrentRows.Num(), IconForConsumable(Def.Id), Def.DisplayName, TEXT("作业消耗品 · 后勤"),
				ConsumableEffectText(Def), ConsumableFlavor(Def.Id),
				FString::Printf(TEXT("价格 %d 结算币"), Def.Price), FString::Printf(TEXT("持有 %d"), Have),
				TEXT("申领"), bAfford, false);
			CurrentRows.Add({ Def.Id, ERowKind::Consumable });
		}
	}
	else if (CurrentSection == ESection::Loadout)
	{
		if (DetailText) { DetailText->SetText(FText::FromString(TEXT("装备: 点击装/卸(最多 2 件)。消耗品: 点击循环设置带入数量。"))); }
		bool bAny = false;
		for (const FGT_EquipDef& Def : GT_MetaCatalog::GetEquipDefs())
		{
			if (!Meta->OwnsItem(Def.Id)) { continue; }
			if (!PassesFilter(FName(TEXT("type_equip")), EquipTierKey(Def.Id))) { continue; }
			bAny = true;
			const bool bEq = Meta->IsEquipped(Def.Id);
			AddItemCard(CurrentRows.Num(), IconForEquip(Def.Id), Def.DisplayName, TEXT("作业装备 · 后勤"),
				EquipEffectText(Def), EquipFlavor(Def.Id), TEXT("拥有 x1"), bEq ? TEXT("已装备") : TEXT("已拥有"),
				bEq ? TEXT("卸下") : TEXT("装备"), bEq ? true : EquippedNum < GT_MetaCatalog::MaxEquipped, bEq);
			CurrentRows.Add({ Def.Id, ERowKind::Equip });
		}
		for (const FGT_ConsumableDef& Def : GT_MetaCatalog::GetConsumableDefs())
		{
			const int32 Stock = Meta->GetConsumableCount(Def.Id);
			if (Stock <= 0) { continue; }
			if (!PassesFilter(FName(TEXT("type_consumable")), FName(TEXT("tier_common")))) { continue; }
			bAny = true;
			const int32 Carry = Meta->GetLoadout().FindRef(Def.Id);
			AddItemCard(CurrentRows.Num(), IconForConsumable(Def.Id), Def.DisplayName, TEXT("作业消耗品 · 后勤"),
				ConsumableEffectText(Def), ConsumableFlavor(Def.Id),
				FString::Printf(TEXT("库存 %d"), Stock), FString::Printf(TEXT("已带入 %d"), Carry),
				FString::Printf(TEXT("带入 %d"), Carry), true, Carry > 0);
			CurrentRows.Add({ Def.Id, ERowKind::Consumable });
		}
		if (!bAny && DetailText)
		{
			DetailText->SetText(FText::FromString(TEXT("尚无可配置的装备/消耗品 —— 先到「后勤申领」购买。")));
		}
	}
	else if (CurrentSection == ESection::Warehouse)
	{
		if (DetailText) { DetailText->SetText(FText::FromString(TEXT("回收物可按单件价值出售换取结算币; 装备/消耗品类入库物资不可在此出售。"))); }
		const TArray<FGT_WarehouseEntry>& Items = Meta->GetWarehouse();
		if (Items.Num() == 0)
		{
			if (DetailText) { DetailText->SetText(FText::FromString(TEXT("后勤仓库空空如也 —— 撤离或失败抢救带回的回收物会存放在这里。"))); }
		}
		for (const FGT_WarehouseEntry& Entry : Items)
		{
			const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(Entry.ItemId);
			// 类型键: 来源 recovered→回收, equipment→作业装备, consumable→消耗; 档位键: 按物品稀有度。
			FName TypeKey = FName(TEXT("type_recovered"));
			if (Entry.Source == FName(TEXT("equipment"))) { TypeKey = FName(TEXT("type_equip")); }
			else if (Entry.Source == FName(TEXT("consumable"))) { TypeKey = FName(TEXT("type_consumable")); }
			const FName TierKey = RarityTierKey(Def ? Def->Rarity : NAME_None);
			if (!PassesFilter(TypeKey, TierKey)) { continue; }
			const FString Name = Def ? Def->DisplayName : Entry.ItemId.ToString();
			FName Reason;
			const bool bSellable = Meta->CanSellItem(Entry.ItemId, Reason);
			const FString Action = bSellable ? FString::Printf(TEXT("出售 +%d"), Entry.Value) : TEXT("不可售");
			const FString Status = bSellable ? FString::Printf(TEXT("单件 %d 结算币"), Entry.Value) : SellReasonLabel(Reason);
			AddItemCard(CurrentRows.Num(), IconForWarehouse(Entry.ItemId), Name, WarehouseSourceLabel(Entry.Source),
				FString::Printf(TEXT("单件价值 %d 结算币"), Entry.Value), TEXT("战区回收的物资, 后勤按当日行情折价收购。"),
				FString::Printf(TEXT("库存 x%d"), Entry.Count), Status, Action, bSellable, false);
			CurrentRows.Add({ Entry.ItemId, ERowKind::Warehouse });
		}
	}
	else if (CurrentSection == ESection::Talent)
	{
		if (DetailText) { DetailText->SetText(FText::FromString(TEXT("天赋为永久解锁(花费结算币), 解锁后每局自动生效, 不占装备位。"))); }
		for (const FGT_TalentDef& Def : GT_MetaCatalog::GetTalentDefs())
		{
			const bool bOwned = Meta->HasTalent(Def.Id);
			const bool bAfford = Gold >= Def.Price;
			const FString Info = bOwned ? TEXT("已解锁 · 永久生效") : FString::Printf(TEXT("解锁 %d 结算币"), Def.Price);
			const FString Status = bOwned ? TEXT("已解锁") : TEXT("未解锁");
			const FString Action = bOwned ? TEXT("已解锁") : TEXT("解锁");
			AddItemCard(CurrentRows.Num(), IconForTalent(Def.Id), Def.DisplayName, TEXT("作业天赋 · 资历"),
				TalentEffectText(Def), TalentFlavor(Def.Id), Info, Status, Action, !bOwned && bAfford, bOwned);
			CurrentRows.Add({ Def.Id, ERowKind::Talent });
		}
	}
	else if (CurrentSection == ESection::Recovery)
	{
		if (DetailText) { DetailText->SetText(FText::FromString(TEXT("累计作业与回收资历(只读)。"))); }
		AddRecoveryPanel();
	}
}

void UGT_DeployTerminalWidget::AddRecoveryPanel()
{
	if (!ContentWrap) { return; }
	UGT_MetaProgressSubsystem* Meta = GetMeta();
	if (!Meta) { return; }

	// 一块宽统计面板(跨内容区, 非卡片网格)。
	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>();
	PanelSize->SetWidthOverride(680.f);
	if (UWrapBoxSlot* WSlot = ContentWrap->AddChildToWrapBox(PanelSize)) { WSlot->SetPadding(FMargin(6.f)); }
	UBorder* Outer = WidgetTree->ConstructWidget<UBorder>();
	Outer->SetBrushColor(GTColLine);
	Outer->SetPadding(FMargin(1.f));
	PanelSize->SetContent(Outer);
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
	Card->SetBrushColor(GTColCard);
	Card->SetPadding(FMargin(20.f, 16.f));
	Outer->SetContent(Card);
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
	Card->SetContent(Col);

	auto AddLine = [this, Col](const FString& Text, int32 Size, const FLinearColor& Color, float TopPad)
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
		T->SetText(FText::FromString(Text));
		T->SetFont(GTFont(Size));
		T->SetColorAndOpacity(FSlateColor(Color));
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(T)) { S->SetPadding(FMargin(0, TopPad, 0, 0)); }
	};

	const FGT_MetaStats& Stats = Meta->GetStats();
	const FGT_RecoverySummary& Rec = Meta->GetRecoverySummary();

	AddLine(TEXT("作业统计"), 18, GTColGold, 0.f);
	AddLine(FString::Printf(TEXT("出勤次数      %d"), Stats.TotalRuns), 15, GTColWhite, 6.f);
	AddLine(FString::Printf(TEXT("成功撤离      %d"), Stats.TotalExtractions), 15, GTColWhite, 3.f);
	AddLine(FString::Printf(TEXT("累计赚取      %d 结算币"), Stats.TotalGoldEarned), 15, GTColWhite, 3.f);

	AddLine(TEXT("回收资历"), 18, GTColGold, 16.f);
	AddLine(FString::Printf(TEXT("累计带回物品  %d 件"), Rec.TotalItems), 15, GTColWhite, 6.f);
	AddLine(FString::Printf(TEXT("累计回收价值  %d 结算币"), Rec.TotalValue), 15, GTColWhite, 3.f);
	AddLine(FString::Printf(TEXT("带物撤离次数  %d"), Rec.TotalExtractionsWithItems), 15, GTColWhite, 3.f);

	AddLine(TEXT("最近带回"), 16, GTColDim, 16.f);
	if (Rec.RecentItemIds.Num() == 0)
	{
		AddLine(TEXT("(暂无记录)"), 15, GTColDim, 4.f);
	}
	for (const FName& Id : Rec.RecentItemIds)
	{
		const FGT_ItemCatalogEntry* Def = GT_ItemCatalog::FindItemDef(Id);
		AddLine(FString::Printf(TEXT("· %s"), Def ? *Def->DisplayName : *Id.ToString()), 15, GTColWhite, 3.f);
	}
}

void UGT_DeployTerminalWidget::RefreshAccount()
{
	UGT_MetaProgressSubsystem* Meta = GetMeta();
	if (!Meta) { return; }
	const int32 Items = GT_MetaCatalog::GetEquipDefs().Num() + GT_MetaCatalog::GetConsumableDefs().Num();
	if (AccountText) { AccountText->SetText(FText::FromString(FString::Printf(TEXT("结算币 %d · %d 项"), Meta->GetGold(), Items))); }
}

void UGT_DeployTerminalWidget::RefreshSummary()
{
	if (!SummaryBox) { return; }
	SummaryBox->ClearChildren();
	UGT_MetaProgressSubsystem* Meta = GetMeta();
	if (!Meta) { return; }

	auto AddLine = [this](const FString& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
		T->SetText(FText::FromString(Text));
		T->SetFont(GTFont(Size));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = SummaryBox->AddChildToVerticalBox(T)) { S->SetPadding(FMargin(0, 3)); }
	};

	AddLine(TEXT("出勤摘要"), 20, GTColGold);
	AddLine(TEXT("── 已装备 ──"), 12, GTColDim);
	const TArray<FName>& Equipped = Meta->GetEquippedItems();
	if (Equipped.Num() == 0) { AddLine(TEXT("(未配置)"), 15, GTColDim); }
	for (const FName& Id : Equipped)
	{
		const FGT_EquipDef* Def = GT_MetaCatalog::FindEquip(Id);
		AddLine(FString::Printf(TEXT("· %s"), Def ? Def->DisplayName : *Id.ToString()), 15, GTColWhite);
	}

	AddLine(TEXT("── 本局加成 ──"), 12, GTColDim);
	const FGT_EquipBonus B = Meta->GetEquipBonus();
	const FGT_TalentEffects T = Meta->GetTalentEffects();
	bool bAnyBonus = false;
	if (B.BonusHP > 0) { AddLine(FString::Printf(TEXT("生命 +%d"), B.BonusHP), 15, GTColGold); bAnyBonus = true; }
	if (B.BonusPower > 0) { AddLine(FString::Printf(TEXT("战斗力 +%d"), B.BonusPower), 15, GTColGold); bAnyBonus = true; }
	const int32 MineRed = B.MineDmgReduce + T.MineDmgReduce;
	if (MineRed > 0) { AddLine(FString::Printf(TEXT("雷险伤害 -%d"), MineRed), 15, GTColGold); bAnyBonus = true; }
	if (B.bMineImmunity) { AddLine(TEXT("首次踩雷免疫"), 15, GTColGold); bAnyBonus = true; }
	if (B.SearchBonus > 0) { AddLine(FString::Printf(TEXT("搜索奖励 +%d%%"), B.SearchBonus), 15, GTColGold); bAnyBonus = true; }
	if (B.bShowExitHint) { AddLine(TEXT("撤离信标提示"), 15, GTColGold); bAnyBonus = true; }
	if (!bAnyBonus) { AddLine(TEXT("(无)"), 15, GTColDim); }

	AddLine(TEXT("── 带入消耗品 ──"), 12, GTColDim);
	const TMap<FName, int32>& Loadout = Meta->GetLoadout();
	if (Loadout.Num() == 0) { AddLine(TEXT("(无)"), 15, GTColDim); }
	for (const TPair<FName, int32>& Pair : Loadout)
	{
		const FGT_ConsumableDef* Def = GT_MetaCatalog::FindConsumable(Pair.Key);
		AddLine(FString::Printf(TEXT("· %s ×%d"), Def ? Def->DisplayName : *Pair.Key.ToString(), Pair.Value), 15, GTColWhite);
	}
}

void UGT_DeployTerminalWidget::RefreshAll()
{
	RefreshAccount();
	RebuildContent();
	RefreshSummary();
}

void UGT_DeployTerminalWidget::ShowSection(ESection Section)
{
	CurrentSection = Section;
	FString Name;
	switch (Section)
	{
	case ESection::Requisition: Name = TEXT("后勤申领"); break;
	case ESection::Loadout:     Name = TEXT("出勤配置"); break;
	case ESection::Warehouse:   Name = TEXT("后勤仓库"); break;
	case ESection::Talent:      Name = TEXT("天赋"); break;
	case ESection::Recovery:    Name = TEXT("回收资历"); break;
	}
	if (SectionTitleText) { SectionTitleText->SetText(FText::FromString(Name)); }
	// 当前页签高亮(选中套圆角金边, 代替原来的"当前页签/"面包屑); 同步 NavVisualState 给 Tick 维护悬停。
	if (NavVisualState.Num() != NavHighlights.Num()) { NavVisualState.Init(0, NavHighlights.Num()); }
	for (int32 i = 0; i < NavHighlights.Num(); ++i)
	{
		const uint8 St = (NavSections.IsValidIndex(i) && NavSections[i] == Section) ? 1 : 0;
		NavVisualState[i] = St;
		SetNavHighlight(NavHighlights[i], St);
	}
	// 筛选条只对申领/出勤配置/仓库(物品)有意义, 其余页隐藏。每次切页重置为"全部"。
	CurrentFilter = FName(TEXT("all"));
	if (FilterRow)
	{
		const bool bShowFilter = (Section == ESection::Requisition || Section == ESection::Loadout || Section == ESection::Warehouse);
		FilterRow->SetVisibility(bShowFilter ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bShowFilter) { RebuildFilterPills(); }
		else { FilterRow->ClearChildren(); FilterPillKeys.Reset(); }
	}
	RefreshAccount();
	RebuildContent();
	RefreshSummary();
}

void UGT_DeployTerminalWidget::HandleRowClicked(int32 Index)
{
	if (!CurrentRows.IsValidIndex(Index)) { return; }
	UGT_MetaProgressSubsystem* Meta = GetMeta();
	if (!Meta) { return; }
	const FRowRef Ref = CurrentRows[Index];
	FName Err;

	switch (Ref.Kind)
	{
	case ERowKind::Consumable:
		if (CurrentSection == ESection::Loadout)
		{
			const int32 Cur = Meta->GetLoadout().FindRef(Ref.Id);
			const int32 Stock = Meta->GetConsumableCount(Ref.Id);
			const FGT_ConsumableDef* Def = GT_MetaCatalog::FindConsumable(Ref.Id);
			int32 Cap = Stock;
			if (Def && Def->MaxCarry > 0) { Cap = FMath::Min(Cap, Def->MaxCarry); }
			const int32 Next = (Cur >= Cap) ? 0 : Cur + 1;
			Meta->SetLoadoutConsumable(Ref.Id, Next);
		}
		else
		{
			Meta->BuyConsumable(Ref.Id, 1, Err);   // 申领页买消耗品
		}
		break;

	case ERowKind::Equip:
		// 装备: 未拥有→购买, 已拥有→装/卸(申领页与出勤配置页一致)。
		if (!Meta->OwnsItem(Ref.Id)) { Meta->BuyItem(Ref.Id, Err); }
		else { Meta->ToggleEquip(Ref.Id, Err); }
		break;

	case ERowKind::Talent:
		Meta->UnlockTalent(Ref.Id, Err);
		break;

	case ERowKind::Warehouse:
	{
		int32 OutGold = 0;
		Meta->SellWarehouseItem(Ref.Id, 1, OutGold, Err);   // 单件出售
		break;
	}
	}
	RefreshAll();
}

void UGT_DeployTerminalWidget::OnNavRequisition() { ShowSection(ESection::Requisition); }
void UGT_DeployTerminalWidget::OnNavLoadout() { ShowSection(ESection::Loadout); }
void UGT_DeployTerminalWidget::OnNavWarehouse() { ShowSection(ESection::Warehouse); }
void UGT_DeployTerminalWidget::OnNavTalent() { ShowSection(ESection::Talent); }
void UGT_DeployTerminalWidget::OnNavRecovery() { ShowSection(ESection::Recovery); }

void UGT_DeployTerminalWidget::OnBackClicked() { OnBackRequested.ExecuteIfBound(); }
void UGT_DeployTerminalWidget::OnDepartClicked() { OnDepartRequested.ExecuteIfBound(); }

void UGT_DeployTerminalWidget::Open()
{
	SetVisibility(ESlateVisibility::Visible);
	ShowSection(ESection::Requisition);
	SetKeyboardFocus();
}

void UGT_DeployTerminalWidget::Close()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UGT_DeployTerminalWidget::IsOpen() const
{
	return GetVisibility() != ESlateVisibility::Collapsed;
}

FReply UGT_DeployTerminalWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnBackRequested.ExecuteIfBound();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
