# Sol : matériau et textures

## Unreal

- **PBR** : la couleur de base (albédo) est la part de lumière que la surface renvoie, sans ombre ni
  reflet. C'est une valeur physique. Ordres de grandeur réels : neige fraîche 0,8–0,9 ; sable sec
  0,3–0,4 ; herbe 0,10–0,25 ; terre sombre 0,05–0,15 ; asphalte 0,05–0,10. Le charbon est à environ 0,04 :
  rien de naturel ne descend sous 0,02 ou ne monte au-dessus de 0,9.
- La **rugosité** dit si le reflet est net (0) ou diffus (1). Un sol naturel sec est à 0,8–1,0 ; mouillé,
  il descend fortement. Le métal (`Metallic`) vaut 0 pour tout sol naturel.
- **Landscape Material** : matériau multi-couches peint couche par couche sur un Landscape.
- **Runtime Virtual Texture (RVT)** : une texture géante calculée à la volée, page par page, où l'on
  « cuit » le mélange des couches et des décalques. Elle économise le coût des matériaux superposés sur de
  grandes surfaces. Elle a un coût mémoire et une latence de mise à jour (`r.VT.MaxUploadsPerFrame`), et
  le format YCoCg améliore la couleur au prix de la mémoire.
- **Répétition** : une texture qui se répète se lit en damier à moyenne distance. Les remèdes sont le
  mélange multi-échelles (macro / méso / micro), le passe-haut sur la photo, la projection triplanaire et
  la variation pilotée par la morphologie (pente, humidité, courbure).

## ANÁSTASIS aujourd'hui

| Quoi | Valeur / mécanisme | Où |
|---|---|---|
| Matériau | `MI_AnastasisGround` (instance de `M_AnastasisGround`), repli `M_AnastasisSlice` | script d'autorité `tools/unreal/ground-material.ps1` + `.py` |
| Sémantique | couleur de sommet = teinte de la simulation (type, rive, humidité) ; UV0 = (roche, litière), UV1 = (travaillé, humidité) | `GROUND_SURFACE_001.md` |
| Albédos | recalés sur le réel : herbe humide 0,10–0,18, litière 0,05–0,10, roche mouillée 0,12–0,20 | idem |
| Textures | 4 photos CC0 Poly Haven 2K (`sparse_grass`, `forest_leaves_02`, `brown_mud_02`, `mossy_rock`), empaquetées par `tools/unreal/ground-textures.py` (Python système) | `GROUND_TEXTURE_001.md` |
| Projection | triplanaire dans un nœud Custom HLSL, normale en espace monde (le maillage n'a pas de tangentes) | idem |
| Variation | bruits à ~600 m, ~60 m, ~11 m, ~70 cm | `ground-material.py`, `handoffs/forest-terrain-p4.md` |
| Coût | 924 instructions pixel, 10 samplers ; GPU dans le bruit (prairie basse 11,0 → 10,9 ms) | `GROUND_TEXTURE_001.md` |
| Absents | RVT, tessellation, displacement, Landscape Material | `AAA_VISUAL_TARGET_LAB.md` : RVT sur le terrain = `DANGEROUS_TO_CHANGE` |

## Règles

- **SOL-01** — Un albédo de sol reste dans la plage réelle de sa matière (tableau ci-dessus). À EV100 14
  sous 75 000 lux, un albédo de 0,5 sort blanc : c'est la cause du « monde de plâtre » corrigé par
  `GROUND_SURFACE_001`.
- **SOL-02** — La photo **module**, elle ne colore pas : albédo divisé par sa moyenne locale, facteur
  neutre en moyenne. La couleur reste celle de la simulation.
- **SOL-03** — Toute photo de sol passe le passe-haut à 15 cm à l'empaquetage : sans lui, le damier
  apparaît à 35 m (interdit par la direction artistique).
- **SOL-04** — Le matériau se change dans `ground-material.py` puis se régénère ; le script refuse un
  matériau qui ne compile pas. Une erreur de `recompile_material` ignorée remplace le sol par le
  matériau par défaut.
- **SOL-05** — Une valeur qui dépend de la pente se recale après chaque changement de forge
  (`fiches/terrain.md`). Aujourd'hui `SlopeRockStart`/`End` (0,62 / 0,82) ne sont plus atteints après
  l'érosion : la roche de pente n'apparaît que sur les berges.

## Vérifier

```powershell
tools\unreal\capture-ground-cover.ps1 -States on,on_notex,bare,bare_notex   # A/B textures photo
```

Puis `compare.py` (skill `anastasis-capture`). Au loin, les mips doivent converger vers le sol sans
texture : un écart lointain entre `on` et `on_notex` est un défaut.

## Ne pas faire

- Poser une texture Megascans ou Poly Haven brute en couleur de base : elle efface les albédos calés et
  la sémantique de la simulation.
- Activer le RVT, Nanite ou le displacement sur le terrain procédural sans mandat
  (`DANGEROUS_TO_CHANGE`).
- Construire le mélange en ~300 nœuds au lieu du nœud Custom.
- Faire `ComponentMask` sur l'alpha d'un `VertexColor` : ne compile pas sous UE 5.8
  (`GROUND_SURFACE_001.md`).

## Ouvert

- Recalibrer les seuils de roche de pente (SOL-05) : à trancher.
- Le RVT ne servirait qu'avec des décalques en masse (pistes, ornières). Pas de besoin mesuré.

## Sol soil-crusade-001 (2026-10-02, KEEP local / artistique PARTIAL)

`ground-material.py` ajoute `SoilHistory` (0 = comportement precedent, 1 = sol).
Le master garde `SoilPilotCenter=(96000,110000,0)` cm et `SoilPilotRadius=38000` cm,
transition sur les 20 % externes. L'instance livree porte le rayon a 10000000 cm
pour couvrir le monde; remettre 38000 pour le pilote initial (graine12345, echelle5).
La pente rendue (1-Nz, 0.10..0.40 pour la roche), UV1.y (proximite humide),
UV0.y (famille litiere) et le champ meso existant gouvernent la matiere.
Matrice minerale/organique proche, attenuee de 2.5 a16m, sous les brins separes.
Les fines sont une signature plausible sur replats humides, pas une reconstruction
sedimentaire. UV0.y suit les tuiles Forest, pas chaque couronne placee.
Aucune geometrie, texture, collision ou logique de simulation ajoutee.
Generation Unreal : 1066 instructions pixel, 10 samplers. Pilote V2 : 18 images,
sortie propre, GPU apres11.379..15.973ms; deltas dans la derive du temoin.
`tools/soil-crusade/capture.ps1 -Label pilot-v2 -Rebuild` compare SoilHistory0/1/0;
`-Label deployed -Deploy` sauvegarde la couverture et controle deux vues hors pilote.
Limites : grain proche parfois trop regulier; pas de preuve PLY ni d'histoire alluviale.
Details et preuves dans `docs/unreal/handoffs/soil-crusade-001.md`.

## Pentes soil-slope-002 (2026-10-02, KEEP borne)

`SlopeSurface=1` ajoute la famille Rock existante a9m, entre10 et350m
(pleine22..180m), selon pente20..41deg et attenuation litiere/humidite.
`SlopeSurface=0` rend le materiau precedent; SoilHistory et le proche restent.
Aucune texture/geometrie ajoutee. 1175 instructions pixel,10samplers.
A/B0/1/0 propre,12images observees : paroi hors_vallee amelioree, prairie/oblique
preservees; pente_face non discriminante. GPUapres13.406..15.489ms, deltas dans
la derive du temoin. Le seuil global16/255 est insuffisant ici : voir les rectangles
explicites de matiere et limites dans `docs/unreal/handoffs/soil-slope-002.md`.
Cette couche ne corrige ni les contours abrupts ni les cassures du maillage.
