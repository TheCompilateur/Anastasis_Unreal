# HANDOFF: headless-gate-001

RELAIS: headless-tests-001

## MISSION

Mandat d'Alexandre du 2026-10-08 (« fait 1 et 2 »), suite de la mesure de headless-tests-001 : sans rendu,
la suite donne le meme verdict que l'ancien run sur les 369 tests, en 313 s et 6,4 Go de memoire privee
au lieu de 637 s et 11,9 Go -- mais le run a attendu 12,2 min a la porte memoire pour 5 min de suite.

1. **`finish` joue la suite, sans rendu, par defaut.** Une mission qui touche Unreal sort
   `HANDOFF_READY::YES (proved)` au lieu de `(queued)` : un test casse se voit chez son agent, pas au lot
   de tout le monde (player-goals-001, lot 5). `-Queue` rend l'ancien `finish` (build seul, `queued`) :
   machine saturee, ou suite que seul le lot peut juger. `-Prove` reste accepte (c'est le defaut).
   Un `EDITOR_GATE::TIMEOUT` pendant la suite sort `FAIL: suite non jouee -- ... finish -Queue`, pas un
   echec de test. Un marqueur `queued` n'est plus repris par un `finish` sans `-Queue` apres rebase :
   la suite n'a jamais tourne, elle tourne.
   Au lot, une mission `proved` dont les arbres Unreal sont ceux du lot lui epargne la suite (regle de
   retest deja en place) : `RETEST::SKIP (..., suite du finish de <m>)`.
2. **La porte memoire pese un editeur sans rendu a sa mesure.** Un lancement `-nullrhi` compte 0,5 editeur
   (ouvert comme a lancer), a ses seuils (2,5 Go de RAM, 5 Go de marge engagee), et passe devant un ticket
   d'agent (priorite 1) de moins de 15 min ; jamais devant un lot (priorite 0) ni une attente plus longue.
   Un editeur a ligne de commande illisible compte entier ; sans WMI, tous comptent entiers. Pour un
   editeur complet, la regle est inchangee (`ouverts + 1 <= 2`).

## FILES_OWNED

- `tools/unreal/editor-launch.ps1` (porte : poids, seuils, file)
- `tools/unreal/agent-worktree.ps1` (`finish` : suite par defaut, `-Queue` ; message de reprise du lot)
- `tools/unreal/test-agent-worktree.ps1` (S30 porte, S31 `finish -Queue`, S20 message)
- `AGENTS.md` (table `finish`, EDITOR_QUEUE_001 points 1 et 7, porte memoire, index)
- `.claude/skills/anastasis-mission/SKILL.md` (section 3)
- `docs/unreal/handoffs/headless-gate-001.md`

## COMMIT

Voir `git log` de `agent/headless-gate-001`. Porte les deux commits de `headless-tests-001` (relais).

## MEC

- BUILD: sans objet (aucun fichier Unreal) ; `finish` sort `(nounreal)`.
- BANC: `tools\unreal\test-agent-worktree.ps1` (2026-10-08) : **78 PASS, 0 FAIL**, dont 13 nouveaux --
  S30 porte (complet + un ouvert passe comme avant ; deux complets bloquent un sans rendu ; 1 complet +
  1 sans rendu bloque un complet et laisse passer un sans rendu ; RAM 2,8 / marge 6 Go bloque un complet,
  laisse passer un sans rendu ; passe devant un agent de 1 min, pas devant une attente de 20 min ni un
  lot ; un complet ne passe devant personne ; `-nullrhi` reconnu, `-nullrhix` non ; charge reelle lisible),
  S31 (`-Queue -Prove` refuse ; `queued` rebase : `finish` rejoue le portail, `finish -Queue` reprend).
- TESTS Unreal : aucun run nouveau. La suite sans rendu elle-meme est mesuree dans la fiche de
  headless-tests-001 (369 / 369, 0 difference).
- COMMANDS:
  - `tools\unreal\agent-worktree.ps1 finish -Mission <m>`          (build + suite sans rendu)
  - `tools\unreal\agent-worktree.ps1 finish -Mission <m> -Queue`   (build seul)

## PROOFS

PROOFS: (aucune)

## SCN

Sans objet.

## PLY

Sans objet.

## ECARTS

Sans objet : ne touche pas `Source/AnastasisSim`.

## INTEGRATION_RISK

- **Ordre :** relais de `headless-tests-001` (declare `RELAIS:` ; elle n'a aucune preuve PIE). Verser
  `headless-tests-001` d'abord, ou celle-ci seule : elle porte ses deux commits.
- **Effet immediat sur tous les agents** : apres versement, chaque `finish` d'une mission Unreal prend
  ~5 min de plus et un demi-editeur. Les agents qui travaillent sur un `agent-worktree.ps1` d'avant
  (worktree non rebase) gardent l'ancien `finish` jusqu'a leur rebase : leurs missions restent `queued`.
- Le premier `finish` reel apres versement est la vraie preuve du chemin `proved` (le banc n'a pas de
  moteur : il voit le build tente, pas la suite jouee).
- La porte lit les lignes de commande par WMI a chaque tour (15 s) : un peu plus lent que `Get-Process`,
  sans effet mesurable sur l'attente.

## STOP

- Ne revendique aucun run de `finish` avec suite reelle : le banc prouve les branchements (suite demandee
  par defaut, `-Queue`, reprise d'un `queued`), pas un `HANDOFF_READY::YES (proved)` obtenu sur moteur.
- Les seuils sans rendu (0,5 ; 2,5 Go ; 5 Go ; 15 min) viennent d'une seule mesure ; a revoir avec les
  lignes `TEST_MODE::` des prochains runs.
- Ne touche ni au lot (`integrate-batch` joue toujours la suite si ses arbres different de toute preuve
  `proved`), ni aux preuves PIE, ni a `report-tests.ps1`.
