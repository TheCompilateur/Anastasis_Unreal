# HANDOFF: worldmap-foothill-continuity-001

## MISSION

Essai borne de continuite geomorphologique entre le bord du terrain forge et
l'anneau de presentation de `Lvl_AnastasisSlice`. Base `main`
`ba327baa961918499756a95e660cf20b9f7d1f69`, branche
`agent/worldmap-foothill-continuity-001`. **REJECT** : le code experimental a
ete retire. Le seul livrable est ce compte rendu. L'audit plus large est dans
la mission `worldmap-forest-horizon-001` ; ces deux missions sont independantes.

## HYPOTHESE ET PATCH ESSAYE

La large plaine nue visible depuis H2/H3 pouvait venir de l'extrusion a plat du
Z au bord de la forge. L'essai lisait la pente normale de la derniere rangee
forgee, lissait ce signal le long du perimetre, puis le prolongeait sur une
distance decroissante et bornee dans l'anneau. La valeur au bord restait exacte ;
les bords humides etaient exclus. CVar A/B :
`anastasis.Terrain.HorizonFoothillContinuity 0/1`.

## PREUVE

- Build Editor Win64 Development : PASS pour le code d'essai.
- Capture du meme worktree, meme seed 12345, memes cinq cameras, meme profil
  d'atmosphere :
  `Saved/HorizonEvidence/worldmap-foothill-reference/` (CVar 0) et
  `Saved/HorizonEvidence/worldmap-foothill-continuity/` (CVar 1).
  Les dix PNG ont ete generes ; H2, H3 et H4 ont ete examines en A/B.
- `compare.py` : H2 4,09 % de pixels avec ecart >16/255, proche de la variance
  de capture de 3,6 % ; H3 **28,08 %** ; H4 0,65 %. Le changement H3 est donc
  reel, mais il forme une levee trop reguliere au premier plan qui masque une
  portion des montagnes. H4 montre aussi une trace plus marquee au bord nord.
  Le tres faible changement H2 ne resout pas l'avant-pays nu.
- Les mesures GPU editeur H2/H3/H4 de reference sont 22,32/18,58/27,09 ms
  p50, et 20,03/16,89/23,96 ms dans la variante. Ces runs non simultanes sur
  une machine chargee ne permettent pas d'attribuer une amelioration de
  performance au patch.

## DECISION

**REJECT**. `AnastasisTerrainHorizon.cpp` a ete restaure exactement depuis la
branche de base. Aucun changement de rendu n'est livre. La pente peripherique
locale n'est pas un signal suffisant pour definir le relief regional. La
correction systemique doit definir d'abord une topologie regionale et des
bassins coherents au-dela du rectangle simule, puis y raccorder materiaux,
couvert vegetal et eau. Un simple offset de hauteur sur l'anneau produit une
levee lisible a hauteur humaine.

## FILES_OWNED

- `docs/unreal/handoffs/worldmap-foothill-continuity-001.md` uniquement.

## PROOFS

PROOFS: (aucune)

Les captures A/B sont des preuves de scene locales, pas une preuve PIE du joueur.
Le `download.png` non suivi du canonique, les autres worktrees et les assets
Unreal restent hors perimetre.

## INTEGRATION_RISK

Aucun code ou asset Unreal a integrer. Cette fiche peut etre versee sans ordre
particulier ; elle complete l'audit `worldmap-forest-horizon-001`.
