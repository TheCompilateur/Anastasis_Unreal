# CAPTURE_VIEWPORT_001 — les trois états de la capture

Trois images, **même signet `FOREST`, même graine, même scène, même commande**.
Seul change le chemin de capture. Elles ne montrent pas le monde : elles montrent
l'outil qui le photographie.

Pourquoi elles sont versionnées : un verdict de ce projet est une image, donc la
preuve qu'un outil de capture est réparé est elle-même une image.

| Image | Chemin de capture | Ce qu'on voit | Octets | SHA-256 (16) |
|---|---|---|---:|---|
| `A_editor_viewport.png` | `FScreenshotRequest` global | le viewport de l'**éditeur** | 1 256 981 | `8734E23F5300FC87` |
| `B_player_view.png` | lecture directe du back buffer | la vue du **joueur** | 983 554 | `8B90F6269D81E613` |
| `C_game_viewport.png` | `TakeHighResScreenShot` (livré) | la vue **demandée** | 997 439 | `77C39D00F0E67C06` |

**`A` — le mauvais viewport.** Géométrie de prototypage grise, plots jaunes, sprites
d'éditeur. Aucun terrain, aucun arbre. La demande de capture était globale, et le
viewport de l'éditeur l'a consommée avant celui du jeu. Le JSON apparié écrit à côté de
cette image est correct : il décrit une scène qui n'est pas sur l'image.

**`B` — le bon viewport, le mauvais instant.** Le premier correctif visait le viewport
explicitement, et c'est bien le viewport de jeu : on y voit l'eau et le relief
d'ANÁSTASIS à droite. Mais la caméra est encore celle du **joueur**, au `PlayerStart`,
dans la salle de prototypage dont on voit le mur à gauche. `SetViewTarget` n'avait pas
encore été appliqué par le gestionnaire de caméra.

C'est l'image qui a prouvé que les deux défauts ne se corrigent pas séparément : le
second était masqué par le premier.

**`C` — ce qui était demandé.** La lisière du signet `FOREST` : clairière au premier
plan, peuplement au fond, sol forestier visible. Huit captures consécutives sur ce
chemin ont toutes rendu ce cadre.

## Reproduire

```powershell
tools\unreal\probe-demo.ps1 -Mission verif -Bookmark FOREST -PreCmds 'anastasis.Terrain.GroundMaterial 1'
```

La ligne à lire dans le journal, qui nomme la surface réellement lue :

```
ANASTASIS_WORLD_CAPTURE_VIEWPORT source=game_viewport map=UEDPIE_0_Lvl_FirstPerson pie=1 size=1170x862
```

Le préfixe `UEDPIE_` est le point : c'est lui qui distingue une capture de jeu d'une
capture d'éditeur. `A` aurait été produite par un monde sans ce préfixe.

## Ce que ces images ne prouvent pas

Qu'il n'existe plus de mauvaise capture. Une neuvième image, prise pendant que trois
éditeurs d'agents se partageaient la machine, est sortie **tramée** — bon viewport,
bonne caméra, bon cadrage, TAA non convergé. Ce troisième défaut n'est pas corrigé et
est documenté dans `docs/unreal/CAPTURE_VIEWPORT_001.md`.

**Regarder chaque image avant de la verser en preuve.** La règle d'`ATMOSPHERE_002`
n'est pas levée par ce correctif ; ces trois images sont la raison pour laquelle elle
existe.
