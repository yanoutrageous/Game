#pragma once

#include "CoreMinimal.h"
#include "Data/GT_ContentDef.h"
#include "Data/GT_EffectTypes.h"
#include "Domains/Inventory/GT_InventoryTypes.h"
#include "GT_ItemDef.generated.h"

// 物品定义资产。7 件 Lua ITEM_DEFS 物品的资产在 Content/Graytail/Items/Defs/,
// 由 Content/Python/create_item_defs.py 生成; 运行时经 GT_ItemCatalog 读取。
UCLASS(BlueprintType)
class GRAYTAIL_API UGT_ItemDef : public UGT_ContentDef
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Item")
	TArray<FGT_EffectSpec> UseEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Item")
	int32 MaxStack = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Item")
	bool bConsumable = false;

	// 物品大类(对齐 Lua ITEM_DEFS 的 type)。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Item")
	EGT_ItemKind Kind = EGT_ItemKind::Relic;

	// 稀有度标签(common/uncommon/rare, 对齐 Lua rarity)。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Item")
	FName Rarity = NAME_None;

	// 基础价值(出售/结算用, 对齐 Lua baseValue)。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Item")
	int32 Value = 0;

	// 使用效果文案(对齐 Lua effectText; 数值化效果用 UseEffects)。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Graytail|Item")
	FString EffectText;
};
