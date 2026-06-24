# -*- coding: utf-8 -*-
"""聚焦导入: 只导 4 张中性/金/青/铜金属面板框(9-slice 弹窗换皮用), 不触动其它 uasset。
离线烤好色相(亮度x色相), 运行时白 tint 直接显示 -> 所见即所得, 不踩 multiply/色彩空间坑。
跑法(完整编辑器, 先关其它编辑器实例):
  UnrealEditor-Cmd.exe <Graytail.uproject> -ExecCmds="py _import_metal_panels.py"
      -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash -stdout
结果 grep Saved/Logs/Graytail.log 的 [GTMetalPanel]。
"""
import os
import unreal

DEST = "/Game/Graytail/UI/common"
NAMES = [
    "ui_panel_metal_neutral", "ui_panel_metal_gold",
    "ui_panel_metal_teal", "ui_panel_metal_copper",
]


def assets_root():
    proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    return os.path.normpath(os.path.join(proj, "..", "..", "assets", "ui", "common"))


def main():
    root = assets_root()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks = []
    for name in NAMES:
        path = os.path.join(root, name + ".png")
        if not os.path.isfile(path):
            unreal.log_error("[GTMetalPanel] MISSING %s" % path)
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
            unreal.log_error("[GTMetalPanel] FAILED %s" % ap)
            continue
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        sub.save_asset(ap, only_if_is_dirty=False)
        ok.append(ap)
    unreal.log("[GTMetalPanel] DONE imported=%d failed=%d -> %s" % (len(ok), len(fail), DEST))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        import traceback
        unreal.log_error("[GTMetalPanel] EXC\n%s" % traceback.format_exc())
    finally:
        unreal.SystemLibrary.quit_editor()
