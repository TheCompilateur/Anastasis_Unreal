# Performance et mesure

## Unreal

- **Temps d'image** : à 60 images/s, 16,7 ms ; à 30, 33,3 ms. Le temps d'image est le plus lent du thread
  de jeu, du thread de rendu et du GPU. Raisonner en **ms**, pas en images/s (les ms s'additionnent).
- Commandes console :
  - `stat unit` : jeu, rendu, GPU ;
  - `stat GPU` : ms par passe (Lumen, ombres, eau, translucidité…) ;
  - `ProfileGPU` : la même chose en détail, pour une image ;
  - `stat SceneRendering` : draw calls et primitives ;
  - `stat RHI` : triangles, mémoire GPU ;
  - `stat streaming` : textures en attente.
- **Draw calls** : chaque mesh distinct coûte du CPU ; l'instanciation (HISM) le fait tomber.
- **Culling** : Unreal élimine ce qui est hors champ et, par requêtes d'occlusion, ce qui est caché. Les
  volumes de visibilité précalculée sont un outil de l'éclairage statique, inutile ici.
- **Scalability** (Low → Epic) : des groupes de qualité (ombres, GI, effets, post) par plateforme.
- Les chiffres d'un rapport générique (« moins de 1 000 draw calls », « 30 M de triangles », « 8 Go de
  textures ») dépendent du jeu et de la machine. Seule la mesure sur la machine réelle vaut.

## ANÁSTASIS aujourd'hui

| Quoi | Valeur | Où |
|---|---|---|
| Machine | RTX 3060, 16 Go de RAM ; un éditeur prend 8 à 13 Go | `AGENTS.md` |
| Porte mémoire | moins de 2 éditeurs, ≥ 3 Go de RAM libre, ≥ 8 Go de marge de mémoire engagée | `AGENTS.md`, `editor-launch.ps1` |
| Carte de jeu | environ 17–21 ms GPU selon la vue (viewport éditeur) | `handoffs/lumen-hit-lighting-001.md` |
| Forêt | environ 48 images/s après le Hit Lighting | idem |
| Vues d'herbe | frame p50 environ 10–15 ms | `handoffs/ground-cover-001.md` |
| Postes connus | Hit Lighting +0,5 à +3,4 ms ; herbe +1,5 à 2 ms ; eau environ 1,07 ms | fiches du domaine |
| Végétation par strate (2026-10-01, viewport 1280×720) | image entière 10,3–14,2 ms ; arbres 0,3–3,75 ; herbe 0,1–3,75 ; sous-bois < 0,4 (bruit) ; scène sans végétation 7,9–8,4 ms ; témoin ≤ 0,34 ms | `handoffs/forest-cost-001.md` |
| Budget écrit | **PERF-05**, approuvé par Alexandre le 2026-10-01 : 16,7 ms GPU par vue, végétation ≤ 6,5 ms, dans le banc `vegetation-cost-capture.ps1` | `handoffs/forest-cost-001.md` |
| Bruit de capture | ~3,6 % des pixels à plus de 16/255 entre deux runs du même état | `PIEGES_UNREAL.md` |

## Règles

- **PERF-01** — Tout ajout visuel donne son coût en ms, vue par vue, mesuré par un script de capture qui
  sort frame p50/p95 ou un `ProfileGPU` (`capture-ground-cover.ps1`, `riverbank-capture.ps1 -Profile`).
  « Ça ne coûte rien » sans chiffre n'est pas une preuve.
- **PERF-02** — Un coût se mesure contre un témoin du même run (A/B), jamais contre un chiffre d'un autre
  jour : charge de la machine, éditeurs des autres agents, VRAM partagée.
- **PERF-03** — Un éditeur en viewport n'est pas un jeu packagé : les ms d'éditeur servent à comparer,
  pas à promettre une fréquence d'image.
- **PERF-04** — Une machine saturée (`EDITOR_GATE::WAIT`, `E_OUTOFMEMORY`) donne des mesures fausses :
  relancer dans une fenêtre calme, ne rien conclure.
- **PERF-05** — Budget GPU, approuvé par Alexandre le 2026-10-01. Dans le banc de
  `vegetation-cost-capture.ps1` (RTX 3060, viewport éditeur de la fenêtre 1280×720, midi sec, six vues) :
  - **16,7 ms GPU par vue** au plus (60 images/s). Au 2026-10-01, la vue la plus chère est à 14,2 ms
    (intérieur de forêt) : 2,5 ms de marge pour tout ce qui viendra ;
  - **végétation ≤ 6,5 ms** dans toute vue (6,2 ms au plus le 2026-10-01) ;
  - un ajout visuel donne son coût dans ce banc, vue par vue, contre un témoin (PERF-01, PERF-02). Il ne
    passe que si chaque vue reste sous les deux plafonds, ou s'il retire ailleurs au moins ce qu'il ajoute.
  Ce budget vaut pour ce banc, pas pour le jeu : un 1080p plein écran a 2,25 fois plus de pixels, et les
  passes qui en dépendent (Lumen, ombres, post) grandiront d'autant. Promettre une fréquence d'image exige
  de mesurer un jeu packagé (PERF-03).

## Vérifier

```powershell
tools\unreal\riverbank-capture.ps1 -Label <x> -Profile          # ProfileGPU par vue
tools\unreal\capture-ground-cover.ps1 -Label <x> -States on,off  # frame p50/p95 + GPU par vue
tools\unreal\measure-tree-cost.ps1                               # triangles, LOD, instances soumises
tools\unreal\vegetation-cost-capture.ps1 -Label <x>             # ms GPU par strate de végétation, vue par vue
```

## Ne pas faire

- Adopter les budgets d'un rapport générique (RTX 4080, 4K, 60 images/s, 8 Go de textures) comme cible.
- Optimiser avant d'avoir mesuré le poste.
- Tuer l'éditeur d'un autre agent pour libérer la machine.

## Ouvert

- **Budget du jeu packagé** : PERF-05 fixe le budget du banc d'éditeur. Celui d'un jeu packagé en 1080p
  plein écran reste à mesurer, puis à fixer par Alexandre.
- Coût GPU de la forêt : mesuré par strate le 2026-10-01 (`handoffs/forest-cost-001.md`). Les 17–21 ms
  ci-dessus et les 10–14 ms de cette mesure ne sont pas comparables : autres vues, autre taille de viewport.
- Lumen Lite (RU-002-01) : avec PERF-05, toutes les vues sont sous le plafond ; rien ne justifie
  aujourd'hui de céder de la qualité de GI pour des ms.
- `stat unit` affiche en 5.8 la VRAM utilisée et son budget (RU-002-18) : à faire lire par les scripts de
  capture pour reconnaître une VRAM saturée (PERF-04).
