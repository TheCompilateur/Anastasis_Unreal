# Modèle de monde : arrière-pays pontique après 1204

**Autorité de conception, pas reconstitution d'un site réel.** Cible : une vallée fictive de l'arrière-pays de Trébizonde, dans les premières décennies suivant 1204 (cible de travail 1204-1225). Le joueur doit lire un monde rhômaïque par ses usages et contraintes, sans carte GPS, costume générique ou monument obligatoire. « Pontique » est une famille de milieux; la date ne transforme pas chaque objet en fait attesté.

## Chaîne causale commune

`relief et exposition -> ruissellement, sources, rivière -> sols et végétation -> passages et franchissements -> accès à l'eau et terres cultivables -> implantation -> travail, stockage, entretien -> bâtiments et traces d'usage -> expérience du joueur`

Chaque agent choisit un maillon et doit montrer l'effet sur le suivant. Une texture, un bâtiment ou une réplique isolés ne suffisent pas. Le monde est fictif : ni côte visible ni correspondance avec un village réel requises. L'eau doit rester présente et opératoire, mais sa présence ne prouve ni potabilité ni accès PNJ.

## Contrat spatial

| ID | Décision de conception | Conséquence visible | Contre-exemple à refuser | Preuve minimale |
|---|---|---|---|---|
| PONT-GEO-01 | Vallée intérieure drainée, relief et eau en système | Affluents rejoignent un cours principal; fonds, pentes et crêtes se lisent ensemble | Lacs et rivières distribués comme ornements | Carte hydro + vue aérienne cohérentes, source/écoulement/issue explicités ou UNKNOWN |
| PONT-GEO-02 | L'eau contraint l'usage humain | Gué, rive praticable, chemin et site d'habitat répondent au terrain | Bâtiment ou chemin posé au plus joli cadrage | Vue à hauteur humaine + trajet réel vers l'eau si un PNJ/joueur est revendiqué |
| PONT-ECO-01 | Végétation dépend d'humidité, pente et exposition | Rive, fond, versant et crête ont des communautés distinctes | Même semis partout ou sécheresse méditerranéenne par défaut | Quatre poses fixes, inventaire spatial et coût GPU si densité changée |
| PONT-HIS-01 | Matériaux et formes viennent d'un usage situé | Réparations, drainage, stockage, accès et usure ont une cause | « Byzantine » réduit à coupoles, aigles et ruines | Source datée/locale pour forme spécifique; scène montrant fonction et entretien |
| PONT-HIS-02 | 1204 est un contexte politique, pas une décoration | Choix sociaux et mémoire de rupture seulement si scénario/simulation les portent | Tous les villageois deviennent réfugiés par supposition | Scène/dialogue et état de simulation; précision chronologique vérifiée |
| PONT-PLY-01 | L'immersion est éprouvée par le joueur | Le joueur peut percevoir, atteindre et utiliser un lieu crédible | Build, capture fixe ou PIE automatisé déclaré « immersion » | Parcours humain documenté, interaction observée, limites nommées |

Ces lignes sont des **règles de conception**. Elles ne prétendent pas que l'on connaît la configuration exacte des vallées ou villages de 1204. `SOURCES.md` précise où s'arrête chaque autorité.

## Brancher une mission

1. Lire `AGENTS.md`, les skills de mission/réalisme pertinents, puis ce modèle et `SOURCES.md`.
2. Choisir au maximum trois ID. Décrire `CURRENT -> TARGET -> GAP`, un effet causal observable et le fichier propriétaire. Ne pas ouvrir un second chantier par découverte adjacente.
3. Copier `BRIEF.template.json` en `docs/historicity/briefs/<mission>.json` seulement si la mission revendique une amélioration historique ou géographique. Renseigner sources, inférence et preuve attendue. Lancer `python tools/historicity/check-brief.py <brief>` avant passation.
4. Lier ce brief dans la fiche de handoff. `check-brief.py` vérifie la traçabilité du dossier, pas la vérité d'une hypothèse ni la qualité visuelle. Le verdict reste MEC / SCN / PLY séparé.
5. Pour une nouvelle source, ajouter son périmètre et sa limite à `SOURCES.md` puis l'ID au registre du vérificateur. Ne pas appliquer à l'arrière-pays un indice issu d'une autre époque ou région sans le marquer `analogie`.

## Point de décision pour les agents actuels

Terrain : vérifier la continuité bassin/rivière/franchissements avant d'ajouter du détail. Bâtiments : relier un type à son sol, matériau, fonction et accès. PNJ : relier déplacement, besoins et travail à ces lieux; une capture ne remplace pas un trajet. Aucune de ces trois missions ne doit être réécrite par ce document; chacune peut joindre son propre brief à la passation.

## Portée actuelle

Les six images fournies par Alexandre le 2026-10-07 montrent réseau d'eau, plaine ouverte, crête lointaine, végétation clairsemée. Elles fondent un diagnostic visuel, pas une mesure de carte, un inventaire des espèces ou une preuve historique. La sélection de site et la concordance eau/simulation disposent déjà d'outils dans `docs/unreal/handoffs/geography-concordance-001.md`; les réutiliser. La direction artistique existante est `docs/visual/P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md`. Un conflit entre celle-ci et une option intérieure plus sèche est une décision de biome à expliciter, jamais à résoudre en douce.

## Épreuve de revue à hauteur de joueur

Choisir **une** traversée de 60 à 90 secondes : départ sur versant ou lisière, descente vers un passage praticable, arrivée à l'eau, puis vue sur un site de travail ou d'habitat. À chaque arrêt, demander :

1. Sans panneau explicatif, peut-on déduire où va l'eau et pourquoi le chemin passe ici ?
2. Le sol, la végétation et les matériaux changent-ils pour une raison lisible, ou seulement pour varier l'image ?
3. Quelle activité humaine a laissé cette trace, et quel besoin ou risque l'explique ?
4. Quel indice est spécifiquement compatible avec le début du XIIIe siècle pontique, quelle source le soutient, et quelle part reste une invention plausible ?
5. Le joueur peut-il agir sur au moins un de ces liens (accéder à l'eau, franchir, travailler, s'abriter), plutôt que le regarder en caméra fixe ?

Conserver les cinq réponses avec captures aériennes et à 1,7 m, état runtime et fichier de sources. Inviter un byzantiniste à critiquer les **inférences**, la chronologie et les anachronismes; une émotion esthétique positive n'est ni une mesure historique ni un PASS automatique. Rejeter une scène si elle produit un signe chronologique spectaculaire mais causalement gratuit.
