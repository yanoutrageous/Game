# -*- coding: utf-8 -*-
"""聚焦导入: 只导 6 张局内物品图标(写实画风, TF_Default 平滑), 不触动其它 uasset。
跑法(完整编辑器, 先关掉其它编辑器实例):
  UnrealEditor-Cmd.exe <Graytail.uproject> -ExecCmds="py _import_item_icons.py"
      -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash -stdout
结果 grep Saved/Logs/Graytail.log 的 [GTIcon]。
"""
import os
import unreal

DEST = "/Game/Graytail/Items/Recovered"
IDS = [
    "broken_copper_wire", "dim_capacitor", "whisper_wick",
    "sealed_core_shard", "static_lens", "blackbox_tag",
    # 2026-06-24 新增 8 个异常回收物(Zuhe2 §9 抠图, 像素风)。
    "old_gear", "anomaly_core_shard", "dead_battery", "fluorescent_shard",
    "damaged_circuit", "old_gauge", "data_disk", "broken_terminal",
]


def assets_root():
    proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    return os.path.normpath(os.path.join(proj, "..", "..", "assets", "item_recovered"))


def main():
    root = assets_root()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks = []
    for name in IDS:
        path = os.path.join(root, name + ".png")
        if not os.path.isfile(path):
            unreal.log_error("[GTIcon] MISSING %s" % path)
            continue
        t = unreal.AssetImportTask()
        t.filename = path
        t.destination_path = DEST
        t.destination_name = name
        t.automated = True
        t.save = False
        t.replace_existing = True
        tasks.append(t)
    tools.import_asset_tasks(tasks)

    sub = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    ok, fail = [], []
    for name in IDS:
        ap = "%s/%s" % (DEST, name)
        tex = unreal.load_asset(ap)
        if not tex:
            fail.append(name)
            unreal.log_error("[GTIcon] FAILED %s" % ap)
            continue
        # 写实画风: 平滑滤波(非像素 Nearest), UI 纹理, 不流送, 无 mip。
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("filter", unreal.TextureFilter.TF_DEFAULT)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        sub.save_asset(ap, only_if_is_dirty=False)
        ok.append(ap)
    unreal.log("[GTIcon] DONE imported=%d failed=%d -> %s" % (len(ok), len(fail), DEST))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        import traceback
        unreal.log_error("[GTIcon] EXC\n%s" % traceback.format_exc())
    finally:
        unreal.SystemLibrary.quit_editor()
