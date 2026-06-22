# -*- coding: utf-8 -*-
"""Create/update the 7 UGT_ItemDef data assets under Content/Graytail/Items/Defs.

Item data mirrors Lua RunInventory.ITEM_DEFS verbatim (ids, names, kinds,
rarity tags, base values, effect/description texts). Idempotent: existing
assets are updated in place.

Run AFTER building (UGT_ItemDef needs the Kind/Rarity/Value/EffectText
fields), as a FULL editor — same constraints as import_png_assets.py:
  UnrealEditor-Cmd.exe <Graytail.uproject>
      -ExecCmds="py create_item_defs.py" -EnablePlugins=PythonScriptPlugin
      -nopause -nosplash -stdout
Results land in Saved/Logs/Graytail.log, grep [GTItemDefs].
"""
import unreal

DEST_PATH = "/Game/Graytail/Items/Defs"

# (id, display_name, kind, rarity, value, effect_text, description, consumable, weight)
ITEM_DEFS = [
    ("broken_copper_wire", "断裂铜线", unreal.GT_ItemKind.RELIC, "common", 8,
     "", "仍然能卖钱，这已经很难得了。", False, 2),
    ("dim_capacitor", "暗淡电容", unreal.GT_ItemKind.RELIC, "common", 10,
     "", "拆下来时它轻轻响了一声，像是在叹气。", False, 3),
    ("whisper_wick", "低语灯芯", unreal.GT_ItemKind.RELIC, "rare", 45,
     "", "它在没有电源的情况下发光，并且偶尔像在催你下班。", False, 1),
    ("sealed_core_shard", "封存核心碎片", unreal.GT_ItemKind.RELIC, "rare", 45,
     "", "被封条压住的裂片仍在缓慢发热。", False, 4),
    ("emergency_bandage", "应急止血贴", unreal.GT_ItemKind.CONSUMABLE, "common", 10,
     "恢复少量生命。", "后勤部称它经过消毒。包装上的日期不建议细看。", True, 1),
    ("static_lens", "静电透镜", unreal.GT_ItemKind.TOOL, "uncommon", 16,
     "可作为后续扫描设备材料。", "透过它看灯光时，会看见不存在的边界线。", False, 3),
    ("blackbox_tag", "黑匣标签", unreal.GT_ItemKind.RECORD, "uncommon", 18,
     "", "标签上的编号被刮掉了，只剩下回收部门的旧印章。", False, 2),
]


def get_or_create(asset_name):
    asset_path = "%s/%s" % (DEST_PATH, asset_name)
    asset = unreal.load_asset(asset_path)
    if asset:
        return asset, False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GT_ItemDef)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = tools.create_asset(asset_name, DEST_PATH, unreal.GT_ItemDef, factory)
    return asset, True


def main():
    subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    created, updated, failed = 0, 0, 0
    for (item_id, name, kind, rarity, value, effect, desc, consumable, weight) in ITEM_DEFS:
        asset, is_new = get_or_create(item_id)
        if not asset:
            failed += 1
            unreal.log_error("[GTItemDefs] FAILED to create %s" % item_id)
            continue
        asset.set_editor_property("content_id", item_id)
        asset.set_editor_property("display_name", name)
        asset.set_editor_property("description", desc)
        asset.set_editor_property("kind", kind)
        asset.set_editor_property("rarity", rarity)
        asset.set_editor_property("value", value)
        asset.set_editor_property("effect_text", effect)
        asset.set_editor_property("consumable", consumable)
        asset.set_editor_property("weight", weight)
        if not subsystem.save_asset("%s/%s" % (DEST_PATH, item_id), only_if_is_dirty=False):
            failed += 1
            unreal.log_error("[GTItemDefs] SAVE FAILED %s" % item_id)
            continue
        if is_new:
            created += 1
        else:
            updated += 1
    unreal.log("[GTItemDefs] DONE created=%d updated=%d failed=%d"
               % (created, updated, failed))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        import traceback
        unreal.log_error("[GTItemDefs] EXCEPTION\n%s" % traceback.format_exc())
    finally:
        unreal.SystemLibrary.quit_editor()