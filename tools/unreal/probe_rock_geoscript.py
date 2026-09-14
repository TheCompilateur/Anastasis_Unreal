"""
ROCK_FORGE_001 -- Gate 3 preflight.

Enumerates the Geometry Script surface this engine build actually exposes, for the
operations the rock grammar depends on (plane cut, boolean, displacement, simplify,
normals, UVs, non-uniform scale). tools/unreal/introspect_geoscript.py established the
convention for this project: confirm signatures against THIS build (UE 5.8.2,
CL 56702186) rather than trusting Epic docs for another version. That caution earned
its keep here -- in 5.8 apply_mesh_plane_cut lives in GeometryScript_MeshBooleans, not
in GeometryScript_MeshEdits, and there is no GeometryScript_MeshNormals library at all.

Read-only. Creates nothing, saves nothing.

Run headless (forward slashes in the path -- a backslash before `t` becomes a TAB
before the commandlet ever sees it):
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<root>/tools/unreal/probe_rock_geoscript.py"
"""
import unreal

# Substrings that matter to the rock grammar. A library or type is interesting if its
# name contains one of these.
KEYWORDS = (
    "boolean", "normal", "uv", "plane", "cut", "noise", "perlin", "displace",
    "simplify", "remesh", "append", "primitive", "transform", "repair", "weld",
    "nanite", "collision",
)


def log(msg):
    unreal.log("[probe_rock] " + str(msg))


def interesting(name):
    low = name.lower()
    return any(k in low for k in KEYWORDS)


def main():
    log("=== BEGIN PROBE (UE build introspection) ===")

    # 1. Every Geometry Script function library on this build, with the calls that
    #    matter. This is what tells you where apply_mesh_plane_cut really lives.
    libs = sorted(n for n in dir(unreal) if n.startswith("GeometryScript_"))
    log("LIBRARIES n=%d" % len(libs))
    for lib_name in libs:
        lib = getattr(unreal, lib_name)
        fns = sorted(f for f in dir(lib) if not f.startswith("_"))
        hits = [f for f in fns if interesting(f)]
        log("LIB %s  (%d calls)" % (lib_name, len(fns)))
        for f in hits:
            doc = (getattr(getattr(lib, f), "__doc__", "") or "").strip().replace("\n", " ")
            log("    %s :: %s" % (f, doc[:400]))

    # 2. Every Geometry Script struct/enum whose name touches those operations, with
    #    its editable fields -- the options objects have to be filled in blind otherwise.
    log("--- TYPES ---")
    types = sorted(
        n for n in dir(unreal)
        if n.startswith("GeometryScript") and not n.startswith("GeometryScript_")
        and interesting(n)
    )
    for type_name in types:
        t = getattr(unreal, type_name)
        members = sorted(m for m in dir(t) if not m.startswith("_"))
        # Enum members come back upper-case; struct fields lower-case.
        log("TYPE %s :: %s" % (type_name, ", ".join(members)[:700]))

    # 3. The two enums the boolean call needs, resolved by shape rather than by a
    #    guessed name: whichever type carries a UNION member is the operation enum.
    log("--- BOOLEAN OPERATION ENUM CANDIDATES ---")
    for n in dir(unreal):
        if not n.startswith("GeometryScript"):
            continue
        t = getattr(unreal, n)
        try:
            members = [m for m in dir(t) if m.isupper()]
        except Exception:
            continue
        if "UNION" in members:
            log("OPERATION ENUM %s :: %s" % (n, ", ".join(sorted(members))))

    log("=== END PROBE ===")


main()
