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
| Budget écrit | **aucun** | — |
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

## Vérifier

```powershell
tools\unreal\riverbank-capture.ps1 -Label <x> -Profile          # ProfileGPU par vue
tools\unreal\capture-ground-cover.ps1 -Label <x> -States on,off  # frame p50/p95 + GPU par vue
tools\unreal\measure-tree-cost.ps1                               # triangles, LOD, instances soumises
```

## Ne pas faire

- Adopter les budgets d'un rapport générique (RTX 4080, 4K, 60 images/s, 8 Go de textures) comme cible.
- Optimiser avant d'avoir mesuré le poste.
- Tuer l'éditeur d'un autre agent pour libérer la machine.

## Ouvert

- **Budget GPU cible** : non écrit. Proposition à soumettre à Alexandre : partir des 17–21 ms mesurés et
  fixer un plafond par vue (par exemple 16,7 ms en vue forêt sur RTX 3060), puis refuser tout ajout qui le
  dépasse sans compensation.
- Coût GPU de la forêt : jamais mesuré seul.
- Le budget conditionne Lumen Lite (RU-002-01) : sans plafond écrit, rien ne justifie de céder de la
  qualité de GI pour des ms.
- `stat unit` affiche en 5.8 la VRAM utilisée et son budget (RU-002-18) : à faire lire par les scripts de
  capture pour reconnaître une VRAM saturée (PERF-04).
