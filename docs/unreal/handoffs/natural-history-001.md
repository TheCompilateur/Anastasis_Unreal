# HANDOFF: natural-history-001

## MISSION

Le monde avant les hommes : naturaliser la distribution existante, sans nouvelle topographie,
hydrologie, simulation, assets ou framework. Base : 506f6db49aea640d4f0a8f96373b2e84d8853590.
Branche agent/natural-history-001 ; propriétaire : cette mission Codex.

État livré : source finale compilée, SCN final UNKNOWN, TESTS QUEUED. Les captures V1/V2
ont conduit à rejeter la réduction excessive du couvert. Aucun KEEP artistique anticipé.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisForestStructure.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisForestStructure.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCover.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCover.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCoverTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcology.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcology.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcologyTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp
- tools/unreal/ground-cover-capture.py
- tools/unreal/proofs.txt
- AGENTS.md
- .claude/skills/anastasis-capture/SKILL.md
- .claude/skills/anastasis-realisme/fiches/vegetation.md
- docs/unreal/handoffs/natural-history-001.md

## COMMIT

Voir le commit qui porte cette fiche (ne pas recopier un SHA autoréférent).

## Lecture du territoire et décision

CONFIRMÉ dans le code : terrain procédural forgé, pente échantillonnable, nappe rendue,
champ RiparianAt, couronnes instanciées, masques d'ouverture, réserves humaines et clairières.
La nappe hors eau peut être artificiellement située sous le sol : sa proximité verticale seule
ne prouve pas une plaine alluviale. Ni texture pédologique mesurée, ni histoire de perturbation.

Les captures archivées micro-ecologie-001 ont été regardées : vaste plaine ouverte traversée
par le réseau, versants boisés, arbres de galerie ; au sol, prairie assez similaire près de
la rive et de la lisière. Ces archives ne prouvent pas le rendu du commit présent.

Défaut causal confirmé : MeadowPocket choisissait humide/sec/pierreux par bruit seul.
Le lot rattache ces états à l'humidité riveraine et à la pente, puis partage ce choix avec
la densité de l'herbe. Le bruit dessine les poches à l'intérieur des habitats éligibles.

| Communauté | Pourquoi ici | Modification |
|---|---|---|
| Prairie ouverte | ouverture existante, faible signal humide | stature et densité normale conservées ; éclaircies modérées seulement dans les poches éligibles |
| Prairie humide / rive | signal riverain, proximité verticale immédiate de l'eau | mélange progressif de laîches ; poches humides ; berges basses végétalisées ou vaseuses |
| Ourlet | proximité des couronnes existantes | herbe haute relative au pré ; arbustes externes et jeunes sujets internes selon la couronne la plus proche |
| Sous-bois | couvert existant et humidité | plaques herbacées conservées ; pas de scolopendre sans signal humide ; jeunes sujets réservés aux ouvertures relatives |
| Versant exposé | pente rendue, crêtes existantes | couverture plus lâche ; roche en poches seulement sur pente sèche ; grands peuplements âgés moins probables |

PLAUSIBLE : contraste de communautés avec eau, ombre et exposition.
APPROXIMATION VISUELLE : seuils, âges, perturbations, distance à la couronne comme proxy de lumière.
Aucune correspondance taxonomique ou historique exacte revendiquée. La liste des espèces disponibles et la sélection des essences arborées
restent intactes ; les proportions des familles herbacées changent avec le site. Les terrasses, confluences et dépressions ne reçoivent pas d'étiquette inventée :
ce premier lot ne les distingue que par les signaux déjà disponibles.

Repères généraux, pas calibration locale : [Winward 2000, lecture transversale des communautés riveraines](https://research.fs.usda.gov/treesearch/5452),
[Heithecker et Halpern 2007, gradients de microclimat de lisière](https://research.fs.usda.gov/treesearch/33105).
Ces études nord-américaines ne valident ni les espèces ni les seuils choisis ici.

## MEC

- BUILD: PASS, compilation initiale, v1 (74.47 s), v2 (45.01 s), repli final (69.28 s).
- TESTS: QUEUED — deux nouveaux tests compilés, exécution attendue au lot.
- Python AST: PASS ; git diff --check: PASS ; index tools/unreal: Missing=[], Stale=[].
- Tests ciblés : Anastasis.GroundCover.NaturalHistoryCounterfactual ; Anastasis.MicroEcology.NaturalHistoryHabitat.
- Le premier oppose uniquement humidité sèche/humide et contrôle déterminisme, budget relatif,
  rejet sous eau ; le second refuse de créer poches humides ou rocheuses sur un plat sec.
- Commande : tools/unreal/anastasis-unreal.ps1 build

## PROOFS

PROOFS: natural-history-capture

Ce script constitue une preuve SCN dans l'éditeur, pas une preuve de déplacement PIE.
Commande : tools/unreal/editor-batch.ps1 -Proofs natural-history-capture
États : reference,natural,reference2 ; ciel 11 h ; graine 12345 ; caméras figées au premier état.
Vues : prairie_eye,lisiere_eye,sousbois_eye,riviere_eye,lande_eye,aerien.
Sortie : Saved/NaturalHistoryEvidence/ ; GPU p50 par vue, habitat.json, images et cameras.json.
Le PASS automatique exige seulement les captures complètes, les hauteurs échantillonnées
identiques et le retour à l'inventaire spatial échantillonné de référence. Le jugement artistique reste humain.

## SCN

V1 REJECT artistique : prairie devenue trop rase et uniforme. 1 303 282 herbes de référence
contre 695 260 en v1, dont herbes hautes 690 657 -> 104 570. Cette baisse ne prouve aucun gain artistique.
18 captures conservées dans Saved/NaturalHistoryEvidence-v1-rejected/.
Terrain : 529 hauteurs échantillonnées identiques dans les trois états (SHA256
08f5acbe66905c261ed674d80c5f70989cc784676bccd0487e9bfd064edf361a).
V1 contrôle FAIL : comparaison incorrecte par noms UObject (suffixes renommés, 1375 différences
mais totaux reference/reference2 = 1 421 665). Instrument corrigé pour v2 : liste triée
mesh + compte + positions première/médiane/dernière par composant ; pas une preuve de chaque instance.
Sortie éditeur v1 : EXCEPTION_ACCESS_VIOLATION après fermeture, donc aucune sortie propre revendiquée.
MicroEcology atteint déjà sa limite stochastique en référence et en v1 (truncated=1) ; reste une limite.

V2 capturée en direct, 18/18 images regardées. Commande :
`tools/unreal/capture-ground-cover.ps1 -Label natural-history-v2 -States 'reference,natural,reference2' -TimeoutSec 600`
avec ANASTASIS_GROUND_VIEWS fixé aux six vues et ANASTASIS_EDITOR_MAX=1.
Dossier : Saved/GroundCoverEvidence/natural-history-v2/ (essai rejeté, pas la source finale).
Raw : capture.log, habitat.json, ground-cover.json, cameras.json, six triplets PNG.
Contrôle de capture PASS à 06:13:46 UTC : 529 hauteurs identiques, inventaire spatial de retour
identique ; total HISM 1 421 665 -> 958 785 -> 1 421 665. Couvert herbacé V2 : 840 498,
herbes hautes 334 803, sous-bois 34 806. Ces nombres ne prouvent aucune amélioration.
Fermeture : crash EXCEPTION_ACCESS_VIOLATION à 06:13:53 UTC après COMPLETE ; aucun éditeur
restant sur le worktree. Aucune stabilité de fermeture revendiquée.

Verdict V2 : prairie principale conservée ; lisière et premier plan de rive trop dénudés.
Sous-bois et peuplements modifiés, lecture aérienne seulement discrète. REJECT de la réduction
agressive de couverture, malgré le PASS instrumental. Les poses lisiere_eye et lande_eye sont
respectivement une approche de crête arborée et un versant boisé raide : pas une traversée exhaustive.

Mesure par `.claude/skills/anastasis-capture/compare.py A.png B.png` :

| Vue | Pixels >16/255 A/B V2 | A/A retour témoin | GPU p50 ref / V2 / retour, ms |
|---|---:|---:|---|
| prairie_eye | 6.88% | 2.89% | 12.65 / 12.70 / 13.16 |
| lisiere_eye | 18.09% | 1.98% | 11.79 / 9.46 / 12.19 |
| sousbois_eye | 18.31% | 9.77% | 16.68 / 15.21 / 16.74 |
| riviere_eye | 23.06% | 8.81% | 14.04 / 12.80 / 14.21 |
| lande_eye | 13.42% | 7.92% | 13.01 / 12.91 / 13.17 |
| aerien | 11.02% | 0.13% | 10.04 / 10.09 / 10.43 |

Comparer à chaque témoin mesuré, pas au ~3.6% générique imprimé par le script.
Ce sont des différences, pas des scores de qualité. GPU du viewport éditeur, pas jeu 1080p,
ni preuve du coût du correctif final. Render-ms proche de zéro : ne pas l'interpréter.

Repli final, compilé après cette capture : retrait complet de la modulation large de vigueur
par bruit, du facteur de taille et de la diminution de probabilité des herbes hautes.
Densité normale conservée ; seulement Keep=0.65 sol nu, 0.60 pierreux, 0.80 sec dans les poches
écologiquement éligibles, et éclaircie modérée des pentes très raides. Eau, lisière, régénération
et maturité conditionnelles conservées. Les tests ne forcent pas un site sec sans poche à s'éclaircir.
SCN de ce repli : UNKNOWN, à rejouer par natural-history-capture au lot. Pas de troisième éditeur
local, accord du coordinateur de la croisade ; aucun KEEP artistique de la source finale.

## PLY

UNKNOWN — aucune traversée interactive, aucun gain de navigation revendiqué.
Les réserves humaines existantes, la collision et les maxima d'instances ne sont pas modifiés.
Interface sol : UV0.y (litière) reste piloté par la tuile Forest, pas par chaque couronne ;
TrunkContact demeure une couche distincte. Ce lot ne réécrit pas les UV.

## ECARTS

AUCUN — aucun fichier Source/AnastasisSim modifié ; règles de présentation uniquement.

## INTEGRATION_RISK

Fichiers chauds : WorldEmbodiment.cpp, proofs.txt, AGENTS.md, ground-cover-capture.py.
Conserver le chargeur ANASTASIS_GROUND_CAMERAS du coordinateur lors de la résolution du script
et les filtres de vues compatibles. Aucun rebase local avant la fin de nos comparaisons. Ne pas écraser les travaux concurrents.
Aucun commit/integration/push des autres missions. La racine canonique reste intacte, notamment
.claude/settings.local.json et download.png, hors périmètre.
GPU et verdict visuel de la source finale à compléter au lot. Le budget des herbes n'est pas augmenté,
mais les changements de familles et de micro-berges peuvent modifier le coût réel.

## STOP

Interrupteur réversible : anastasis.Dressing.NaturalHistory 0, puis réincarner.
Pas de nouvelle flore, pas de routes/champs, pas de terrain remanié, pas de paysage rempli.
Pas de reconstruction botanique de Nicée ; pas de simulation de succession.
Ne pas confondre build, captures fixes, qualité écologique et preuve joueur.
S'arrêter au finish queued ; intégration réservée à l'intégrateur.
