# Brancher un asset visuel — procédure agent assets

Public : l'agent qui intègre les vrais meshes/matériaux ANÁSTASIS.
Tu n'as pas besoin de lire le C++, ni d'ouvrir `Source/AnastasisSim/`.

## Ce que tu édites

Un seul asset :

```
/Game/Anastasis/Presentation/DA_AnastasisPresentation
```

C'est un Data Asset (`UAnastasisPresentationRegistry`). Il contient une liste
`Entries`, une par type sémantique du monde. Aujourd'hui : `Forest` et `Ruin`.

## Procédure

1. Ouvrir l'éditeur sur le projet canonique.
2. Content Browser → `Content/Anastasis/Presentation/` → double-cliquer
   `DA_AnastasisPresentation`.
3. Déplier `Entries`, choisir l'entrée dont `Semantic Type` vaut `Forest`.
4. Déplier `Variants`. Dans `Variants[0]`, assigner ton mesh dans `Mesh`.
   - Pour plusieurs arbres : `+` sur `Variants`, assigner un mesh par variante.
     La variante est choisie **de façon déterministe** par tuile — même monde,
     même seed ⇒ même arbre au même endroit, à chaque lancement.
   - `Material Override` est facultatif. Laissé vide, le `Tint` de l'entrée est
     appliqué sur le matériau placeholder partagé. Renseigné, il remplace tout.
5. Ajuster si besoin : `Min/Max Uniform Scale` (enveloppe d'échelle),
   `Jitter Radius Fraction` (dispersion XY, fraction d'une tuile),
   `Random Yaw`, `Tint`.
6. Sauvegarder l'asset.
7. Lancer (PIE ou `tools\unreal\probe-demo.ps1`).

Aucune modification de `AnastasisSim`. Aucune modification du resolver C++.
Aucune recompilation.

## Vérifier que ça a pris

Dans le log :

```
ANASTASIS_PRESENTATION_REGISTRY source=asset path=... entries=2
ANASTASIS_PRESENTATION dressing_instances=N tree_tiles=... ruin_tiles=... source=asset components=...
```

`source=asset` ⇒ tes données sont utilisées.
`source=code_defaults` ⇒ l'asset est introuvable ou vide, le moteur est retombé
sur les placeholders codés en dur : corrige l'asset, ne touche pas au C++.

## Échelles — l'unité qui compte

1 tuile = 100 uu (≈ 1 m). Les scales sont des facteurs appliqués au mesh brut.
Les placeholders actuels sont les primitives moteur (~100 uu), d'où
`Min/Max Uniform Scale` autour de 1.6–2.4 pour un arbre. **Un vrai mesh d'arbre
déjà à l'échelle réelle veut une enveloppe proche de 1.0** — sinon tu obtiens
des arbres de 30 m. Ajuste l'enveloppe en même temps que le mesh.

## Ce que tu ne peux pas faire ici (et pourquoi)

- **Ajouter un type sémantique** (`House`, `Market`, `Road`…) : la liste vient de
  `AnastasisWorld::ETileType`. Un type qui n'existe pas dans la simulation ne
  peut pas être représenté. Ce n'est pas une limite de ce fichier, c'est une
  limite de vérité. Voir `docs/unreal/VISUAL_PIPELINE_AUDIT.md`.
- **Changer où les instances apparaissent** : la présence est décidée par la
  simulation (le type de la tuile), jamais par ces données. Tu choisis à quoi ça
  ressemble, pas où ça pousse.
- **Désactiver proprement une catégorie** : décoche `Enabled` sur l'entrée. C'est
  un interrupteur explicite, distinct d'une donnée manquante.

## Si l'asset est absent

Le système ne casse pas : il logge `source=code_defaults` et rend les
placeholders. Un asset manquant n'a jamais d'effet sur la simulation.

Pour le recréer depuis zéro :

```powershell
ANASTASIS_PRESENTATION_REBUILD=1  # écrase les retouches manuelles
```
avec `tools/unreal/presentation-registry.py`, qui est la source d'autorité de
l'asset.
