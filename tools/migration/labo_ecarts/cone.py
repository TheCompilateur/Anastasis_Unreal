"""Cone causal statique d'un ecart, lu dans le code de la reference JS (niveau 1, INF).

Ce n'est PAS un analyseur JavaScript : des expressions regulieres sur un index de
fonctions. Il ne voit ni les alias (`const n = npc`), ni les appels indirects
(`obj[nom]()`), ni l'ordre d'execution. Il sert a ne rien oublier d'evident en
ecrivant la prediction, pas a la remplacer : un champ qu'il ne liste pas peut
quand meme etre touche, et un lecteur qu'il liste peut ne jamais s'executer.

Ce qu'il rend, pour une liste de fonctions d'entree (le code omis ou remplace
par l'ecart) :

  portee      fonctions atteignables depuis les entrees, jusqu'a `profondeur`
  ecritures   champs ecrits (`npc.x = `, `a.relations[...] = `, `npc[key] = `)
              par fonction, objets usuels de la reference (npc, other, a, b, sim...)
  tirages     sites qui consomment `sim.rng` (appel direct, ou passage de la
              fonction `sim.rng` a une autre)
  dt          appels qui recoivent `dt` (ce qu'un changement de pas touche)
  lecteurs    pour chaque champ demande, les fonctions de TOUT `src/` qui le
              lisent sans l'ecrire
"""

from __future__ import annotations

import json
import re
from collections import defaultdict
from pathlib import Path

MOTS_CLES = {
    "if", "for", "while", "switch", "return", "function", "catch", "typeof", "new", "await",
    "import", "export", "const", "let", "var", "else", "do", "try", "throw", "delete", "void",
    "super", "class", "constructor", "of", "in", "async", "yield", "case",
}
OBJETS = r"(?:npc|other|a|b|actor|source|target|teller|listener|from|to|partner|mentor|n|sim|this)"
RE_FONCTION = re.compile(
    r"^(?:export\s+)?(?:async\s+)?function\s*\*?\s*([A-Za-z_$][\w$]*)\s*\(", re.M)
RE_METHODE = re.compile(r"^  (?:async\s+)?([A-Za-z_$][\w$]*)\s*\([^;{]*\)\s*\{\s*$", re.M)
RE_FLECHE = re.compile(
    r"^(?:export\s+)?const\s+([A-Za-z_$][\w$]*)\s*=\s*(?:async\s*)?(?:\([^)]*\)|[A-Za-z_$][\w$]*)\s*=>", re.M)
RE_APPEL = re.compile(r"(?<![\w$.])([A-Za-z_$][\w$]*)\s*\(")
RE_APPEL_METHODE = re.compile(r"\b(?:sim|this)\.([A-Za-z_$][\w$]*)\s*\(")
RE_ECRITURE = re.compile(
    rf"\b({OBJETS})\.([A-Za-z_$][\w$]*)(?:\.[\w$]+|\[[^\]]+\])*\s*(?:=(?!=)|\+=|-=|\*=|/=|\+\+|--)")
RE_ECRITURE_CLE = re.compile(rf"\b({OBJETS})\[([A-Za-z_$][\w$]*)\]\s*=(?!=)")
RE_TIRAGE = re.compile(r"\bsim\.rng\s*\(|\bthis\.rng\s*\(")
RE_PASSE_RNG = re.compile(r"[,(]\s*(?:sim|this)\.rng\s*[,)]")
RE_DT = re.compile(r"(?<![\w$.])([A-Za-z_$][\w$.]*)\s*\(([^()]*\bdt\b[^()]*)\)")


def _sans_commentaires(texte: str) -> str:
    texte = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), texte, flags=re.S)
    return re.sub(r"(?<![:\"'`\\])//[^\n]*", "", texte)


def _fin_parametres(texte: str, ouvrante: int) -> int:
    """Position juste apres la `)` qui ferme la `(` en `ouvrante` (`options = {}` ne doit pas passer pour le corps)."""
    profondeur = 0
    for j in range(ouvrante, len(texte)):
        if texte[j] == "(":
            profondeur += 1
        elif texte[j] == ")":
            profondeur -= 1
            if profondeur == 0:
                return j + 1
    return ouvrante


def _corps(texte: str, debut: int) -> tuple[str, int]:
    """Corps entre accolades a partir de la premiere `{` apres `debut` (chaines ignorees, grossierement)."""
    if texte[debut:debut + 1] == "(":
        debut = _fin_parametres(texte, debut)
    i = texte.find("{", debut)
    if i < 0:
        return "", debut
    profondeur, j, chaine = 0, i, None
    while j < len(texte):
        c = texte[j]
        if chaine:
            if c == "\\":
                j += 2
                continue
            if c == chaine:
                chaine = None
        elif c in "\"'`":
            chaine = c
        elif c == "{":
            profondeur += 1
        elif c == "}":
            profondeur -= 1
            if profondeur == 0:
                return texte[i:j + 1], j + 1
        j += 1
    return texte[i:], len(texte)


class Index:
    """Toutes les fonctions nommees de `src/` : nom -> [(fichier, ligne, corps)]."""

    def __init__(self, ref: Path):
        self.ref = ref
        self.fonctions: dict[str, list[tuple[str, int, str]]] = defaultdict(list)
        self.textes: dict[str, str] = {}
        for chemin in sorted((ref / "src").rglob("*.js")):
            rel = chemin.relative_to(ref).as_posix()
            texte = _sans_commentaires(chemin.read_text(encoding="utf-8", errors="replace"))
            self.textes[rel] = texte
            for motif in (RE_FONCTION, RE_METHODE, RE_FLECHE):
                for m in motif.finditer(texte):
                    nom = m.group(1)
                    if nom in MOTS_CLES:
                        continue
                    if motif is RE_FLECHE:
                        corps, _ = _corps(texte, m.end())
                    else:
                        corps, _ = _corps(texte, texte.rfind("(", m.start(), m.end()))
                    ligne = texte.count("\n", 0, m.start()) + 1
                    self.fonctions[nom].append((rel, ligne, corps))

    def appels(self, corps: str) -> set[str]:
        noms = {m.group(1) for m in RE_APPEL.finditer(corps)} | {m.group(1) for m in RE_APPEL_METHODE.finditer(corps)}
        return {n for n in noms if n in self.fonctions and n not in MOTS_CLES}

    def portee(self, entrees: list[str], profondeur: int) -> dict[str, int]:
        vus = {e: 0 for e in entrees if e in self.fonctions}
        front = list(vus)
        for d in range(1, profondeur + 1):
            suivant = []
            for nom in front:
                for _, _, corps in self.fonctions[nom]:
                    for appele in self.appels(corps):
                        if appele not in vus:
                            vus[appele] = d
                            suivant.append(appele)
            front = suivant
        return vus

    def lecteurs(self, champ: str) -> list[str]:
        lit = re.compile(rf"\.{re.escape(champ)}\b(?!\s*(?:=(?!=)|\+=|-=|\+\+|--))|\[[\"']{re.escape(champ)}[\"']\]")
        sites = []
        for nom, defs in self.fonctions.items():
            for fichier, ligne, corps in defs:
                if lit.search(corps):
                    sites.append(f"{fichier}:{ligne} {nom}")
        return sorted(set(sites))


def cone(ref: Path, entrees: list[str], profondeur: int = 2, champs: list[str] | None = None) -> dict:
    idx = Index(ref)
    absentes = [e for e in entrees if e not in idx.fonctions]
    portee = idx.portee(entrees, profondeur)
    ecritures: dict[str, set[str]] = defaultdict(set)
    tirages: list[str] = []
    passages: list[str] = []
    dt: dict[str, set[str]] = defaultdict(set)
    for nom, d in sorted(portee.items(), key=lambda kv: (kv[1], kv[0])):
        for fichier, ligne, corps in idx.fonctions[nom]:
            site = f"{fichier}:{ligne} {nom} (prof. {d})"
            for m in RE_ECRITURE.finditer(corps):
                ecritures[f"{m.group(1)}.{m.group(2)}"].add(site)
            for m in RE_ECRITURE_CLE.finditer(corps):
                ecritures[f"{m.group(1)}[{m.group(2)}]"].add(site)
            n = len(RE_TIRAGE.findall(corps))
            if n:
                tirages.append(f"{site} : {n} tirage(s) sim.rng()")
            if RE_PASSE_RNG.search(corps):
                passages.append(site)
            if d <= 1:
                for m in RE_DT.finditer(corps):
                    if m.group(1).split(".")[-1] not in MOTS_CLES:
                        dt[nom].add(m.group(1))
    champs_npc = sorted({k.split(".", 1)[1] for k in ecritures if "." in k and not k.startswith(("sim.", "this."))})
    return {
        "reference": str(ref),
        "entrees": entrees,
        "entreesAbsentes": absentes,
        "profondeur": profondeur,
        "portee": {"fonctions": len(portee), "parProfondeur": {
            str(d): sorted(n for n, p in portee.items() if p == d) for d in range(profondeur + 1)}},
        "ecritures": {k: sorted(v) for k, v in sorted(ecritures.items())},
        "champsHabitantEcrits": champs_npc,
        "tirages": tirages,
        "passagesDeSimRng": passages,
        "recoiventDt": {k: sorted(v) for k, v in sorted(dt.items())},
        "lecteurs": {c: idx.lecteurs(c) for c in (champs or [])},
    }


def rapport_markdown(c: dict, max_lignes: int = 40) -> str:
    l = [f"Entrees : {', '.join(c['entrees'])} ; profondeur {c['profondeur']} ; "
         f"{c['portee']['fonctions']} fonctions atteintes."]
    if c["entreesAbsentes"]:
        l.append(f"ABSENTES de l'index : {', '.join(c['entreesAbsentes'])}")
    for d, noms in c["portee"]["parProfondeur"].items():
        l.append(f"- profondeur {d} ({len(noms)}) : {', '.join(noms[:max_lignes])}{' ...' if len(noms) > max_lignes else ''}")
    l.append(f"\nTirages `sim.rng()` dans la portee : {len(c['tirages'])} site(s)")
    l += [f"- {t}" for t in c["tirages"][:max_lignes]]
    l.append(f"\n`sim.rng` passe en argument : {len(c['passagesDeSimRng'])} site(s)")
    l += [f"- {t}" for t in c["passagesDeSimRng"][:max_lignes]]
    l.append(f"\nChamps d'habitant ecrits ({len(c['champsHabitantEcrits'])}) : {', '.join(c['champsHabitantEcrits'])}")
    if c["recoiventDt"]:
        l.append("\nAppels qui recoivent `dt` (profondeur <= 1) :")
        l += [f"- {k} -> {', '.join(v)}" for k, v in c["recoiventDt"].items()]
    for champ, sites in c["lecteurs"].items():
        l.append(f"\nLecteurs de `{champ}` dans tout src/ : {len(sites)}")
        l += [f"- {s}" for s in sites[:max_lignes]]
    return "\n".join(l)


if __name__ == "__main__":
    import argparse

    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--ref", required=True)
    p.add_argument("--entree", action="append", required=True)
    p.add_argument("--profondeur", type=int, default=2)
    p.add_argument("--champ", action="append", default=[])
    p.add_argument("--json", action="store_true")
    a = p.parse_args()
    res = cone(Path(a.ref), a.entree, a.profondeur, a.champ)
    print(json.dumps(res, indent=2, ensure_ascii=False) if a.json else rapport_markdown(res))
