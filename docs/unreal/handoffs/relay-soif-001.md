# relay-soif-001

RELAIS: soif-dabord-001

Relais de soif-dabord-001 (agent fini, branche en conflit avec main après faim-champs-001 et ecarts-repair-001) :
« quand on meurt de soif, on boit d'abord ; boire assoiffé soulage » (écart n° 58, `anastasis.Village.ThirstFirst`,
défaut 1). Fiche d'origine : `docs/unreal/handoffs/soif-dabord-001.md` (reprise telle quelle par le cherry-pick).

## Résolution

- `AnastasisVillage.h`, `AnastasisVillageStateDigest.cpp`, `AnastasisSimulationSubsystem.cpp` : les deux côtés gardés
  (faim-champs n° 59 puis soif-dabord n° 58 : drapeaux, accesseurs, CVar, application à l'hôte, lecture d'état).
- `ECARTS.md` : registre de main + fiche n° 58 entière (ordre des numéros, pas d'union). `ECARTS::PASS fiches=53 fail=0`.
- `SaveFormatVersion` : 8. faim-champs-001 (dans main) et soif-dabord-001 étaient tous deux passés de 6 à 7 avec des
  parcours d'état différents ; le relais, qui lit les deux, prend 8.
- `STATE_FIELDS::PASS`.

PROOFS: save-load-pie, npc-life-pie, chronicle-pie

## ECARTS

n° 58 ouvert (repris de soif-dabord-001, inchangé).

## INTEGRATION_RISK

Sauvegardes écrites en version 7 refusées (changement de format attendu).