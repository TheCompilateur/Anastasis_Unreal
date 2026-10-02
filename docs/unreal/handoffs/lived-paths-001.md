# HANDOFF: lived-paths-001

## MISSION

Prolonger anthropic-landscape-001 : herbe couchee dans l'axe des passages observes, usure modulee par Wetness de la tuile, preuve du meme trajet utilise puis abandonne. Aucun nouveau systeme parallele.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisAnthropicMemory.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisAnthropicMemory.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisAnthropicSubsystem.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisAnthropicTests.cpp
- tools/unreal/lived-paths-capture.py
- tools/unreal/proofs.txt (une ligne)
- AGENTS.md (une ligne)
- docs/unreal/handoffs/lived-paths-001.md

## COMMIT

Commit portant cette fiche sur agent/lived-paths-001, base 73248bc.

## MEC

- BUILD: UNKNOWN avant portail ; resultat externe du finish fait foi.
- TESTS: QUEUED. Nouveau test Anastasis.Anthropic.WetnessAxisAndAbandonment : trajet sec/humide, aller-retour, terrain non visite, recuperation, diagonale unique invisible.
- `tools/unreal/agent-worktree.ps1 finish -Mission lived-paths-001`
- Garde-fous existants conserves : 4096 cellules, 256 cellules rendues, 8192 touffes ; rejet des sauts et lacunes temporelles. Aucun fichier du simulateur portable modifie.
- Le double angle stocke l'axe : des retours ne s'annulent pas, des croisements reduisent la coherence. Inclinaison jusqu'a 60 degres, hauteur minimale 35 %, racines immobiles.
- Wetness [0,1] multiplie l'usure par [1,1.5]. Seuil de distance conserve pour rendre invisible une premiere traversee. Demi-vie huit jours simules conservee. Reglages artistiques, pas une loi de compaction mesuree.

## PROOFS

PROOFS: anthropic-memory-pie,lived-paths-capture

## SCN

UNKNOWN. Nouvelle preuve preparee, a executer au lot. Maisons et puits via scenarios existants ; attente de vrais passages et d'herbe modifiee. Camera sur une touffe effectivement modifiee, a +170 cm relativement a sa racine (la pente sous la camera reste a inspecter). Simulation figee, Display 0/1/0 aux memes positions et meme historique. Les douze PNJ sont ensuite retires, compteur people=0 requis, avance 16 jours et baisse de force requise. Captures reference/used/reference2/abandoned et route.json.

Le script ne prouve pas que le trajet a ete choisi POUR relier maison et puits : il prouve le passage reel dans ce scenario. Examiner les images et mesurer used/reference contre reference/reference2 avec .claude/skills/anastasis-capture/compare.py avant verdict. apply_ms est le cout CPU de modification HISM, pas le GPU.

## PLY

UNKNOWN. Parcours a hauteur humaine et lisibilite artistique encore requis. Pas de persistance entre sauvegardes : memoire pendant la session seulement.

## ECARTS

AUCUN — Source/AnastasisSim intact ; lecture de Wetness et positions uniquement.

## INTEGRATION_RISK

- Aucun lien avec night-soundscape-001 ; base deja porteuse du prototype anthropique.
- Fichiers Anthropic partages avec anthropic-landscape-001 ; conserver les changements concurrents sans duplication.
- CVar Memory reste 0 par defaut selon le contrat existant tant que PIE/A-B/GPU non valides. `anastasis.Anthropic.Memory 1` active ; `Display 0` masque sans effacer, `Display 1` restitue ; `Memory 0` restaure ET efface.
- Le rendu modifie les transforms HISM existantes, sans asset, materiau, terrain, collision ou navigation nouvelle (VEG-01/03/05, SOL-02). Pas de peinture de boue inventee : le sol existant se decouvre entre les touffes.
- Le protocole restaure les transforms uniquement si elles correspondent encore a notre derniere application. Memoires non sauvegardees et echantillonnage accelere incomplet restent des limites explicites.
- Preuve capture a rejouer en fin de lot ; inspection artistique/GPU encore absente. Aucun editeur autonome lance.

## STOP

HANDOFF_READY uniquement ; integration par session designee. Pas de revendication de sentier finalise, de preuve joueur, de gain GPU ou de retour causal vers la navigation.

## INTEGRATION (integrateur, 2026-10-02)

Rebase sur main (ba327ba) apres anthropic-paths-002 : la CVar `anastasis.Anthropic.Display` est fusionnee dans `anastasis.Anthropic.Draw` de main (meme role, memoire conservee) ; `StrengthAt` garde le melange des cellules voisines de main, applique a `Wear` ; le rapport garde tous les champs de main (`drawn`, `focus_*`) et ajoute `display`, `peak*`, `people`, `view_*` ; les deux tests coexistent (`ObservedContinuity`, `WetnessAxisAndAbandonment`). Preuve `lived-paths-capture` passee a `Draw`.
