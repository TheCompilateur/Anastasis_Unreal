# Planches de référence visuelle — monde pontique

Références de direction artistique produites hors moteur. **Ce ne sont pas des specs
d'implémentation** : rien ici n'autorise à démarrer la forêt, le PCG ou les bâtiments.
Elles sont classées ici pour ne plus traîner à la racine du projet, et parce qu'elles
documentent l'intention visuelle que la tranche 32×32 commence à approcher.

| Planche | Sujet |
|---|---|
| `pontique-vallee-lac-ambiance.png` | ambiance générale — vallée, lac, brume, relief boisé |
| `pontique-anatomie-du-sol-materiau.png` | gradient de sol terre sèche → humus → boue → saturation → rive → eau peu profonde |
| `pontique-foret-ecologie-verticale.png` | strates forestières, écologie verticale, cibles PCG |

Lien avec le travail en cours : `pontique-anatomie-du-sol-materiau.png` décrit exactement
la transition que `AnastasisTerrainSurface` approxime aujourd'hui avec un seul degré de
liberté — la bande `Shore` entre la terre et l'eau. Le reste du gradient reste à faire et
n'est pas planifié.

Voir aussi `docs/visual/P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md`.
