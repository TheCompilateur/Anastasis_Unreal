---
name: anastasis-historicite
description: Modèle commun d'immersion pontique et rhômaïque post-1204 pour les missions terrain, eau, végétation, bâtiments, PNJ et joueur; sources, inférences et preuves.
---

# Historicité située

Lire `docs/historicity/MODELE.md` et `docs/historicity/SOURCES.md` avant toute revendication de réalisme géographique ou historique. Conserver `docs/visual/P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md` comme direction artistique; la cible choisie par Alexandre est une vallée fictive de l'arrière-pays, avec eau, sans localisation réelle imposée. Ce skill complète `anastasis-mission` et `anastasis-realisme`; il ne les remplace pas.

Pour une mission, sélectionner au maximum trois règles PONT, écrire un brief depuis `docs/historicity/BRIEF.template.json`, puis valider `python tools/historicity/check-brief.py <brief>`. Relier le brief à la fiche de passation. Le validateur dit seulement si l'affirmation est traçable; aucun PASS historique automatique.

Toujours séparer : fait attesté local, analogie régionale, hypothèse de conception, inconnu. Les captures de terrain montrent SCN; le déplacement, le travail et l'usage vécus exigent PLY. Ne jamais transformer une analogie moderne de végétation en certitude médiévale. Ne jamais placer une forme « byzantine » uniquement pour son symbole : expliciter site, fonction, matériaux, entretien et période.

S'arrêter au maillon causal possédé par la mission. Les agents bâtiment, PNJ et map gardent leurs fichiers et leur autonomie locale. Ce protocole fournit un vocabulaire et une frontière de preuve communs, pas une nouvelle autorité qui réécrit leurs travaux.
