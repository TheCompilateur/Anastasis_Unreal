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
| `pontique-camp-installation-rive.png` | ambiance camp établi en rive — feu commun, icône, séchage du poisson, quai |
| `pontique-lisiere-defrichement-pcg.png` | logique de lisière forêt → coupe → souches → clairière → camp naissant, cible PCG |
| `pontique-hydrologie-systeme-eau.png` | coupe hydrologique — eau profonde/peu profonde, rive saturée, vase, roseaux, zone inondable |
| `pontique-tier0-jour3-arrivee.png` | Tier 0 — jour 3, débarquement, bagages sauvés, icône, aucun bâtiment permanent |
| `pontique-tier1-jour30-implantation.png` | Tier 1 — jour 30, évolution J0→J30 du site, premiers abris et organisation |
| `pontique-tier2-an1-village-rhomaios.png` | Tier 2 — an 1, premier village avec sanctuaire, habitat, artisanat, berges |
| `pontique-grammaire-architecturale-batiment.png` | grammaire de génération de bâtiment modulaire (abri → noyau → extension → maison améliorée) |
| `pontique-props-materiel-refugies.png` | bibliothèque de props — céramiques, textiles, outils, objets sacrés, mobilier léger |
| `pontique-usure-materiaux-vieillissement.png` | vieillissement procédural par matériau (bois/pierre/torchis/textile/métal/tuile/corde) sur un même lieu |
| `pontique-synthese-lumiere-meteo-canonique.png` | planche de synthèse — récapitule tiers/grammaire/props/usure, ajoute 6 états lumière/météo canoniques sur une même géométrie |
| `pontique-production-frame-test-integration.png` | frame de production Unreal 5.8.2 (Lumen/Nanite/Foliage/World Partition ON) — scène neutre pour valider terrain+forêt+eau+bâtiments+PNJ avant tout habillage spectaculaire |

Lien avec le travail en cours : `pontique-anatomie-du-sol-materiau.png` décrit exactement
la transition que `AnastasisTerrainSurface` approxime aujourd'hui avec un seul degré de
liberté — la bande `Shore` entre la terre et l'eau. Le reste du gradient reste à faire et
n'est pas planifié.

Les onze planches ajoutées le 2026-09-12 (`pontique-camp-installation-rive.png` à
`pontique-production-frame-test-integration.png`) forment un même paquet de direction
artistique : PCG de lisière, water system, progression Tier 0/1/2, grammaire de bâtiment,
props et vieillissement de matériaux. Ce sont des cibles visuelles et une bibliothèque
d'intention, pas des specs d'implémentation ni un mandat pour démarrer la génération de
bâtiments, les PNJ ou un nouveau système de matériaux. Un doublon exact de
`pontique-tier0-jour3-arrivee.png` (`ChatGPT Image 12 sept. 2026, 13_07_07.png`, même
hachage) a été supprimé plutôt que classé une deuxième fois.

Voir aussi `docs/visual/P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md`.
