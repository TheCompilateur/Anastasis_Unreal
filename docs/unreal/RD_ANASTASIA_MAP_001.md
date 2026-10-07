# R&D Anastasia — triage sur la carte de jeu

Date : 2026-10-07. Source examinée : *Résumé exécutif*, 8 pages, fichier fourni par Alexandre, SHA-256 `B45D100DD35BDF17D7438F4D3D2B6B6CC9C683438C920994FED4A73A84306320`. La liste classée occupe les pages 2 à 6 ; le plan, les ressources et les outils sont aux pages 6 à 8. Le PDF est un rapport ChatGPT Deep Research : ses renvois `【4†L29-L38】` ne donnent pas les URL ni la bibliographie nécessaires pour vérifier chaque affirmation. Les priorités S+/S/A/B/C et les coûts en mois sont ses propositions, pas des mesures du jeu.

## Verdict

**Ne pas ouvrir vingt chantiers UE.** La première branche discriminante est de mesurer les saccades attribuables aux PSO dans un jeu construit depuis la carte courante. UE 5.8 active déjà la précache PSO et celle des composants par défaut ; aucun override n'apparaît dans `Config/`. Une nouvelle pipeline de cache, ou un seuil de « −25 % de hitches » pris du PDF, n'a pas encore de phénomène mesuré à corriger. [Epic, PSO Precaching](https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine).

Cette décision n'ajoute ni moteur de rendu ni comportement PNJ. Si le diagnostic PSO est négatif, **STOP PSO** ; choisir le prochain goulot à partir d'une trace de la partie, pas d'une note de priorité documentaire.

## État de départ et portée de la preuve

| Objet | Observation vérifiée dans le dépôt | Limite |
|---|---|---|
| Base | `main` à `e681e6296b2b9b4c21b8d10d7db8bd0a459ee3f8` lors de l'ouverture du worktree ; `Lvl_AnastasisSlice.umap` blob Git `49674dbd9b33268826d45d13b661f0a9549ea96b` | À rafraîchir après le lot d'intégration en cours ; un travail non intégré d'un autre agent n'est pas la carte du jeu. |
| Carte | `EditorStartupMap` et `GameDefaultMap` : `/Game/Anastasis/Maps/Lvl_AnastasisSlice` (`Config/DefaultEngine.ini`) | La configuration ne prouve pas seule le niveau chargé par un éditeur vivant. |
| Monde rendu | `GenerateWorld(seed, 96, 96)`, graine 12345, `TileWorldSize=400 cm`, facteur de présentation 5 : environ 1,9 km ; `ExperimentalTerrain` est un `UProceduralMeshComponent` (`WorldView`) | Le maillage rendu ne remplace ni la sémantique du simulateur ni la navigation. |
| Présentation | Sol section 0, eau section 1 et rivières section 2 ; arbres et herbe par HISM ; Lumen et VSM, DX12/SM6 (`WorldView`, `Config/DefaultEngine.ini`, `MAP_INTELLIGENCE.md`) | Pas de Landscape natif ni de World Partition dans ce pipeline. |
| Habitants | `anastasis.Village.StartVillagers=12` ; simulation dans `AnastasisSim`, projection par `Anastasis_UnrealV2` (`AnastasisSimulationSubsystem.cpp`) | Le scénario d'ouverture n'est pas une preuve de charge de 1 000 habitants. |
| État actuel | StateTree/GameplayStateTree déjà activés, tâches village et prototype Shooter présents ; sons de marche/travail/rivière rendus par `AnastasisSoundscapeSubsystem` | Les sons sont sélectionnés autour de l'auditeur joueur ; ce flux audio ne constitue pas un événement sensoriel canonique pour la simulation. |
| Performance | Les scripts de capture mesurent des p50/p95 en éditeur ; `PERF-05` borne son banc de végétation à 16,7 ms GPU sur RTX 3060 (`fiches/performance.md`) | Aucune série comparable de hitches PSO du jeu construit ou de marche joueur n'a été trouvée. Les ms d'éditeur ne permettent pas de l'inférer. |

**OBS** : le PDF décrit une piste R&D. **INF** : plusieurs pistes sont hors du chemin causal de la carte actuelle. **UNKNOWN** : part des saccades réellement due aux PSO, performance et lisibilité en jeu construit, effet d'une précache supplémentaire.

## Triage des vingt propositions du PDF

`MESURER` = préparer une preuve avant de modifier. `GARDER` = capacité déjà présente ou idée pertinente sans chantier nouveau. `ATTENDRE` = problème possible mais non observé. `ÉCARTER` = n'apporte pas de réponse à la carte et au jeu actuels. Ce sont des décisions de priorité locale, non des jugements universels sur la technique.

| # | Technique du PDF | Décision sur ANÁSTASIS | Fait qui changerait la décision |
|---:|---|---|---|
| 1 | Précache PSO avancée | **MESURER** : précache UE déjà active par défaut ; chercher `Missed` et `Too Late` corrélés aux images lentes. | Trace d'un jeu construit avec saccades attribuées à ces états. |
| 2 | `FCustomRenderPassBase` / RDG | **ATTENDRE** : aucun effet visuel requis ne demande une passe dédiée ; l'API existe, mais son existence ne démontre pas un gain. | Effet défini, impossible ou trop coûteux avec le pipeline actuel, A/B en ms et pixels. |
| 3 | Mass Entity | **ATTENDRE** : la simulation portable et la présentation des 12 habitants sont déjà séparées ; migration structurelle disproportionnée sans profil CPU. | Charge PNJ significative et temps de simulation/présentation par sous-système. |
| 4 | Plugin flecs tiers | **ÉCARTER** au stade actuel : même question que Mass avec une dépendance de plus. | Mass démontré insuffisant sur un vrai seuil de population. |
| 5 | Simulation distribuée / Diarkis | **ÉCARTER** : aucun besoin multi-serveur établi pour la carte et la démonstration courantes. | Architecture réseau et charge démontrées impossibles sur une machine. |
| 6 | Lockstep déterministe | **GARDER le principe**, pas le lockstep UE complet : `AnastasisSim` possède déjà ses tests de parité ; imposer l'égalité bit à bit des frames Chaos/rendu n'est pas la même exigence. | Besoin de replay réseau et périmètre déterministe explicitement défini. |
| 7 | AA FXAA/SMAA | **ATTENDRE** : seules des images appariées à hauteur humaine peuvent isoler flou/scintillement et coût. | Artefact TSR reproductible sur les vraies herbes/feuillages. |
| 8 | Animation GPU d'instances | **ATTENDRE** : pertinente si la projection corporelle des PNJ domine CPU/GPU à population cible. | Profil et nombre de corps visibles, qualité d'animation requise. |
| 9 | Brixelizer / GI voxel | **ÉCARTER** : Lumen est configuré et mesuré ; pas de défaut GI attribué à Lumen qui justifie un moteur GI parallèle. | A/B de qualité et coût montrant une limite précise du pipeline actuel. |
| 10 | AI Perception Hearing | **ATTENDRE** : l'audio joueur est une sortie de présentation ; définir d'abord quels événements du monde le PNJ peut entendre et comment ils entrent dans le simulateur. | Scène de jeu où un bruit causal devrait changer une décision PNJ, avec témoin sans bruit. |
| 11 | Économie macro émergente | **ATTENDRE** : prolonger les boucles ressources/besoins/bâtiments existantes avant prix et marchés. | Échanges, stocks et acteur économique à représenter sur la carte. |
| 12 | Mémoire probabiliste PNJ | **ATTENDRE** : l'oubli ne doit pas masquer une mémoire déterministe incomplète. | Événement connu, souvenir et décision observables, puis A/B d'oubli contrôlé. |
| 13 | StateTree | **GARDER** : plugins et tâches village déjà présents ; mesurer l'usage réel avant « adoption ». | Écart entre tâches exposées et décisions effectivement pilotées en PIE. |
| 14 | Niagara GPU | **ATTENDRE** : un candidat pour une météo visible, soumis à la géographie, au budget GPU et à la preuve image. | Manque visuel défini sur la carte courante, même météo/caméra en A/B. |
| 15 | nDisplay multi-GPU | **ÉCARTER** : outil d'affichage en cluster sans rapport établi avec la vue joueur unique. | Installation multi-écran ou production virtuelle réellement demandée. |
| 16 | Chaos PBD / cheveux | **ÉCARTER** : coût et pipeline de personnages non justifiés par une interaction jouable actuelle. | Besoin corporel précis, budget et preuve joueur. |
| 17 | « Streaming runtime de shader » | **ATTENDRE** : distinguer changement de paramètres/instances de matériaux et compilation de nouvelles permutations ; le PDF ne le fait pas. | Transition de matériau observée qui saccade, avec cause de compilation vérifiée. |
| 18 | Navigation Flow Field / ZoneGraph | **ATTENDRE** : la route actuelle doit être mesurée et reliée à un échec de déplacement. | Charge de planification ou blocage mesuré sur la géographie courante. |
| 19 | Multitâche C++/Blueprint | **ATTENDRE** : profiler la branche CPU avant d'ajouter un ordre concurrent au simulateur. | Temps GameThread dominant et travail isolable sans changer le résultat causal. |
| 20 | Accéléromètre VR | **ÉCARTER** : aucune expérience VR ni capteur cible dans le jeu courant. | Mandat matériel VR explicite. |

Sources techniques vérifiées pour les axes susceptibles de changer la décision : [Epic, précache PSO](https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine), [Epic, MassGameplay](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-mass-gameplay-in-unreal-engine), [Epic, StateTree](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-state-tree-in-unreal-engine), [Epic, AI Perception](https://dev.epicgames.com/documentation/en-us/unreal-engine/ai-perception-in-unreal-engine), [Epic, passe personnalisée](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FCustomRenderPassBase?lang=en-US). Les autres lignes ne revendiquent pas une faisabilité UE démontrée par ce travail : elles statuent sur leur pertinence causale locale.

## Expérience unique proposée : saccades PSO sur la map jouée

**Question** : sur `Lvl_AnastasisSlice`, quelle part des images lentes d'un premier parcours est associée à `PSOPrecache: Missed` ou `Too Late` ? La documentation Epic donne `r.PSOPrecache.Validation=1` pour des comptes légers, `=2` pour les détails, `stat PSOPrecache` et les pistes Insights correspondantes. [Epic, validation et suivi](https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine#validationandtracking).

1. Construire un jeu **Development** depuis un commit précis, avec map, RHI DX12, résolution 1080p, seed 12345, météo et heure fixées. Consigner GPU, pilote, cache pilote et paramètres graphiques. Un build Editor seul n'est pas cette preuve.
2. Capturer le premier chargement puis le même parcours rejoué : arrivée, marche dans le village et en forêt, rive, apparition des bâtiments et PNJ. Conserver les temps d'image p50/p95/p99, les images >33,3 ms, le nombre de `Missed`/`Too Late`, et l'horodatage des événements. Le premier passage et le second ne sont pas un A/B d'optimisation ; ils diagnostiquent les effets du cache.
3. **KEEP l'hypothèse PSO** si des saccades visibles coïncident avec des manques PSO reproductibles sur une configuration de cache documentée. Alors seulement tester une intervention unique, par exemple attente de compilation avant entrée en jeu ou cache groupé des passes manquantes, avec mesure A/B sur le même trajet.
4. **REJECT / STOP** si aucun manque n'accompagne les images lentes, ou si la machine est saturée et la mesure non comparable. La branche suivante doit suivre le poste CPU/GPU réellement dominant. Ne pas configurer `SetGameUsageMask`, ajouter un cache groupé ni annoncer « −25 % » avant ces données.

La [documentation Epic des caches groupés](https://dev.epicgames.com/documentation/en-us/unreal-engine/manually-creating-bundled-pso-caches-in-unreal-engine) les décrit comme complément lorsque la précache runtime ne suffit pas, et demande de parcourir un jeu construit pour collecter ses PSO. Cela place explicitement l'essai après la mesure, pas avant.

## Cadence révisée

Le plan du PDF (quatre sprints de six semaines, cluster nDisplay, Brixelizer, économie, lockstep) est une proposition de portefeuille R&D sans capacité ni goulot observé du jeu. Pour cette carte : une **fenêtre de diagnostic PSO** se termine par `KEEP/REJECT`; si `REJECT`, reprendre la trace de performance ou une preuve joueur, sans lancer le reste du portefeuille. Les budgets FTE, les dates et les profils Low/Medium/High du PDF ne sont pas adoptés comme engagements du projet : seule la machine RTX 3060 / 16 Go est documentée ici, et le budget du jeu construit reste à définir sur mesure.

## Frontières

- **MEC** : vérification du dépôt et des sources Epic ; aucun gain de frametime revendiqué.
- **SCN** : la géométrie et la configuration sont reliées au jeu ; une inspection vivante de la carte doit encore fixer le niveau et le commit observés.
- **PLY** : `UNKNOWN` ; aucune marche joueur ni jeu construit mesuré dans cette mission.
- Le dossier n'autorise aucun changement de terrain, de matériau, d'IA ou de config PSO sans l'expérience discriminante correspondante.
