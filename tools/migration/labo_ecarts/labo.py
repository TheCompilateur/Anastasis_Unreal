"""Laboratoire de jugement des ecarts de portage — point d'entree.

Methode : docs/migration/ecarts/METHODE.md. Outils : LISEZMOI.md (meme dossier).

  python tools/migration/labo_ecarts/labo.py cone     --ref <jsref> --experience <exp.json>
  python tools/migration/labo_ecarts/labo.py lancer   --ref <jsref> --experience <exp.json> --out <dossier>
  python tools/migration/labo_ecarts/labo.py analyser --experience <exp.json> --out <dossier> [--json <f>] [--md <f>]

`lancer` fait tourner les bras (bras.mjs, un processus Node par bras), saute ceux
dont le releve existe deja et est complet ; `analyser` ne lance rien, il lit.
Python 3.10+, bibliotheque standard seulement.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ICI = Path(__file__).resolve().parent
sys.path.insert(0, str(ICI))

import cone as cone_mod  # noqa: E402
import observables as obs  # noqa: E402
import stats  # noqa: E402

DEPOT = ICI.parents[2]
RESEED_BIS = 100000  # temoin-bis de la replique k : reseed = RESEED_BIS + k


# --- experience ----------------------------------------------------------------

def lire_experience(chemin: str) -> dict:
    exp = json.loads(Path(chemin).read_text(encoding="utf-8"))
    if exp.get("kind") != "labo-experience":
        raise SystemExit(f"{chemin} : pas une experience (kind={exp.get('kind')})")
    return exp


def calibration_de(exp: dict, terrain: str) -> list[list[str]]:
    """Paires A/A d'un terrain : `exp["calibration"][terrain]`, par defaut temoin contre temoin-bis."""
    return (exp.get("calibration") or {}).get(terrain, [["aucun", "aucun@bis"]])


def bras_a_lancer(exp: dict) -> list[dict]:
    """Tous les passages : (terrain, replique, bras). Le temoin-bis (`aucun@bis`) est toujours ajoute,
    et tout temoin nomme par une paire de calibration (`aucun@<base>` : reseed base + k)."""
    out = []
    for t in exp["terrains"]:
        calib = [b for paire in calibration_de(exp, t["terrain"]) for b in paire]
        bras = list(dict.fromkeys(exp["bras"] + ["aucun@bis"] + calib))
        for k in range(t["repliques"]):
            for b in bras:
                out.append({"terrain": t["terrain"], "replique": k, "bras": b, "jours": t["jours"]})
    return out


def chemin_releve(out: Path, exp: dict, p: dict) -> Path:
    return out / exp["ecart"] / p["terrain"] / f"{p['bras'].replace('@', '_')}-r{p['replique']:03d}.jsonl"


def releve_complet(chemin: Path) -> bool:
    if not chemin.exists():
        return False
    try:
        with open(chemin, "rb") as f:
            f.seek(0)
            # la ligne `fin` porte l'empreinte reduite de chaque tick : elle est longue, on lit son debut.
            for ligne in f:
                if ligne.startswith(b'{"kind":"fin"'):
                    return True
            return False
    except OSError:
        return False


def commande_bras(ref: str, exp: dict, p: dict, sortie: Path) -> list[str]:
    ecart, bis = (p["bras"].split("@") + [None])[:2]
    cmd = ["node", str(ICI / "bras.mjs"), "-ref", ref, "-terrain", p["terrain"], "-replique", str(p["replique"]),
           "-jours", str(p["jours"]), "-ecart", ecart, "-every", str(exp.get("every", 60)),
           "-empreinte-every", str(exp.get("empreinteEvery", 900)), "-out", str(sortie)]
    if bis == "bis":
        cmd += ["-reseed", str(RESEED_BIS + p["replique"])]
    elif bis is not None:
        cmd += ["-reseed", str(int(bis) + p["replique"])]
    return cmd


def lancer(args) -> None:
    exp = lire_experience(args.experience)
    out = Path(args.out)
    todo = [p for p in bras_a_lancer(exp) if args.refaire or not releve_complet(chemin_releve(out, exp, p))]
    total = len(bras_a_lancer(exp))
    print(f"{exp['ecart']} : {total} bras, {len(todo)} a lancer, {args.travailleurs} travailleur(s)", flush=True)
    debut = time.time()
    echecs = []

    def un(p):
        sortie = chemin_releve(out, exp, p)
        sortie.parent.mkdir(parents=True, exist_ok=True)
        r = subprocess.run(commande_bras(args.ref, exp, p, sortie), capture_output=True, text=True)
        if r.returncode != 0:
            echecs.append((p, r.stderr.strip()[-800:]))
        return p

    fait = 0
    with ThreadPoolExecutor(max_workers=args.travailleurs) as pool:
        for p in pool.map(un, todo):
            fait += 1
            if fait % 10 == 0 or fait == len(todo):
                print(f"  {fait}/{len(todo)} ({time.time() - debut:.0f} s)", flush=True)
    for p, err in echecs:
        print(f"ECHEC {p} :\n{err}", file=sys.stderr)
    if echecs:
        raise SystemExit(f"{len(echecs)} bras en echec")


# --- analyse -------------------------------------------------------------------

def tolerance(table: dict, cle: str, moyenne_a: float) -> float | None:
    t = table.get(cle)
    if t is None and "." in cle:
        t = table.get(cle.split(".")[0] + ".*")
    if t is None:
        return None
    if "abs" in t:
        return t["abs"]
    base = abs(moyenne_a) * t["rel"] if moyenne_a == moyenne_a else 0.0
    return max(base, t.get("plancher", 0.0))


def _resume(xs):
    xs = [x for x in xs if x is not None and not (isinstance(x, float) and math.isnan(x))]
    if not xs:
        return None
    xs = sorted(xs)
    return {"n": len(xs), "min": xs[0], "mediane": xs[len(xs) // 2], "max": xs[-1], "moyenne": sum(xs) / len(xs)}


def analyser_comparaison(exp, tol, out: Path, terrain: dict, comp: dict) -> dict:
    a_id, b_id = comp["A"], comp["B"]
    N = terrain["repliques"]
    rel = {}
    for bras in dict.fromkeys([a_id, b_id, "aucun", "aucun@bis"]):
        rel[bras] = [obs.lire_releve(chemin_releve(out, exp, {"terrain": terrain["terrain"], "replique": k, "bras": bras}))
                     for k in range(N)]
    gA = [obs.grandeurs(r) for r in rel[a_id]]
    gB = [obs.grandeurs(r) for r in rel[b_id]]

    # micro : divergence A_k / B_k, et decorrelation rapportee au temoin-bis.
    micros = [obs.micro(rel[a_id][k], rel[b_id][k]) for k in range(N)]
    bis = [obs.micro(rel["aucun"][k], rel["aucun@bis"][k]) for k in range(N)]
    ratios = [m["sepPosDernierJour"] / b["sepPosDernierJour"] for m, b in zip(micros, bis)
              if b["sepPosDernierJour"] and b["sepPosDernierJour"] > 0]
    ic_ratio = stats.bootstrap_moyenne(ratios)[0.95] if len(ratios) > 1 else (float("nan"), float("nan"))
    t_div = [m["tDivReduite"] for m in micros]
    divergent = [t for t in t_div if t is not None]
    act_b = [r["fin"]["activation"].get("activations", 0) for r in rel[b_id]]
    act_a = [r["fin"]["activation"].get("activations", 0) for r in rel[a_id]]

    # grandeurs : jugees (dans la table) et rapportees.
    cles = sorted(set().union(*[g.keys() for g in gA + gB]))
    regle = 0.02
    jugees, rapportees = {}, {}
    for cle in cles:
        a = [g.get(cle) for g in gA]
        b = [g.get(cle) for g in gB]
        if cle.startswith(("part_but.", "duree_episode.")):
            but = cle.split(".", 1)[1]
            parts = [g.get(f"part_but.{but}", 0.0) or 0.0 for g in gA]
            if sum(parts) / len(parts) < regle:
                # but marginal : 0 si absent pour les parts, non juge pour les durees.
                if cle.startswith("part_but."):
                    rapportees[cle] = stats.juger([x or 0.0 for x in a], [x or 0.0 for x in b], None)
                continue
            if cle.startswith("part_but."):
                a = [x or 0.0 for x in a]
                b = [x or 0.0 for x in b]
        ma = stats.moyenne([x for x in a if x is not None and x == x])
        delta = tolerance(tol["grandeurs"], cle, ma)
        r = stats.juger(a, b, delta)
        r["echelle"] = (tol["grandeurs"].get(cle) or tol["grandeurs"].get(cle.split(".")[0] + ".*") or {}).get("echelle")
        (jugees if delta is not None else rapportees)[cle] = r

    # Regle v2 : correction de Holm sur les p des grandeurs jugees (familles = une comparaison).
    stats.holm(jugees)
    ratio_moy = stats.moyenne(ratios) if ratios else float("nan")
    dormant = sum(act_b) == 0 and sum(act_a) == 0 and not comp.get("calibration")
    v1 = decider(jugees, gA, gB, cles, dormant, ratio_moy, tol["decorrelation"], cle_verdict="verdict")
    v2 = decider(jugees, gA, gB, cles, dormant, ratio_moy, tol["decorrelation"], cle_verdict="verdictHolm")
    verdict, nature, ruptures = v2["verdict"], v2["nature"], v2["ruptures"]

    return {
        "terrain": terrain,
        "comparaison": comp,
        "activation": {"A": _resume(act_a), "B": _resume(act_b)},
        "bit": {
            "verdict": "IDENTIQUE" if not divergent else "DIVERGE",
            "pairesDivergentes": len(divergent),
            "paires": N,
            "tDivReduite": _resume(divergent),
            "tDivCompleteEchantillonne": _resume([m["tDivCompleteEchantillonne"] for m in micros]),
        },
        "micro": {
            "sepPosDernierJour_AB": _resume([m["sepPosDernierJour"] for m in micros]),
            "sepPosDernierJour_temoinBis": _resume([b["sepPosDernierJour"] for b in bis]),
            "sepBesoinsDernierJour_AB": _resume([m["sepBesoinsDernierJour"] for m in micros]),
            "tSep1Case_s": _resume([m["tSep1Case"] for m in micros]),
            "decorrelation": {"moyenne": ratio_moy, "ic95": ic_ratio, "n": len(ratios)},
            "serieMediane": serie_mediane([m["serie"] for m in micros]),
            "serieMedianeTemoinBis": serie_mediane([b["serie"] for b in bis]),
        },
        "jugees": jugees,
        "rapportees": rapportees,
        "ruptures": ruptures,
        "verdict": verdict,
        "nature": nature,
        "regleV1": v1,
        "cout_s": {"A": _resume([r["fin"]["secondes"] for r in rel[a_id]]),
                   "B": _resume([r["fin"]["secondes"] for r in rel[b_id]])},
    }


def decider(jugees, gA, gB, cles, dormant, ratio_moy, seuils, cle_verdict) -> dict:
    """Verdict d'une comparaison. `cle_verdict` : `verdict` (regle v1, sans correction) ou `verdictHolm` (v2).

    v1 est la regle declaree avant la premiere mesure. Elle juge chaque grandeur a 5 % sans tenir
    compte du nombre de grandeurs (~50) : la calibration A/A (temoin contre temoin-bis) montre ce
    que cela coute. v2 exige, pour DIFFERENT, que le p corrige par Holm soit < 0.05.
    """
    ruptures = []
    for cle in ("survivants_initiaux", "population_fin"):
        if cle in jugees and jugees[cle][cle_verdict] == "DIFFERENT":
            ruptures.append(f"survie : {cle} {jugees[cle]['diff']:+.3f}")
    buts = {c.split(".", 1)[1] for c in cles if c.startswith("part_but.")}
    for but in sorted(buts):
        pa = stats.moyenne([g.get(f"part_but.{but}", 0.0) or 0.0 for g in gA])
        pb = stats.moyenne([g.get(f"part_but.{but}", 0.0) or 0.0 for g in gB])
        if (pa >= 0.02 and pb < 0.002) or (pb >= 0.02 and pa < 0.002):
            ruptures.append(f"but `{but}` : part {pa:.3f} -> {pb:.3f}")
    for cle, r in jugees.items():
        if r[cle_verdict] == "DIFFERENT" and r["delta"] and abs(r["diff"]) > 3 * r["delta"]:
            ruptures.append(f"ampleur : {cle} {r['diff']:+.3g} > 3 x delta ({r['delta']:.3g})")
    differents = sorted(c for c, r in jugees.items() if r[cle_verdict] == "DIFFERENT")
    indetermines = sorted(c for c, r in jugees.items() if r[cle_verdict] == "INDETERMINE")
    if dormant:
        verdict = "DORMANT"
    elif ruptures:
        verdict = "RUPTURE"
    elif differents:
        verdict = "DERIVE"
    elif indetermines:
        verdict = "INDETERMINE"
    else:
        verdict = "NEUTRE"
    if verdict == "DORMANT":
        nature = "dormant"
    elif verdict == "RUPTURE":
        nature = "rupture"
    elif verdict == "DERIVE":
        nature = "derivant"
    elif verdict == "NEUTRE" and ratio_moy >= seuils["seuilDecorrele"]:
        nature = "chaotique"
    elif verdict == "NEUTRE":
        nature = "local (neutre, trajectoires non decorrelees)"
    else:
        nature = "indeterminee (N insuffisant)"
    return {"verdict": verdict, "nature": nature, "ruptures": ruptures, "differents": differents,
            "indetermines": indetermines}


def serie_mediane(series: list[list], points: int = 12) -> list:
    """Separation mediane (positions, besoins) a `points` instants repartis : la vitesse d'ecartement."""
    if not series or not series[0]:
        return []
    n = min(len(s) for s in series)
    idx = sorted({int(round(i * (n - 1) / (points - 1))) for i in range(points)})
    out = []
    for i in idx:
        pos = sorted(s[i][1] for s in series)
        bes = sorted(s[i][2] for s in series)
        out.append([round(series[0][i][0], 2), pos[len(pos) // 2], bes[len(bes) // 2]])
    return out


def confronter(prediction: dict, res: dict) -> dict:
    """Prediction (INF) contre mesure (EVD) : par terrain, nature predite / mesuree, attendus un par un."""
    out = {"naturePredite": prediction.get("nature"), "natureMesuree": res["nature"],
           "nature": "CONFIRMEE" if res["nature"].startswith(prediction.get("nature", "?")) else "REFUTEE",
           "attendus": []}
    for at in prediction.get("attendus", []):
        cle = at["grandeur"]
        if cle == "bit.tDivReduite":
            t = res["bit"]["tDivReduite"]
            val = t["mediane"] if t else None
        elif cle == "micro.decorrelation":
            val = res["micro"]["decorrelation"]["moyenne"]
        else:
            r = res["jugees"].get(cle) or res["rapportees"].get(cle)
            val = r["diff"] if r else None
        attendu = at["attendu"]
        ok = None
        if val is not None and not (isinstance(val, float) and math.isnan(val)):
            if attendu == "equivalent":
                r = res["jugees"].get(cle)
                ok = bool(r and r["equivalent"])
            elif attendu == "hausse":
                r = res["jugees"].get(cle) or res["rapportees"].get(cle)
                ok = bool(r and r["exclutZero"] and r["diff"] > 0)
            elif attendu == "baisse":
                r = res["jugees"].get(cle) or res["rapportees"].get(cle)
                ok = bool(r and r["exclutZero"] and r["diff"] < 0)
            elif attendu.startswith(">="):
                ok = val >= float(attendu[2:])
            elif attendu.startswith("<="):
                ok = val <= float(attendu[2:])
            elif attendu.startswith("=="):
                ok = val == float(attendu[2:])
        out["attendus"].append({**at, "mesure": val, "tenu": ok})
    return out


def analyser(args) -> dict:
    exp = lire_experience(args.experience)
    tol = json.loads((ICI / "tolerances.json").read_text(encoding="utf-8"))
    out = Path(args.out)
    resultats = {"kind": "labo-resultats", "ecart": exp["ecart"], "experience": exp, "tolerances": tol["version"],
                 "analyseLe": time.strftime("%Y-%m-%d"), "terrains": []}
    for terrain in exp["terrains"]:
        calibrations = [{"id": f"calibration_AA:{a}/{b}", "A": a, "B": b, "calibration": True,
                         "question": "deux temoins : meme mecanique, autres suites de tirages. Ce que la methode "
                                     "declare sans aucun ecart = son taux de faux positifs."}
                        for a, b in calibration_de(exp, terrain["terrain"])]
        for comp in exp["comparaisons"] + calibrations:
            res = analyser_comparaison(exp, tol, out, terrain, comp)
            pred = (exp.get("predictions") or {}).get(f"{terrain['terrain']}/{comp['id']}")
            if pred:
                res["confrontation"] = confronter(pred, res)
            resultats["terrains"].append(res)
    if args.json:
        Path(args.json).parent.mkdir(parents=True, exist_ok=True)
        def arrondi(o):
            if isinstance(o, float):
                return o if (math.isnan(o) or math.isinf(o) or o == 0) else float(f"{o:.6g}")
            if isinstance(o, dict):
                return {k: arrondi(v) for k, v in o.items()}
            if isinstance(o, (list, tuple)):
                return [arrondi(v) for v in o]
            return o
        leger = arrondi(json.loads(json.dumps(resultats, default=float)))
        Path(args.json).write_text(json.dumps(leger, separators=(",", ":"), ensure_ascii=False, allow_nan=True) + "\n", encoding="utf-8")
    md = markdown(resultats)
    if args.md:
        Path(args.md).write_text(md, encoding="utf-8")
    print(md)
    return resultats


# --- rendu ---------------------------------------------------------------------

def _f(x, chiffres=3):
    if x is None:
        return "—"
    if isinstance(x, float):
        if math.isnan(x):
            return "nan"
        if math.isinf(x):
            return "inf"
        return f"{x:.{chiffres}g}"
    return str(x)


def markdown(R: dict) -> str:
    L = [f"# Resultats — ecart {R['ecart']} (tolerances v{R['tolerances']}, analyse {R['analyseLe']})", ""]
    for res in R["terrains"]:
        t, c = res["terrain"], res["comparaison"]
        L.append(f"## {t['terrain']} — {c['id']} : A = `{c['A']}`, B = `{c['B']}` — N = {t['repliques']}, D = {t['jours']} j")
        L.append("")
        v1 = res["regleV1"]
        L.append(f"**Verdict statistique (regle v2, Holm) : {res['verdict']}** — nature mesuree : **{res['nature']}**  ")
        L.append(f"Regle v1 (declaree, sans correction) : {v1['verdict']} / {v1['nature']} — "
                 f"{len(v1['differents'])} grandeur(s) DIFFERENT, {len(v1['indetermines'])} INDETERMINE  ")
        b = res["bit"]
        td = b["tDivReduite"]
        L.append(f"**Verdict au bit : {b['verdict']}** ({b['pairesDivergentes']}/{b['paires']} paires ; premier tick divergent "
                 f"median {_f(td['mediane']) if td else '—'}, min {_f(td['min']) if td else '—'}, max {_f(td['max']) if td else '—'})")
        L.append("")
        a = res["activation"]
        L.append(f"Activation (B) : {_f(a['B']['moyenne'] if a['B'] else None)} par passage en moyenne "
                 f"(min {_f(a['B']['min'] if a['B'] else None)}). Cout d'un passage : A {_f(res['cout_s']['A']['mediane'])} s, "
                 f"B {_f(res['cout_s']['B']['mediane'])} s (medianes).")
        m = res["micro"]
        dec = m["decorrelation"]
        L.append(f"Decorrelation (sep. A/B au dernier jour / sep. temoin/temoin-bis) : {_f(dec['moyenne'])} "
                 f"[IC95 {_f(dec['ic95'][0])} ; {_f(dec['ic95'][1])}], n = {dec['n']}. "
                 f"1re separation >= 1 case : median {_f(m['tSep1Case_s']['mediane']) if m['tSep1Case_s'] else '—'} s.")
        if m["serieMediane"]:
            L.append("")
            L.append("| t (s) | sep. positions A/B (cases, mediane) | sep. besoins A/B | sep. positions temoin/bis |")
            L.append("|---:|---:|---:|---:|")
            bis = {round(x[0], 2): x for x in m["serieMedianeTemoinBis"]}
            for tt, p, q in m["serieMediane"]:
                L.append(f"| {tt} | {_f(p)} | {_f(q)} | {_f(bis.get(round(tt, 2), [None, None])[1])} |")
        if res["ruptures"]:
            L.append("")
            L.append("Criteres de rupture declenches : " + " ; ".join(res["ruptures"]))
        L.append("")
        L.append("| grandeur | echelle | A | B | diff | IC95 | IC90 | delta | p | p Holm | effet (sd) | v1 | v2 (Holm) |")
        L.append("|---|---|---:|---:|---:|---|---|---:|---:|---:|---:|---|---|")
        for cle, r in sorted(res["jugees"].items(), key=lambda kv: (kv[1].get("echelle") or "", kv[0])):
            L.append(f"| `{cle}` | {r.get('echelle') or ''} | {_f(r['moyenneA'])} | {_f(r['moyenneB'])} | {_f(r['diff'])} | "
                     f"[{_f(r['ic95'][0])} ; {_f(r['ic95'][1])}] | [{_f(r['ic90'][0])} ; {_f(r['ic90'][1])}] | {_f(r['delta'])} | "
                     f"{_f(r['p_signe'], 2)} | {_f(r['p_holm'], 2)} | {_f(r['effet_sd'], 2)} | {r['verdict']} | {r['verdictHolm']} |")
        conf = res.get("confrontation")
        if conf:
            L.append("")
            L.append(f"Prediction : nature predite **{conf['naturePredite']}**, mesuree **{conf['natureMesuree']}** → **{conf['nature']}**")
            for at in conf["attendus"]:
                L.append(f"- `{at['grandeur']}` attendu {at['attendu']} — mesure {_f(at['mesure'])} — "
                         f"{'TENU' if at['tenu'] else 'NON TENU (non demontre ou contraire)' if at['tenu'] is False else 'non jugeable'}")
        L.append("")
    return "\n".join(L)


def cone(args) -> None:
    exp = lire_experience(args.experience)
    spec = exp["cone"]
    for bloc in spec:
        res = cone_mod.cone(Path(args.ref), bloc["entrees"], bloc.get("profondeur", 1), bloc.get("champs", []))
        print(f"### {bloc['titre']}\n")
        print(cone_mod.rapport_markdown(res))
        print()


def main() -> None:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("cone")
    c.add_argument("--ref", required=True)
    c.add_argument("--experience", required=True)
    l = sub.add_parser("lancer")
    l.add_argument("--ref", required=True)
    l.add_argument("--experience", required=True)
    l.add_argument("--out", required=True)
    l.add_argument("--travailleurs", type=int, default=max(1, min(4, (os.cpu_count() or 2) // 3)))
    l.add_argument("--refaire", action="store_true")
    a = sub.add_parser("analyser")
    a.add_argument("--experience", required=True)
    a.add_argument("--out", required=True)
    a.add_argument("--json")
    a.add_argument("--md")
    args = p.parse_args()
    {"cone": cone, "lancer": lancer, "analyser": analyser}[args.cmd](args)


if __name__ == "__main__":
    main()
