# VILLAGE_WEATHER_001 — les habitants vivent sous le ciel qu'on voit

Mission `village-weather-001`, 2026-10-01. Suite de DAY_NIGHT_WEATHER_001 : le ciel montrait la
météo de la simulation, mais les habitants l'ignoraient — il pouvait pleuvoir à verse sur un
fermier qui cueillait comme par beau temps. La référence JS, elle, fait réagir ses habitants.

## Ce que fait la référence, et ce qui est porté

| Référence (`fee66ae`) | Ce que ça fait | Porté | Preuve |
|---|---|---|---|
| `readSimWeather` | la météo qu'un habitant lit, à l'heure (graine, jour, `dayFrac`) | oui | vecteurs |
| `weatherGoalBiasFromState` | pluie, neige, vent, saison teintent CHAQUE ligne de la table : sous la pluie moins d'exploration et de cueillette, plus d'atelier, de repos, de compagnie ; l'hiver la table et l'abri ; l'automne la récolte ; le printemps l'exploration | oui | vecteurs |
| `shouldSeekRainShelter`, `shelterRainScore` | sous l'orage (pluie ≥ 0,48), le but « s'abriter » pour qui est dehors à un travail exposé ou d'un métier d'extérieur | oui | vecteurs |
| porte d'orage de `commitGoalChoice` | lâcher un but exposé pour l'abri, retenir le travail à reprendre | oui | assemblage |
| `shelterRainAccess`, entrée (`buildingForIndoorAction`, `workplaceAcceptsIndoorGoal`) | foyer, poste, bâtiment couvert, sinon la place | oui (sans `bestKnownBed`, sans taverne) | assemblage |
| `shelterRainDuration`, `performShelterRain`, `updateInside` | 22 à 38 s à l'abri, récupération, énergie +14, moral +2, délai de grâce 18 s, reprise | oui (`craft` → `observer`) | formules + assemblage |
| `applyRainExposure` | dehors sous l'orage, l'énergie fond, la santé au-delà de 0,7 | oui | formules |
| bloc pluie de `movementSpeedFactor` | on presse le pas vers l'abri, on ralentit sinon | oui (le reste du facteur : non) | formules |

Les fonctions exportées sont prouvées **bit à bit** contre la référence exécutée
(`tools/migration/parity/weather-behavior.mjs`, 2 724 vecteurs) ; celles que la référence n'exporte
pas, contre leur formule transcrite (`Anastasis.Sim.MeteoHabitants.Formules`).

## Deux faits de la référence, reproduits tels quels

- **Le puits abrite.** `catalog.js` range le puits dans le groupe `civic`, et l'entrée pour
  s'abriter accepte tout bâtiment civique. Un grenier qui n'est pas son poste (groupe `food`), non.
- **Sous un vrai orage, la ligne d'abri gagne la table d'elle-même** (`shelterRainScore` + biais
  météo 16t + 22 ≈ 140 à pluie 0,9) : la porte d'orage n'a pas à intervenir, et la référence ne
  retient alors aucun but à reprendre — après l'abri elle passe à `craft` (non porté : `observer`),
  puis l'habitant redécide. La porte ne tranche que près du seuil (mesuré à pluie 0,5 : voir plus bas).

## Sans hôte, pas de météo

Les tests d'assemblage du village tournent sans l'hôte de simulation. Le village n'y lit **aucune**
météo (ciel d'été sec : chaque terme vaut exactement 0) ; il ne la lit que si l'hôte lui donne sa
graine (`SetWeatherSeed`, au `Reset`) ou si un test la force (`SetForcedWeather`, le `sim.forceWeather`
de la référence). Les preuves existantes du village restent donc au bit près.

## Pour voir

`Anastasis.Village.ForceWeather 0.9` en PIE impose un orage aux habitants (`off` rend la météo de la
simulation). Le ciel n'est pas forcé. Preuve scénarisée : `tools/unreal/village-weather-pie.py`.

## Preuves

Voir la fiche de passation `docs/unreal/handoffs/village-weather-001.md` (valeurs du dernier run).

## Ce qui reste

- Pas de pluie visible (aucun système Niagara du projet) ni de sol mouillé : le comportement réagit,
  l'image de la pluie reste le ciel gris.
- Écart n° 2 du village : un habitant qui a une cible la garde jusqu'à l'arrivée ; l'orage n'est donc
  pris en compte qu'à sa prochaine décision, pas au milieu d'un trajet.
- Les autres métiers d'extérieur de la référence (bûcheron, berger…) n'existent pas encore ici.
