# -*- coding: utf-8 -*-
"""聚焦导入: 6 个状态栏小图标(2.png 抠图, 套用 2UI 风格), 不触动其它 uasset。
跑法(完整编辑器, 无 -unattended): UnrealEditor-Cmd.exe <uproject>
  -ExecCmds="py _import_stat_icons.py" -EnablePlugins=PythonScriptPlugin -nopause -nosplash -stdout
结果 grep Saved/Logs/Graytail.log 的 [GTStatIcon]。"""
import os, unreal
DEST = "/Game/Graytail/UI/hud"
NAMES = ["stat_hp", "stat_power", "stat_pending", "stat_locked", "stat_parts", "stat_searched"]
def root():
    p = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    return os.path.normpath(os.path.join(p, "..", "..", "assets", "ui", "hud"))
def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools(); tasks = []
    for n in NAMES:
        path = os.path.join(root(), n + ".png")
        if not os.path.isfile(path): unreal.log_error("[GTStatIcon] MISSING %s" % path); continue
        t = unreal.AssetImportTask(); t.filename = path; t.destination_path = DEST
        t.destination_name = n; t.automated = True; t.save = False; t.replace_existing = True
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    sub = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem); ok = 0
    for n in NAMES:
        ap = "%s/%s" % (DEST, n); tex = unreal.load_asset(ap)
        if not tex: unreal.log_error("[GTStatIcon] FAILED %s" % ap); continue
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("filter", unreal.TextureFilter.TF_DEFAULT)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        sub.save_asset(ap, only_if_is_dirty=False); ok += 1
    unreal.log("[GTStatIcon] DONE imported=%d -> %s" % (ok, DEST))
if __name__ == "__main__":
    try: main()
    except Exception:
        import traceback; unreal.log_error("[GTStatIcon] EXC\n%s" % traceback.format_exc())
    finally: unreal.SystemLibrary.quit_editor()
