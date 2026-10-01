# GROUND_COVER_001 / sol sous l'herbe — preuves visuelles

Capture `tools\unreal\capture-ground-cover.ps1 -Label tint-v2 -States on,notint,off`
(worktree `ground-soil-tint-001`), `Lvl_AnastasisSlice`, `EmbodyCanonical(12345)`, mêmes caméras
(`cameras.json`) :

- `*_on` : herbe + sol teinté (`anastasis.GroundCover.SoilTint 1`, défaut) ;
- `*_notint` : **mêmes touffes**, sol non teinté (`SoilTint 0`) — l'A/B ne mesure que la teinte ;
- `aerien_off` : sans herbe du tout.

```
ANASTASIS_SOIL_TINT enabled=1 tinted_vertices=135805 mean_amount=0.522
```

| Vue | Ce qu'elle montre |
|---|---|
| `aerien` | ~300 m au-dessus de la vallée : l'herbe y est coupée (108 m), seul le SOL parle. Teinté, la vallée se lit en prairie verte tachetée là où la couverture varie ; non teinté, vert-jaune pâle uniforme |
| `prairie_eye` | à 1,7 m : le sol sous la prairie verdit et se fond avec les touffes au lieu de les découper sur fond clair |
| `oblique`, `riviere_eye`, `hors_vallee_eye`, `lande_eye` | même A/B ailleurs (rive : laîches plus sombres ; lande : terre plus brune) |

## Coût

Aucun au rendu : une couleur de sommet calculée une fois par incarnation. GPU (`GetFrameTimingsMs`)
avec / sans teinte : aérien 12,0 / 12,0 ms, prairie 12,7 / 13,3 ms (`ground-cover.json`).

## Ce qui a dû être corrigé (tint-v1 → tint-v2)

tint-v1 journalisait 135 805 sommets teintés et rendait un sol **identique** à l'image :

- `UProceduralMeshComponent::UpdateMeshSection` ne recopie rien — couleurs comprises — si le
  tableau de positions n'a pas exactement le nombre de sommets de la section. Des positions vides
  (pour « ne pas toucher la géométrie ») sautaient toute la copie. Positions repassées, inchangées.
- La section est créée sans conversion sRGB (`bSRGBConversion = false`) : ses octets sont
  linéaires. Relus en sRGB et réécrits en sRGB, la teinte aurait été fausse une fois la copie
  réparée. Relus `ReinterpretAsLinear`, réécrits `ToFColor(false)`.
