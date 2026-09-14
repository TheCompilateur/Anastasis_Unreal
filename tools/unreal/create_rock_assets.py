"""
ROCK_FORGE_001 -- ANASTASIS_ROCK_GRAMMAR_V1.

Builds the rock vocabulary of ANASTASIS from a parametric genome, via Geometry
Script, headless. This is a GRAMMAR, not a collection: six archetypes are genome
presets, and each one yields N bounded variants from a seed. Adding a variant is
a number, not a sculpt.

    GENOME -> ARCHETYPE -> seed -> bounded variance -> DynamicMesh -> StaticMesh

WHY STONE HAS NO "BEFORE" MESH
------------------------------
EAnastasisSemanticType::Stone has no entry in the presentation registry (neither in
DA_AnastasisPresentation nor in UAnastasisPresentationRegistry::CreateCodeDefaults),
so AnastasisPresentation::ResolvePresentation returns false for every Stone tile and
AAnastasisWorldEmbodiment::PlaceDressing draws nothing there. Stone today is ground
COLOUR only (AnastasisTerrainSurface HighlandRock). The blocks and cylinders read as
"the current rocks" are the legacy DEBUG HISM cubes (anastasis.Terrain.Surface 0) and
the Ruin archetype's cylinder. This script therefore creates a vocabulary that did not
exist rather than replacing one that did. See docs/unreal/ROCK_FORGE_001.md.

GROUND CONTACT WITHOUT A C++ CHANGE
-----------------------------------
AnastasisPresentation::ResolveInstanceTransform lifts every instance by a CONSTANT
0.5 * EngineBasicShapeSize * Scale (= 50 * Scale), independent of actual mesh bounds.
So local Z = -50 is exactly where the terrain surface lands. SM_Tree/SM_Ruin recenter
their bounds to [-50,+50] to sit ON that plane. A rock must not sit on the ground like
furniture -- it must come OUT of it. So these meshes are deliberately NOT recentered:
the rock's contact plane is placed at local Z = -50 and its mass CONTINUES BELOW, down
to -50 - burial. That buried skirt is what the terrain swallows. Zero resolver change,
zero recompile -- the burial is expressed in mesh space, where it belongs.

Signatures are resolved against THIS engine build at runtime (see pick()), the
convention tools/unreal/introspect_geoscript.py established for this project: do not
assume Epic docs for another version match. Run tools/unreal/probe_rock_geoscript.py
to see what this build exposes.

Run headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="tools/unreal/create_rock_assets.py"

Env:
  ANASTASIS_ROCK_ONLY            comma-separated archetype ids to build (default: all)
  ANASTASIS_ROCK_KEEP_EDITOR=1   do not quit the editor when done
"""
import math
import os
import random
import zlib

import unreal


def stable_seed(*parts):
    """Deterministic seed from a name. NEVER Python's hash().

    str.__hash__ is salted per process (PYTHONHASHSEED), so seeding from hash() made
    every run of this script emit a different SM_Rock_Massive_01 -- which is the one
    thing a grammar must not do. crc32 is stable across processes and engine sessions,
    so a given archetype+variant is the same rock forever.
    """
    return zlib.crc32("|".join(str(p) for p in parts).encode("utf-8")) & 0x3FFFFFFF

PACKAGE_PATH = "/Game/Anastasis/Rock"

# AnastasisPresentation::EngineBasicShapeSize is 100.0 and the resolver lifts by half
# of it. CONTACT_Z is therefore where the ground plane cuts the mesh, in local space.
CONTACT_Z = -50.0


def log(msg):
    unreal.log("[create_rock_assets] " + str(msg))


def warn(msg):
    unreal.log_warning("[create_rock_assets] " + str(msg))


def pick(lib_names, fn_names):
    """First (library, function) pair that exists in this build.

    Geometry Script has moved calls between static libraries across UE versions
    (booleans and plane cut especially). Probing beats betting on one spelling --
    the same defensive shape tools/unreal/presentation-registry.py uses for
    editor-property names.
    """
    for lib_name in lib_names:
        lib = getattr(unreal, lib_name, None)
        if lib is None:
            continue
        for fn_name in fn_names:
            fn = getattr(lib, fn_name, None)
            if fn is not None:
                return lib, fn, "%s.%s" % (lib_name, fn_name)
    return None, None, None


# ----------------------------------------------------------------------------------
# ANASTASIS_ROCK_GENOME
# ----------------------------------------------------------------------------------
# Every parameter below changes the SILHOUETTE. Anything that only changed shading was
# cut -- GEOMETRY_FIRST, and COMPLEXITY_MUST_EARN_EXISTENCE. Dropped from the initial
# brief's list, with the reason: height/width/depth collapse into mass + two ratios;
# vertical_bias and flattening are one axis read from two ends (height_ratio);
# base_width/top_width collapse into taper; edge_softness is a normals/material
# concern, not a silhouette one; rotation_bias belongs to placement (the registry's
# bRandomYaw), not to the mesh.
#
#   mass          overall size in UU (radius of the generating mass)
#   height_ratio  Z extent / XY extent.  <1 squat and low, >1 standing
#   width_ratio   Y extent / X extent.   != 1 gives a non-round footprint, which is
#                 what makes the form legible from directly above (AERIAL gate)
#   taper         top width / base width. <1 pyramidal and stable, ~1 columnar,
#                 >1 top-heavy and overhung
#   asymmetry     lateral drift of the upper mass against the lower one, 0..1. The
#                 single parameter that most kills the "centred blob" read
#   facets        how many large planar fractures are cut into the mass
#   facet_depth   how deep each cut bites, as a fraction of mass. Large faces, not noise
#   erosion       low-frequency displacement amplitude as a fraction of mass. Kept SMALL
#                 on purpose: micro-noise everywhere is explicitly off-direction
#   burial        how far the mass continues BELOW the contact plane, as a fraction of
#                 total height. This is the Gate 4 parameter
#   debris        number of small satellite masses welded to the base (0 = none)
# ----------------------------------------------------------------------------------

# Per-parameter variance envelope. CONTROLLED_VARIATION: a variant is the archetype
# plus a bounded nudge, never a reroll. RANDOM != NATURAL -- an unbounded reroll
# destroys the geological family the archetype exists to express.
VARIANCE = {
    "mass": 0.18,
    "height_ratio": 0.16,
    "width_ratio": 0.14,
    "taper": 0.14,
    "asymmetry": 0.22,
    "facet_depth": 0.18,
    "erosion": 0.25,
    "burial": 0.12,
}

ARCHETYPES = {
    # Gate 3 proves the pipeline on this one before any other exists.
    "ROCK_MASSIVE": dict(
        mass=115.0, height_ratio=0.86, width_ratio=0.82, taper=0.54,
        asymmetry=0.28, facets=5, facet_depth=0.30, erosion=0.045,
        burial=0.20, debris=0,
    ),
    # Wide, low, mostly swallowed by the ground. Reads as bedrock surfacing.
    "ROCK_LOW": dict(
        mass=140.0, height_ratio=0.34, width_ratio=1.42, taper=0.68,
        asymmetry=0.30, facets=4, facet_depth=0.26, erosion=0.040,
        burial=0.34, debris=2,
    ),
    # Standing, but never a column: asymmetry and taper are what stop it being one.
    "ROCK_VERTICAL": dict(
        mass=92.0, height_ratio=1.74, width_ratio=0.74, taper=0.44,
        asymmetry=0.46, facets=5, facet_depth=0.34, erosion=0.035,
        burial=0.22, debris=1,
    ),
    # Two masses and the gap between them: a fracture, not two rocks side by side.
    "ROCK_SPLIT": dict(
        mass=118.0, height_ratio=1.05, width_ratio=1.15, taper=0.58,
        asymmetry=0.55, facets=6, facet_depth=0.42, erosion=0.030,
        burial=0.24, debris=2,
    ),
    # The isolated block. Rounder, fewer and shallower fractures.
    "ROCK_BOULDER": dict(
        mass=104.0, height_ratio=0.94, width_ratio=0.90, taper=0.80,
        asymmetry=0.22, facets=7, facet_depth=0.18, erosion=0.055,
        burial=0.14, debris=0,
    ),
    # Angular, for breaks in relief. The most aggressive facets of the family.
    "ROCK_CLIFF_FRAGMENT": dict(
        mass=108.0, height_ratio=1.24, width_ratio=0.68, taper=0.36,
        asymmetry=0.40, facets=4, facet_depth=0.52, erosion=0.022,
        burial=0.26, debris=1,
    ),
}

# Gate 6 -- CLUSTER GRAMMAR. Not a seventh rock: a COMPOSITION of the family, with a
# deliberate visual hierarchy. Never O O O O of equal siblings.
#   (archetype, scale vs primary, offset in units of primary mass, yaw degrees)
CLUSTER_RECIPE = [
    ("ROCK_MASSIVE", 1.00, (0.00, 0.00), 0.0),     # PRIMARY_MASS -- dominant, unmissable
    ("ROCK_LOW", 0.62, (1.05, 0.42), 37.0),        # SECONDARY_MASS -- supports, never equals
    ("ROCK_CLIFF_FRAGMENT", 0.41, (-0.72, 0.86), -64.0),
    ("ROCK_BOULDER", 0.23, (0.55, -0.95), 112.0),  # SMALL_FRAGMENT -- debris reads as debris
]

VARIANTS_PER_ARCHETYPE = 3


def vary(base, seed):
    """Archetype + seed -> bounded variant. Deterministic for a given seed."""
    rng = random.Random(seed)
    out = dict(base)
    for key, spread in VARIANCE.items():
        if key not in out:
            continue
        out[key] = out[key] * (1.0 + rng.uniform(-spread, spread))
    # Facet COUNT varies by at most one, and never below three: fewer than three cuts
    # stops reading as fracture and starts reading as a dented ball.
    out["facets"] = max(3, int(base["facets"] + rng.choice((-1, 0, 0, 1))))
    out["taper"] = max(0.18, min(1.35, out["taper"]))
    out["burial"] = max(0.06, min(0.45, out["burial"]))
    return out


# ----------------------------------------------------------------------------------
# Geometry Script pipeline
# ----------------------------------------------------------------------------------

def new_mesh():
    return unreal.DynamicMesh()


def base_mass(mesh, genome, rng):
    """The generating mass: a coarse sphere, later cut into faces.

    A sphere (not a box) is the right seed for a stylized rock: plane cuts then CREATE
    the flat faces, so every face is a deliberate fracture. Starting from a box gives
    six faces you did not choose and then have to hide.
    """
    prim_opts = unreal.GeometryScriptPrimitiveOptions()
    radius = genome["mass"]

    lib, fn, name = pick(
        ("GeometryScript_Primitives",),
        ("append_sphere_box", "append_box_sphere", "append_sphere_lat_long"),
    )
    if fn is None:
        raise RuntimeError("no sphere primitive available in this build")

    xf = unreal.Transform(location=unreal.Vector(0.0, 0.0, 0.0))
    try:
        if "box" in name:
            mesh = fn(mesh, prim_opts, xf, radius=radius,
                      steps_x=4, steps_y=4, steps_z=4,
                      origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
        else:
            mesh = fn(mesh, prim_opts, xf, radius=radius,
                      latitude_steps=9, longitude_steps=12,
                      origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    except Exception as exc:  # noqa: BLE001 -- signature drift across builds
        warn("sphere call %s failed (%s); retrying positionally" % (name, exc))
        mesh = fn(mesh, prim_opts, xf, radius)
    return mesh


def scale_mesh(mesh, sx, sy, sz):
    lib, fn, name = pick(("GeometryScript_MeshTransforms",), ("scale_mesh",))
    if fn is None:
        raise RuntimeError("scale_mesh unavailable")
    try:
        return fn(mesh, unreal.Vector(sx, sy, sz), unreal.Vector(0.0, 0.0, 0.0))
    except Exception:
        return fn(mesh, unreal.Vector(sx, sy, sz))


def translate_mesh(mesh, dx, dy, dz):
    return unreal.GeometryScript_MeshTransforms.translate_mesh(
        mesh, unreal.Vector(dx, dy, dz))


def normal_to_rotator(normal):
    """Rotator whose +Z axis points along `normal`."""
    nx, ny, nz = normal
    length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
    nx, ny, nz = nx / length, ny / length, nz / length
    yaw = math.degrees(math.atan2(ny, nx))
    pitch = math.degrees(math.asin(max(-1.0, min(1.0, nz))))
    # A rotator orients +X; carrying +Z onto the normal means pitching back 90 degrees.
    return unreal.Rotator(0.0, pitch - 90.0, yaw)


def plane_cut(mesh, origin, normal, fill=True):
    """One planar fracture. Material on the plane's +Z side is removed."""
    lib, fn, name = pick(
        ("GeometryScript_MeshEdits", "GeometryScript_MeshBooleans",
         "GeometryScript_MeshModeling"),
        ("apply_mesh_plane_cut",),
    )
    if fn is None:
        return mesh, False
    opts = None
    opt_type = getattr(unreal, "GeometryScriptMeshPlaneCutOptions", None)
    if opt_type is not None:
        opts = opt_type()
        for prop, value in (("fill_holes", fill), ("fill_spans", False),
                            ("flip_cut_side", False)):
            try:
                opts.set_editor_property(prop, value)
            except Exception:
                pass
    frame = unreal.Transform(
        location=unreal.Vector(origin[0], origin[1], origin[2]),
        rotation=normal_to_rotator(normal),
    )
    try:
        mesh = fn(mesh, frame, opts) if opts is not None else fn(mesh, frame)
        return mesh, True
    except Exception as exc:  # noqa: BLE001
        warn("plane cut failed: %s" % exc)
        return mesh, False


def erode(mesh, genome, rng):
    """Low-frequency displacement. Deliberately weak -- this is not a detail pass."""
    amplitude = genome["erosion"] * genome["mass"]
    if amplitude <= 0.01:
        return mesh
    lib, fn, name = pick(("GeometryScript_MeshDeformers",),
                         ("apply_perlin_noise_to_mesh",))
    if fn is None:
        warn("no perlin deformer in this build; skipping erosion")
        return mesh
    try:
        layer = unreal.GeometryScriptPerlinNoiseLayerOptions()
        layer.set_editor_property("magnitude", amplitude)
        # Frequency is per-UU; a long wavelength keeps the large faces readable.
        layer.set_editor_property("frequency", 1.0 / max(35.0, genome["mass"] * 0.55))
        layer.set_editor_property("random_seed", rng.randint(1, 1 << 20))
        opts = unreal.GeometryScriptPerlinNoiseOptions()
        # This build exposes a single `base_layer`, not a `layers` array. Setting the
        # array name silently did nothing and every rock came out un-eroded.
        opts.set_editor_property("base_layer", layer)
        try:
            opts.set_editor_property(
                "empty_behavior",
                unreal.GeometryScriptEmptySelectionBehavior.FULL_MESH_SELECTION)
        except Exception:
            pass
        mesh = fn(mesh, unreal.GeometryScriptMeshSelection(), opts)
    except Exception as exc:  # noqa: BLE001
        warn("erosion failed: %s" % exc)
    return mesh


def weld_in(mesh, other, location, scale, yaw):
    """Union a second mass in. Falls back to a plain append when booleans are absent."""
    xf = unreal.Transform(
        location=unreal.Vector(location[0], location[1], location[2]),
        rotation=unreal.Rotator(0.0, 0.0, yaw),
        scale=unreal.Vector(scale, scale, scale),
    )
    lib, fn, name = pick(("GeometryScript_MeshBooleans",), ("apply_mesh_boolean",))
    if fn is None:
        warn("no boolean available; satellite mass dropped")
        return mesh
    try:
        opts = unreal.GeometryScriptMeshBooleanOptions()
        for prop, value in (("fill_holes", True), ("simplify_output", True),
                            ("simplify_planar_tolerance", 0.5)):
            try:
                opts.set_editor_property(prop, value)
            except Exception:
                pass
        return fn(mesh, unreal.Transform(), other, xf,
                  unreal.GeometryScriptBooleanOperation.UNION, opts)
    except Exception as exc:  # noqa: BLE001
        warn("boolean union failed: %s" % exc)
        return mesh


def planar_simplify(mesh, angle_threshold=1.5):
    """Merge coplanar triangles produced by the cuts.

    A plane cut leaves a fan of triangles across one flat face. Collapsing them costs
    nothing visually -- the face is flat either way -- and it is what keeps a stylized
    rock cheap enough not to need Nanite. Runs BEFORE normals, so the split-normal pass
    sees the final topology.
    """
    lib, fn, name = pick(("GeometryScript_MeshSimplification",),
                         ("apply_simplify_to_planar",))
    if fn is None:
        return mesh
    try:
        opts = unreal.GeometryScriptPlanarSimplifyOptions()
        try:
            opts.set_editor_property("angle_threshold", angle_threshold)
        except Exception:
            pass
        return fn(mesh, opts)
    except Exception as exc:  # noqa: BLE001
        warn("planar simplify failed: %s" % exc)
        return mesh


def finalize_normals(mesh, opening_angle=34.0):
    """Crisp facets: hard edges along the fractures, smooth across the rest.

    This build has no GeometryScript_MeshNormals -- the library is GeometryScript_Normals,
    and GeometryScriptCalculateNormalsOptions carries no angle threshold. The threshold
    lives on GeometryScriptSplitNormalsOptions.opening_angle_deg, so splitting is what
    actually produces the faceted read.
    """
    lib, fn, name = pick(("GeometryScript_Normals",), ("compute_split_normals",))
    if fn is None:
        return mesh
    try:
        split = unreal.GeometryScriptSplitNormalsOptions()
        for prop, value in (("split_by_opening_angle", True),
                            ("opening_angle_deg", opening_angle),
                            ("split_by_face_group", False)):
            try:
                split.set_editor_property(prop, value)
            except Exception:
                pass
        calc = unreal.GeometryScriptCalculateNormalsOptions()
        for prop, value in (("angle_weighted", True), ("area_weighted", True)):
            try:
                calc.set_editor_property(prop, value)
            except Exception:
                pass
        return fn(mesh, split, calc)
    except Exception as exc:  # noqa: BLE001
        warn("split normals failed: %s" % exc)
        return mesh


def park_on_contact_plane(mesh, genome):
    """Place the contact plane at local Z = CONTACT_Z, mass continuing below it.

    Gate 4 expressed in mesh space. `burial` of the total height ends up UNDER the
    terrain, so the silhouette that meets the ground is the rock's widest live
    section, not its bottom rim.
    """
    bbox = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    height = max(1.0, bbox.max.z - bbox.min.z)
    cx = (bbox.min.x + bbox.max.x) * 0.5
    cy = (bbox.min.y + bbox.max.y) * 0.5
    contact_local_z = bbox.min.z + height * genome["burial"]
    mesh = translate_mesh(mesh, -cx, -cy, CONTACT_Z - contact_local_z)
    after = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    log("  parked bounds z=[%.1f, %.1f] contact=%.1f buried=%.1f" % (
        after.min.z, after.max.z, CONTACT_Z, CONTACT_Z - after.min.z))
    return mesh


def build_rock(genome, seed, park=True):
    """GENOME -> DynamicMesh, contact plane parked at local Z = CONTACT_Z."""
    rng = random.Random(seed ^ 0x5F3A91)
    mesh = new_mesh()
    mesh = base_mass(mesh, genome, rng)

    # Aspect: the single step that turns a ball into a stature.
    mesh = scale_mesh(mesh, 1.0, genome["width_ratio"], genome["height_ratio"])

    r = genome["mass"]
    top = r * genome["height_ratio"]

    # Taper, as two opposed slanted cuts rather than a vertex-space taper: a cut
    # leaves a FACE, which is the look this direction wants, where a smooth taper
    # leaves a cone.
    taper_inset = r * (1.0 - genome["taper"]) * 0.60
    drift = genome["asymmetry"]
    for i in range(2):
        ang = rng.uniform(0.0, math.pi * 2.0)
        # The drift makes the two cuts unequal -- that inequality IS the asymmetry.
        bias = 1.0 + (drift if i == 0 else -drift) * 0.8
        nx, ny = math.cos(ang), math.sin(ang)
        origin = (nx * (r - taper_inset * bias), ny * (r - taper_inset * bias), top * 0.35)
        mesh, _ = plane_cut(mesh, origin, (nx, ny, 0.55))

    # The fractures proper.
    for i in range(int(genome["facets"])):
        ang = rng.uniform(0.0, math.pi * 2.0)
        tilt = rng.uniform(-0.45, 0.75)
        depth = genome["facet_depth"] * rng.uniform(0.72, 1.0)
        nx, ny = math.cos(ang), math.sin(ang)
        # Keep cuts above the contact plane: a fracture below ground is invisible work.
        zc = rng.uniform(-top * 0.10, top * 0.80)
        origin = (nx * r * (1.0 - depth),
                  ny * r * genome["width_ratio"] * (1.0 - depth), zc)
        mesh, _ = plane_cut(mesh, origin, (nx, ny, tilt))

    mesh = erode(mesh, genome, rng)

    # Satellite masses: debris that belongs to the same rock, welded at the base.
    for i in range(int(genome.get("debris", 0))):
        sat_genome = dict(genome)
        sat_genome["mass"] = genome["mass"] * rng.uniform(0.16, 0.30)
        sat = new_mesh()
        sat = base_mass(sat, sat_genome, rng)
        sat = scale_mesh(sat, 1.0, rng.uniform(0.8, 1.25), rng.uniform(0.55, 0.9))
        ang = rng.uniform(0.0, math.pi * 2.0)
        dist = r * rng.uniform(0.78, 1.06)
        mesh = weld_in(
            mesh, sat,
            (math.cos(ang) * dist,
             math.sin(ang) * dist * genome["width_ratio"],
             -top * rng.uniform(0.30, 0.62)),
            1.0, math.degrees(ang),
        )

    if park:
        mesh = park_on_contact_plane(mesh, genome)
        mesh = planar_simplify(mesh)
        mesh = finalize_normals(mesh)
    return mesh


def build_cluster(seed):
    """Gate 6: PRIMARY_MASS + SECONDARY_MASS + SMALL_FRAGMENT as one composed mesh."""
    rng = random.Random(seed ^ 0xC1057)
    mesh = new_mesh()
    primary_mass = ARCHETYPES["ROCK_MASSIVE"]["mass"]
    for arch_id, rel_scale, (ox, oy), yaw in CLUSTER_RECIPE:
        genome = vary(ARCHETYPES[arch_id], rng.randint(1, 1 << 28))
        # Parts are NOT parked individually: the cluster is parked once, as one mass,
        # so the members keep their height relationship to each other.
        part = build_rock(genome, rng.randint(1, 1 << 28), park=False)
        mesh = weld_in(mesh, part,
                       (ox * primary_mass, oy * primary_mass, 0.0), rel_scale, yaw)
    mesh = park_on_contact_plane(mesh, ARCHETYPES["ROCK_MASSIVE"])
    mesh = planar_simplify(mesh)
    mesh = finalize_normals(mesh)
    return mesh


def save_static_mesh(mesh, asset_name):
    asset_path = PACKAGE_PATH + "/" + asset_name
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)
    opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    # NANITE: OFF for V1, deliberately. The mission's own rule is NANITE_ENABLE iff
    # measurable_or_structurally_justified, and neither holds here -- after
    # apply_simplify_to_planar these are a few hundred triangles of large flat faces.
    # Nanite's cost is fixed overhead per mesh; its benefit is triangle density these
    # rocks do not have and must not grow, because SILHOUETTE is the deliverable and
    # micro-detail is explicitly off-direction. Revisit only if a future rock carries
    # real displaced detail. Left explicit rather than defaulted so the choice is
    # visible in the pipeline, not inherited by accident.
    for prop, value in (("enable_nanite", False),
                        ("enable_collision", True),
                        ("enable_recompute_normals", False),
                        ("enable_recompute_tangents", True)):
        try:
            opts.set_editor_property(prop, value)
        except Exception:
            pass
    new_asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
        mesh, asset_path, opts
    )
    if new_asset is None:
        unreal.log_error("[create_rock_assets] FAIL no asset at " + asset_path)
        return None
    # Collision through Geometry Script rather than an editor subsystem: under
    # -run=pythonscript, get_editor_subsystem(StaticMeshEditorSubsystem) returns None
    # (the editor subsystems are not brought up in a commandlet), and
    # EditorStaticMeshLibrary is deprecated in 5.8. This path needs neither.
    try:
        col_opts = unreal.GeometryScriptCollisionFromMeshOptions()
        for prop, value in (
                ("method", unreal.GeometryScriptCollisionGenerationMethod.CONVEX_HULLS),
                # A rock is one convex-ish mass; a handful of hulls is plenty, and the
                # buried skirt never needs its own shape.
                ("max_convex_hulls_per_mesh", 4),
                ("simplify_hulls", True),
                ("convex_hull_target_face_count", 24)):
            try:
                col_opts.set_editor_property(prop, value)
            except Exception:
                pass
        unreal.GeometryScript_Collision.set_static_mesh_collision_from_mesh(
            mesh, new_asset, col_opts,
            unreal.GeometryScriptSetStaticMeshCollisionOptions())
    except Exception as exc:  # noqa: BLE001
        warn("collision skipped for %s: %s" % (asset_name, exc))

    unreal.EditorAssetLibrary.save_asset(new_asset.get_path_name())
    bounds = new_asset.get_bounding_box()
    # There is no MeshQueries triangle-count call in this build; the count lives on the
    # DynamicMesh object itself.
    tris = "?"
    for getter in ("get_triangle_count", "get_vertex_count"):
        fn = getattr(mesh, getter, None)
        if fn is not None:
            try:
                tris = "%s=%s" % (getter.replace("get_", ""), fn())
                break
            except Exception:
                continue
    log("SAVED %s outcome=%s tris=%s bounds z=[%.1f,%.1f] xy=[%.1f,%.1f]" % (
        asset_name, outcome, tris, bounds.min.z, bounds.max.z,
        bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y))
    return new_asset


def main():
    log("=== ROCK_FORGE_001 build begin ===")
    only = os.environ.get("ANASTASIS_ROCK_ONLY", "").strip()
    wanted = [a.strip() for a in only.split(",") if a.strip()] if only else list(ARCHETYPES.keys())

    built = []
    for arch_id in wanted:
        if arch_id not in ARCHETYPES:
            warn("unknown archetype %s" % arch_id)
            continue
        base = ARCHETYPES[arch_id]
        for variant in range(VARIANTS_PER_ARCHETYPE):
            seed = stable_seed(arch_id, variant)
            genome = dict(base) if variant == 0 else vary(base, seed)
            short = arch_id.replace("ROCK_", "").title().replace("_", "")
            name = "SM_Rock_%s_%02d" % (short, variant + 1)
            log("build %s seed=%d genome=%s" % (
                name, seed,
                {k: (round(v, 3) if isinstance(v, float) else v)
                 for k, v in sorted(genome.items())}))
            try:
                mesh = build_rock(genome, seed)
                if save_static_mesh(mesh, name):
                    built.append(name)
            except Exception as exc:  # noqa: BLE001
                unreal.log_error("[create_rock_assets] FAILED %s: %s" % (name, exc))

    if not only:
        try:
            if save_static_mesh(build_cluster(7717), "SM_Rock_Cluster_01"):
                built.append("SM_Rock_Cluster_01")
        except Exception as exc:  # noqa: BLE001
            unreal.log_error("[create_rock_assets] FAILED cluster: %s" % exc)

    log("=== ROCK_FORGE_001 done, %d assets: %s ===" % (len(built), ", ".join(built)))
    return True


main()

if os.environ.get("ANASTASIS_ROCK_KEEP_EDITOR", "0") != "1":
    unreal.SystemLibrary.quit_editor()
