# HANDOFF: night-soundscape-001

## MISSION

La nuit habitee : premier socle sonore causal. Les pas suivent la distance parcourue par les corps visibles ; les coups suivent les increments de WorkSession du chantier. Une source d'eau suit la section de riviere reellement rendue dans le monde du joueur. Aucune boucle de village ni changement artificiel au coucher du soleil.

## FILES_OWNED

- Source/Anastasis_UnrealV2/Audio/AnastasisSoundscapeSignal.h
- Source/Anastasis_UnrealV2/Audio/AnastasisSoundscapeSignal.cpp
- Source/Anastasis_UnrealV2/Audio/AnastasisSoundscapeSubsystem.h
- Source/Anastasis_UnrealV2/Audio/AnastasisSoundscapeSubsystem.cpp
- Source/Anastasis_UnrealV2/Audio/AnastasisSoundscapeTests.cpp
- tools/unreal/soundscape-pie.py
- tools/unreal/proofs.txt (une ligne)
- AGENTS.md (une ligne d'index)
- docs/unreal/handoffs/night-soundscape-001.md

## COMMIT

Commit portant cette fiche sur agent/night-soundscape-001 ; base 73248bc.

## MEC

- BUILD: UNKNOWN avant finish ; verdict dans la passation du portail.
- TESTS: QUEUED, pas de resultat revendique.
- COMMANDS: `tools/unreal/agent-worktree.ps1 finish -Mission night-soundscape-001`
- Tests ajoutes : Anastasis.Audio.Soundscape.Causality et Signal (silence sans geste, interieur, teleportation, absence de backlog, session changee, PCM borne, continuite des blocs d'eau).
- Plafonds : 12 impulsions de 250 ms, une voix d'eau, 12000 triangles examines par maillage toutes les 0,5 s ; pas de file d'evenements retardes. La file PCM d'eau reste sous 36000 octets.
- Reglages : `anastasis.Soundscape.Enabled 0/1`, `anastasis.Soundscape.Volume 0..1` (defaut 0.6). Blueprint/Python `AnastasisSoundscapeDebugLibrary.get_soundscape_status(world)` retourne compteurs de dispatch et budgets, PAS une mesure d'audibilite.

## PROOFS

PROOFS: soundscape-pie

## SCN

UNKNOWN. La preuve preparee place une camera pres d'un vrai FirstSite livre, attend trois dispatchs de coups, fige la simulation et verifie l'arret des nouveaux gestes, puis controle le nettoyage des voix a off. La preuve est a rejouer dans le lot, sans editeur autonome.

## PLY

UNKNOWN. Ecoute chantier -> maisons -> riviere encore requise. Palette procedurale de maquette (impacts bois, pas sourds/granuleux, eau filtree), pas une banque de prises de son finalisee. Aucun claim de realisme acoustique.

## ECARTS

AUCUN — aucun fichier Source/AnastasisSim modifie ; observateur de presentation uniquement, RNG local independant.

## INTEGRATION_RISK

- Pas de dependance a sky-continuity-002 ; aucun fichier du ciel touche.
- AGENTS.md et proofs.txt sont partages : conserver les ajouts concurrents.
- Systeme actif dans les mondes game/PIE avec joueur ; serveur dedie exclu. Sons de PNJ visibles uniquement, pas du corps joueur cache.
- Coups observes par echantillonnage : l'acceleration peut en omettre, et le dernier coup dont la session est effacee dans le meme tick peut etre omis. Aucune invention pour compenser.
- Eau : section 2 ExperimentalTerrain uniquement. Sans rubans rendus, silence ; lacs, mer, pluie et vent hors perimetre. Source ponctuelle approximee, sans acoustique interieure ni occlusion.
- Validation du peripherique/mixeur et ecoute humaine restent necessaires. Build et compteurs ne suffisent pas a les prouver.

## STOP

Passation a l'integrateur unique apres finish. Aucun integrate, push ou nouvel editeur. Pas de preuve artistique, d'audition ou de nuit complete revendiquee.
