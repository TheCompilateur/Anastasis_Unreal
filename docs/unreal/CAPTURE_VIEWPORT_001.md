# CAPTURE_VIEWPORT_001 — la capture photographie enfin ce qu'on lui demande

`ATMOSPHERE_002` laissait deux défauts ouverts dans `Anastasis.World.Capture`, en les
désignant comme « une mission, pas un à-côté » et « la plus rentable de ce couloir ».
Ce document est cette mission. Les deux sont corrigés.

L'enjeu n'est pas le confort : **un verdict de ce projet est une image**. Un outil qui
rapporte `CAPTURE::PASS` sur une image fausse ne coûte pas un re-run, il coûte une preuve.

## Défaut 1 — la capture pouvait photographier l'éditeur

`RequestCapture` appelait :

```cpp
FScreenshotRequest::RequestScreenshot(Path, /*bInShowUI*/ false, /*bAddFilenameSuffix*/ false);
```

Ceci pose un drapeau **global** au moteur. En PIE, deux clients de viewport dessinent
chaque frame — celui de l'éditeur et celui du jeu — et le **premier** qui passe dans
`ProcessScreenShots` consomme la demande et écrit le fichier. Le résultat ne dépend donc
pas de ce qu'on a demandé, mais de l'ordre de dessin.

Le garde-fou prévu par le moteur, `FScreenshotRequest::ShouldRestrictToGameViewport()`,
n'est consulté que dans la branche `bShowUI == true` de
`UGameViewportClient::ProcessScreenShots` (`GameViewportClient.cpp`, UE 5.8) :

```cpp
if (bShowUI && FSlateApplication::IsInitialized())
{
    ...
    if (FScreenshotRequest::ShouldRestrictToGameViewport())   // <- seulement ici
```

Ce chemin demande `bShowUI = false`. Le drapeau y est du code mort. `ATMOSPHERE_002`
l'avait déjà essayé, constaté inerte, et **reverté plutôt que livré** — « un correctif
inerte portant un commentaire confiant est pire qu'un bug connu, parce que le suivant
arrête de chercher ». Rien n'a été retenté de ce côté.

## Défaut 2 — la capture pouvait précéder la bascule de caméra

`GotoBookmark` fait `PC->SetViewTarget(Camera)`. Le gestionnaire de caméra n'applique
pas cette bascule dans l'appel : il l'applique à sa prochaine mise à jour. Entre les
deux, le viewport rend encore la vue du **joueur**, lequel apparaît au `PlayerStart`,
à l'intérieur de la géométrie de prototypage de `Lvl_FirstPerson`.

Le temporisateur de 0,5 s avant la prise était « une supposition, pas une garantie »
(`ATMOSPHERE_002`).

Ce défaut-là était **masqué** par le défaut 1 : la demande globale étant consommée
quelques frames plus tard, la bascule avait généralement eu lieu entre-temps. Corriger
le viewport sans corriger le moment l'a donc rendu visible immédiatement — une lecture
directe du back buffer rendait la vue du joueur, mi-salle de prototypage, mi-terrain.
Les deux se corrigent ensemble ou pas du tout.

## Le correctif

**Un seul appel remplace les deux problèmes** : on ne « demande pas une capture
d'écran », on demande **à ce viewport-là** de se redessiner et de se capturer.

```cpp
FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
Config.SetFilename(PendingCaptureShotPath);
Config.SetMaskEnabled(false);
Config.SetHDRCapture(false);
Config.SetResolution(Size.X, Size.Y, 1.0f);
Viewport->TakeHighResScreenShot();
```

`FViewport::TakeHighResScreenShot()` pose `bTakeHighResScreenShot` **sur ce viewport**,
force un redessin, rend des frames de chauffe, désactive le flou de mouvement pendant la
prise et capture dans sa propre cible de rendu. La cible n'est plus une intention, c'est
un argument : il n'y a plus de course, donc plus rien à ré-essayer.

C'est exactement le mécanisme de `HighResShot`, celui qu'utilise `observe-slice.py` — et
qui n'a jamais rendu une image fausse sur ce projet. Le correctif consiste surtout à
cesser d'avoir deux chemins de capture dont un seul est fiable.

Trois changements l'accompagnent, chacun pour une raison distincte :

- **Le viewport est résolu explicitement**, via `World->GetGameViewport()`. Hors PIE il
  n'y en a pas, et la capture **refuse** avec une raison plutôt que de photographier
  l'éditeur en silence : une capture de ce système est un verdict de jeu.
- **`SetViewTarget` est rendu instantané explicitement** (`BlendTime = 0`). C'est déjà
  le défaut du moteur, mais l'écrire empêche qu'un réglage de projet n'introduise un
  fondu — et un fondu, sur un chemin de preuve, veut dire une capture en cours de route.
- **Le temps de repos passe de 0,5 s à 1 s**, ce qui couvre la mise à jour du
  gestionnaire de caméra, le streaming et la convergence du TAA.

Enfin, la ligne de journal dit désormais **ce qui a réellement été lu** :

```
ANASTASIS_WORLD_CAPTURE_VIEWPORT source=game_viewport map=UEDPIE_0_Lvl_FirstPerson pie=1 size=1170x862 shot=...
```

`GetMapName()` et non `GetName()` : seul le premier porte le préfixe `UEDPIE_`, et c'est
ce préfixe qui distingue à la lecture du journal une capture de jeu d'une capture
d'éditeur. Un nom ambigu dans une ligne de preuve ne prouve rien — la première version de
cette ligne imprimait `Lvl_FirstPerson` pour le monde PIE, et n'aurait rien prouvé.

## Ce que la correction supprime

`FScreenshotRequest::OnScreenshotRequestProcessed`, le `FDelegateHandle` qui l'accompagnait
et le minuteur de repli de 8 s disparaissent : ils existaient pour surveiller une demande
globale asynchrone qui n'existe plus. La complétion est maintenant l'apparition du fichier,
attendue par un tick avec un plafond de 15 s.

## Preuve

Même signet `FOREST`, même graine, même scène — c'est le point de vue sur lequel le
défaut se manifestait le plus. Les trois états sont versionnés côte à côte dans
`docs/visual/capture-viewport-001/` : mauvais viewport, bon viewport au mauvais
instant, et ce qui était demandé.

| Chemin | Abouties | Regardées | Correctes |
|---|---:|---:|---:|
| Global `FScreenshotRequest` (avant) | 4 | 4 | **2** — deux rendaient le viewport éditeur |
| Lecture directe du back buffer (intermédiaire, non livré) | 5 | 2 | **0** — vue du joueur, salle de prototypage |
| `TakeHighResScreenShot` sur le viewport de jeu | 8 | 8 | **8** |

La colonne « regardées » n'est pas décorative : une capture comptée n'est pas une capture
vérifiée, et le chemin intermédiaire a trois images jamais ouvertes, donc non comptées
comme correctes. Les huit du chemin livré ont toutes été ouvertes.

Les huit portent la même ligne de journal, et c'est elle qui nomme la surface lue :

```
ANASTASIS_WORLD_CAPTURE_VIEWPORT source=game_viewport map=UEDPIE_0_Lvl_FirstPerson pie=1 size=1170x862
```

**Deux runs n'ont pas abouti et ne sont comptés nulle part** : leur éditeur n'a jamais
fini de démarrer dans le délai imparti, trois éditeurs d'agents différents se disputant
la machine (`la mémoire AssetCompile estimée est supérieure à la mémoire disponible`).
C'est de la contention de poste, pas un défaut de capture — mais un échec non abouti
n'est pas un succès, et il n'est donc pas compté comme tel.

## Ce qui n'est PAS corrigé : la convergence temporelle

Sur les huit, une capture (`hires8`) montre le bon viewport, la bonne cible de vue, le
bon cadrage — et un **tramage visible** sur le sol et les arbres : le TAA n'avait pas
convergé. L'image reste lisible et n'est fausse sur rien ; elle est simplement plus
bruitée que les sept autres. Elle a été prise pendant que trois éditeurs d'agents se
partageaient la machine.

Ce n'est donc ni le défaut 1 ni le défaut 2 : c'est un troisième, plus bénin, que ce
correctif ne prétend pas régler. Les frames de chauffe du chemin haute résolution le
réduisent sans le supprimer, et sous charge il ressort. À noter pour qui comparera deux
captures au pixel près — ce bruit-là n'est pas une différence de rendu.

## Ce qui reste vrai

**Regarder chaque image avant de la verser en preuve.** Le correctif retire deux causes
connues de fausse capture ; il ne transforme pas l'outil en oracle. La règle de
`ATMOSPHERE_002` reste la règle.

## Limites connues

1. **Aucun test automatisé ne couvre ce chemin.** Une capture demande un viewport de jeu
   vivant, donc une session PIE : la suite d'automation n'en monte pas. La preuve est
   la ligne `pie=1` du journal plus les images, pas un test.
2. **`SetResolution` écrit dans une configuration globale** (`GetHighResScreenshotConfig`).
   Deux captures simultanées dans le même éditeur se marcheraient dessus ; `bCaptureInFlight`
   l'interdit déjà pour ce système, mais un `HighResShot` lancé à la main en parallèle ne
   serait pas protégé.
3. **Le temps de repos reste un nombre choisi**, pas une garantie. Il est maintenant assez
   large pour la bascule de caméra observée, mais rien n'attend un signal explicite du
   gestionnaire de caméra.
4. **La convergence du TAA n'est pas garantie** — voir la section ci-dessus. Une capture
   sur huit est sortie tramée sous charge.
