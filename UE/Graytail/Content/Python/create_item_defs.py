# -*- coding: utf-8 -*-
"""Create/update the UGT_ItemDef data assets under Content/Graytail/Items/Defs.

15 回收物/消耗品。稀有度 5 档(2026-06-24 扩): common/rare/epic/legendary/mythic
= 白/蓝/紫/金/红, 每档 >=2 件。新增 8 件异常回收物图标来自 Zuhe2 §9。
负重系统已删, 不再写 weight 字段。幂等: 已存在的资产就地更新。

Run AFTER building (UGT_ItemDef needs the Kind/Rarity/Value/EffectText
fields), as a FULL editor (无 -unattended, 否则 import 后 StatusBar 同步崩):
  UnrealEditor-Cmd.exe <Graytail.uproject>
      -ExecCmds="py create_item_defs.py" -EnablePlugins=PythonScriptPlugin
      -nopause -nosplash -stdout
Results land in Saved/Logs/Graytail.log, grep [GTItemDefs].
"""
import unreal

DEST_PATH = "/Game/Graytail/Items/Defs"

# (id, display_name, kind, rarity, value, effect_text, description, consumable)
# rarity: common 白 / rare 蓝 / epic 紫 / legendary 金 / mythic 红
ITEM_DEFS = [
    # --- 白 common(垃圾杂物, 最多) ---
    ("broken_copper_wire", "断裂铜线", unreal.GT_ItemKind.RELIC, "common", 8,
     "", "仍然能卖钱，这已经很难得了。", False),
    ("dim_capacitor", "暗淡电容", unreal.GT_ItemKind.RELIC, "common", 10,
     "", "拆下来时它轻轻响了一声，像是在叹气。", False),
    ("old_gear", "旧齿轮", unreal.GT_ItemKind.RELIC, "common", 9,
     "", "齿牙磨得发亮，却再也咬不进任何机器。", False),
    ("broken_terminal", "残损终端", unreal.GT_ItemKind.RECORD, "common", 13,
     "", "屏幕还亮着一行光标，等待一个永远不会来的指令。", False),
    # --- 蓝 rare(可用残件) ---
    ("dead_battery", "断电池组", unreal.GT_ItemKind.RELIC, "rare", 15,
     "", "外壳鼓胀，接口处结着白霜似的盐。别靠近明火。", False),
    ("old_gauge", "旧压力表", unreal.GT_ItemKind.RELIC, "rare", 16,
     "", "指针卡在红区，像是记住了某次没人幸存的超压。", False),
    ("damaged_circuit", "损坏电路板", unreal.GT_ItemKind.TOOL, "rare", 20,
     "可作为后续扫描设备材料。", "焊点烧出几个黑洞，剩下的线路仍在徒劳地导通。", False),
    # --- 紫 epic(数据/光学记录) ---
    ("static_lens", "静电透镜", unreal.GT_ItemKind.TOOL, "epic", 26,
     "可作为后续扫描设备材料。", "透过它看灯光时，会看见不存在的边界线。", False),
    ("blackbox_tag", "黑匣标签", unreal.GT_ItemKind.RECORD, "epic", 28,
     "", "标签上的编号被刮掉了，只剩下回收部门的旧印章。", False),
    ("data_disk", "数据盘", unreal.GT_ItemKind.RECORD, "epic", 30,
     "", "盘面有划痕，读出来的全是回收部门的加密废话。", False),
    # --- 金 legendary(自发光异物) ---
    ("whisper_wick", "低语灯芯", unreal.GT_ItemKind.RELIC, "legendary", 48,
     "", "它在没有电源的情况下发光，并且偶尔像在催你下班。", False),
    ("fluorescent_shard", "荧光碎片", unreal.GT_ItemKind.RELIC, "legendary", 54,
     "", "在黑暗里自顾自地亮，凑近会闻到臭氧味。", False),
    # --- 红 mythic(异常核心) ---
    ("sealed_core_shard", "封存核心碎片", unreal.GT_ItemKind.RELIC, "mythic", 78,
     "", "被封条压住的裂片仍在缓慢发热。", False),
    ("anomaly_core_shard", "异常核心碎片", unreal.GT_ItemKind.RELIC, "mythic", 98,
     "", "它在你手心轻轻搏动了一下。后勤建议：不要带回家。", False),
    # --- 消耗品(不参与回收物掉落) ---
    ("emergency_bandage", "应急止血贴", unreal.GT_ItemKind.CONSUMABLE, "common", 10,
     "恢复少量生命。", "后勤部称它经过消毒。包装上的日期不建议细看。", True),
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
    for (item_id, name, kind, rarity, value, effect, desc, consumable) in ITEM_DEFS:
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
