"""
RUIN_GRAMMAR_V1 -- le vocabulaire des vestiges d'ANASTASIS, par Geometry Script.

    GENOME -> ARCHETYPE -> seed -> variance bornee -> DynamicMesh -> StaticMesh

CE QU'EST UNE RUINE ICI, ET CE QU'ELLE N'EST PAS
------------------------------------------------
docs/visual/reference/pontique-grammaire-architecturale-batiment.png fait autorite, et
elle decrit un monde post-1204 ou des refugies rhomaioi RECONSTRUISENT : « Les memes
mains reconstruisent, avec moins, mais toujours avec sens ». Ce n'est pas l'antiquite.

La planche decompose une maison modeste. Le panneau SOUBASSEMENT dit « Pierre locale,
assise seche. Drainage, contact terrain ». Le panneau MURS dit « Ossature bois,
clayonnage, torchis, maconnerie mixte ». Autrement dit : le bois, le torchis et la
toiture disparaissent, **la pierre reste**. Ce qui subsiste d'une maison n'est pas un pan
de mur dresse, c'est son SOUBASSEMENT -- une empreinte basse, assisee, orthogonale.

La simulation dit la meme chose : une tuile Ruin porte `Resource = Stone, Amount = 8`,
et la planche liste « reemplois » parmi ses principes de generation. Une ruine est un
soubassement de pierre ET un tas de pierre reutilisable.

Le mesh precedent (SM_Ruin_Generic_01, cinq boites dont deux pans verticaux) racontait
l'inverse : des murs dresses, hauts de 65 a 118 uu -- au genou d'un arbre de canopee qui
en fait 500. 448 exemplaires eparpilles au yaw aleatoire. Ca se lisait comme un semis de
pierres tombales, et c'etait exact : trop petit pour du bati, trop vertical pour du bloc.

GENOME ORTHOGONAL, PAS EROSIF
-----------------------------
L'inverse exact du genome rocheux de create_rock_assets.py, et c'est voulu :

    roche                        ruine
    asymetrie controlee    ->    orthogonalite, angles droits
    fracture (plane cut)   ->    assises horizontales empilees
    aucun plan             ->    une empreinte de batiment
    yaw aleatoire          ->    UN site = UN axe (voir AnastasisRuinDressing)

Le pivot suit la meme convention que les roches : le plan de contact est pose a
Z local = -50, parce que ResolveInstanceTransform remonte d'une constante 50 x Scale
independante des bounds. Ce qui passe sous ce plan est enfoui -- ici c'est `settle`, le
tassement du soubassement dans le sol, qui est precisement ce que la planche appelle
« contact terrain ».

Echelle : une tuile fait 100 uu, soit un metre (meme regle que CreateCodeDefaults pour
les arbres). Un noyau d'habitation modeste fait donc 400 a 700 uu de cote.

TOUJOURS RECABLER APRES AVOIR REGENERE
--------------------------------------
save_static_mesh() fait delete_asset puis recree. Supprimer un mesh que
DA_AnastasisPresentation reference fait TOMBER la reference : apres une regeneration,
l'entree RUIN du registre etait revenue a son unique SM_Ruin_Generic_01 d'origine, et
462 placements planifies ne dessinaient plus rien -- sans la moindre erreur.

    create_ruin_grammar.py   PUIS   set_ruin_presentation.py

Jamais l'inverse. Le meme piege attend create_rock_assets.py / set_rock_presentation.py.

Run headless :
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<root>/tools/unreal/create_ruin_grammar.py"

Env :
  ANASTASIS_RUIN_ONLY           archetypes a batir, separes par des virgules
  ANASTASIS_RUIN_KEEP_EDITOR=1  ne pas quitter l'editeur
"""
import math
import os
import random
import zlib

import unreal

PACKAGE_PATH = "/Game/Anastasis/Architecture"
CONTACT_Z = -50.0


def log(msg):
    unreal.log("[create_ruin_grammar] " + str(msg))


def warn(msg):
    unreal.log_warning("[create_ruin_grammar] " + str(msg))


def stable_seed(*parts):
    """crc32, jamais hash() : str.__hash__ est sale par processus."""
    return zlib.crc32("|".join(str(p) for p in parts).encode("utf-8")) & 0x3FFFFFFF


def pick(lib_names, fn_names):
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
# ANASTASIS_RUIN_GENOME
# ----------------------------------------------------------------------------------
#   plan_w / plan_d  empreinte du batiment disparu, en UU (100 uu = 1 m)
#   wall_t           epaisseur du mur. Une assise seche est epaisse : 45 a 70 uu
#   course_h         hauteur d'une assise
#   courses          nombre d'assises encore debout
#   breach           part du perimetre effondree, 0..1
#   decay            irregularite du haut des assises (pierres manquantes)
#   rubble           tas de pierre de reemploi au pied
#   settle           tassement sous le plan de contact -- « drainage, contact terrain »
#   lean             devers des assises hautes, en degres. Faible : c'est du bati
# ----------------------------------------------------------------------------------

VARIANCE = {
    "plan_w": 0.16, "plan_d": 0.16, "wall_t": 0.12, "course_h": 0.10,
    "breach": 0.22, "decay": 0.25, "settle": 0.18, "lean": 0.30,
}

ARCHETYPES = {
    # LE vestige canonique : l'empreinte au sol d'un noyau d'habitation, plan encore
    # lisible. C'est ce que la planche appelle SOUBASSEMENT.
    # Trois assises, pas deux, et des murs plus epais : la premiere passe donnait 52 cm
    # de haut sur une empreinte de 4,3 x 5,6 m, ce qui se lisait comme une trace au sol et
    # non comme une structure. 0,8 a 1 m reste exact -- un soubassement de pierre seche
    # fait couramment 0,6 a 1,2 m -- et c'est la hauteur a partir de laquelle la masse
    # porte une ombre, donc existe.
    "RUIN_SOUBASSEMENT": dict(
        plan_w=430.0, plan_d=560.0, wall_t=72.0, course_h=29.0, courses=3,
        breach=0.30, decay=0.45, rubble=3, settle=18.0, lean=1.5, kind="plan",
    ),
    # Un angle : deux murs qui se rencontrent. Les angles tiennent mieux que les pans,
    # donc il monte plus haut la ou les deux murs se contreventent.
    "RUIN_ANGLE": dict(
        plan_w=390.0, plan_d=430.0, wall_t=74.0, course_h=29.0, courses=5,
        breach=0.52, decay=0.55, rubble=2, settle=16.0, lean=2.2, kind="corner",
    ),
    # Un pan isole, casse aux deux bouts.
    "RUIN_MUR": dict(
        plan_w=520.0, plan_d=0.0, wall_t=68.0, course_h=27.0, courses=4,
        breach=0.40, decay=0.60, rubble=2, settle=14.0, lean=2.8, kind="wall",
    ),
    # Le foyer : « Foyer en pierre, evacuation fumee, coeur de la maison ». L'element
    # interieur le plus durable -- souvent la derniere chose lisible d'une maison.
    "RUIN_FOYER": dict(
        plan_w=195.0, plan_d=175.0, wall_t=56.0, course_h=25.0, courses=3,
        breach=0.35, decay=0.40, rubble=2, settle=10.0, lean=1.0, kind="hearth",
    ),
    # « Cloture, enclos » : mur de pierre seche, long et tres bas. Marque un territoire,
    # pas un abri.
    "RUIN_ENCLOS": dict(
        plan_w=760.0, plan_d=0.0, wall_t=52.0, course_h=24.0, courses=2,
        breach=0.46, decay=0.62, rubble=1, settle=12.0, lean=3.4, kind="wall",
    ),
    # « Reemplois » : la pierre triee, en attente d'etre reprise. Aucun plan, et c'est
    # le seul archetype de la famille qui n'en a pas.
    "RUIN_REEMPLOI": dict(
        plan_w=250.0, plan_d=210.0, wall_t=0.0, course_h=24.0, courses=0,
        breach=0.0, decay=0.0, rubble=7, settle=8.0, lean=0.0, kind="pile",
    ),
}

VARIANTS_PER_ARCHETYPE = 3


def vary(base, seed):
    rng = random.Random(seed)
    out = dict(base)
    for key, spread in VARIANCE.items():
        if key in out and isinstance(out[key], float) and out[key] != 0.0:
            out[key] = out[key] * (1.0 + rng.uniform(-spread, spread))
    if out["courses"] > 0:
        out["courses"] = max(1, int(base["courses"] + rng.choice((-1, 0, 0, 1))))
    out["breach"] = max(0.0, min(0.78, out["breach"]))
    return out


# ----------------------------------------------------------------------------------
# Construction
# ----------------------------------------------------------------------------------

def new_mesh():
    return unreal.DynamicMesh()


PRIM = unreal.GeometryScriptPrimitiveOptions()


def add_box(mesh, dx, dy, dz, x, y, z, yaw=0.0, roll=0.0):
    xf = unreal.Transform(
        location=unreal.Vector(x, y, z),
        rotation=unreal.Rotator(roll, 0.0, yaw),
    )
    return unreal.GeometryScript_Primitives.append_box(
        mesh, PRIM, xf,
        dimension_x=dx, dimension_y=dy, dimension_z=dz,
        steps_x=0, steps_y=0, steps_z=0,
        origin=unreal.GeometryScriptPrimitiveOriginMode.BASE,
    )


def course_run(mesh, genome, rng, length, cx, cy, axis_yaw, courses=None):
    """Un mur : des assises empilees, chacune legerement decalee.

    Le decalage et la variation de longueur d'une assise a l'autre sont ce qui fait
    lire « assise seche » plutot que « bloc extrude ». Ils restent PETITS : c'est du
    bati, l'orthogonalite doit survivre.
    """
    t = genome["wall_t"]
    ch = genome["course_h"]
    n = int(courses if courses is not None else genome["courses"])
    breach = genome["breach"]
    decay = genome["decay"]

    for i in range(n):
        # Les assises hautes sont les plus entamees : une ruine s'erode par le haut.
        high = i / max(1.0, float(n - 1)) if n > 1 else 0.0
        lost = breach * (0.35 + 0.65 * high)
        run = length * max(0.18, 1.0 - lost)
        # Le segment survivant ne reste pas centre : il glisse vers une extremite.
        slide = (length - run) * 0.5 * rng.uniform(-1.0, 1.0)
        jx = rng.uniform(-1.0, 1.0) * t * 0.10
        yaw = axis_yaw + rng.uniform(-1.0, 1.0) * genome["lean"] * 0.35
        roll = rng.uniform(-1.0, 1.0) * genome["lean"] * 0.25 * high

        a = math.radians(axis_yaw)
        ox = math.cos(a) * slide - math.sin(a) * jx
        oy = math.sin(a) * slide + math.cos(a) * jx

        # Hauteur d'assise entamee vers le haut : le sommet n'est jamais plat.
        h = ch * (1.0 - decay * 0.30 * high * rng.uniform(0.0, 1.0))
        mesh = add_box(mesh, run, t, h, cx + ox, cy + oy, i * ch, yaw=yaw, roll=roll)

        # Quelques pierres qui debordent : l'assise n'est pas un parement regulier.
        if rng.random() < 0.55:
            s = t * rng.uniform(0.45, 0.85)
            along = rng.uniform(-0.42, 0.42) * run
            px = cx + ox + math.cos(a) * along - math.sin(a) * t * 0.5
            py = cy + oy + math.sin(a) * along + math.cos(a) * t * 0.5
            mesh = add_box(mesh, s, s * rng.uniform(0.7, 1.1), ch * rng.uniform(0.55, 0.9),
                           px, py, i * ch, yaw=yaw + rng.uniform(-14.0, 14.0))
    return mesh


def scatter_rubble(mesh, genome, rng, count, spread_w, spread_d):
    """Pierre de reemploi au pied. Petite, posee a plat, jamais dressee."""
    ch = genome["course_h"] if genome["course_h"] > 0 else 24.0
    for _ in range(int(count)):
        s = ch * rng.uniform(0.55, 1.15)
        mesh = add_box(
            mesh,
            s * rng.uniform(0.9, 1.6), s * rng.uniform(0.8, 1.3), s * rng.uniform(0.45, 0.8),
            rng.uniform(-0.5, 0.5) * spread_w,
            rng.uniform(-0.5, 0.5) * spread_d,
            -genome["settle"] * rng.uniform(0.0, 0.35),
            yaw=rng.uniform(0.0, 360.0),
            roll=rng.uniform(-7.0, 7.0),
        )
    return mesh


def build_ruin(genome, seed):
    rng = random.Random(seed ^ 0x9E3779B9)
    mesh = new_mesh()
    kind = genome["kind"]
    w, d, t = genome["plan_w"], genome["plan_d"], genome["wall_t"]

    if kind == "plan":
        # Les quatre murs du noyau. Chacun perd une part differente : un plan lisible
        # mais jamais complet.
        for yaw, length, cx, cy in (
                (0.0, w, 0.0, -d * 0.5),
                (0.0, w, 0.0, d * 0.5),
                (90.0, d, -w * 0.5, 0.0),
                (90.0, d, w * 0.5, 0.0)):
            mesh = course_run(mesh, genome, rng, length, cx, cy, yaw)
        # Les angles tiennent mieux : une assise de plus a chaque coin.
        for sx in (-1.0, 1.0):
            for sy in (-1.0, 1.0):
                if rng.random() < 0.72:
                    mesh = course_run(mesh, genome, rng, t * 2.1,
                                      sx * w * 0.5, sy * d * 0.5, 0.0,
                                      courses=int(genome["courses"]) + 1)
    elif kind == "corner":
        mesh = course_run(mesh, genome, rng, w, w * 0.5, 0.0, 0.0)
        mesh = course_run(mesh, genome, rng, d, 0.0, d * 0.5, 90.0)
        mesh = course_run(mesh, genome, rng, t * 2.3, 0.0, 0.0, 0.0,
                          courses=int(genome["courses"]) + 1)
    elif kind == "wall":
        mesh = course_run(mesh, genome, rng, w, 0.0, 0.0, 0.0)
    elif kind == "hearth":
        # Sole du foyer, puis trois cotes : l'ouverture regarde toujours quelque part.
        mesh = add_box(mesh, w, d, genome["course_h"] * 0.7, 0.0, 0.0, 0.0)
        mesh = course_run(mesh, genome, rng, w, 0.0, -d * 0.5, 0.0)
        mesh = course_run(mesh, genome, rng, d, -w * 0.5, 0.0, 90.0)
        mesh = course_run(mesh, genome, rng, d, w * 0.5, 0.0, 90.0)

    mesh = scatter_rubble(mesh, genome, rng, genome["rubble"],
                          max(w, 160.0) * 1.25, max(d, 160.0) * 1.25)

    mesh = park_on_contact_plane(mesh, genome)
    mesh = planar_simplify(mesh)
    mesh = finalize_normals(mesh)
    return mesh


def park_on_contact_plane(mesh, genome):
    """Plan de contact a Z = -50 ; `settle` de matiere passe dessous.

    Meme convention que create_rock_assets.py, pour la meme raison : le lift du
    resolver est une constante 50 x Scale, independante des bounds.
    """
    bbox = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    cx = (bbox.min.x + bbox.max.x) * 0.5
    cy = (bbox.min.y + bbox.max.y) * 0.5
    contact = bbox.min.z + genome["settle"]
    mesh = unreal.GeometryScript_MeshTransforms.translate_mesh(
        mesh, unreal.Vector(-cx, -cy, CONTACT_Z - contact))
    after = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    log("  parked z=[%.1f, %.1f] enfoui=%.1f hauteur_visible=%.1f" % (
        after.min.z, after.max.z, CONTACT_Z - after.min.z, after.max.z - CONTACT_Z))
    return mesh


def planar_simplify(mesh, angle_threshold=1.5):
    lib, fn, _ = pick(("GeometryScript_MeshSimplification",), ("apply_simplify_to_planar",))
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


def finalize_normals(mesh, opening_angle=28.0):
    """Angle plus serre que pour la roche : un bloc taille a des aretes franches."""
    lib, fn, _ = pick(("GeometryScript_Normals",), ("compute_split_normals",))
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
        return fn(mesh, split, calc)
    except Exception as exc:  # noqa: BLE001
        warn("split normals failed: %s" % exc)
        return mesh


def save_static_mesh(mesh, asset_name):
    asset_path = PACKAGE_PATH + "/" + asset_name
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)
    opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    # Nanite off, meme raison que la roche : quelques centaines de triangles de grandes
    # faces plates. La silhouette est le livrable.
    for prop, value in (("enable_nanite", False), ("enable_collision", True),
                        ("enable_recompute_normals", False),
                        ("enable_recompute_tangents", True)):
        try:
            opts.set_editor_property(prop, value)
        except Exception:
            pass
    new_asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
        mesh, asset_path, opts)
    if new_asset is None:
        unreal.log_error("[create_ruin_grammar] FAIL " + asset_path)
        return None
    try:
        col = unreal.GeometryScriptCollisionFromMeshOptions()
        for prop, value in (
                ("method", unreal.GeometryScriptCollisionGenerationMethod.CONVEX_HULLS),
                ("max_convex_hulls_per_mesh", 6), ("simplify_hulls", True),
                ("convex_hull_target_face_count", 24)):
            try:
                col.set_editor_property(prop, value)
            except Exception:
                pass
        unreal.GeometryScript_Collision.set_static_mesh_collision_from_mesh(
            mesh, new_asset, col, unreal.GeometryScriptSetStaticMeshCollisionOptions())
    except Exception as exc:  # noqa: BLE001
        warn("collision skipped for %s: %s" % (asset_name, exc))
    unreal.EditorAssetLibrary.save_asset(new_asset.get_path_name())
    b = new_asset.get_bounding_box()
    tris = "?"
    fn = getattr(mesh, "get_triangle_count", None)
    if fn is not None:
        try:
            tris = fn()
        except Exception:
            pass
    log("SAVED %-30s tris=%-6s empreinte=%.0f x %.0f  haut=%.0f" % (
        asset_name, tris, b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - CONTACT_Z))
    return new_asset


def main():
    log("=== RUIN_GRAMMAR_V1 begin ===")
    only = os.environ.get("ANASTASIS_RUIN_ONLY", "").strip()
    wanted = [a.strip() for a in only.split(",") if a.strip()] if only else list(ARCHETYPES)

    built = []
    for arch_id in wanted:
        if arch_id not in ARCHETYPES:
            warn("archetype inconnu " + arch_id)
            continue
        base = ARCHETYPES[arch_id]
        for variant in range(VARIANTS_PER_ARCHETYPE):
            seed = stable_seed(arch_id, variant)
            genome = dict(base) if variant == 0 else vary(base, seed)
            short = arch_id.replace("RUIN_", "").title()
            name = "SM_Ruin_%s_%02d" % (short, variant + 1)
            log("build %s seed=%d courses=%d breach=%.2f" % (
                name, seed, genome["courses"], genome["breach"]))
            try:
                if save_static_mesh(build_ruin(genome, seed), name):
                    built.append(name)
            except Exception as exc:  # noqa: BLE001
                unreal.log_error("[create_ruin_grammar] FAILED %s: %s" % (name, exc))

    log("=== RUIN_GRAMMAR_V1 done, %d assets: %s ===" % (len(built), ", ".join(built)))


main()

if os.environ.get("ANASTASIS_RUIN_KEEP_EDITOR", "0") != "1":
    unreal.SystemLibrary.quit_editor()
