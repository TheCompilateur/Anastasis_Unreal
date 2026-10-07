# HANDOFF: world-theatre-light-001

## MISSION

WORLD_THEATRE v2.1, la lumière. Mesuré : le lointain d'ANÁSTASIS est la chose la plus claire de l'image
(×4,4 du village vers la chaîne). v2.1 l'inverse et le prouve, sans toucher aux réglages d'atmosphère des autres :
un post-traitement de distance posé sur l'acteur du théâtre assombrit, désature et refroidit ce qui est loin,
bande basse du ciel comprise, zénith intact. Conception et mesures : section « v2.1 » de
`docs/unreal/world-theatre-001/WORLD_THEATRE_001.md`.

Livré :
1. `M_WorldTheatreDistance` (avant tonemapper) et `M_WorldTheatreDepthProbe` (profondeur en gris, instrument),
   `/Game/WorldTheatre/`, source d'autorité `tools/unreal/world-theatre-light-material.ps1` + `.py`.
2. `UAnastasisWorldTheatreSubsystem::UpdateLight` : `UPostProcessComponent` non borné, priorité -100, sur l'acteur
   du théâtre ; `anastasis.Theatre.Light` **0 par défaut**, réglages `anastasis.Theatre.Light.*` à chaud ;
   `anastasis.Theatre.DepthProbe`.
3. `world-theatre-analyze.py --luminance` : luminance, saturation et froideur médianes par plan de distance, sur la
   profondeur vraie de la sonde (validée : 0,89–1,08 contre la géométrie, ciel concordant à 96–99 %).
4. `world-theatre-capture.py` : états nommés (`depth`, `masses`, `light`, `light_soft`, `light_strong`), plusieurs
   heures dans le même éditeur (`ANASTASIS_THEATRE_HOURS`) ; preuve `world-theatre-light-capture` au registre.

## FILES_OWNED

- `Content/WorldTheatre/M_WorldTheatreDistance.uasset`, `Content/WorldTheatre/M_WorldTheatreDepthProbe.uasset` (LFS)
- `tools/unreal/world-theatre-light-material.ps1`, `tools/unreal/world-theatre-light-material.py`
- modifiés, propriété de world-theatre-001 (même auteur) : `Source/Anastasis_UnrealV2/WorldTheatre/AnastasisWorldTheatreSubsystem.{h,cpp}`,
  `tools/unreal/world-theatre-analyze.py`, `tools/unreal/world-theatre-capture.py`, `docs/unreal/world-theatre-001/WORLD_THEATRE_001.md`
- lignes ajoutées : `AGENTS.md` (index), `tools/unreal/proofs.txt` (1 entrée)

## COMMIT

PENDING

## MEC

- BUILD: PASS (worktree, Editor Win64 Development).
- TESTS: suite non jouée ici (lot). Aucun code pur nouveau : les tests `Anastasis.WorldTheatre.*` de v1 restent valables.
- `world-theatre-light-material.ps1` : `WORLD_THEATRE_LIGHT_MATERIAL::PASS`, aucun « Failed to compile » après le marqueur.
- `editor-batch.ps1 -Proofs world-theatre-light-capture` : `PROOF::PASS (509.2s)`, `EDITOR_BATCH::PASS 1/1`
  (9 vistas × 11 h / 19 h × masses, light, light_strong, masses2 + sonde).
- `python tools/unreal/world-theatre-analyze.py Saved/WorldTheatreEvidence/v1/read --vistas docs/unreal/world-theatre-001/vistas.json --luminance Saved/WorldTheatreEvidence/light`

## PROOFS

PROOFS: (aucune)

(`world-theatre-light-capture` est un outil de capture, COMPLETE = images écrites ; la couche est coupée par défaut :
rien à rejouer au lot.)

## SCN

`Saved/WorldTheatreEvidence/light/` du worktree. Horizon / plan moyen à 11 h, avant -> défaut : V1 4,43 -> 2,35 ;
V2 1,60 -> 0,83 ; V3 1,05 -> 0,53 ; V5 1,26 -> 0,59 ; V6 1,22 -> 0,58. Ciel médian inchangé, témoin identique à 0,001.
À l'œil : la muraille passe du blanc craie au gris ardoise et **retrouve son relief** ; à 19 h, la chaîne sort de la
brume comme une masse sombre. 19 h : aucun chiffre revendiqué (scène trop sombre, une valeur incohérente).

## PLY

UNKNOWN : aucune marche joueur ; effet de post-traitement visible partout, sans interaction.

## ECARTS

AUCUN — `Source/AnastasisSim/` non touché.

## INTEGRATION_RISK

- **Dépendance** : branche créée depuis `agent/world-theatre-001`, versé au lot 5 (`main` 25ebf1fe) ; rebasée sur `main`
  avant `finish`, les commits de v1 tombent.
- Couche **coupée par défaut** : intégrer ne change aucune image.
- Un `UPostProcessComponent` non borné de priorité -100 qui n'apporte que deux matériaux pondérés ; aucune valeur
  d'exposition, de brouillard ou de perspective aérienne n'est écrite. Si l'atmosphère change (agents air / ciel),
  la couche suit : elle multiplie ce qui est rendu.
- Assets LFS neufs dans `Content/WorldTheatre/` (dossier propre à la mission).

## STOP

- Ne revendique pas un lointain « inquiétant » prouvé en jeu : c'est une valeur et une couleur mesurées sur 9 vues.
- La saturation du lointain monte légèrement à 11 h (0,29 -> 0,32 en V1) : la teinte froide l'emporte sur la désaturation.
- À 19 h la base de la chaîne se lit en frise dentelée (lisière des masses de v1).
- Aucune variation dans le lointain (ombres de nuages, brouillards sombres par secteur), pas de nuit, aucun lien à la simulation : v2.2.
- Coût GPU du post-traitement non mesuré.
