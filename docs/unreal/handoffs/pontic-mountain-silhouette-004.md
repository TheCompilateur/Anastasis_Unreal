# HANDOFF: pontic-mountain-silhouette-004

## MISSION

CURRENT: depuis le bassin, la chaine principale se lit comme une paroi claire continue.
TARGET: distinguer quelques massifs et cols larges sans changer le terrain habite.
GAP: les cols existants sont locaux, alors que la crete conserve sa hauteur sur de longs segments.
Mission de presentation uniquement ; aucune revendication geographique ou historique.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisTectonics.{h,cpp}` : modulation lente de la chaine principale.
- `Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainHorizon.cpp` : CVar de construction de l'anneau, defaut retenu 0,5.
- `.claude/skills/anastasis-realisme/fiches/terrain.md` : valeur et portee de la CVar.
- cette fiche.

## COMMIT

Branche `agent/pontic-mountain-silhouette-004` ; consulter son HEAD pour le SHA.
Base des captures : `main` = `0af22c5fc`, carte `Lvl_AnastasisSlice`, graine 12345.

## MEC

- Premier build dans le worktree : `BUILD::PASS`, 21 actions, 186,26 s.
- Trois intensites sur le meme binaire : 0 (temoin), 1 (essai fort), 0,5 (retenue).
- A intensite 1, roche de l'anneau 7 143 -> 4 477 sommets ; a 0,5 : 5 392. Altitude maximale 3 875 m inchangee.
- Le parametre ne modifie que `Out.Range` (anneau de presentation), ni la forge du bassin ni `AnastasisSim`.
- Build et suite du commit final : a renseigner apres `finish`.

## PROOFS

PROOFS: (aucune)

## SCN

- Captures fixes, a hauteur humaine depuis le bassin :
  `tools\unreal\capture-horizon.ps1 -Mode skyline -States B -PreCmds 'anastasis.Terrain.MountainSaddles <0|0.5>'`.
  A/B/A sur huit directions, meme camera, carte, graine et binaire :
  `Saved/HorizonEvidence/silhouette-004-all-{A,half,A2}/S<azimut>_B.png`.
- Dans la bande de l'horizon (y=230..549), pixels differents de plus de 16/255 :
  S000 13,13 % contre 2,80 % entre temoins ; S045 20,13 % contre 0,35 % ;
  S090 19,88 % contre 0,47 % ; S135 5,09 % contre 0,75 %.
  S180/S225/S270/S315 restent proches de leur variance temoin (au plus 1,03 % d'ecart A/B).
- La vallee reste dans sa variance de capture : S045 6,09 % A/B contre 6,46 % A/A2 ;
  S090 4,77 % contre 4,04 %. S000 est bruite par la vegetation (23,81 % contre 23,75 %).
- Vue regardee : a 0,5 les deux grands sommets demeurent, et le centre de S045 s'abaisse assez pour
  separer les volumes. A 1, le massif oriental perd trop de hauteur : REJECT. Les sept autres
  directions de B ont ete regardees ; pas de nouvelle ouverture vide evidente.
- KEEP borne a une silhouette plus lisible. La roche reste claire et lisse, avec une rupture de
  versant abrupte notamment a S090 : photorealisme et emerveillement non atteints.
- Les `gpu_ms_p50` de S045/S090 de la serie complete sont du meme ordre (environ 23-24 ms),
  sans protocole de charge isolee ; pas de claim de performance.

## PLY

UNKNOWN : cameras fixes dans l'editeur, aucun parcours joueur ni jeu package.

## ECARTS

AUCUN — `Source/AnastasisSim/` non touche.

## INTEGRATION_RISK

- Capture et build portent sur `0af22c5fc` ; si `main` ou ses binaires changent avant le lot,
  rejouer au minimum S045/S090 et le temoin, puis comparer les huit directions pour un verdict global.
- La CVar est lue au moment de `AnastasisTerrainHorizon::Build` : changer sa valeur sans re-incarner
  le monde ne change pas l'anneau deja construit.
- La correction de surface, d'atmosphere et de details rocheux est une autre mission ; cette branche
  ne l'inclut pas. Les captures ne valident pas la qualite d'un jeu package.

## STOP

Arreter les modifications de forme apres cette A/B/A. Ne pas grossir ou superposer les huit meshes
pontiques rejetes auparavant sans une preuve causale et visuelle distincte.
