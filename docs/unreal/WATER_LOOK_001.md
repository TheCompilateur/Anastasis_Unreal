# WATER_LOOK_001 — l'eau se lit comme de l'eau

Suite de HYDRO_NETWORK_001 : le réseau était juste, mais vue d'en haut l'eau se lisait
« comme de la peinture, pas encore remplie d'eau » (Alexandre, 2026-09-30). Cette mission
traite l'aspect, sans toucher la structure du réseau : mêmes rivières, mêmes lacs, mêmes
contrôles.

Bascule : `anastasis.Terrain.WaterLook` (1 par défaut, 0 = l'eau d'avant), appliquée à
l'incarnation.

## Diagnostic : quatre causes

| Constat | Cause |
|---|---|
| aplat bleu uniforme | `M_AnastasisShoreWater` est translucide à 0.88–0.96, une couleur, sans reflet ni mouvement |
| rive en escalier | la nappe est la grille de 5 m ; le lit s'arrêtait 10 cm sous l'eau et la berge repartait 12 cm au-dessus **dans la même maille** : la ligne d'eau suivait les sommets |
| rivière posée sur l'herbe | revanche de 60 cm en plaine et talus de 25 m : la berge ne se lit pas |
| halo cyan autour de l'eau | la couleur de sommet garde le bleu des tuiles d'eau de la simulation, même là où le réseau ne met plus d'eau |

## Ce qui change

> **Erratum, WATER_VOLUME_001 (2026-10-10).** L'absorption et la diffusion décrites au point 1 n'étaient pas
> calculées : le pin Opacity n'était pas branché (1), le moteur sautait tout le volume. Elles ne le sont
> que depuis `docs/unreal/handoffs/water-opacity-001.md`, avec d'autres valeurs (palette « trouble ») et en
> centimètres.

1. **`M_AnastasisWater`** — modèle d'ombrage *Single Layer Water* d'Unreal sur le maillage
   procédural, sans le plugin Water (choix du 2026-09-30 : pas de plugin).
   - absorption (0.55, 0.16, 0.11 /m) et diffusion (0.015, 0.040, 0.048 /m) selon la
     profondeur réellement vue : clair au bord, sombre au large, sans canal de sommet ;
   - reflets du ciel, rugosité 0.035 ;
   - normales animées en HLSL, sans texture : cinq vagues directionnelles (0.8 à 5.5 m),
     advectées par le courant en flowmap à deux phases (UV2 = sens × vitesse).
   - Source d'autorité : `tools/unreal/water-look.ps1` (+ `.py`). `-Rebuild` vide le graphe et le
     recâble **en place** : quand une carte tient le matériau, `delete_asset` échoue sans bruit. Le
     lanceur ouvre `/Engine/Maps/Entry` et refuse un échec Python ou de compilation.
2. **Rubans d'eau** (`AnastasisDrainage::BuildRiverRibbons`, section 2) : une bande lisse par
   rivière, au niveau de l'eau, qui déborde de 3,5 m sous les berges. Un affluent est posé
   1,5 cm sous son récepteur, une rivière 2 cm sous un lac : à une confluence, c'est le
   récepteur qui couvre. La grille ne dessine plus que lacs, mers et mares
   (`FNetwork::LakeWaterTriangles`) ; elle garde les hauteurs d'eau des rivières, que lisent
   forêt, lieux et couvert.
3. **Berges marquées** : revanche de 1 m en plaine (au lieu de 60 cm), talus de 1,5 × la largeur
   (au lieu de 3 ×), et surtout un **talus droit qui traverse la ligne d'eau** (pente 0,15), du
   lit jusqu'à la levée. La rive visible est l'intersection d'un plan d'eau et d'une pente
   régulière : elle est interpolée, plus quantifiée à la maille.
4. **Couleurs du fond et de la rive** : fond immergé en gravier (bord) puis vase (large) ; toute
   terre sèche qui porte encore du bleu de tuile (B > R + 0,04 : aucune teinte de terre n'a plus
   de bleu que de rouge) reprend la couleur de la terre sèche la plus proche, puis est lissée.
   La relaxation seule (120 itérations) ne traversait pas un ancien bras de 30 mailles : le test
   en trouvait 2 800 sommets encore bleus, ce sont les liserés cyan qui bordaient les lacs.
   L'humidité du sol (UV1.y), qui abaisse la rugosité dans `M_AnastasisGround`, est plafonnée à
   40 m d'une eau réellement rendue : le lustre humide ne vaut plus que près de l'eau.

## Mesures (seed 12345, Human_Geography_V2, 4 caméras de la vitrine)

| | WaterLook 0 | WaterLook 1 |
|---|---|---|
| contrôles du réseau (montée, rétrécissement, confluence, embouchure, isolé, lac sans rôle) | tous à 0 | tous à 0 |
| `bank_containment` | 0.947 | **0.980** |
| rubans / triangles (aucun retourné) | — | 19 / 2 654 |
| sommets secs peints en bleu d'eau | non mesuré | **0** |
| triangles de nappe (grille) | 32 880 | 21 868 (lacs, mers, mares) |

Avant / après, mêmes caméras, même build : `docs/visual/water-look-001/`.

Test : `Anastasis.Terrain.Drainage.WaterLook` — réseau cohérent avec berges marquées, nappe sans
rivières, un ruban par rivière tourné vers le haut comme la grille, flowmap normalisée, aucune
terre sèche ni aucun fond peint en bleu de tuile.

## Résidus connus, hors périmètre

- **Halos clairs bleutés sur le sol sec** (vue 2, p. ex. autour de (78 000, 77 000), à 23 m
  d'altitude et loin de toute eau) : présents à l'identique avec `WaterLook 0`, et intacts après
  les corrections ci-dessus. Ni eau (la grille n'en met pas là), ni couleur de sommet bleue (le
  test en compte 0), ni humidité (plafonnée à 0 à cet endroit) : un effet du matériau de sol ou
  de l'éclairage, à traiter dans une mission sol.
- **Bancs clairs aux confluences** : une pointe de berge émerge entre deux lits qui se
  rejoignent (vue 3). Lisible comme un banc de confluence ; non corrigé.
- Anneau d'horizon : signalé par HYDRO_NETWORK_001, inchangé ici.
- Écume, roseaux, cascades : non commencés (étapes suivantes possibles du plan proposé).
