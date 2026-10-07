# HANDOFF: worldmap-piedmont-folds-001

## MISSION

Tester si l'avant-pays nu cote montagne de la carte actuelle vient de l'amorce
trop tardive de la ceinture de plis dans `AnastasisTectonics`. Base `main`
`ba327baa961918499756a95e660cf20b9f7d1f69` ; branche
`agent/worldmap-piedmont-folds-001`.

## FILES_OWNED

- `docs/unreal/handoffs/worldmap-piedmont-folds-001.md` uniquement.

## COMMIT

Voir le commit de cette fiche. Le changement C++ essaye a ete restaure avant
commit ; aucun code ou asset de terrain n'est livre.

## MEC

- Essai : debut de `Out.Folds` avance de `U=2,5` a `0,8 km`, et nervures de
  `U=2,0` a `0,8 km`, dans le champ tectonique existant. Aucune nouvelle bosse
  aleatoire ni modification du terrain simule.
- `tools/unreal/anastasis-unreal.ps1 build` dans ce worktree : **PASS**.
- Le premier `capture-horizon.ps1` a expire avant les images pendant une
  contention de trois editeurs ; resultat visuel de ce run : **UNKNOWN**.
- Le second run a produit les cinq PNG et `HORIZON_COMPLETE`, puis l'editeur
  a eu une exception dans `PythonScriptPlugin` pendant sa fermeture. Les images
  sont lisibles ; la fermeture n'est pas un PASS sain.
- `git diff --check` apres retrait du code : PASS.

## PROOFS

PROOFS: (aucune)

## SCN

Témoin : `worldmap-foothill-continuity-001/Saved/HorizonEvidence/worldmap-foothill-reference/`,
capture de `Lvl_AnastasisSlice` avec le code de base, seed 12345, cinq cameras,
profil d'atmosphere du jeu. Variante :
`Saved/HorizonEvidence/worldmap-piedmont-folds-retry/` de ce worktree. Les vues
H2, H3 et H4 ont ete regardees deux par deux. `compare.py` (pixels avec ecart
>16/255) : H2 **2,66 %**, H3 **0,69 %**, H4 **0,45 %**. La variance de deux runs
du meme etat est environ 3,6 %. Le piémont et la grande plaine restent donc
visuellement identiques. La variante est **REJECT** pour absence de gain visible.

## PLY

UNKNOWN. Pas de marche joueur en PIE pour un code retire.

## INTEGRATION_RISK

Fiche seule, sans effet Unreal ; peut etre versee avec les autres missions
documentaires. Aucun ordre de branche requis. `download.png` et
`tools/unreal/architecture-pie.py` non suivis dans le canonique restent exclus.

## STOP

Ne pas revendiquer que le piémont est corrige. Le champ de plis, meme avance,
ne produit pas une lecture regionale suffisante sur les vues courantes. La
prochaine branche devra traiter couvert et sols de l'avant-pays, sans ajouter
quelques arbres isoles ou une couleur uniforme.
