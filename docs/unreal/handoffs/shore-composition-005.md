# HANDOFF: shore-composition-005

## MISSION

Comparer une composition de rive sans grille, puis une frange plus basse, sur le terrain
4 m livré par Claude (82a337b). Base de travail d78c68d, branche agent/shore-composition-005,
worktree géré C:/Users/alex_/.codex/worktrees/asset-map-002/ANASTASIS_UNREAL.

## FILES_OWNED

- tools/unreal/capture-shore-composition.py : étude bornée de placement.
- tools/unreal/capture-reed-form.py : démarrage explicite pour permettre sa réutilisation.
- tools/unreal/capture-reed-form.ps1 : sélection validée du script à capturer.
- docs/unreal/handoffs/shore-composition-005.md : résultat et limites.

Les deux assets de Content/Anastasis/EcotoneReview004/ restent locaux et non commités.
Leurs hashes et celui du roseau original sont identiques à la preuve précédente.
Aucun C++, matériau, mesh ou niveau modifié par cette capture. Aucun merge/push.

## COMMIT

Voir git log agent/shore-composition-005. Le commit porte uniquement sur les quatre
fichiers listés ci-dessus. Les changements du canonique et ceux de Claude sont exclus.

## MEC

PASS observés :

- Analyse syntaxique Python et PowerShell.
- Exécution réelle dans Unreal : 8 captures 1600 x 900, processus dédié terminé exit 0.
- Relecture du mesh courbe et de son matériau statique WPO=0.
- Transforms des acteurs, mesh, matériau, échelles et caméras relus après application.
- Caméras et placements A strictement identiques au manifeste reed-form-004-4m-b.
- A/B : 21 touffes, mêmes orientations et échelles ; seules les positions changent.
- B : les 21 racines sont hors de la grille précédente, minimum entre racines 45,945 cm.
- C : B inchangé + 28 touffes sur marge sèche ; hauteur 90,343 à 107,899 cm.
- Support échantillonné : variation maximale 1,651 cm, seuil 8 cm.
- Source/ et Config/ identiques à 82a337b, confirmé par git diff --quiet.
- Les trois hashes d'assets sont inchangés.

Aucune suite d'automation C++ relancée ; aucun nouveau BUILD::PASS revendiqué.
Le build utilisé a été vérifié dans reed-form-004-4m. Pas de seal ni de portail finish.
KNOWN_EXPECTED_FAILURE : non évalué dans cette passe. Les messages de démarrage
Condition failed ne constituent pas une campagne de tests. HttpListener signale un
port déjà occupé ; les captures utilisent Python local, pas ce serveur.

Commande :

```powershell
tools/unreal/capture-reed-form.ps1 -CaptureScript capture-shore-composition.py -OutDir <nouveau-dossier-absolu>
```

La recette précédente peut créer les deux assets de revue s'ils manquent ; elle ne
remplace pas les assets originaux. Dans cette exécution, les assets existaient et
n'ont pas changé. Une correction de métadonnée après capture calcule saved_assets
selon leur présence initiale, au lieu de toujours écrire false. Cette correction
n'affecte aucun placement ni rendu ; sa syntaxe est vérifiée. La version exécutée
est conservée avec les preuves pour éviter de confondre les deux versions.

## SCN

**KEEP B comme candidat de composition. REJECT C en l'état. Rive complète PARTIAL.**

| Variante | Touffes | Triangles nominaux | Observation |
|---|---:|---:|---|
| A | 21 | 70 560 | Contrôle : grille espacée, répétition lisible |
| B | 21 | 70 560 | Groupes irréguliers, trouées ; effet de rangées réduit |
| C | 49 | 164 640 | Bord gauche adouci, mais amas droit trop chargé |

Les huit images ont été inspectées. B retire notamment la rangée vers le premier
plan et forme des groupes inégaux. Ce gain reste local. C ajoute 133 % de triangles
pour un résultat insuffisant : ce n'est pas une mesure de coût GPU, et le nombre
n'est pas à lui seul le motif du rejet. La concentration et la répétition des petits
roseaux restent visibles. Les plantes réduites ne prouvent aucune croissance ni
diversité botanique ; c'est une variation visuelle du même mesh.

Les contrôles nus montrent encore les découpes anguleuses de l'eau, le sol flou,
la bande noire d'horizon et un repère orange d'éditeur. Ajouter des plantes ne
corrige pas ces défauts. Pas de photoréalisme ni de scène présentable revendiqué.
Le support racinaire est échantillonné ; aucune garantie de collision sur toutes
les feuilles ni de stabilité animée. Matériau statique, vent non évalué.

Preuves locales :
C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eab3-eaab-7610-8f6d-bb6dd660a518/shore-composition-005a/

- bare/A/B/C, vues eye et context ; capture.log et capture-hashes.json.
- manifest.json : tous les placements, échelles, supports, caméras, seeds et variante.
- validation.json : contrôles du manifeste, hashes des entrées exécutées et des assets.
- capture-shore-composition.executed.py : version exacte exécutée pour ces huit PNG.

## PLY

UNKNOWN. PLAYER NOT_IMPLEMENTED ; pas de PIE ni de preuve joueur.

## INTEGRATION_RISK

Étude locale par acteurs temporaires, aucun niveau sauvegardé, aucun branchement
au dressing de production. La zone et les caméras sont verrouillées sur un site
pour permettre la comparaison ; aucune généralisation au monde entier.
Base terrain 82a337b conservée : les modifications suivantes de Claude, encore
non livrées au contrôle de cette passe, ne sont pas couvertes par cette preuve.
Le portail finish du dépôt ne prend pas en charge le chemin du worktree géré.
Aucune intégration automatique autorisée par cette passation.

## STOP

Passe de composition terminée. Retenir B pour la prochaine revue artistique.
Next discriminant : reprendre les mêmes vues sur le prochain terrain livré par
Claude avec le contour d'eau corrigé, avant de densifier à nouveau la frange.
