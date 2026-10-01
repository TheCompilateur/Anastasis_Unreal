"""Statistique appariee du laboratoire — bibliotheque standard seulement.

Une experience donne, pour chaque grandeur, N differences appariees
d_k = valeur(bras B, replique k) - valeur(bras A, replique k) : meme etat de
depart, meme suite de tirages au depart, un seul changement. On en tire :

  ic95        intervalle de confiance a 95 % de la moyenne des d_k, bootstrap
              percentile (B tirages, graine fixe : le chiffre se rejoue)
  p_signe     test de permutation par inversion de signe (H0 : d symetrique
              autour de 0), bilateral, Monte-Carlo a graine fixe
  tost        equivalence (deux tests unilateraux a 5 %) : l'IC a 90 % de la
              moyenne est-il dans [-delta, +delta] ? delta est DECLARE avant la
              mesure (tolerances.json), jamais derive des donnees du bras B
  effet_sd    moyenne des d_k en ecarts-types naturels : ecart-type, entre
              repliques, de la grandeur dans le bras A

Pas de loi de Student : la bibliotheque standard ne l'a pas, et le bootstrap
n'en a pas besoin. Avec N >= 20 l'ecart entre les deux est sans consequence ici ;
en dessous, le rapport le signale.
"""

from __future__ import annotations

import math
import random
import statistics

B_BOOTSTRAP = 4000
B_PERMUTATION = 4000
GRAINE_STATS = 20261001


def moyenne(xs: list[float]) -> float:
    return statistics.fmean(xs) if xs else float("nan")


def ecart_type(xs: list[float]) -> float:
    return statistics.stdev(xs) if len(xs) > 1 else 0.0


def _quantile(trie: list[float], q: float) -> float:
    if not trie:
        return float("nan")
    pos = q * (len(trie) - 1)
    i = int(math.floor(pos))
    j = min(i + 1, len(trie) - 1)
    return trie[i] + (trie[j] - trie[i]) * (pos - i)


def bootstrap_moyenne(d: list[float], niveaux=(0.95, 0.90), b: int = B_BOOTSTRAP, graine: int = GRAINE_STATS) -> dict:
    """IC percentile de la moyenne, aux niveaux demandes. Rend {niveau: (bas, haut)}."""
    if len(d) < 2:
        return {n: (float("nan"), float("nan")) for n in niveaux}
    rng = random.Random(graine)
    n = len(d)
    moyennes = sorted(statistics.fmean(rng.choices(d, k=n)) for _ in range(b))
    return {niv: (_quantile(moyennes, (1 - niv) / 2), _quantile(moyennes, 1 - (1 - niv) / 2)) for niv in niveaux}


def p_inversion_signe(d: list[float], b: int = B_PERMUTATION, graine: int = GRAINE_STATS) -> float:
    """p bilateral du test de permutation par inversion de signe sur la moyenne."""
    d = [x for x in d if x == x]
    if not d or all(x == 0 for x in d):
        return 1.0
    rng = random.Random(graine)
    obs = abs(statistics.fmean(d))
    extremes = 0
    for _ in range(b):
        s = statistics.fmean([x if rng.random() < 0.5 else -x for x in d])
        if abs(s) >= obs - 1e-15:
            extremes += 1
    return (extremes + 1) / (b + 1)


def juger(a: list[float], b: list[float], delta: float | None) -> dict:
    """Jugement d'une grandeur : a et b alignes par replique (None = absent, paire ecartee)."""
    paires = [(x, y) for x, y in zip(a, b) if x is not None and y is not None
              and not (isinstance(x, float) and math.isnan(x)) and not (isinstance(y, float) and math.isnan(y))]
    d = [y - x for x, y in paires]
    av = [x for x, _ in paires]
    bv = [y for _, y in paires]
    n = len(d)
    ic = bootstrap_moyenne(d)
    sd_naturel = ecart_type(av)
    m = moyenne(d)
    res = {
        "n": n,
        "moyenneA": moyenne(av),
        "moyenneB": moyenne(bv),
        "sdA": sd_naturel,
        "diff": m,
        "ic95": ic[0.95],
        "ic90": ic[0.90],
        "p_signe": p_inversion_signe(d),
        "effet_sd": (m / sd_naturel) if sd_naturel > 0 else (0.0 if m == 0 else float("inf")),
        "identiques": sum(1 for x in d if x == 0),
        "delta": delta,
    }
    exclut_zero = not (res["ic95"][0] <= 0 <= res["ic95"][1])
    if all(x == 0 for x in d):
        exclut_zero = False
    equivalent = delta is not None and -delta <= res["ic90"][0] and res["ic90"][1] <= delta
    if equivalent:
        verdict = "EQUIVALENT"
    elif exclut_zero:
        verdict = "DIFFERENT"
    else:
        verdict = "INDETERMINE"
    res["exclutZero"] = exclut_zero
    res["equivalent"] = equivalent
    res["verdict"] = verdict
    return res
