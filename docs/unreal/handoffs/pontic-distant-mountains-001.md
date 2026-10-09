# HANDOFF: pontic-distant-mountains-001

## MISSION

Créer huit véritables modèles 3D de montagnes à partir des huit familles de la
planche `docs/visual/reference/pontic-mountain-assets.png`. Cette planche est un
brief d'images, sans géométrie, heightmaps ni textures sources séparées.

## FILES_OWNED

- `Content/Anastasis/PonticMountains/` : huit StaticMesh et un Material (Git LFS)
- `tools/unreal/create-pontic-mountains.py/.ps1` : générateur déterministe UE 5.8.2
- `docs/visual/reference/pontic-mountain-assets.png` : copie identique de la planche
- `AGENTS.md` : index du générateur
- `.claude/skills/anastasis-realisme/fiches/terrain.md` : statut visuel des assets
- cette fiche

## COMMIT

Branche `agent/pontic-distant-mountains-001` ; consulter son `HEAD` pour le SHA.
Passation **assets seulement** : aucun changement au rendu de la carte.

## MEC

- Huit volumes fermés, chacun avec silhouette régionale propre et, selon le cas,
  canopées 3D intégrées. De 6 164 à 22 424 sommets et de 12 148 à 39 248
  triangles ; contrôle hors Unreal : `MESH_TOPOLOGY::PASS count=8`, chaque
  arête partagée par exactement deux triangles.
- Le générateur UE a sauvegardé les huit `.uasset` et le matériau :
  `PONTIC_MOUNTAINS::PASS count=8`. Les hauteurs des reliefs vont de 500 à
  2 100 m. Le matériau est conservé lors d'une régénération, car son nettoyage
  après chargement déclenchait l'assertion UE `!IsRooted`.
- Compilation UE : `BUILD::PASS` durant les essais visuels. Le livrable final
  sans les edits C++ rejetés doit être rejoué au portail `finish`.
- Syntaxe du générateur : `PYTHON_SYNTAX::PASS`. Index `tools/unreal/` :
  `MISSING={}`, `STALE={}`. `.uasset` et PNG : attribut `filter=lfs`.
- Suite sans rendu finale : UNKNOWN avant `finish`.

## PROOFS

PROOFS: (aucune)

## SCN

**UNKNOWN pour ce livrable :** les assets ne sont pas placés dans la carte.
Cinq essais expérimentaux d'intégration ont été capturés OFF/ON/OFF, puis
rejetés visuellement et retirés du code : superposer les meshes donnait des
collines lisses aux raccords nets ; une élévation locale de l'anneau produisait
des cônes et une pyramide neigeuse ; la correction limitée de roche/neige
restait quasi indiscernable. Le dernier ON/OFF touchait 3,01 à 3,37 % des
pixels selon les trois vues, avec 2,05 à 3,20 % de variance OFF/OFF2. Ces
captures instrumentales ne constituent pas une preuve de photoréalisme.

## PLY

UNKNOWN : aucun jeu packagé ni parcours joueur pour ces assets non placés.

## ECARTS

AUCUN — `Source/AnastasisSim/` non touché.

## INTEGRATION_RISK

- Cette mission ajoute des modèles dans Content mais ne transforme pas encore
  l'horizon visible. Ne pas présenter leur présence comme un PASS visuel.
- Les huit `.uasset` et la copie PNG passent par Git LFS.
- La prochaine mission visuelle devra étudier l'anneau 3D existant, ses
  matériaux, des références de terrain/DEM et des captures A/B/A à mêmes
  caméra, graine et météo. Grossir les mêmes bosses est une voie rejetée.

## STOP

Garder les modèles comme sources d'auteur. Ne pas intégrer la pose expérimentale
rejetée ni déclarer le photoréalisme accompli.
