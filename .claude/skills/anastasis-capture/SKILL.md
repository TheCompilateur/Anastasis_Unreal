---
name: anastasis-capture
description: Preuve visuelle ANÁSTASIS — capture A/B d'un réglage (CVar, matériau, asset) avec les scripts tools/unreal, puis comparaison chiffrée des images. À charger dès que le verdict d'une mission est une image, ou qu'un changement visuel doit être montré.
---

# Preuve visuelle : capture A/B et comparaison

Un verdict de ce projet est souvent une image. Une image se regarde **et** se mesure.

## 1. Choisir le script

L'index d'`AGENTS.md` (« Preuves visuelles et mesures ») liste un script par sujet : tranche
(`capture-slice.ps1`), rive, relief, horizon, ciel, lieux, herbe, arbres, village en PIE… Prendre celui
dont les caméras regardent ton sujet ; n'en écrire un nouveau que si aucun ne le cadre (et l'indexer).

Le principe d'un A/B : **une seule variable change**. Même binaire, même carte, même graine, mêmes
caméras, même soleil. La variable passe par une CVar (`-PreCmds`, `-States`) plutôt que par deux builds.

```powershell
tools\unreal\capture-slice.ps1 -Mode 2 -Out avant.png -PreCmds 'anastasis.X 0'
tools\unreal\capture-slice.ps1 -Mode 2 -Out apres.png -PreCmds 'anastasis.X 1'
```

## 2. Pendant la capture

- Le lanceur est discret : fenêtre hors écran, sans focus. Pour **voir** l'éditeur travailler, ou débloquer
  une boîte de dialogue : `$env:ANASTASIS_EDITOR_VISIBLE = '1'`.
- Un nouveau lanceur de capture garde
  `-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False`.
- Échec sans image : `docs/unreal/PIEGES_UNREAL.md`, section Captures (focus, scène statique, VRAM saturée
  par les éditeurs des autres agents → relancer, ne pas tuer).

## 3. Regarder

Ouvrir **chaque** image (outil Read) avant de la classer en preuve. Défauts connus : capture PIE qui
photographie l'éditeur, capture en plein fondu de caméra, caméra qui vise un coin du monde (scripts
antérieurs à la tuile de 4 m).

## 4. Mesurer

```powershell
python .claude\skills\anastasis-capture\compare.py avant.png apres.png --heatmap diff.png
```

Donne l'écart moyen par canal, la part de pixels qui diffèrent de plus de 16/255, les luminosités
moyennes, et une carte de l'écart amplifiée ×8 (à regarder : où est l'écart ?).

**Une capture n'est pas reproductible au pixel.** Mesuré le 2026-09-30, même état, deux runs :
~3,6 % des pixels à plus de 16/255 (anticrénelage temporel, bruit Lumen), et un premier run parfois plus
sombre. Un effet ne se revendique qu'au-delà de cette variance — en cas de doute, refaire une capture
témoin du même état et comparer les deux témoins entre eux.

## 5. Rapporter

Dans la fiche de passation : commande exacte, chemins des images, chiffres de `compare.py`, et ce que
l'image ne montre pas. Les images restent dans `Saved/` ; on ne commite pas d'artefact généré, sauf
planche choisie dans `docs/visual/` quand la mission le demande.

## Naturalisation ecologique (natural-history-001)

Reutiliser `ground-cover-capture.py` : etats `reference,natural,reference2` basculent uniquement
`anastasis.Dressing.NaturalHistory`, ciel fixe a 11 h. `ANASTASIS_GROUND_VIEWS` permet de borner
le lot a prairie_eye,lisiere_eye,sousbois_eye,riviere_eye,lande_eye,aerien. Une vue demandee
absente fait echouer le run. La preuve `natural-history-capture` est au registre :
`editor-batch.ps1 -Proofs natural-history-capture`.

Le PASS de ce script couvre la fin des captures, les hauteurs du sol **echantillonnees**
inchangees et les inventaires (mesh, compte, positions echantillonnees) identiques entre les references.
Les noms UObject ne sont pas des identites stables apres reincarnation. Il ne juge ni
la qualite ecologique, ni la botanique historique, ni la marche en PIE. Regarder les six
triplets, mesurer leur ecart contre reference/reference2, lire le GPU par vue.
