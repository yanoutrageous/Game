# -*- coding: utf-8 -*-
"""Import assets/Fonts/FusionPixel.otf as a UFontFace for game UI.

Creates one asset:
  /Game/Graytail/UI/Fonts/FusionPixel_Face  -- UFontFace wrapping the .otf glyphs

That is all the editor needs: the engine's Python does NOT expose the inner
FFontData/FTypeface structs, so the UFontFace cannot be wired into a UFont asset
from script. Instead, code wraps this face in a runtime FStandaloneCompositeFont
(see Source/Graytail/UI/GT_UIStyle.h, GT_UIStyle::Font). Runtime rasterization
from the OTF (not a baked atlas) is what lets CJK glyphs render at any size.

Run as a FULL editor (same constraints as import_png_assets.py -- real RHI,
a window flashes and closes; -run=pythonscript / -nullrhi crash in Slate/RHI;
do NOT add -unattended, it crashes in main-frame init for this project):
  UnrealEditor-Cmd.exe <Graytail.uproject>
      -ExecCmds="py _import_fusionpixel_font.py" -EnablePlugins=PythonScriptPlugin
      -nopause -nosplash -stdout
Grep results in Saved/Logs/Graytail.log for [GTFont].
"""
import os
import unreal

PROJECT_DIR = unreal.Paths.project_dir()                       # ...\Game\UE\Graytail\
REPO_ROOT = os.path.normpath(os.path.join(PROJECT_DIR, "..", ".."))
OTF = os.path.join(REPO_ROOT, "assets", "Fonts", "FusionPixel.otf")

DEST = "/Game/Graytail/UI/Fonts"
FACE_NAME = "FusionPixel_Face"
FACE_PATH = DEST + "/" + FACE_NAME


def main():
    if not os.path.isfile(OTF):
        unreal.log_error("[GTFont] OTF not found: %s" % OTF)
        return
    unreal.log("[GTFont] OTF source: %s" % OTF)

    if unreal.EditorAssetLibrary.does_asset_exist(FACE_PATH):
        unreal.EditorAssetLibrary.delete_asset(FACE_PATH)

    task = unreal.AssetImportTask()
    task.filename = OTF
    task.destination_path = DEST
    task.destination_name = FACE_NAME
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    face = unreal.load_asset(FACE_PATH)
    if face is None:
        unreal.log_error("[GTFont] FontFace import failed (no asset at %s)" % FACE_PATH)
        return
    unreal.log("[GTFont] FontFace imported: %s (%s)" % (FACE_PATH, type(face).__name__))
    unreal.log("[GTFont] DONE")


try:
    main()
except Exception:
    import traceback
    unreal.log_error("[GTFont] EXCEPTION\n%s" % traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
