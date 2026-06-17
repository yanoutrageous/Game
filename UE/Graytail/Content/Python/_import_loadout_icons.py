# -*- coding: utf-8 -*-
"""聚焦导入: S6 局外组新物品图标(源在 assets/item_loadout, 写实画风 TF_DEFAULT)。
只导这 5 张, 不触动其它 uasset。
跑法(完整编辑器, 先关其它编辑器实例):
  UnrealEditor-Cmd.exe <Graytail.uproject> -ExecCmds="py _import_loadout_icons.py"
      -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash -stdout
结果 grep Saved/Logs/Graytail.log 的 [GTLoadoutIcon]。
"""
import os
import unreal

DEST = "/Game/Graytail/UI/deploy"
# (源 png 文件名(在 assets/item_loadout), 目标 uasset 名)。
SOURCES = [
    ("anomaly_fang.png", "ui_icon_anomaly_fang"),
    ("lockdown_crystal.png", "ui_icon_lockdown_crystal"),
    ("company_badge.png", "ui_icon_company_badge"),
    ("salvage_magnet.png", "ui_icon_salvage_magnet"),
    ("lucky_coin.png", "ui_icon_lucky_coin"),
]


def loadout_root():
    proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    return os.path.normpath(os.path.join(proj, "..", "..", "assets", "item_loadout"))


def main():
    root = loadout_root()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks = []
    dest_names = []
    for src, dest_name in SOURCES:
        path = os.path.join(root, src)
        if not os.path.isfile(path):
            unreal.log_error("[GTLoadoutIcon] MISSING %s" % path)
            continue
        t = unreal.AssetImportTask()
        t.filename = path
        t.destination_path = DEST
        t.destination_name = dest_name
        t.automated = True
        t.save = False
        t.replace_existing = True
        tasks.append(t)
        dest_names.append(dest_name)
    tools.import_asset_tasks(tasks)

    sub = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    ok, fail = [], []
    for name in dest_names:
        ap = "%s/%s" % (DEST, name)
        tex = unreal.load_asset(ap)
        if not tex:
            fail.append(name)
            unreal.log_error("[GTLoadoutIcon] FAILED %s" % ap)
            continue
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("filter", unreal.TextureFilter.TF_DEFAULT)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        sub.save_asset(ap, only_if_is_dirty=False)
        ok.append(ap)
    unreal.log("[GTLoadoutIcon] DONE imported=%d failed=%d -> %s" % (len(ok), len(fail), DEST))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        import traceback
        unreal.log_error("[GTLoadoutIcon] EXC\n%s" % traceback.format_exc())
    finally:
        unreal.SystemLibrary.quit_editor()
