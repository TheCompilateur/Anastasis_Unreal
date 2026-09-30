# ANASTASIS Map Intelligence

Outil permanent Python **Editor-only**, lecture seule du terrain. Menu :
**Tools > ANASTASIS Map Intelligence**. Aucun module C++ ni plugin supplémentaire.
La simulation et le WorldProbeSubsystem existants restent propriétaires du runtime.
Ce module ajoute la métrologie de géographie humaine à côté de leurs inspections.

## Utilisation

Après intégration, redémarrer l'éditeur : `Content/Python/init_unreal.py` enregistre le menu.
Sur une session déjà ouverte, console **Python** :

```python
from anastasis_map_intelligence import editor as ami
ami.register()
```

Pour essayer la branche sans intégration, ajouter son répertoire `Content/Python` à
`sys.path` avant l'import. Exemple propre à cette livraison :

```python
import sys
sys.path.insert(0, r'C:/Users/alex_/.codex/worktrees/map-intelligence-001/ANASTASIS_UNREAL/Content/Python')
from anastasis_map_intelligence import editor as ami
ami.register()
```

1. Ouvrir la carte et charger les régions voulues (l'outil ne charge pas World Partition).
2. **Edit settings JSON** ouvre `Saved/Anastasis/MapAudit/Settings.json`.
3. Enregistrer les paramètres puis **Analyze loaded terrain**. Le JSON est relu à chaque analyse.
4. Les commandes **Show** affichent pente, rugosité, habitabilité, agriculture, navigation,
   connectivité terrain, candidats ou corridors. **Clear visualization** retire le callback.
5. **Save Baseline**, puis, après intervention externe, **Save HumanGeographyV2**.
6. **Compare Baseline / V2** refuse les emprises, cartes, méthodes, grilles ou paramètres différents.

`Latest.json` et `Latest.md` sont remplacés à chaque analyse réussie. Les snapshots nommés
ne sont jamais écrasés. Noms supplémentaires : `ami.save_snapshot('Nom')`,
`ami.compare_snapshots('Avant','Apres')`. Les fichiers générés ne doivent pas être commités.
Une analyse échouée ou annulée efface le résultat en mémoire : impossible d'exporter par
accident la mesure précédente comme si elle venait de réussir.

## Paramètres de design

Aucun seuil ne constitue une norme historique, agricole ou de construction.
Tous les paramètres ci-dessous figurent dans le JSON, la clé de comparaison et les exports.

| Paramètre initial | Valeur | Sens |
|---|---:|---|
| step_m | 2 m | pas demandé maximal ; grille ajustée exactement à l'emprise |
| roughness_radius_m | 2 m | rayon carré de la rugosité locale |
| relief_radius_m | 8 m | rayon carré du relief min/max |
| settlement_slope_deg | 8° | plafond candidat habitat |
| agriculture_slope_deg | 12° | plafond agriculture géométrique |
| pasture_slope_deg | 20° | plafond pâturage / terrain secondaire |
| traversable_slope_deg | 30° | plafond du graphe de déplacement terrain |
| difficult_slope_deg | 45° | au-delà : terrain sévère |
| settlement_roughness_m | 0,35 m | RMS maximal pour habitat |
| agriculture_roughness_m | 0,70 m | RMS maximal pour agriculture |
| min_settlement_area_m2 | 200 m² | minimum d'une poche habitat |
| min_settlement_width_m | 8 m | largeur approximative minimale |
| min_agriculture_area_m2 | 100 m² | minimum d'une poche agricole |
| water_margin_m | 0,25 m | marge au-dessus de la surface d'eau observée |

Par Python, sans fichier de configuration : `ami.configure(step_m=1.0)`.
Si `Settings.json` existe, ses valeurs reprennent priorité au prochain Analyze.
Un aperçu à 8 m coûte moins cher ; 1 m est plus fin. À 0,5 m, augmenter explicitement
le budget de voisinage si nécessaire. Ce n'est pas une recommandation universelle :
le pas doit être petit devant les plateformes et passages recherchés.

## Méthodes et limites

- **Source actuelle** : le composant nommé `ExperimentalTerrain`, section 0 sol,
  section 1 eau. Ces sélecteurs sont configurables. Les sommets sont transformés
  en coordonnées monde et convertis de cm en m. Interpolation barycentrique au centre
  des cellules ; pente du triangle réel, sans normale lissée. Plusieurs composants
  homonymes provoquent un refus explicite.
- **Landscape natif chargé** : traces directement contre les composants de collision
  Landscape, sans toucher aux heightmaps ou Edit Layers. Mesure de la représentation
  de collision ; précision liée à son LOD, pas promesse de lire chaque texel visuel.
- **Pente** : maximum entre pente locale et pentes vers les voisins directs. Cette
  précaution empêche qu'une différence de hauteur entre deux cellules soit ignorée.
- **Rugosité** : RMS des résidus à la tangente locale sur le voisinage carré configuré.
  Une pente plane a une rugosité nulle. Le **relief** est max(z)-min(z), sur son propre rayon.
- **Continuité** : connexions par côtés, jamais par simple contact diagonal. Les trous
  restent inconnus. Les seuils d'aire et de largeur éliminent les petites plateformes.
  Longueur = plus grande dimension de la boîte XY ; largeur = aire/longueur.
  Ces dimensions sont approximatives et ne prouvent pas un rectangle constructible.
- **Eau** : section d'eau du maillage actuel ; pour WaterBody, empreinte de collision
  activée + requête de surface. Sans collision exploitable, `UNKNOWN` et limitation
  explicite. La proximité est une distance horizontale approximée sur grille à huit
  voisins jusqu'aux échantillons mouillés, pas une distance cadastrale à la rive.
- **Navigation** : projections sur le NavMesh chargé de l'agent par défaut ; arêtes
  acceptées seulement après raycast navigation sans obstacle. Absence, construction,
  verrouillage ou zéro projection réussie : `UNKNOWN`. Pas de génération NavMesh.
  Les détours sous-échantillonnés et liens off-mesh ne sont pas représentés.
- **Corridors** : propagation multi-source à coût `longueur*(1+pente/seuil)^2`, entre
  poches admissibles. Chemin, coût, pente maximale, point haut et goulot approximatif
  sont exportés. Le goulot utilise une distance aux cellules non traversables ou au
  bord de carte. Pas de reconnaissance hydrologique formelle de cols/talwegs.
  Les sorties naturelles comptent les composantes connexes de bord accessibles.
- **Candidats** : six sous-scores visibles [0,1], six poids configurables, score /100,
  et fraction de poids réellement documentée. Une donnée manquante est `null`,
  jamais un zéro inventé. Le score est renormalisé sur les poids disponibles.
  Comparer aussi cette couverture ; le rang n'est pas une preuve d'habitabilité.
  L'agriculture autour exclut l'emprise du candidat lui-même et utilise un rayon XY.
- **Surfaces** : projection XY, pas aire développée du relief. Les pourcentages prennent
  pour dénominateur le terrain effectivement échantillonné ; eau/marge et pente inconnue
  ne figurent pas dans les trois tranches de pente. Les catégories habitat/agriculture
  ne remplacent ni pédologie, hydrologie, rendements ni coûts de terrassement.

La connectivité du terrain est une hypothèse géométrique ; `walkable != habitable`.
Le pourcentage navigable n'est pas le pourcentage accessible depuis un point de départ
joueur. Les identifiants de composantes permettent de choisir ce point ensuite.

## Comparaisons et provenance

`comparison_identity` conserve carte, source, méthode, chemins des terrains chargés, moteur, grille et
paramètres ; `comparison_key` en est le SHA256. `sample_sha256` couvre les hauteurs,
gradients, eau, navigation et arêtes mesurées. Un changement d'échelle 95 m vers 1,9 km
rend donc les snapshots **INCOMPARABLES** : il faut choisir le même domaine/protocole
pour une comparaison appariée. Les rapports individuels restent utilisables séparément.

Le rapport conserve version moteur, cibles chargées et transforms, Water/Nav status,
World Partition si exposé et présence des classes PCG/Water. Une classe disponible
ne prouve pas que la carte l'utilise. Le périmètre reste **loaded_only**.
L'analyse est synchrone sur le thread éditeur ; annulation disponible aux étapes de
lecture. Le noyau borné n'est pas interruptible au milieu de chaque boucle.

## Performance et non-mutation

Aucun Actor ni asset créé, aucun `save`, aucune écriture de Landscape, spline ou mesh.
Affichage par points debug en avant-plan, durée 0,25 s, réémis toutes les 0,2 s par callback Slate. Au plus 2 500 points
par défaut, même si la grille est plus grande ; pas de nettoyage global des dessins
appartenant à d'autres outils. Changement de monde : arrêt de l'affichage. Après Clear, les derniers points expirent en 0,25 s de tick éditeur. Au maximum deux émissions se chevauchent à cadence normale.

Budgets configurables : cellules, voisinage, requêtes navigation, candidats, corridors,
points affichés. Un dépassement refuse l'analyse avec une action explicite. Les chemins
exportés peuvent être limités : `corridors_truncated` le dit. Les candidats sont classés
avant troncature. Temps du noyau et temps total d'acquisition/analyse sont exportés.

## Validation reproductible

Depuis la racine de la branche, PowerShell :

```powershell
$env:PYTHONPATH = Join-Path (Get-Location) 'Content/Python'
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' -B -m unittest discover -s Content/Python/anastasis_map_intelligence/tests -v
```

Smoke dans la console Python de l'éditeur, sans fermeture de la session :

```python
from anastasis_map_intelligence import smoke
smoke.run()
```

Le smoke vérifie menu, répétabilité, export, comparaison, budget d'affichage, retrait du
callback, stabilité de la liste des Actors et des packages de carte marqués dirty.
Il ne constitue pas à lui seul une inspection visuelle des pixels.

APIs vérifiées dans les headers du moteur installé **5.8.2 / CL 56702186** et par leurs
signatures dans un éditeur dédié. En particulier, `navigation_raycast` renvoie un Vector
si obstrué, `None` si libre ; `project_point_to_navigation` renvoie Vector ou None.
Tests sur doublures pour les branches Landscape/WaterBody/NavMesh ; la carte réelle
actuelle permet de vérifier directement la branche ProceduralMesh seulement.

Références publiques Epic :
[lecture ProceduralMesh](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/ProceduralMeshLibrary),
[LandscapeProxy](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Landscape/ALandscapeProxy).
La méthode C++ `GetHeightAtLocation` n'étant pas exposée en Python, le chemin Landscape
emploie la trace de composant publiquement exposée dans `PrimitiveComponent.h`.
