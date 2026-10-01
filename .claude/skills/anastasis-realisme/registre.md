# Registre des recommandations

Chaque affirmation tirée d'une recherche, avec son sort dans ANÁSTASIS. Un agent qui lit « il faut
faire X » dans une recherche ou un tutoriel cherche X ici **avant** de l'appliquer.

| Statut | Sens | Ce qu'on en fait |
|---|---|---|
| `APPLIQUÉ` | le projet le fait déjà | rien ; la règle citée dit comment |
| `ÉQUIVALENT` | le projet atteint le même but par un autre mécanisme | ne pas remplacer le mécanisme du projet |
| `REJETÉ` | contredit une décision documentée ou une mesure | ne pas refaire sans fait nouveau |
| `OUVERT` | pas décidé, peut-être utile | candidat de mission, sur mandat |
| `HORS_PÉRIMÈTRE` | ne concerne pas ce projet (outil externe, plateforme, gameplay) | ignorer |
| `FAUX` | erroné ou invérifiable dans la source | ignorer ; ne jamais citer comme référence |

Sources : `docs/recherche/realisme-unreal/README.md`.

## RU-001 — « Résumé exécutif » (ChatGPT Deep Research, reçu le 2026-10-01)

Rapport générique de pipeline AAA de monde ouvert. Il suppose un Landscape, World Partition, Megascans,
SpeedTree, le plugin Water et une RTX 4080 : presque rien de cela n'existe dans le projet, par choix. Ses
références (`【77†L211-L216】`) renvoient à des pages que le rapport ne fournit pas. Ce qui est vrai a été
recoupé avec les notes de version d'UE 5.8 ou avec le code du projet.

### Cadrage

| ID | Affirmation | Statut | Pourquoi | Règle |
|---|---|---|---|---|
| RU-001-01 | Choisir entre photoréel et stylisé ; UE 5.8 ajoute un toon shader expérimental (Substrate) | `APPLIQUÉ` | photoréel sobre fixé par `P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md` ; le toon shader est hors sujet | loi 2 |
| RU-001-02 | Viser une RTX 4080, 60 images/s en 4K, ou consoles | `FAUX` | la machine réelle est une RTX 3060 ; aucune console visée | PERF-03 |
| RU-001-03 | Stabiliser la géométrie avant l'esthétique | `APPLIQUÉ` | ordre suivi : relief, puis sol, eau, végétation | TER-01 |
| RU-001-04 | Chaque ajout (pont, moulin, champ) doit suivre la logique du simulateur | `APPLIQUÉ` | le monde est une projection de la simulation | TER-01 |
| RU-001-05 | Calendrier : 40 jours en solo, 3 semaines en équipe | `HORS_PÉRIMÈTRE` | estimation sans base | — |

### Terrain

| ID | Affirmation | Statut | Pourquoi | Règle |
|---|---|---|---|---|
| RU-001-06 | Mesh Terrain (5.8, expérimental) pour surplombs et tunnels | `OUVERT` | existe bien en 5.8 ; construit pour l'édition à la main | TER, Ouvert |
| RU-001-07 | Heightmap générée par Gaea, World Creator ou Houdini | `HORS_PÉRIMÈTRE` | le relief vient de la simulation (graine 12345) | TER-01 |
| RU-001-08 | Convertir la map en World Partition | `REJETÉ` | monde de 1,9 km, une seule carte rebâtie à l'incarnation, aucun acteur placé à la main | TER-02 |
| RU-001-09 | Régler résolution, sections et LOD du Landscape | `HORS_PÉRIMÈTRE` | pas de Landscape ; l'équivalent est `Forge.Subdiv` | TER-01 |
| RU-001-10 | HLOD via World Partition | `REJETÉ` | pas de World Partition ; rien ne montre un besoin | TER-02 |

### Sol

| ID | Affirmation | Statut | Pourquoi | Règle |
|---|---|---|---|---|
| RU-001-11 | Landscape Material multi-couches (roche, herbe, terre) | `ÉQUIVALENT` | `M_AnastasisGround` : canaux UV morphologiques, triplanaire, quatre familles | SOL-02 |
| RU-001-12 | Textures Megascans ou Substance | `ÉQUIVALENT` | photos CC0 Poly Haven qui **modulent** ; une photo brute effacerait les albédos calés | SOL-02 |
| RU-001-13 | RVT contre la répétition sur de grandes surfaces | `ÉQUIVALENT` | passe-haut à 15 cm + bruits multi-échelles ; RVT sur le terrain = `DANGEROUS_TO_CHANGE` | SOL-03 |
| RU-001-14 | RVT : tuiles de 1024, format YCoCg (+25 % de mémoire), `r.VT.MaxUploadsPerFrame` | `HORS_PÉRIMÈTRE` | pas de RVT | — |

### Végétation

| ID | Affirmation | Statut | Pourquoi | Règle |
|---|---|---|---|---|
| RU-001-15 | Herbe par Landscape Grass Output | `ÉQUIVALENT` | herbe en HISM tuilées par le C++ (`GROUND_COVER_001`) | VEG-03 |
| RU-001-16 | Arbres SpeedTree ou Megaplants | `ÉQUIVALENT` | arbres GeometryScript, 7 essences × 3 formes (`create_tree_asset.py`) | VEG-02 |
| RU-001-17 | Placement : à la main sur les rives et lisières, Procedural Foliage ailleurs | `ÉQUIVALENT` | placement C++ déterministe, structure d'âges, groupes et clairières | VEG-01, VEG-05 |
| RU-001-18 | Procedural Vegetation Editor (expérimental) | `OUVERT` | existe en 5.7–5.8 ; pas de mandat | VEG, Ouvert |
| RU-001-19 | Instancier l'herbe et les végétaux répétés | `APPLIQUÉ` | HISM partout | VEG-01 |
| RU-001-20 | Varier formes, orientations et tailles | `APPLIQUÉ` | essences, formes, couches d'âge, émergents | VEG-05 |
| RU-001-21 | Activer Nanite sur les meshes compatibles | `REJETÉ` pour le feuillage, `OUVERT` pour l'opaque héros | feuillage en cartes + WPO ; roche, mur et ruine sont « NANITE GOOD CANDIDATE » (`AAA_VISUAL_TARGET_LAB.md`) | VEG, Ne pas faire |
| RU-001-22 | Nanite : régler le Fallback LOD, le couper sur les grandes surfaces pour la collision | `HORS_PÉRIMÈTRE` | pas de Nanite en production | — |

### Eau

| ID | Affirmation | Statut | Pourquoi | Règle |
|---|---|---|---|---|
| RU-001-23 | Plugin Water : rivières et lacs sur splines, vagues, réflexion, réfraction | `REJETÉ` | le carving exige un Landscape ; choix d'Alexandre du 2026-09-30 | EAU-01 |
| RU-001-24 | Le but : réflexion, réfraction, vagues | `ÉQUIVALENT` | Single Layer Water, absorption selon la profondeur, flowmap | EAU-02 |
| RU-001-25 | Ponts, moulins et pêcheries le long des berges | `HORS_PÉRIMÈTRE` | relève de la simulation, pas du rendu | — |
| RU-001-26 | Collisions de rivière, flottaison Chaos | `HORS_PÉRIMÈTRE` | `PLAYER` = `NOT_IMPLEMENTED` | — |

### Ciel et lumière

| ID | Affirmation | Statut | Pourquoi | Règle |
|---|---|---|---|---|
| RU-001-27 | SkyAtmosphere + soleil + SkyLight + ExponentialHeightFog, cycle dynamique | `APPLIQUÉ` | `AAnastasisWorldAtmosphere`, `AnastasisSkyClock` | ATM-02 |
| RU-001-28 | Volumetric Clouds | `APPLIQUÉ` | nuages à 1,5 km, 3 km d'épaisseur, couverture selon la météo | ATM |
| RU-001-29 | Lune, réglages de nuit | `APPLIQUÉ` | lune à 0,3 lux, une seule lumière directionnelle principale | ECL-04 |
| RU-001-30 | Lumen GI, qualité selon la cible | `APPLIQUÉ` | Lumen en ray tracing matériel, Hit Lighting | ECL-03 |
| RU-001-31 | Lumen Lite, « deux fois plus rapide » | `OUVERT` | annoncé par Epic pour 5.8 ; non mesuré ici | ECL-07 |
| RU-001-32 | Lumen : « Surfel » contre « Hardware RTGI » | `FAUX` | Lumen trace en logiciel (champs de distance) ou en matériel ; pas de mode Surfel | ECL |
| RU-001-33 | Lumen Scene Detail à 100 % et plus, Max Trace Distance | `OUVERT` | réglages réels de Lumen, jamais mesurés ici | ECL-07 |
| RU-001-34 | VSM requises par Nanite ; `r.Shadow.Virtual.Enable=1` | `APPLIQUÉ` | actives, même sans Nanite | ECL-05 |
| RU-001-35 | `r.Shadow.Virtual.Max` (taille max), « 16k pour la plupart des AAA » | `FAUX` | cette CVar n'existe pas ; 16k virtuels est la résolution fixe par niveau de clipmap ; la netteté se règle par `r.Shadow.Virtual.ResolutionLodBiasDirectional` | ECL |
| RU-001-36 | Ombres douces par SMRT ; Contact Shadow Length ; Distance Field Shadow Distance | `OUVERT` | ombre de contact identifiée comme écart « bas coût » (`AAA_VISUAL_TARGET_LAB.md`) ; jamais en A/B | ECL-06 |
| RU-001-37 | SkyLight `Stationary` pour l'ambiance, ou `Movable` pour Lumen | `FAUX` | avec Lumen, le SkyLight est `Movable` en capture temps réel ; `Stationary` relève de l'éclairage précalculé | ECL-02 |

### Post-traitement

| ID | Affirmation | Statut | Pourquoi | Règle |
|---|---|---|---|---|
| RU-001-38 | Volume global : eye adaptation, LUT, DOF, SSAO, bloom doux, vignettage, flares | `REJETÉ` | exposition fixe ; LUT et bloom interdits pour contrefaire la matière ; DOF exclu des preuves | POST-01, POST-02 |
| RU-001-39 | Surveiller le tonemapper pour éviter la surexposition | `APPLIQUÉ` | exposition fixe, drapeau `CLIPPED` d'`atmosphere-metrics.py` | ECL-01 |
| RU-001-40 | Limiter l'auto-exposition et la DOF pour un rendu stable | `APPLIQUÉ` | exposition fixe à EV100 14 | ECL-01 |
| RU-001-41 | Bloom et flares à 50 % | `REJETÉ` | direction artistique ; profil de labo à 0,35 en console seulement | POST-01, POST-04 |
| RU-001-42 | Scalability Groups par plateforme | `OUVERT` | aucune plateforme cible définie | PERF, Ouvert |

### Performance

| ID | Affirmation | Statut | Pourquoi | Règle |
|---|---|---|---|---|
| RU-001-43 | LOD manuels pour les meshes non Nanite | `APPLIQUÉ` partiellement | arbres à 3 LOD ; props, abri et bâtiments à 1 LOD (`AAA_VISUAL_TARGET_LAB.md`) | VEG-04 |
| RU-001-44 | Culling par volumes d'occlusion construits au préalable | `FAUX` | UE 5 élimine par requêtes d'occlusion dynamiques ; la visibilité précalculée relève de l'éclairage statique, interdit ici | PERF |
| RU-001-45 | Particules et brume en Niagara GPU | `OUVERT` | pluie et neige invisibles faute de Niagara | ATM, Ouvert |
| RU-001-46 | Budgets mémoire par `/Script/Engine.RendererSettings.RHI.*` | `FAUX` | aucun réglage de budget de ce nom ; le pool de textures se règle par `r.Streaming.PoolSize` | PERF |
| RU-001-47 | Profiler avec `stat unit`, `stat GPU`, `stat RHI`, `stat SceneRendering` | `APPLIQUÉ` | plus `ProfileGPU` par les scripts de capture | PERF-01 |
| RU-001-48 | Budgets : moins de 30 M de triangles, moins de 10 000 draw calls ; ailleurs moins de 500 à 1 000 | `FAUX` | chiffres génériques et contradictoires entre eux ; seule la mesure sur la RTX 3060 vaut | PERF-02 |
| RU-001-49 | Textures : moins de 8 Go sur PC, moins de 4 Go sur console | `HORS_PÉRIMÈTRE` | aucune plateforme cible ; le projet n'a presque pas de textures | — |

### Outils et flux

| ID | Affirmation | Statut | Pourquoi | Règle |
|---|---|---|---|---|
| RU-001-50 | ZBrush/Blender → FBX ; Substance → textures PBR ; Quixel Bridge | `ÉQUIVALENT` | les assets naissent de scripts d'autorité (`tools/unreal/create_*.py`) | VEG-02, SOL-04 |
| RU-001-51 | MetaHumans pour les PNJ | `HORS_PÉRIMÈTRE` | habitants en portraits PNG (`VILLAGER_PNG_001.md`) | — |
| RU-001-52 | Chaos : végétation froissée ; « Chaos Cloth pour plantes vivantes prêt en 5.8 » | `FAUX` | affirmation non sourcée, introuvable dans les notes de version | — |
| RU-001-53 | Plugin « Chaos Terrain de Trajectoire » | `FAUX` | introuvable ; vraisemblablement inventé | — |
| RU-001-54 | Megascans « gratuit pour UE » | `HORS_PÉRIMÈTRE` | la licence Megascans a changé avec Fab ; à vérifier si un jour on en importe | — |
