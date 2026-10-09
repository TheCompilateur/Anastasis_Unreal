# HANDOFF: pontic-horizon-realism-002

## MISSION

Attribuer la grande paroi claire de l'horizon aux volumes, aux couleurs ou a l'air avant
d'integrer les huit montagnes 3D deja presentes dans `Content/Anastasis/PonticMountains/`.
Les captures precedentes avaient rejete leur simple superposition sur l'anneau.

## FILES_OWNED

- `tools/unreal/capture-horizon.ps1/.py` : `-View <nom>` pour une direction,
  validation du nom, comptage des acteurs d'incarnation avant reconstruction.
- `.claude/skills/anastasis-capture/SKILL.md` : usage du mode cible.
- cette fiche. Aucune modification finale dans `Source/`, `Content/` ou la carte.

## COMMIT

Branche `agent/pontic-horizon-realism-002` ; consulter son `HEAD` pour le SHA.

## MEC

- Base : `main` a `4ff9c34a`, worktree isole. Huit `StaticMesh` pontiques verifies
  presents dans `main` par `git ls-tree`.
- Build initial UE, builds des essais C++, puis recompilation apres retrait de tous
  les essais C++ : `BUILD::PASS` a chaque etape. Les binaires finaux correspondent
  a la source restauree.
- `capture-horizon.ps1 -Label pontic-002-baseline -Mode skyline -Atmosphere -States B` :
  huit images, `HORIZON_COMPLETE`, 285 anneaux, 221 920 sommets, 441 560 triangles,
  altitude maximale 3 875 m, neige 1 273, roche 7 143, foret 42 323.
- L'option nouvelle `-Mode skyline -View S045 -States B` a produit une seule image
  `S045_B.png` et `CAPTURE::PASS` ; journal : un seul acteur
  `AnastasisWorldEmbodiment_0` existait et fut reconstruit.
- `-Mode skyline -View S999 -States B` a retourne immediatement
  `CAPTURE::FAIL vue inconnue: S999`, sans lancement d'editeur.
- Les images instrumentales et journaux restent sous
  `Saved/HorizonEvidence/pontic-002-{baseline,on,nofog,snow-mask,ring-colour,aerial-05}/`.

## PROOFS

PROOFS: (aucune)

## SCN

**Aucun KEEP visuel.** Le temoin `baseline/S045_B.png` montre une longue paroi
bleu-blanc au-dessus d'un relief proche brun et lisse. Les huit vues sont liees a la
meme carte et a la graine 12345.

- Variation de la chaine arriere par secteurs : build et capture, puis **REJECT**.
  Les sommets fortement enneiges sont passes de 1 273 a 138, mais la paroi visible
  dans `on/S045_B.png` reste presque identique ; `S225` temoin varie de 1,66 %
  des pixels au seuil 16/255. L'essai a ete retire du C++.
- `ShowFlag.Fog 0` : dans `nofog/S045_B.png`, le relief proche gagne du contraste,
  tandis que la paroi arriere reste claire. Ce drapeau ne neutralise pas necessairement
  toute la perspective de `SkyAtmosphere`.
- Masque neige marque en magenta : pas de tache magenta visible sur la paroi.
  Masque **de tout l'anneau** marque en magenta dans `ring-colour/S045_B.png` :
  le relief proche et les sommets deviennent roses, les creux eloignes restent
  tres bleu clair. Un seul acteur d'incarnation est present. Les marqueurs ont
  ete retires du C++.
- Perspective aerienne `1,2 -> 0,5` sur `aerial-05/S045_B.png` : contraste
  accru mais ciel noir avec l'erreur du dome, et `gpu_ms_p50=0,31` incoherent.
  **REJECT comme preuve** ; aucun reglage d'atmosphere n'est conserve.

`CAPTURE::PASS` certifie l'ecriture des images, pas leur validite artistique. Plusieurs
runs ont montre l'erreur de couverture du dome celeste ; leurs vues noires sont exclues.

## PLY

UNKNOWN : aucun parcours joueur ni jeu package pour cette mission.

## ECARTS

AUCUN — `Source/AnastasisSim/` non touche.

## INTEGRATION_RISK

- Passation d'outillage et de diagnostic seulement. Les huit meshes ne sont toujours
  pas places dans la carte. Ne pas presenter cette mission comme un horizon
  photorealiste ou comme une integration de ces assets.
- La cause de la paroi claire est couplee entre couleurs/surface du lointain et
  perspective aerienne. Examiner le graphe **reel** de `M_AnastasisFarTerrain`
  dans l'editeur et obtenir des captures sans erreur de dome avant de toucher
  aux valeurs par defaut. Le no-fog seul ne discrimine pas toute l'atmosphere.
- `main` peut avoir avance depuis `4ff9c34a` ; revalider les arbres et captures
  a l'integration.

## STOP

Aucune modification de la carte, de l'anneau ou de l'atmosphere retenue. Le prochain
essai doit faire une seule variation de la surface lointaine, a hauteur humaine,
avec un ciel valide et temoin repete, avant de deplacer la geometrie ou les meshes.
