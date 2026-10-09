# Protocole asset V1 — contrat, validation, scène

Une image de référence, un `UStaticMesh` sauvé et un objet visible dans ANÁSTASIS
sont trois états distincts. Une mission d'asset livre une **famille bornée** et
conserve la chaîne `référence → générateur → .uasset → consommateur → capture`.

## Contrat de mission

Copier `contracts/_TEMPLATE.json` vers `contracts/<mission>.json`. Renseigner :

- `id`, `family`, `intent` : une seule fonction visuelle et son périmètre ;
- `references` et `reference_provenance` : planches, briefs ou sources et leur
  origine/licence, ou `UNKNOWN` explicite quand aucun import externe n'en dépend ; une image IA
  reste une référence, jamais une preuve de géométrie ou d'histoire ;
- `generator_files` : scripts ou fichiers sources rejouables ; les `.uasset`
  binaires seuls ne sont pas une source de fabrication ;
- `assets` : chemin Unreal, matières par slot, nombre de triangles par LOD, dimensions
  maximales en cm, intervalle du Z minimal (pivot/contact au sol), et référence
  textuelle du consommateur actuel ;
- `capture_proof` : nom de la preuve du registre `tools/unreal/proofs.txt` ;
- `collision_policy`, `keep`, `reject` : contrat de décision, non vérifié
  automatiquement dans cette V1.

Les chiffres d'un asset existant se relèvent dans l'éditeur avant d'être figés
au contrat. Pour un nouvel asset, ils se déterminent selon l'échelle et la
densité d'instances prévues, puis se révisent avec une mesure de scène ; ne pas
copier les budgets d'une autre famille.

## Portes

1. **Reachability** : trouver le consommateur réel. Le validateur vérifie que
   les chaînes de référence déclarées sont dans ses fichiers source ; cette
   lecture ne prouve ni l'exécution ni la visibilité. Si le consommateur manque,
   livrer « bibliothèque seulement » ou ouvrir une mission C++ distincte.
2. **Production** : fabriquer dans un worktree par script d'autorité Unreal,
   ou importer depuis une source 3D externe conservée. Ne jamais écrire un
   `.uasset` comme du texte. Ne régénérer que les assets possédés par la mission.
3. **MEC** : exécuter `asset-contract-validate.py` dans un **nouveau processus
   éditeur**. Il lit le contrat et les assets sauvés : type, chemin, matériaux,
   LOD, triangles, bounds et pivot. Un `ASSET_CONTRACT PASS` n'établit pas la
   fidélité des pixels, l'absence de collision ni la reproductibilité binaire.
4. **SCN** : rejouer `capture_proof` sur la même carte, graine, lumière et caméra,
   avec A/B/A quand la preuve le permet. Ouvrir chaque image, comparer au témoin
   répété et mesurer le GPU si l'asset est multiplié. Le PASS instrumental de
   capture n'est pas un verdict esthétique.
5. **PLY** : parcours humain à vitesse normale si une interaction, un obstacle,
   une lecture à hauteur de joueur ou un comportement est revendiqué.
6. **Passation** : fiche `docs/unreal/handoffs/<mission>.md`, `PROOFS:` au registre,
   commit puis `agent-worktree.ps1 finish`. Seul l'intégrateur désigné rejoue le
   lot et avance `main`.

## Pilote V1 : PonticMicro

`contracts/pontic-micro-v1.json` fige les quatre meshes déjà consommés par
`AnastasisPonticWaterMicro` et `AnastasisWorldEmbodiment`. Le validateur est
inscrit comme `asset-contract-pontic-micro` ; la capture existante
`pontic-water-micro-capture` reste l'autorité SCN instrumentale. Aucune
géométrie, matière, règle de placement ou carte n'est modifiée par la V1.

```powershell
tools\unreal\editor-batch.ps1 -Proofs asset-contract-pontic-micro,pontic-water-micro-capture
```

Le contrôle de collision, les UV/normales détaillées, la licence d'une source
externe et le verdict joueur restent des lignes explicites de la fiche de
passation, selon le type d'asset. La V1 ne les déclare pas PASS par défaut.
