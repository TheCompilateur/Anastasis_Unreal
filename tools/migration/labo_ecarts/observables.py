"""Grandeurs tirees d'un releve `labo-releve` v1 (bras.mjs, ou demain le harnais C++).

Trois echelles :

  micro         entre DEUX releves apparies : premier tick divergent (empreinte
                reduite a chaque tick, empreinte complete du harnais tous les M
                ticks), ecartement des trajectoires dans le temps
  comportement  un releve : repartition des buts, part en `observer`, part du
                temps dedans, duree des episodes de but, changements de but
  monde         un releve : survie, faim et soif, energie, moral, sante, stocks,
                recolte, liens, oui-dire, batiments, tresor

Toutes les grandeurs d'un releve sont des moyennes sur les habitants et les
echantillons ; un echantillon = un releve tous les `every` ticks. Les seuils
utilises ici (faim forte >= 70) sont ecrits ici, une fois, et cites dans les dossiers.
"""

from __future__ import annotations

import json
import math
from collections import Counter, defaultdict
from pathlib import Path

FAIM_FORTE = 70.0
SOIF_FORTE = 70.0


def lire_releve(chemin: Path) -> dict:
    entete, echantillons, fin = None, [], None
    with open(chemin, encoding="utf-8") as f:
        for ligne in f:
            o = json.loads(ligne)
            if o.get("kind") == "labo-releve":
                entete = o
            elif o.get("kind") == "fin":
                fin = o
            else:
                echantillons.append(o)
    if entete is None or fin is None:
        raise ValueError(f"{chemin} : releve incomplet (entete ou fin absente)")
    if entete.get("format") != 1:
        raise ValueError(f"{chemin} : format {entete.get('format')}, attendu 1")
    return {"entete": entete, "echantillons": echantillons, "fin": fin}


def _moy(xs):
    xs = [x for x in xs if x is not None]
    return sum(xs) / len(xs) if xs else float("nan")


def grandeurs(rel: dict) -> dict[str, float]:
    """Grandeurs d'un releve (comportement + monde). Cles stables : ce sont elles que les tolerances nomment."""
    e = rel["entete"]
    ech = rel["echantillons"]
    pas_s = e["every"] * e["dt"]
    jours = e["ticks"] * e["dt"] / e["dayLength"]
    g: dict[str, float] = {}

    # --- comportement -----------------------------------------------------------
    buts = Counter()
    total = 0
    dedans = 0
    episodes = defaultdict(list)       # but -> durees (s)
    changements = 0
    habitants_jours = 0.0
    courant: dict[str, tuple[str, int]] = {}
    for s in ech[1:]:                   # l'echantillon 0 est l'etat de depart
        for a in s["actors"]:
            buts[a["goal"]] += 1
            total += 1
            dedans += a["in"]
            prev = courant.get(a["id"])
            if prev is None:
                courant[a["id"]] = (a["goal"], 1)
            elif prev[0] == a["goal"]:
                courant[a["id"]] = (prev[0], prev[1] + 1)
            else:
                episodes[prev[0]].append(prev[1] * pas_s)
                changements += 1
                courant[a["id"]] = (a["goal"], 1)
    for but, n in courant.values():   # episodes tronques par la fin : comptes quand meme
        episodes[but].append(n * pas_s)
    if ech:
        habitants_jours = _moy([len(s["actors"]) for s in ech]) * jours
    for but, n in buts.items():
        g[f"part_but.{but}"] = n / total if total else float("nan")
    g["part_observer"] = buts.get("observer", 0) / total if total else float("nan")
    g["part_dedans"] = dedans / total if total else float("nan")
    toutes = [d for ds in episodes.values() for d in ds]
    g["duree_episode_moy_s"] = _moy(toutes)
    for but, ds in episodes.items():
        g[f"duree_episode.{but}"] = _moy(ds)
    g["changements_but_par_habitant_jour"] = changements / habitants_jours if habitants_jours else float("nan")

    # --- monde ------------------------------------------------------------------
    debut, fin = ech[0], ech[-1]
    ids0 = {a["id"] for a in debut["actors"]}
    idsF = {a["id"] for a in fin["actors"]}
    g["survivants_initiaux"] = len(ids0 & idsF)
    g["population_fin"] = len(idsF)
    tous = [a for s in ech[1:] for a in s["actors"]]
    g["faim_moy"] = _moy([a["hu"] for a in tous])
    g["soif_moy"] = _moy([a["th"] for a in tous])
    g["energie_moy"] = _moy([a["en"] for a in tous])
    g["social_moy"] = _moy([a["so"] for a in tous])
    g["loisir_moy"] = _moy([a["le"] for a in tous])
    g["hygiene_moy"] = _moy([a["hy"] for a in tous])
    g["moral_moy"] = _moy([a["mo"] for a in tous])
    g["sante_min"] = min((a["he"] for a in tous if a["he"] is not None), default=float("nan"))
    g["part_faim_forte"] = _moy([1.0 if (a["hu"] or 0) >= FAIM_FORTE else 0.0 for a in tous])
    g["part_soif_forte"] = _moy([1.0 if (a["th"] or 0) >= SOIF_FORTE else 0.0 for a in tous])
    g["stock_bati_moy"] = _moy([sum(b["food"] or 0 for b in s["buildings"]) for s in ech[1:]])
    g["stock_bati_fin"] = sum(b["food"] or 0 for b in fin["buildings"])
    g["nourriture_portee_fin"] = sum(a["food"] or 0 for a in fin["actors"])
    # Nets : prelevements moins repousse (les tuiles repoussent pendant la mesure).
    g["nourriture_recoltee_tuiles"] = debut["tiles"]["food"] - fin["tiles"]["food"]
    g["bois_preleve_tuiles"] = debut["tiles"]["wood"] - fin["tiles"]["wood"]
    g["pierre_prelevee_tuiles"] = debut["tiles"]["stone"] - fin["tiles"]["stone"]
    g["liens_positifs_fin"] = sum(a["relPos"] for a in fin["actors"])
    g["liens_somme_fin"] = sum(a["relSum"] for a in fin["actors"])
    g["oui_dire_fin"] = sum(a["ouiDire"] for a in fin["actors"])
    g["batiments_fin"] = sum(1 for b in fin["buildings"] if (b["progress"] or 0) >= 1)
    g["chantiers_fin"] = sum(1 for b in fin["buildings"] if (b["progress"] or 0) < 1)
    g["tresor_fin"] = fin.get("treasury") if fin.get("treasury") is not None else float("nan")
    return g


def micro(a: dict, b: dict) -> dict:
    """Divergence entre deux releves apparies (meme terrain, meme replique)."""
    ea, eb = a["fin"]["empreintesReduites"], b["fin"]["empreintesReduites"]
    t_reduite = next((t for t, (x, y) in enumerate(zip(ea, eb)) if x != y), None)
    t_complete = None
    sa = {s["t"]: s for s in a["echantillons"]}
    sb = {s["t"]: s for s in b["echantillons"]}
    for t in sorted(set(sa) & set(sb)):
        if "empreinte" in sa[t] and "empreinte" in sb[t] and sa[t]["empreinte"] != sb[t]["empreinte"]:
            t_complete = t
            break
    serie = separation(a, b)
    fenetre = []
    if serie:
        t_fin = serie[-1][0]
        fenetre = [(p, q) for t, p, q in serie if t >= t_fin - a["entete"]["dayLength"]]
    return {
        "tDivReduite": t_reduite,
        "tDivCompleteEchantillonne": t_complete,
        "sepPosDernierJour": _moy([p for p, _ in fenetre]),
        "sepBesoinsDernierJour": _moy([q for _, q in fenetre]),
        "tSep1Case": next((t for t, p, _ in serie if p >= 1.0), None),
        "serie": serie,
    }


def separation(a: dict, b: dict) -> list[tuple[float, float, float]]:
    """(temps s, distance moyenne des positions en cases, ecart moyen des besoins) par echantillon commun."""
    sb = {s["t"]: s for s in b["echantillons"]}
    dt = a["entete"]["dt"]
    out = []
    for s in a["echantillons"]:
        o = sb.get(s["t"])
        if o is None:
            continue
        pb = {x["id"]: x for x in o["actors"]}
        pos, bes = [], []
        for x in s["actors"]:
            y = pb.get(x["id"])
            if y is None or x["x"] is None or y["x"] is None:
                continue
            pos.append(math.hypot(x["x"] - y["x"], x["y"] - y["y"]))
            bes.append(_moy([abs((x[k] or 0) - (y[k] or 0)) for k in ("hu", "th", "en", "so", "le", "hy")]))
        if pos:
            out.append((s["t"] * dt, _moy(pos), _moy(bes)))
    return out
