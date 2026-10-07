# Fertilite -> alimentation : protocole v1

Experience de simulation C++ uniquement. Aucun mecanisme de production change.

## Conditions figees avant execution

- Automation : `Anastasis.Sim.Village.Fertilite.AlimentationAB` et `PlafondTemoin`.
- Monde plat 32 x 32, un champ (8,11), stock initial 4, culture Grain.
- Graine 12345 ; debut jour 1 a t=37.8 ; 64800 ticks a 1/60, soit 12 jours de duree.
- Grenier (16,11), puits (16,18), sans maison ; 3 habitants, un fermier.
- La maison de la premiere fixture permettait la branche `eat` en interieur :
  la faim baissait sans portion prelevee (`Anastasis.Sim.Village.Granary` couvre ce
  comportement). Le premier run donnait +12 de repousse, +12 livraisons, 0 repas
  dans les deux bras : chaine non atteinte, pas un effet alimentaire positif.
  Retirer uniquement la maison laisse le grenier comme source du repas confirme.
- Bras A fertilite 0.75 ; B 1.30 ; repetition A2 0.75. Rien d'autre ne change.
- Vrai FAnastasisSimulation::Tick : meteo, decisions autonomes, horloge, repousse de minuit,
  recolte, transport et repas. Aucun besoin reinitialise, aucun objectif force, aucun stock injecte.
- Le champ unique est un choix de pression sur la ressource, pas une preuve prealable de penurie.

## Mesures et invariants

Chaque tick : conservation nourriture = champs + sacs + batiments + repas - repousse,
borne du champ [0,37], temps de champ vide/plein, personnes en faim critique,
temps du fermier avec un besoin critique. Un tick vaut 1/60 seconde de simulation ;
5400 ticks valent un jour. Le compteur de faim est en personnes-ticks.

Echantillons cumulatifs a chaque changement de jour et au dernier tick : repousse,
champ, sacs, grenier, recolte, livraisons et repas. Les lignes FERTILITY_AB_SAMPLE
restent dans le rapport d'automation. Le changement de jour inclut deja sa repousse ;
ne pas interpreter ces lignes comme un bilan avant minuit. Premier et dernier jours partiels.
A/A2 doivent avoir les memes lignes numeriques et le meme digest final.

Le controle PlafondTemoin exige +4 contre +7 pour un stock initial de 1 et une
repousse de base 5 ; a 36, les deux font +1 ; a 37, les deux font +0.

## Decision predeclaree

Un echec de conservation, de fixture, de bornes ou de repetition invalide l'instrument
(et echoue le test). Le succes du test d'automation signifie seulement instrument valide.
Le resultat economique distinct est porte par FERTILITY_AB_RESULT effect= :

- UNKNOWN_CHAIN_NOT_REACHED : recolte, livraison ou repas absent dans au moins un bras.
- NO_POSITIVE_REGROWTH_CONTRAST : aucune augmentation nette de repousse.
- REGROWTH_WITHOUT_MORE_DELIVERY : plus de repousse, pas plus de livraison.
- DELIVERY_WITHOUT_MORE_MEALS : plus de repousse et de livraison, pas plus de repas.
- MORE_REGROWTH_DELIVERY_AND_MEALS : les trois compteurs augmentent.

Les deltas sont signes, y compris la faim : un effet negatif reste visible.
KEEP de l'hypothese complete seulement dans le dernier cas, pour CETTE fixture et
CETTE duree. Sinon, hypothese non demontree ici ; examiner champ plein/vide et
besoins du fermier avant une nouvelle intervention. Ces diagnostics n'identifient
pas a eux seuls une cause. Aucun seuil ajuste apres lecture pour obtenir un succes.

## Frontiere de preuve

Ce n'est ni une preuve de la carte jouee, ni de navigation physique, ni une mesure
de fertilite pedologique, ni une demonstration generale sur plusieurs graines/saisons.
L'experience couvre seulement le printemps et le sous-ensemble de simulation porte.
La suite Anastasis du lot execute ces tests ; aucune nouvelle preuve PIE ni editeur autonome.
Run cible local du 2026-10-07 apres retrait de la maison : 2 PASS, 0 FAIL ;
A/A2 identiques, A = 16 repousse / 12 livraisons / 12 repas, B = 28 / 18 / 18.
`effect=MORE_REGROWTH_DELIVERY_AND_MEALS`, deltas +12 / +6 / +6.
La faim critique augmente toutefois de 618 personnes-ticks dans B (3150 -> 3768) ;
le mecanisme qui produit cette difference n'est pas isole. Le fermier accumule
30136 vs 32537 ticks avec un besoin critique. Ce resultat ne prouve pas une
amelioration de la sante ou de la survie du village. Le lot doit rejouer la
suite sur son propre arbre avant tout claim d'integration.
