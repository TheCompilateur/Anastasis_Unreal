# RIVER_USE_001 — une riviere utile a un habitant

## Question

Un habitant assoiffe rejoint-il de lui-meme une berge physiquement exposee de la
riviere rendue, y boit-il, et sa soif baisse-t-elle ? Un seul site, un PNJ, aucun puits.
Ce chantier prepare une experience discriminante ; il ne corrige pas la simulation
sans observation du premier maillon en echec.

## Etat et lien avec geography-concordance-001

A la preparation, geography-concordance-001 est encore queued (f5e6fe6).
Aucun desaccord reel n'est donc presume. Faire passer son acquisition en premier au
lot ; elle peut ensuite orienter le prochain site. Le pilote courant est preregistre :
berge rendue la plus proche qui satisfait les conditions ci-dessous, pres du village
calcule par SettlementSite. Le choix ne filtre pas les berges selon leur reconnaissance
semantique, leur acces par le graphe, ni le succes d'un PNJ.
Le pilote compile independamment de geography-concordance-001. Si sa version du rapport
est integree, water_concordance est conserve integralement dans context.site.

## Controle de l'experience

- Monde PIE uniquement ; Begin refuse un village contenant habitants ou batiments.
- StartVillagers=0 avant PIE. Aucun reset ni suppression d'un village existant.
- Naissance explicite d'un PNJ : soif 80, faim 10, energie 80, vitesse 4 tuiles/s.
- Aucune affectation de but/cible/chemin, aucune incarnation joueur, aucun appel Perform.
- Terrain reel du monde PIE : section 0 sol, 1 nappe, 2 rubans. Un composant unique visible.
- Reference spatiale : site choisi par SettlementSite, echelle lue dans son releve.
- Recherche de berge dans 300 m : transition sec/mouille en coupes tous les 2 m,
  jusqu'a 24 m de chaque bord de ruban, pente seche <=18 degres. Les rubans debordent
  sous terre : leur bord de triangle n'est PAS une rive. L'eau temoin doit etre au-dessus
  du sol et ne pas etre couverte par un lac.
- Depart a 40–120 m de cette berge, sur une tuile praticable, seche, pente <=18 degres,
  non buvable dans la simulation. Pas d'essai sur une autre berge apres un echec.
- TimeScale=0 pendant la preparation, puis 0.25 ; Speed=1, Warp=1. CVars restaurees.

## Mesures

Chaque pas observe conserve temps simule, identite du PNJ/monde, position, but,
activite, cible, source de decision et sa date, chemin planifie, echec navigation,
compteur de boissons, soif, terrain sec/pente, distance au point d'eau expose temoin.
La trajectoire observee se distingue du chemin planifie.
Les segments entre observations sont controles tous les 0.5 m sur le terrain rendu ;
ce sont des cordes echantillonnees, pas une reconstitution de chaque sous-pas interne.

## KEEP / REJECT

PASS local exige ensemble :

1. PNJ autonome unique, aucun batiment pendant le scenario.
2. Intention drink avec cible et source shore observee.
3. Au moins 20 m parcourus et 20 m de deplacement depuis le depart.
4. Pas de sol inconnu, inonde ou de pente >18 degres sur la trajectoire echantillonnee.
5. Compteur DrinksTaken accru ET soif reduite d'au moins 10 points.
6. Au moment du constat : proximite de l'eau exposee temoin <=10 m et de la berge
   choisie <=20 m, en ajoutant l'incertitude de deplacement depuis le precedent
   echantillon (5.2 tuiles/s * echelle * dt, borne 4 * 1.3 pour la meteo).
7. Empreinte SHA256 des trois maillages identique au debut et a la fin.

Un puits, un autre habitant, une incarnation ou une consommation ailleurs => FAIL.
Une lacune temporelle >0.05 s simulee, saut de position non explicable par la borne,
absence de sol, changement de geometrie ou absence de consommation apres 60 s
simulees => UNKNOWN. Le registre n'accepte que PASS ; UNKNOWN bloque la preuve.
Les seuils sont ceux de ce protocole, pas des normes universelles de buvabilite.

## Execution et preuves brutes

`tools/unreal/editor-batch.ps1 -Proofs river-use-pie` dans la file commune uniquement.
`tools/unreal/editor-batch.ps1 -Proofs river-use-pie -DryRun` prepare la configuration
sans editeur. Le lot d'integration rejoue la preuve declaree dans la passation.

Sorties : Saved/RiverUseEvidence/<time_ns>/journey.json et geometry.json.
Le premier contient le contexte, la selection, toutes les observations, le verdict et
les limites ; le second les sommets/metres et triangles des trois sections. Chemin
exact logue avec le verdict. Aucun asset sauvegarde. Les hauteurs sont celles des maillages CPU ; displacement materiau/vagues et pixels
ne sont pas observes. Les snapshots sont des mesures,
pas des captures d'ecran ni une preuve joueur.

Tests hors editeur : PYTHONPATH=Content/Python, puis
`python -B -m unittest discover -s Content/Python/tests -p test_river_use.py -v`.

## Suite conditionnelle

- Reconnaissance non observee : examiner la cible et Type/Shore au voisinage de la berge.
- Intention observee, trajet bloque : examiner echec de chemin et ecart relief/navigation.
- Boisson loin de l'eau exposee : comparer la source semantique et la geometrie locale.
- Trajet complet : cloturer ce pilote sans modifier les regles, puis envisager les sols.

Ces categories orientent l'enquete ; un timeout ne prouve pas une cause. Aucun fichier
AnastasisSim n'est modifie, aucun ecart de parite n'est introduit. SCN local != PLY.
