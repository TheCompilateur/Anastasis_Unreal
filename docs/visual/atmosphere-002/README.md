# ATMOSPHERE_002 — preuve visuelle A/B

Comparaison contrôlée. **Tout est identique entre les deux images** : carte
`/Game/FirstPerson/Lvl_FirstPerson` en PIE, seed `12345`, monde canonique 96×96,
signet `SHORE` (`(2450,-250,958)` pitch `-30.5` yaw `45` fov `85`), soleil
75 000 lux à −38/−55, brouillard global `density=0.012`, exposition figée
EV100 = 14. Seule change la CVar `anastasis.Atmosphere.Mist`.

| Image | CVar | Mesuré dans le snapshot |
|---|---|---|
| `A_shore_mist_off.png` | `0` | `mist_volume_count = 0` |
| `B_shore_mist_on.png` | `1` | `mist_volume_count = 69` |

Ce qu'on voit, dit tel quel : dans `B`, toute la bande lointaine au-dessus de la
rive est voilée — arbres et sol estompés — pendant que le premier plan reste net.
C'est le comportement attendu d'une brume de rive : elle est posée bas, sur la
marge d'eau, et elle ne monte pas sur les terres sèches du premier plan. L'effet
est **retenu** : la direction artistique interdit l'atmosphère qui masque des
assets faibles. `MistMaxExtinction` (0.65) est le bouton qu'une passe artistique
monterait.

Reproduire :

```powershell
tools\unreal\probe-demo.ps1 -Mission shore-mist-off -Bookmark SHORE -PreCmds 'anastasis.Atmosphere.Mist 0'
tools\unreal\probe-demo.ps1 -Mission shore-mist-on  -Bookmark SHORE
```

## Comment ces deux images ont été obtenues — et pourquoi ça mérite d'être écrit

Il a fallu **deux essais** pour obtenir `B` : la première tentative a photographié
le viewport de l'éditeur au lieu du PIE. Ce n'est pas un défaut de la brume, c'est
un défaut de l'outil de capture, et il est documenté dans
`docs/unreal/ATMOSPHERE_002.md` avec son diagnostic au niveau du moteur. Tant
qu'il n'est pas corrigé, **toute image de ce projet doit être regardée avant
d'être versée en preuve**.
