# Brancher un asset visuel — présentation

Le Data Asset `/Game/Anastasis/Presentation/DA_AnastasisPresentation` porte
les entrées Forest et Ruin. Il fait autorité lorsqu'il est chargé. Travailler
dans un worktree dédié ; la racine canonique est réservée à l'intégration.

## Modifier le rendu existant

1. Ouvrir le projet du worktree dans l'éditeur.
2. Ouvrir `Content/Anastasis/Presentation/DA_AnastasisPresentation`.
3. Pour Forest, conserver les deux familles × quatre statures des huit variantes
   et leurs biais d'échelle. Le choix est déterministe et tient compte du site.
4. Le slot 0 porte `M_AnastasisVegetation`, le slot supplémentaire 1
   `M_AnastasisBark`. Un remplacement de mesh doit respecter ces slots ou
   explicitement adapter les overrides.
5. Pour Ruin, le binding de référence est
   `/Game/Anastasis/Architecture/SM_Ruin_Generic_01`.
6. Sauvegarder, puis vérifier dans une nouvelle session de rendu.
   Le resolver garde le registre en cache : ne pas supposer qu'un changement
   sur disque rafraîchit automatiquement une session déjà ouverte.

Les arbres générés sont normalisés sur une hauteur de 100 uu. L'enveloppe
3.6–5.0, le multiplicateur de couche écologique et le biais de variante
déterminent leur taille finale. Un arbre importé déjà à taille réelle demande
une enveloppe adaptée ; ne pas recopier ces facteurs aveuglément.

## Créer ou reconstruire

Exécuter `tools/unreal/presentation-registry.py` dans Unreal :

- Registre présent, sans option : inspection seule, retouches conservées.
- Registre absent : création complète, huit arbres avec matériaux et ruine dédiée.
- Reconstruction explicite : remplacement des entrées dans le même asset,
  sans suppression/recréation du package. Les retouches sont alors remplacées.

Dans le PowerShell qui lancera l'éditeur :

```powershell
$env:ANASTASIS_PRESENTATION_REBUILD = '1'
# Exécuter tools/unreal/presentation-registry.py dans cet éditeur isolé.
# Une fois terminé :
Remove-Item Env:ANASTASIS_PRESENTATION_REBUILD
```

Le script charge tous les meshes et matériaux requis avant de modifier
l'asset. Si une dépendance manque, il échoue sans toucher aux entrées existantes.
Un échec de sauvegarde est une erreur, jamais un message de réussite.

La recette réutilise `set_tree_grammar.py` (variantes et paramètres Forest)
et la cible RUIN de `set_presentation_meshes.py`. Ces modules sont importables
sans exécuter leurs commandes d'édition. Ils restent utilisables séparément
pour ne recâbler qu'une catégorie.

## Distinguer reconstruction et secours runtime

Si le registre est absent ou vide, le C++ utilise `CreateCodeDefaults` :
huit variantes d'arbres et un cylindre de secours pour Ruin. Ce repli dégradé
reste explicite et n'est pas la recette de reconstruction du Data Asset.
Cette correction Python ne change pas le C++ ni les assets sauvegardés existants.

`source=asset` dans `ANASTASIS_PRESENTATION_REGISTRY` confirme le chargement
du registre ; cela ne prouve pas à lui seul chaque variante ou son apparence.
Un mesh sélectionné manquant peut faire échouer sa résolution.

`inspect_presentation_registry.py` lit les chemins réellement enregistrés.
Le log de reconstruction précise `evidence=memory` : après une sauvegarde,
une seconde session indépendante est nécessaire pour prouver la relecture
disque. Une comparaison visuelle reste nécessaire pour accepter le rendu.

## Frontières

Le registre choisit le look, pas les décisions de simulation. Le placement
écologique et l'ancrage relèvent de WorldEmbodiment/EcologicalDressing.
Désactiver une entrée via Enabled est une décision de présentation explicite.
Ajouter une nouvelle catégorie sémantique ou une boucle village/joueur
demande une mission distincte.

Tests de contrat sans moteur :

```powershell
python -B tools/unreal/tests/test_presentation_registry.py
```

Ils couvrent reconstruction, diversité, matériaux, préservation, dépendances
manquantes, création et sauvegarde en échec. Ils ne certifient pas la
sérialisation Unreal ni le rendu.
