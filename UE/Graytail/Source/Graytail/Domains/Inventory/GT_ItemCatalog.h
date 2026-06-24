#pragma once

#include "CoreMinimal.h"
#include "Domains/Inventory/GT_InventoryTypes.h"

// 物品静态表(对齐 Lua RunInventory.ITEM_DEFS)。
// 数据源 = Content/Graytail/Items/Defs/ 下的 UGT_ItemDef 资产(create_item_defs.py 生成),
// 首次访问时按固定顺序加载并缓存; 本接口只暴露值类型, 消费方不接触 UObject。
namespace GT_ItemCatalog
{
	// 全部物品定义(只读)。
	GRAYTAIL_API const TArray<FGT_ItemCatalogEntry>& GetAllItemDefs();

	// 按 Id 查定义, 找不到返回 nullptr。
	GRAYTAIL_API const FGT_ItemCatalogEntry* FindItemDef(FName ItemId);

	// 物品基础价值, 未知物品返回 0。
	GRAYTAIL_API int32 GetItemValue(FName ItemId);

	// 掉落品质 -> 具体物品(对齐 Lua QUALITY_ITEMS 表)。None/未知品质返回 NAME_None。
	GRAYTAIL_API FName GetQualityItemId(EGT_ItemQuality Quality);

	// 一组堆叠的总价值(按基础价值累加), 对齐 Lua GetCarriedItemValue。
	GRAYTAIL_API int32 GetCarriedItemsValue(const TArray<FGT_ItemStack>& Stacks);

	// 物品 UI 图标的 /Game 包路径(逐物品映射, 未知物品按大类兜底)。UI 层自行 LoadObject。
	GRAYTAIL_API FString GetItemIconAssetPath(FName ItemId);
}