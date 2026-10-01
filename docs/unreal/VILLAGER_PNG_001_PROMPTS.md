# VILLAGER_PNG_001 -- fiche de generation des 32 habitants

Generee par `python tools/unreal/villager-png.py prompts` depuis `SourceArt/Characters/villager-population.json` :
ne pas editer a la main, changer le manifeste et regenerer.

## Mode d'emploi

1. **Une conversation ChatGPT neuve par categorie** (six au total) : dans une meme conversation, le
   generateur a tendance a reprendre le visage de l'image precedente.
2. Joindre **une** planche comme reference de style, sans plus :
   - hommes et garcons : `C:\dev\Jeux IV Kingdoms\assets\references\npc\planche-homme-modulaire-rts.png`
   - femmes et filles : `C:\dev\Jeux IV Kingdoms\assets\references\npc\planche-femme-modulaire-rts.png`
3. **Un prompt = une image = une personne.** Coller le bloc tel quel.
4. Avant d'enregistrer, comparer avec les precedents de la categorie. Si le visage ou la silhouette
   rappelle un autre habitant, ou la planche : repondre
   *"Regenerate: completely different face and body, keep only the style."*
5. Verifier : corps entier (tete et pieds visibles), personne seule, pas de sol ni d'ombre portee.
6. Enregistrer sous le **nom exact** de l'en-tete (`CHR_M_Adult_001.png`...) dans
   `C:\dev\ANASTASIS_WORKTREES\villager-png-001\SourceArt\Characters\Raw\`.
   Fond transparent, ou a defaut vert d'incrustation uni : les deux sont acceptes, le detourage est
   fait ensuite. Eviter un fond gris ou beige : il a la couleur du lin ecru.
7. Un depot partiel suffit pour commencer : chaque image deposee est traitee et verifiee.

Statures en jeu (le PNG est remis a cette taille, les pieds sur une meme ligne) :

| Id | Age | Stature |
|---|---|---|
| `CHR_M_Adult_001` | 24 | 178 cm |
| `CHR_M_Adult_002` | 38 | 163 cm |
| `CHR_M_Adult_003` | 31 | 172 cm |
| `CHR_M_Adult_004` | 45 | 181 cm |
| `CHR_M_Adult_005` | 27 | 166 cm |
| `CHR_M_Adult_006` | 52 | 170 cm |
| `CHR_M_Adult_007` | 34 | 176 cm |
| `CHR_M_Adult_008` | 41 | 168 cm |
| `CHR_F_Adult_001` | 22 | 166 cm |
| `CHR_F_Adult_002` | 36 | 152 cm |
| `CHR_F_Adult_003` | 29 | 160 cm |
| `CHR_F_Adult_004` | 44 | 164 cm |
| `CHR_F_Adult_005` | 26 | 156 cm |
| `CHR_F_Adult_006` | 33 | 170 cm |
| `CHR_F_Adult_007` | 39 | 161 cm |
| `CHR_F_Adult_008` | 48 | 150 cm |
| `CHR_M_Elder_001` | 68 | 168 cm |
| `CHR_M_Elder_002` | 72 | 158 cm |
| `CHR_M_Elder_003` | 63 | 168 cm |
| `CHR_M_Elder_004` | 77 | 152 cm |
| `CHR_F_Elder_001` | 66 | 155 cm |
| `CHR_F_Elder_002` | 74 | 140 cm |
| `CHR_F_Elder_003` | 61 | 162 cm |
| `CHR_F_Elder_004` | 69 | 150 cm |
| `CHR_M_Child_001` | 6 | 112 cm |
| `CHR_M_Child_002` | 10 | 135 cm |
| `CHR_M_Child_003` | 12 | 146 cm |
| `CHR_M_Child_004` | 8 | 125 cm |
| `CHR_F_Child_001` | 5 | 106 cm |
| `CHR_F_Child_002` | 9 | 130 cm |
| `CHR_F_Child_003` | 11 | 142 cm |
| `CHR_F_Child_004` | 7 | 118 cm |

## Hommes adultes (`Adult_Male`)

### CHR_M_Adult_001.png

```text
Full-body image of a 24-year-old man, tall and lean, wiry. Long narrow face, prominent hooked nose, deep-set dark eyes, thick eyebrows almost joined in the middle, ears that stick out slightly. Short tight black curls. Only a sparse young beard on the chin, cheeks bare. Olive skin. Very upright posture. Knee-length undyed grey-beige wool tunic, rope belt, dark wool leggings, cloth-wrapped shoes.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_M_Adult_002.png

```text
Full-body image of a 38-year-old man, short and stocky, barrel chest, thick neck, heavy forearms. Wide square face, heavy jaw, broad flat nose once broken and bent to his left, small eyes under a low brow. Head shaved to dark stubble, receding hairline. Clean-shaven with a dark shadow of stubble. Deeply tanned weathered skin. Feet planted wide. Linen tunic with sleeves rolled up, sleeveless brown leather vest, leather belt, scuffed leather boots.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_M_Adult_003.png

```text
Full-body image of a 31-year-old man, medium height, average build with a slightly soft belly. Round face, full cheeks, small upturned nose, wide-set light hazel eyes, thin eyebrows, light freckles. Wavy chestnut-brown hair to the shoulders, tied back. Short neat full beard. Fair olive skin. Relaxed, slightly slouching. Faded woad-blue wool tunic, leather belt with a small pouch, brown leggings.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_M_Adult_004.png

```text
Full-body image of a 45-year-old man, tall, robust, broad-shouldered, heavy-boned, big hands. Long rectangular face, high cheekbones, strong aquiline nose, narrow grey eyes, bushy eyebrows, deep forehead lines. Dark hair cut short, grey at the temples. Long full dark beard streaked with grey. Ruddy weathered skin. Slight forward stoop of a lifelong labourer. Olive-green tunic under a heavy brown hooded wool cloak, hood down.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_M_Adult_005.png

```text
Full-body image of a 27-year-old man, short and very thin, narrow shoulders, one shoulder slightly higher than the other. Triangular face with a pointed chin, prominent cheekbones, hollow cheeks, small straight nose, large dark eyes, arched eyebrows. Thick straight black hair cut in a fringe. Thin moustache only, no beard. Light brown skin. Nervous, tense posture. Patched ochre tunic a size too large for him, rope belt.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_M_Adult_006.png

```text
Full-body image of a 52-year-old man, medium height, heavy, pot-bellied, rounded shoulders. Broad oval face, double chin, bulbous nose with small red veins, heavy-lidded eyes. Bald crown with a fringe of grey-brown hair around the sides. Clean-shaven. Ruddy cheeks. Leans back on his heels. Long calf-length tunic of faded madder red, wide cloth sash for a belt.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_M_Adult_007.png

```text
Full-body image of a 34-year-old man, tall, sinewy and athletic, long arms. Long oval face, square cleft chin, long straight nose, light green eyes, an old scar cutting through his left eyebrow. Short curly auburn hair. Short auburn beard, patchy and irregular on the cheeks. Pale skin, sunburnt nose. Confident upright stance. Short grey wool tunic, wool leggings cross-gartered with leather thongs, worn leather boots.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_M_Adult_008.png

```text
Full-body image of a 41-year-old man, medium-short, average build. Heart-shaped face, wide forehead, narrow jaw, thin long nose with a bump, close-set brown eyes, heavy bags under the eyes, large ears. Unkempt straight dark brown hair to the collar. Medium-length forked beard. Dark olive skin. Tired posture, head tilted slightly. Undyed beige linen tunic, dark green wool mantle thrown over one shoulder.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```


## Femmes adultes (`Adult_Female`)

### CHR_F_Adult_001.png

```text
Full-body image of a 22-year-old woman, tall for her time, slim and long-limbed. Long oval face, high forehead, long straight nose, large almond-shaped dark eyes, thick straight eyebrows. Black hair in a single long braid over the shoulder, head uncovered. Olive skin. Very upright. Long undyed linen tunic to the ankles, narrow woven belt, leather shoes.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_F_Adult_002.png

```text
Full-body image of a 36-year-old woman, short, sturdy, wide-hipped, strong forearms. Round wide face, broad cheekbones, small snub nose, small eyes, hint of a double chin, ruddy cheeks. Brown hair hidden under an ochre kerchief knotted at the nape. Dark brown wool tunic to the ankles with a stained undyed apron.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_F_Adult_003.png

```text
Full-body image of a 29-year-old woman, medium height, average build. Square face, strong jaw, wide mouth, prominent straight nose, light hazel eyes, freckles across the nose. Wavy chestnut hair partly showing under a cream linen veil pushed back from the forehead. Fair skin. Relaxed stance. Long faded woad-blue tunic, leather belt.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_F_Adult_004.png

```text
Full-body image of a 44-year-old woman, medium-tall, thin, bony and angular. Long narrow face, sunken cheeks, sharp chin, hooked nose, deep lines from nose to mouth, small grey eyes, thin lips. Hair fully covered by a long dark brown veil falling over the shoulders. Weathered skin. Slightly hunched. Long charcoal-grey wool tunic, plain and severe.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_F_Adult_005.png

```text
Full-body image of a 26-year-old woman, short-medium, plump and soft-rounded. Round heart-shaped face, dimpled cheeks, small rounded nose, wide-set large brown eyes, thick arched eyebrows, a faint smile. Thick curly black hair in a low loose bun with escaping strands, head uncovered. Warm light-brown skin. Faded madder-red overdress over a cream underdress.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_F_Adult_006.png

```text
Full-body image of a 33-year-old woman, tall, broad-shouldered, strong and muscular. Rectangular face, heavy brow, broad nose, wide jaw, deep-set eyes with sun-squint lines. Dark blonde hair in two braids wrapped around the head. Tanned weathered skin. Solid upright stance. Olive-green wool tunic hitched up at the belt over a longer undyed undertunic, sturdy shoes.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_F_Adult_007.png

```text
Full-body image of a 39-year-old woman, medium height, average to slightly heavy. Oval face, visibly asymmetric: left eye a little smaller and lower than the right. Long thin nose, pronounced chin, faint pockmarks on the cheeks. Auburn-brown hair under a white linen headscarf tied at the nape. Pale skin. Head slightly tilted. Undyed grey-brown wool tunic, a wool shawl around the shoulders.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_F_Adult_008.png

```text
Full-body image of a 48-year-old woman, short, small and wiry, slightly bowed legs. Small triangular face, pointed chin, high cheekbones, small hooked nose, quick dark eyes with deep crow's feet. Black hair streaked with grey in a bun, under a loose ochre veil. Dark olive leathery skin. Rust-brown tunic, mended many times, visible patches.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```


## Hommes ages (`Elder_Male`)

### CHR_M_Elder_001.png

```text
Full-body image of a 68-year-old man, tall but stooped, thin, long arms. Long gaunt face, prominent cheekbones, large hooked nose, deep wrinkles, white bushy eyebrows. Bald on top, long thin white hair at the sides. Long flowing white beard. Pale skin with age spots. Leans on a plain wooden staff held in his right hand. Long dark brown wool tunic and a worn cloak.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_M_Elder_002.png

```text
Full-body image of a 72-year-old man, short, stocky and round, bow-legged. Wide round face, fleshy nose, small eyes nearly hidden in wrinkles, sagging jowls. Short stubbly grey hair. Clean-shaven with white stubble. Ruddy skin. Hunched. Undyed wool tunic with a sheepskin vest, fleece side out.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_M_Elder_003.png

```text
Full-body image of a 63-year-old man, medium height, lean, wiry, still strong. Square weathered face, hard jaw, flattened broken nose, left eyelid scarred and half closed. Thick grey hair cut short. Short grizzled salt-and-pepper beard. Deep tan, leathery skin. Upright, almost military bearing. Faded blue wool tunic, broad leather belt, old boots.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_M_Elder_004.png

```text
Full-body image of a 77-year-old man, small and frail, very thin, strongly bent spine. Narrow face, sunken toothless mouth, long thin nose, cloudy pale eyes, large ears. Sparse wispy white hair. Thin, irregular long white goatee. Papery pale skin. Walks with a short knotted cane in his left hand. Oversized grey wool cloak over a long tunic.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```


## Femmes agees (`Elder_Female`)

### CHR_F_Elder_001.png

```text
Full-body image of a 66-year-old woman, medium height, heavy-set and broad. Wide round face, deep laugh lines, broad nose, double chin. White hair under a long black veil. Olive skin with age spots. Upright but heavy. Long black wool tunic of a widow.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_F_Elder_002.png

```text
Full-body image of a 74-year-old woman, small, very thin, strongly hunched. Narrow deeply wrinkled face, hooked nose, sharp chin, sunken cheeks, small dark eyes. A thin grey braid showing under a loose cream kerchief. Dark leathery skin. Holds a walking stick in her right hand. Faded brown layered tunic and a heavy shawl.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_F_Elder_003.png

```text
Full-body image of a 61-year-old woman, tall, lean and dignified. Long oval face, high cheekbones, straight nose, fine wrinkles, light grey-blue eyes. Silver hair in a bun, uncovered, a dark blue veil resting on the shoulders. Pale skin. Very upright. Faded woad-blue long tunic, woven belt.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_F_Elder_004.png

```text
Full-body image of a 69-year-old woman, short, plump and soft. Round face, small upturned nose, many soft wrinkles, rosy cheeks, a mole on the chin. Frizzy iron-grey hair escaping an ochre headscarf. Slight stoop. Faded madder-red tunic with an undyed apron.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```


## Garcons (`Child_Male`)

### CHR_M_Child_001.png

```text
Full-body image of a 6-year-old boy, small and chubby, belly pushed forward. Round face, big cheeks, small nose, large dark eyes. Messy curly black hair. Olive skin. Short undyed wool tunic to the knees, barefoot.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_M_Child_002.png

```text
Full-body image of a 10-year-old boy, skinny and long-limbed. Narrow face, ears sticking out, a long nose for his age, freckles. Light brown straight hair cut short and unevenly. Fair skin. Oversized ochre tunic with a rope belt, cloth-wrapped shoes.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_M_Child_003.png

```text
Full-body image of a 12-year-old boy, sturdy and broad for his age. Square face, strong eyebrows, wide flat nose, serious expression. Straight dark hair in a bowl cut. Tanned skin. Grey wool tunic, wool leggings, a small hooded cape.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_M_Child_004.png

```text
Full-body image of an 8-year-old boy, slight and shy, arms held close to the body. Oval face, pointed chin, big light hazel eyes, thin eyebrows. Wavy reddish-brown hair. Pale skin with sunburnt cheeks. Patched faded blue tunic, bare legs, leather shoes.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```


## Filles (`Child_Female`)

### CHR_F_Child_001.png

```text
Full-body image of a 5-year-old girl, small and round. Round face, chubby cheeks, button nose. Dark curly hair in two short pigtails. Olive skin. Long cream linen tunic, barefoot.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_F_Child_002.png

```text
Full-body image of a 9-year-old girl, thin. Long oval face, a long nose, large serious dark eyes, thick eyebrows. Long straight black hair in a single braid. Brown skin. Rust-brown tunic with a woven belt.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_F_Child_003.png

```text
Full-body image of an 11-year-old girl, tall and lanky for her age. Heart-shaped face, freckles, wide mouth. Light brown hair loose to the shoulders under a small cream kerchief. Fair skin. Olive-green tunic with a small apron.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```

### CHR_F_Child_004.png

```text
Full-body image of a 7-year-old girl, sturdy, slightly pigeon-toed. Square-ish face, broad nose, small eyes. Straight dark brown hair cut at chin length. Tanned skin. Faded madder-red tunic, leather shoes.

Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character render with matte textures: same rendering style, lighting and level of detail as the attached reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side of the image. Camera at chest height, no perspective distortion. The whole figure is visible from the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light spilling on the person. No floor, no ground, no cast shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, no saturated colours, nothing modern.
```
