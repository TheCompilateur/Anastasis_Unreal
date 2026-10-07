# FIELD_TRUTH_001 — terres cultivées et fond alluvial

## Défaut observé

`AnastasisHumanGeography::Apply` appliquait au fond de vallée une alternance de teintes nommée « parcelles ». Sa fonction sinusoïdale ne lisait ni `ETileType::Field`, ni `ECropId`. Elle pouvait donc dessiner du parcellaire agricole là où le monde généré ne portait aucun champ. Le rendu du sol `Worked` connaît pourtant déjà le type `Field`; la variation de couleur de la couche Human Geography contredisait cette autorité.

## Contrat mis en place

- Autorité : `FWorldVisualSnapshot` capturé du monde généré. Un champ est une tuile `Field`; son `CropId` choisit une tonalité sobre. Une métadonnée de culture sans type `Field` ne crée pas de parcelle.
- Le reste du fond de vallée reçoit une teinte alluviale continue. Les transitions de la vallée, ses rives, sa couverture herbacée et ses matériaux restent produits par leurs systèmes existants.
- `Amount` n'entre pas dans la couleur : ce snapshot est initial et immuable. Montrer une récolte ou une repousse exigerait de lire `FVillage::LiveTileAt` en runtime, avec une mise à jour locale du rendu. Cette mission ne prétend pas l'avoir fait.
- `anastasis.Terrain.FieldTruth 1` est le nouveau défaut. `0` reproduit exactement le sinus précédent pour un A/B sur le même binaire. La bascule prend effet à la prochaine incarnation de la surface.
- Aucun type, stock, fertilité, trajectoire, eau, collision, `.umap` ou `.uasset` n'est modifié.

## Preuve et borne

`Anastasis.Terrain.HumanGeography.FieldTruth` vérifie qu'une tuile sans `Field` ne reçoit aucune fausse parcelle, qu'un `Field` reçoit la teinte de son `CropId`, que `Amount` ne simule pas une récolte dans une image statique, et qu'un recadrage du snapshot conserve les coordonnées source. La suite Unreal reste au lot d'intégration.

La preuve visuelle encore nécessaire est un A/B/A à graine, heure, carte et caméras identiques (fond de vallée vu du sol et oblique) en réincarnant avec `FieldTruth 0 / 1 / 0`. Il faut regarder les images et comparer à la variance de capture avant toute affirmation de gain artistique. La fonction supprime une fausse causalité ; elle ne valide ni l'emplacement historique des cultures, ni l'apparence finale des parcelles, ni leur évolution durant la partie.

## Décision pour l'étape suivante

Si le rendu des champs est trop discret ou trop géométrique à hauteur humaine, construire les assets de culture sur la même autorité `Field + CropId`, puis brancher l'état vivant. Ne pas réintroduire un motif décoratif sous le nom de parcellaire.