# HANDOFF: shore-contact-006

## MISSION

Améliorer le raccord sol/eau du terrain 4 m de Claude, sur la base de terrain 82a337b
et la composition 249e36b. Travail isolé dans
C:/Users/alex_/.codex/worktrees/asset-map-002/ANASTASIS_UNREAL,
branche agent/shore-contact-006. Aucun merge/push ni modification d'AnastasisSim.
La forge livrée par Claude est inchangée depuis 82a337b ; sa passation 526b8c1 a
été lue : le remplacement du masque seul avait été essayé puis rejeté.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForge.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForgeTests.cpp
- tools/unreal/capture-shore-contact.py
- tools/unreal/capture-reed-form.ps1
- docs/unreal/handoffs/shore-contact-006.md

Les deux assets générés précédemment dans Content/Anastasis/EcotoneReview004/
restent locaux, hors commit. Les changements concurrents sont exclus.

## COMMIT

Voir git log agent/shore-contact-006. Unité de passation limitée aux cinq fichiers.

## PATCH

CVar opt-in : anastasis.Terrain.Forge.ShoreProfile 1, puis régénérer l'embodiment.
Défaut 0 : la présentation livrée reste disponible et sert de contrôle.

- Lissage des hauteurs rendues dans le voisinage 4 x 4 des tuiles terre/eau.
- Centres des tuiles et sommets de bord immuables ; déplacement vertical <= 100 cm.
- Contraintes de pente par arête : aucune arête auparavant <= 60 degrés ne peut
  dépasser ce seuil sur les cas validés. Repli sur les hauteurs originales si la
  projection ne converge pas dans sa limite d'itérations.
- Couverture d'eau calculée depuis les hauteurs finales, avec protection des centres
  secs de simulation déjà sous la nappe globale. Ces exceptions ne sont pas inondées.
- Normales et échantillonnage actif suivent le terrain final via la forge existante.

## MEC

BUILD final : PASS, exit 0, 87,14 s hors attente du verrou global.
Une compilation a été réessayée automatiquement par UBA sous pression mémoire puis
terminée avec succès ; la compilation n'est pas un verdict de performance.

Validation finale : **9 PASS / 0 KNOWN_EXPECTED_FAILURE / 0 FAIL**, run complet,
exit 0, filtre Anastasis.Terrain.Forge. Tests : Banks, CarriesMorphology, ChunkSeam,
Contract, NoCliffs, NoSpikes, NoStaircase, SampleHeight, ShoreProfile.

Le test ShoreProfile compare 0/1 sur trois graines ; les huit autres contrôlent
le réglage livré (option 0). Équerrage = longueur de contour dont la direction est
à moins de 10 degrés d'un axe, pondérée par la longueur des segments.

| Graine | Équerrage avant/après | Pente berge p90 avant/après | Fosses sèches avant/après |
|---|---|---|---|
| 12345 | 58,17 % / 21,08 % | 48,55 / 44,67 degrés | 303 / 1 |
| 54321 | 47,44 % / 28,53 % | 45,57 / 34,12 degrés | 343 / 7 |
| 42 | 49,65 % / 32,98 % | 51,50 / 35,44 degrés | 192 / 4 |

Sur les trois graines : zéro centre déplacé, zéro centre nouvellement inondé ou
asséché, zéro nouvelle arête >60 degrés, zéro déplacement hors bande, delta Z
maximal 100 cm. Les résidus de fosses sont exclusivement les centres secs protégés
à plus de 0,5 cm sous la nappe : ce ne sont pas des échecs masqués. Sur seed 12345,
deux centres sont protégés, dont un seulement dépasse ce seuil de profondeur.
Les écarts classe/hauteur antérieurs restent 8, 21 et 10 ; ils ne sont pas revendiqués
comme corrigés. Le test de couverture vérifie séparément l'eau réellement rendue.

La première hypothèse (profil catégoriel bicubique) a été rejetée : +1 235 arêtes
raides sur seed 12345 et équerrage accru malgré une meilleure pente au bord de l'eau.
Les logs sont conservés. Aucun échec nouveau n'a été ajouté au registre des échecs connus.

## SCN

**KEEP comme option de raccord local ; scène complète PARTIAL.**
Les huit images finales ont été inspectées. Les vues nues montrent la disparition
des grands angles triangulaires du site et un pied de versant plus continu. Les
vues avec plantes confirment le raccord à placements XY/yaw/échelle identiques.
Les caméras sont exactement celles du manifeste shore-composition-005a.

Contrôle dans l'éditeur après reconstruction : zéro changement de couverture
d'eau aux centres des tuiles, avant comme après. Support échantillonné des roseaux :
variation maximale 1,651 cm avant, 2,186 cm après. Huit PNG, hashes et manifeste
complets ; aucun asset ni niveau sauvegardé. Les trois hashes d'assets sont inchangés.
La première capture avait révélé deux centres inondés ailleurs dans la carte ;
la protection ajoutée est vérifiée dans les tests finaux ET les captures finales.

Le sol reste flou/tacheté, la nappe stylisée, les ombres fines granuleuses. Un petit
haut-fond reste visible ; horizon noir et repère orange d'éditeur sont hors périmètre.
Aucun photoréalisme ni scène finale revendiqué.

## PLY

UNKNOWN, PLAYER NOT_IMPLEMENTED dans cette base. Pas de PIE ni de preuve joueur.

## INTEGRATION_RISK

- Activation expérimentale explicite ; pas d'intégration canonique automatique.
- Les divergences préexistantes entre hauteur et classe terre/eau sont conservées,
  comptées séparément. La couverture finale ne doit pas changer l'état des centres.
- Validation ciblée sur des mondes 96 x 96, subdivision 4, graines 12345/54321/42.
  La suite ChunkSeam existante porte sur le réglage livré, pas sur l'option activée.
  Le lissage de voisinage sur d'autres découpes/subdivisions reste à examiner avant
  une activation générale ; les sommets de bord sont conservés.
- Pas de mesure GPU ni de budget de génération stabilisé ; autres éditeurs/builds
  actifs pendant les captures. La passe n'est pas une érosion conservative.
- Les roseaux gardent XY/yaw/échelle ; leur Z est recalé sur le sol de chaque variante.
- Les captures réutilisent un manifeste de composition local, identifié par SHA256.
- Portail finish du dépôt incompatible avec ce chemin de worktree géré : contrôles
  directs documentés, aucun seal revendiqué.

## PREUVES ET REPRODUCTION

Racine locale :
C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eab3-eaab-7610-8f6d-bb6dd660a518/

- shore-contact-006-build-c.log : build final.
- shore-contact-006-tests-c/ : automation.log + results.json, classification du dépôt.
- shore-contact-006-capture-b/ : capture.log, manifest.json, hashes, validation.json
  (empreintes des sources et contrôles) et huit PNG finaux.
- tests-a : hypothèse rejetée ; tests-b et capture-a : avant protection des centres.
  capture-a conserve les deux sources C++ exactes utilisées à cette étape.

```powershell
$env:ANASTASIS_SHORE_PLACEMENTS='<chemin absolu du manifeste shore-composition-005a>'
tools/unreal/capture-reed-form.ps1 -CaptureScript capture-shore-contact.py -OutDir <nouveau-dossier>
```

PNG : bare = avant sans plantes ; A = après sans plantes ; B = avant avec roseaux ;
C = après avec roseaux. Deux vues fixes, eye/context, même graine et lumière.

## STOP

Lot borné terminé et vérifié. Next : revue artistique de la berge et de ses
matières, puis décision d'intégration distincte. Ne pas généraliser les résultats
à toute l'hydrologie, aux chunks ou au jeu en mouvement.
