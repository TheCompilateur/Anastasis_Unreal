# REED_FORM_004 — forme et matériau des roseaux

## Périmètre

Six tiges par touffe, mêmes placements pour toutes les variantes. La nouvelle
forme ajoute des courbures, 24 feuilles longues à pli central et des épis ramifiés.
Hauteur calculée 179,93 cm ; largeur maximale des feuilles 112,61 cm par touffe.
Les pieds restent dans un rayon inférieur à 11 cm. Ni simulation ni terrain modifiés.

Matériau dédié : copie de `M_AnastasisVegetation`, dont seule l'entrée World Position
Offset est remplacée par une constante nulle. Les shaders/couleurs de feuillage,
la rugosité et la transmission sont conservés. Le matériau original n'est pas édité.
Le candidat est statique : une animation de vent correcte reste hors de cette passe.

## Comparaison

- `bare` : site sans roseaux de l'étude.
- `A` : ancien mesh et matériau original.
- `B` : ancien mesh, matériau dédié avec WPO nul.
- `C` : nouvelle géométrie/couleurs de sommets, même matériau que B.

A→B isole la suppression du déplacement matériel. B→C évalue ensemble la forme
et sa palette de sommets. Ne pas attribuer toute la différence à la courbure seule.
Nombre de tiges, racines, rotations, échelles unitaires, caméra et éclairage communs.
Le vent d'A n'est pas calé sur un instant identique ; aucune preuve de stabilité en
mouvement ne découle de ces images fixes.

Critère : conserver un candidat dont les feuilles et les silhouettes se lisent mieux
sans multiplier les tiges ; refuser une victoire artistique globale si le site, les
matières ou les défauts fins restent loin de la référence.

Référence examinée : `docs/visual/reference/pontique-etat-zero-5-lisieres-transitions.png`.

## Sources et génération

- `tools/unreal/create-reed-form.py` : recette de géométrie pure puis adaptation Unreal.
- `tools/unreal/capture-reed-form.py` : création si absente, relecture et comparaison.
- `tools/unreal/capture-reed-form.ps1` : éditeur dédié, huit fichiers et hashes attendus.
- Ancien mesh : `Content/Anastasis/Ecotone/SM_Ecotone_Reed_01.uasset`, identique à fb68398.
- Candidat local : `Content/Anastasis/EcotoneReview004/SM_Reed_Curved_01.uasset`.
- Matériau local : `Content/Anastasis/EcotoneReview004/M_Reed_Static_01.uasset`.

Les deux nouveaux fichiers générés restent locaux et exclus du commit, conformément
à la règle de dépôt sur les artefacts générés. La recette est versionnée et refuse
l'écrasement d'un mesh existant. Aucun asset original supprimé/remplacé, aucune carte
sauvegardée, aucun branchement automatique au dressing.

La géométrie produit 2172 sommets et 3360 triangles par touffe. Les assertions de
la recette vérifient les aires non nulles, les normales normalisées et le pied près
de Z=0. Vérification exécutée en Python puis construction réelle dans Unreal.
Cela ne mesure ni le coût GPU ni les collisions en gameplay.

## Bases et preuves

Première passe sur l'étape 3 à 1 m : `85e0627`, recette conservée par `a9b6426` sur
`agent/reed-form-004`. Huit images dans `reed-form-004b`; une tentative préalable
`004a` s'arrête sur `__file__` absent du lancement Python par console, corrigé par
un chemin relatif au projet Unreal.

Pendant ce travail, Claude a livré `82a337b` (TileWorldSize=400), worktree propre au
moment de la lecture. L'étude a repris cette base dans **le même worktree isolé**,
branche `agent/reed-form-004-4m`, et seulement les fichiers propres de cette étude.
Aucune intégration canonique, aucun merge de branches d'agents.

Compilation de la base 4 m : Build.bat Editor Win64 Development, UE 5.8.2 CL 56702186,
code de sortie 0, 34,11 secondes. L'opérateur du dépôt refuse le chemin du worktree
géré par Codex ; sa commande de build a été utilisée directement. Aucune modification
C++ par cette étude et aucune nouvelle suite C++ revendiquée.

Le script vérifie l'emprise rendue de 38 000 cm entre centres extrêmes (96 cases de
4 m), recharge le mesh et sa liaison matériau, et inspecte l'entrée WPO réellement
sauvegardée. Les caméras appliquées sont relues. Le filtre d'ancrage contrôle le centre
et huit points d'un cercle de 16 cm (écart admis 8 cm) ; il ne vérifie pas tous les
sommets des feuilles. Ombres/occlusion par les autres objets et effets temporels restent
à juger dans les images. PLAYER NOT_IMPLEMENTED.

## Verdict observé sur la base 4 m

**KEEP comme candidat de forme ; SCN PARTIAL pour la rive complète.**
C est plus lisible comme végétation : feuilles courbes, rupture des verticales et
épis ramifiés, à nombre égal de tiges. B seul ne supprime pas la silhouette de
piquet ; la coupure WPO rend certains contours proches moins fragmentés, mais
les images ne prouvent pas une correction générale des artefacts temporels.
Le candidat reste stylisé, les détails/ombres fins restent granuleux, et les
placements sont encore espacés sur une grille de 80 cm. Le sol, les découpes de
l'eau et l'horizon noir restent très loin de la référence. Aucun verdict AAA.

Preuve finale :
`C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eab3-eaab-7610-8f6d-bb6dd660a518/reed-form-004-4m-b/`

- 8 PNG 1600×900 : `bare/A/B/C`, vues `eye/context` ; marqueur COMPLETE shots=8.
- `manifest.json` : placements, caméras, ref terrain, hash de recette et conditions.
- `capture-hashes.json` : SHA256 des huit images après fermeture du processus dédié.
- `CANDIDATE_RELOAD_PASS` : hauteur 179,933 cm et liaison au matériau dédié.
- `MATERIAL_RELOAD_PASS wpo=constant_zero` : entrée WPO inspectée après relecture.
- 963 racines candidates ; 21 touffes retenues, soit 126 tiges principales.
- Support centre + cercle : variation maximale 1,541 cm (seuil 8 cm).
- Caméra œil à 165 cm du sol, rayon caméra/cible dégagé de 69,984 cm au minimum
  sur les échantillons testés. Point visé XY=(5658,46 ; 2630,77).
- Position et rotation réellement appliquées aux 21 acteurs vérifiées par assertions.
- Géométrie candidate : 21 × 3360 = 70 560 triangles nominaux, sans mesure GPU.

La première passe 4 m, `reed-form-004-4m-a`, avait déjà produit huit images ; `b`
ajoute les vérifications de transforms et coupe des indicateurs d'éditeur. Un
petit repère orange reste visible dans le ciel malgré ces réglages : les images
restent des captures de diagnostic, pas des images de présentation finale.
Les coordonnées que le log EditorActorSubsystem dit « Attempting to add actor »
ne sont pas utilisées comme preuve : les transforms finales sont relues.

Identité des trois assets dans `reed-form-004-asset-hashes.json`, à la racine des
sorties de cette conversation. Les deux nouveaux assets sont conservés localement,
non commités ; recette, scripts et rapport constituent la passation versionnée.

[MEC] build observé + génération + relecture réelles + capture ; [SCN] gain local
observé, ensemble PARTIAL ; [PLY] UNKNOWN, pas de PIE. Aucun merge/push.

Au dernier contrôle, Claude avait déjà repris des modifications non commitées de
la forge et de ses captures. Cette preuve reste verrouillée sur `82a337b` et ne
prétend pas couvrir ces travaux ultérieurs.

## Next / stop

Cette passe de forme est terminée. Prochain test borné : supprimer la régularité
des placements sur une petite rive du terrain livré, puis juger le contact visuel
sol/eau/végétation. Ne pas réactiver le matériau de vent des arbres sur les roseaux
sans une animation ancrée au pied et une preuve en mouvement. Ne pas intégrer
cette étude comme système de dressing performant : elle emploie des acteurs
statiques temporaires pour la comparaison.
