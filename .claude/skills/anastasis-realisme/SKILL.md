---
name: anastasis-realisme
description: Réalisme du rendu ANÁSTASIS dans Unreal 5.8 — comment le moteur fait la lumière, l'atmosphère, le sol, la végétation, l'eau et le post-traitement, ce que le projet en a déjà décidé et mesuré, et comment prouver une amélioration. À charger avant toute mission visuelle (« plus réaliste », « AAA », « ça fait faux », un réglage de rendu, un matériau, un asset de décor), et avant d'appliquer une recommandation tirée d'une recherche ou d'un tutoriel Unreal.
---

# Réalisme : comprendre Unreal, appliquer dans ANÁSTASIS

Règles de fond : `AGENTS.md`. Preuve visuelle : skill `anastasis-capture`. Pièges : `docs/unreal/PIEGES_UNREAL.md`.

Ce skill est le pont entre **le savoir général sur Unreal** (recherches, tutoriels, docs Epic) et **ce
projet**, qui ne ressemble pas au monde ouvert type d'un tutoriel : terrain en `ProceduralMeshComponent`
issu de la simulation, végétation en HISM transitoires générée par GeometryScript, eau en Single Layer
Water sur ce même maillage. Pas de Landscape, pas de World Partition, pas de plugin Water, pas de Nanite
en production, pas de RVT, pas de Megascans. Chacun de ces choix est **délibéré et documenté**.

## 1. Les cinq lois

1. **La lumière est physique, l'exposition est fixe.** Soleil 75 000 lux, EV100 = 14 le jour. À cette
   exposition un albédo de 0,5 sort blanc. Une image trop claire ou trop sombre se corrige **à la
   source** (albédo, intensité, densité), jamais par l'auto-exposition, une LUT ou le bloom.
2. **Le réalisme se gagne dans le contenu, pas dans le post.** La direction artistique interdit la
   LUT et le bloom « pour contrefaire la qualité des matériaux ».
3. **Un mécanisme du projet ne se remplace pas par l'outil standard de l'industrie.** « Les AAA
   utilisent Landscape / World Partition / Water / Nanite / RVT » n'est pas une raison. Le registre
   dit pourquoi chacun est absent. En changer est une décision d'Alexandre, pas d'un agent.
4. **Rien n'est plus réaliste sans A/B.** Une seule variable, mêmes caméras, image regardée **et**
   mesurée, écart au-delà de la variance de capture (~3,6 % de pixels à plus de 16/255).
5. **Tout coût se mesure en ms sur la machine réelle.** RTX 3060, 16 Go de RAM, éditeur de 8 à 13 Go.
   Pas une RTX 4080 en 4K.

## 2. Trouver la fiche : par symptôme

| Ce que tu vois ou ce qu'on te demande | Fiche |
|---|---|
| sous-bois noir, ombres bouchées, lumière plate, « Lumen », « ombres », « GI » | `fiches/eclairage.md` |
| image délavée, mur de brouillard, voile laiteux, ciel cramé, nuit noire, lever/coucher | `fiches/atmosphere.md` |
| relief en escalier, falaises fausses, bord du monde visible, « Landscape », « World Partition » | `fiches/terrain.md` |
| sol en plâtre, damier de tuiles, sol lisse, pente sans roche, « textures », « RVT » | `fiches/sol.md` |
| arbres en plastique, forêt clairsemée, herbe en tapis, « Nanite », « SpeedTree », « PCG » | `fiches/vegetation.md` |
| eau bleue carrelage, rive tranchée, rivière qui ne coule pas, « plugin Water » | `fiches/eau.md` |
| « color grading », « bloom », « DOF », « LUT », « tonemapper », herbe ou branches qui scintillent, « TSR » | `fiches/post-traitement.md` |
| image lente, fps, mémoire, « HLOD », « budget », « profiler » | `fiches/performance.md` |
| une recommandation précise lue dans une recherche | `registre.md` (chercher son identifiant) |

## 3. Lire une fiche

Chaque fiche a la même forme, dans cet ordre :

| Section | Ce qu'elle contient | Usage |
|---|---|---|
| **Unreal** | comment le moteur fonctionne, indépendamment du projet | comprendre |
| **ANÁSTASIS aujourd'hui** | mécanisme, fichiers, CVars `anastasis.*`, valeurs en vigueur | savoir où agir |
| **Règles** | identifiants `XXX-nn`, chiffrées, applicables | appliquer, citer dans la fiche de passation |
| **Vérifier** | script de capture, CVar d'A/B, métrique, seuil | prouver |
| **Ne pas faire** | ce qui a été essayé et rejeté, avec la mesure | éviter de refaire |
| **Ouvert** | ce qui n'est pas décidé | proposer, jamais trancher seul |

Les valeurs citées sont celles de `main` au 2026-10-01. **Le code fait foi** : avant d'agir sur une
valeur, la relire dans le fichier indiqué. Si elle a changé, corriger la fiche dans le même commit.

## 4. Appliquer une recommandation

1. La trouver dans `registre.md`. Statut `APPLIQUÉ` ou `ÉQUIVALENT` : rien à faire, le projet l'a déjà,
   autrement. `REJETÉ` : ne pas la refaire sans fait nouveau. `FAUX` : l'ignorer. `HORS_PÉRIMÈTRE` :
   idem. `OUVERT` : candidat de mission, **sur mandat**.
2. Absente du registre : la vérifier (doc Epic, code moteur, essai en A/B), puis l'y inscrire avec un
   statut, même si on ne l'applique pas.
3. L'appliquer par une CVar `anastasis.*` ou par le script d'autorité de l'asset (index d'`AGENTS.md`),
   jamais par une retouche manuelle dans l'éditeur (elle ne survit pas au rebuild).
4. Prouver (fiche, section **Vérifier**), puis citer la règle et les chiffres dans la fiche de passation.

## 5. Ingérer une nouvelle recherche

Le dossier `docs/recherche/realisme-unreal/` reçoit les recherches brutes (PDF, notes). Une recherche
n'enseigne rien à un agent tant qu'elle n'est pas passée ici :

1. Déposer la source : `docs/recherche/realisme-unreal/RU-<nnn>_<titre>.<ext>`, et une ligne dans son
   `README.md`.
2. Découper la source en affirmations vérifiables. Une affirmation = une ligne du registre
   `RU-<nnn>-<nn>`, avec son statut et sa preuve (fichier du projet, doc Epic, mesure).
3. Ce qui est vrai et utile pour le projet devient une règle dans la fiche du domaine, avec renvoi à la
   ligne du registre. Ce qui contredit une décision documentée reste `REJETÉ` ou `OUVERT` : on ne
   réécrit pas une décision sur la foi d'une recherche.
4. Les sources générées par IA (rapports « Deep Research ») citent souvent des références non
   vérifiables (`【77†L211】`) et inventent des noms de CVars : chaque affirmation technique se vérifie.

## 6. Décisions en attente d'Alexandre

Un agent ne les tranche pas. Il peut les rappeler :

- **Direction artistique.** `P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md` interdit la « sécheresse
  méditerranéenne ». Les missions `forest-terrain-p1` à `p4` ont pourtant passé arbres, herbe et sol en
  méditerranéen (pin d'Alep, olivier, prairie olive-paille), alors que l'atmosphère reste « pontique
  humide ». Aucun document n'acte ce changement.
- **Cible matérielle et budget GPU.** Aucun budget en ms n'est écrit. Seule la RTX 3060 est mesurée.
