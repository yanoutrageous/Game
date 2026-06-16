# -*- coding: utf-8 -*-
"""聚焦导入: 只导 2 张部署装备图标(磨刀石/绝缘套, 写实画风 TF_Default), 不触动其它 uasset。
跑法(完整编辑器, 先关其它编辑器实例):
  UnrealEditor-Cmd.exe <Graytail.uproject> -ExecCmds="py _import_deploy_icons.py"
      -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash -stdout
结果 grep Saved/Logs/Graytail.log 的 [GTDeployIcon]。
"""
import os
import unreal

DEST = "/Game/Graytail/UI/deploy"
NAMES = [
    "ui_icon_whetstone", "ui_icon_insulated_gloves",
    "ui_icon_talent_map", "ui_icon_talent_mine", "ui_icon_talent_monster",
    "ui_icon_talent_extract", "ui_icon_talent_event",
]


def assets_root():
    proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    return os.path.normpath(os.path.join(proj, "..", "..", "assets", "ui", "deploy"))


def main():
    root = assets_root()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks = []
    for name in NAMES:
        path = os.path.join(root, name + ".png")
        if not os.path.isfile(path):
            unreal.log_error("[GTDeployIcon] MISSING %s" % path)
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
    for name in NAMES:
        ap = "%s/%s" % (DEST, name)
        tex = unreal.load_asset(ap)
        if not tex:
            fail.append(name)
            unreal.log_error("[GTDeployIcon] FAILED %s" % ap)
            continue
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("filter", unreal.TextureFilter.TF_DEFAULT)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        sub.save_asset(ap, only_if_is_dirty=False)
        ok.append(ap)
    unreal.log("[GTDeployIcon] DONE imported=%d failed=%d -> %s" % (len(ok), len(fail), DEST))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        import traceback
        unreal.log_error("[GTDeployIcon] EXC\n%s" % traceback.format_exc())
    finally:
        unreal.SystemLibrary.quit_editor()
