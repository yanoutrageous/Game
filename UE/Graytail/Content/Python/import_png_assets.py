# -*- coding: utf-8 -*-
"""Batch-import Game/assets PNGs into Content/Graytail as UTexture2D assets.

Pixel-art settings applied to every texture: Filter=Nearest, no mipmaps,
UserInterface2D compression, never stream, LODGroup=UI.

Name hygiene: trailing `_YYYYMMDDHHMMSS` timestamps are stripped; when several
source files collapse to the same asset name, the newest timestamp wins.
Non-ASCII filenames and `cgt-*` generation intermediates are skipped (logged).

Writes a source->asset manifest to Saved/GTImport/asset_manifest.txt.

Automated run (close any other editor instance first):
  UnrealEditor-Cmd.exe <Graytail.uproject>
      -ExecCmds="py import_png_assets.py" -EnablePlugins=PythonScriptPlugin
      -unattended -nopause -nosplash -stdout
Must run as a FULL editor (real RHI; a window appears briefly and closes
itself): -run=pythonscript has no Slate app and AssetTools' post-import
content browser sync asserts; -nullrhi dies building the level editor
(ModeManagerInteractiveToolsContext). The bare script name resolves via
Content/Python on sys.path; absolute paths with spaces get mangled by command
line parsing. The script quits the editor when done.
Results land in Saved/Logs/Graytail.log, grep [GTImport].
"""
import os
import re

import unreal

CONTENT_BASE = "/Game/Graytail"

# Most specific prefix first. Source dir (relative to Game/assets, forward
# slashes) -> content subpath. Remainder of the path is mirrored below target.
DIR_MAP = [
    ("Textures/generated/icons/64", "UI/Icons64"),
    ("Textures/generated/icons/32", "UI/Icons32"),
    ("Textures/generated/rooms", "Rooms"),
    ("Textures/generated/characters", "Sprites/Characters"),
    ("Textures/generated/props", "Sprites/Props"),
    ("Textures/generated/ui", "UI/Misc"),
    ("Textures", "Sprites/Misc"),
    ("image", "Sprites"),
    ("ui", "UI"),
    ("item_consumable", "Items/Consumable"),
    ("item_equipment", "Items/Equipment"),
    ("item_recovered", "Items/Recovered"),
]

TIMESTAMP_RE = re.compile(r"_(20\d{12})$")
JUNK_RE = re.compile(r"^cgt-")


def assets_root():
    proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    return os.path.normpath(os.path.join(proj, "..", "..", "assets"))


def map_dest(rel_dir):
    rel = rel_dir.replace("\\", "/")
    for prefix, target in DIR_MAP:
        if rel == prefix:
            return "%s/%s" % (CONTENT_BASE, target)
        if rel.startswith(prefix + "/"):
            return "%s/%s/%s" % (CONTENT_BASE, target, rel[len(prefix) + 1:])
    if rel:
        return "%s/Unsorted/%s" % (CONTENT_BASE, rel)
    return "%s/Unsorted" % CONTENT_BASE


def sanitize_name(base):
    """Returns (asset_name, timestamp_or_None)."""
    ts = None
    m = TIMESTAMP_RE.search(base)
    if m:
        ts = m.group(1)
        base = base[: m.start()]
    base = re.sub(r"[^A-Za-z0-9_]", "_", base)
    base = re.sub(r"_+", "_", base).strip("_")
    return base, ts


def collect():
    root = assets_root()
    unreal.log("[GTImport] assets root: %s" % root)
    chosen = {}  # asset object path -> dict(file, order, dest, name)
    skipped = []  # (relative file, reason)
    dropped = []  # (relative file, kept relative file)
    for dirpath, _dirs, files in os.walk(root):
        for f in sorted(files):
            if not f.lower().endswith(".png"):
                continue
            full = os.path.join(dirpath, f)
            rel_file = os.path.relpath(full, root).replace("\\", "/")
            base = os.path.splitext(f)[0]
            try:
                base.encode("ascii")
            except UnicodeEncodeError:
                skipped.append((rel_file, "non-ascii name"))
                continue
            if JUNK_RE.match(base):
                skipped.append((rel_file, "generation intermediate (cgt-*)"))
                continue
            name, ts = sanitize_name(base)
            if not name:
                skipped.append((rel_file, "empty after sanitize"))
                continue
            rel_dir = os.path.relpath(dirpath, root)
            if rel_dir == ".":
                rel_dir = ""
            dest = map_dest(rel_dir)
            key = "%s/%s" % (dest, name)
            order = ts if ts else "99999999999999"  # untimestamped = canonical
            entry = {"file": full, "rel": rel_file, "order": order,
                     "dest": dest, "name": name}
            prev = chosen.get(key)
            if prev is None:
                chosen[key] = entry
            elif entry["order"] > prev["order"]:
                dropped.append((prev["rel"], rel_file))
                chosen[key] = entry
            else:
                dropped.append((rel_file, prev["rel"]))
    return chosen, skipped, dropped


def apply_pixel_art_settings(tex):
    tex.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    tex.set_editor_property("never_stream", True)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)


def save_asset(asset_path):
    subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    return subsystem.save_asset(asset_path, only_if_is_dirty=False)


def main():
    chosen, skipped, dropped = collect()
    entries = sorted(chosen.values(), key=lambda e: (e["dest"], e["name"]))
    unreal.log("[GTImport] planned=%d skipped=%d dropped_dupes=%d"
               % (len(entries), len(skipped), len(dropped)))
    for rel, reason in skipped:
        unreal.log("[GTImport] SKIP %s (%s)" % (rel, reason))
    for loser, winner in dropped:
        unreal.log("[GTImport] DUPE dropped %s, kept %s" % (loser, winner))

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks = []
    for e in entries:
        task = unreal.AssetImportTask()
        task.filename = e["file"]
        task.destination_path = e["dest"]
        task.destination_name = e["name"]
        task.automated = True
        task.save = False  # saved once below, after settings are applied
        task.replace_existing = True
        tasks.append(task)
    tools.import_asset_tasks(tasks)

    imported, failed, manifest = [], [], []
    for e in entries:
        asset_path = "%s/%s" % (e["dest"], e["name"])
        tex = unreal.load_asset(asset_path)
        if not tex:
            failed.append(e["rel"])
            unreal.log_error("[GTImport] FAILED %s -> %s" % (e["rel"], asset_path))
            continue
        apply_pixel_art_settings(tex)
        if not save_asset(asset_path):
            failed.append(e["rel"])
            unreal.log_error("[GTImport] SAVE FAILED %s" % asset_path)
            continue
        imported.append(asset_path)
        manifest.append("%s -> %s" % (e["rel"], asset_path))

    saved_dir = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_saved_dir())
    manifest_dir = os.path.join(saved_dir, "GTImport")
    os.makedirs(manifest_dir, exist_ok=True)
    manifest_file = os.path.join(manifest_dir, "asset_manifest.txt")
    with open(manifest_file, "w", encoding="utf-8") as fh:
        fh.write("\n".join(manifest) + "\n")

    unreal.log("[GTImport] DONE imported=%d failed=%d skipped=%d dupes=%d manifest=%s"
               % (len(imported), len(failed), len(skipped), len(dropped),
                  manifest_file))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        import traceback
        unreal.log_error("[GTImport] EXCEPTION\n%s" % traceback.format_exc())
    finally:
        unreal.SystemLibrary.quit_editor()
