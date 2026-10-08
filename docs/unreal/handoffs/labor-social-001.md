# HANDOFF: labor-social-001 (ERGON)

## MISSION

Lier le bois effectivement coupe par un bûcheron autonome au stock d'un chantier ouvert, puis à la construction d'un puits utilisable par un autre habitant. Portée : une seule chaîne productive et un seul type de transfert, sans politique économique générale.

## PROVENANCE ET COORDINATION OIKOS

- ERGON : `agent/labor-social-001`, `C:\dev\ANASTASIS_WORKTREES\labor-social-001`, créée sur `423955c5577ca1be29500e7d106cc9475b1b43ed`, puis rebasée sur le `main` observé à `640fa3e81a522997912ef45941f6f3410051b99e`.
- OIKOS observé : `agent/oikos-vivant-001`, `C:\dev\ANASTASIS_WORKTREES\oikos-vivant-001`, HEAD au Gate 0 `039c51fbd37681b2f5dd3a95727313768ab023a9` avec changements non commis ; HEAD propre observé ensuite `f123159ae156140cbf33a4b703439cc61301055e`. Son `FILES_OWNED` fait foi pour sa maison et sa présentation.
- Contrat écrit partagé : `C:\dev\ANASTASIS_WORKTREES\.coordination\labor-social-001-oikos-vivant-001.md`. Dépôt de coordination, sans accusé de réception OIKOS ; ce fichier n'est pas une messagerie temps réel.
- Interface : `FNpc` conserve les besoins, le but, l'inventaire et le trajet ; `FBuilding::Materials` conserve le devis, le stock de chantier et la consommation. ERGON ne touche aucun fichier `Source/Anastasis_UnrealV2/` possédé par OIKOS ; aucune interface de maison n'est ajoutée. Le rebase sur `main` n'a pas eu de conflit ; il ne vaut pas validation de l'empilement futur avec OIKOS.

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h`
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp`
- `Source/AnastasisSim/Private/Village/AnastasisVillageWood.cpp`
- `Source/AnastasisSim/Private/Tests/AnastasisVillageWoodTests.cpp`
- `Source/AnastasisSim/ECARTS.md`
- `docs/unreal/handoffs/labor-social-001.md`

## COMMIT

Branche `agent/labor-social-001` ; le marqueur de passation porte le HEAD exact après `finish`. Aucun merge ou push ERGON.

## MEC

- Reproduction avant correction : `Anastasis.Sim.Wood.LocalConservation` attestait le bois coupé et retenu en `InventoryWood` ; le bâtisseur lit `Materials.StockWood`. Un premier scénario échouait au choix `build` improductif, puis au repli `PerformFoodSupply` interceptant `deliver` avant la livraison bois. Ces échecs instrumentés ne sont pas des PASS.
- `Anastasis.Sim.Wood` sur le commit rebasé : `TESTS::PASS`, 5 cas annoncés, 5 PASS, 0 FAIL. `SiteNoGhost` maintient le stock de chantier à zéro quand la forêt est au seuil de réserve ; `AutonomousWellChain` mesure récolte, trajet, transfert, conservation, consommation, puits achevé, usage par un autre habitant et soif du travailleur après le travail.
- Conditions initiales du scénario : puits existant pour les travailleurs, gisement de 30 bois, chantier de puits ouvert avec 18 pierres créditées. Aucune commande joueur pendant les 60 Hz simulés ; tout le bois vient du gisement par le bûcheron.
- BUILD : `BUILD::PASS` après rebase sur `640fa3e8` (15 actions, 73.15 s). Le tout premier build, antérieur au rebase, avait refusé son postflight car `ECARTS.md` avait changé pendant la compilation ; il n'est pas compté comme preuve.
- GATES : `check-ecarts.mjs -base main -handoff docs/unreal/handoffs/labor-social-001.md` → `ECARTS::PASS`, 0 FAIL ; `check-state-fields.mjs -base main` → `STATE_FIELDS::PASS`, structures=44, aucun champ ajouté ; `git diff --check main...HEAD` → 0 erreur.
- NON-RÉGRESSION locale : `Wood.Bounds`, `Wood.ExhaustedStand`, `Wood.LocalConservation`, `Wood.SiteNoGhost` et `Wood.AutonomousWellChain` passent ensemble sur la base rebasée. Le dernier scénario exerce le bâtisseur, le puits et l'accès à l'eau. Le run élargi `Chantier+Puits` a été retiré de la file avant tout lancement Unreal : cinq demandes d'éditeur le précédaient, avec un cook canonique actif. La suite intégrateur n'est pas revendiquée.

## PROOFS

PROOFS: (aucune)

## SCN

LOCAL_FIX : une ligne `deliver` pour le bois du bûcheron vise un vrai chantier en manque, et `Perform` l'envoie au registre de matériaux avant le repli alimentaire. Quand tous les chantiers ouverts sont bloqués par le bois, `build` ne retient pas le bûcheron. Aucun rendement ni devis modifié.

SYSTEM_EFFECT dans le test du simulateur : 12 bois récoltés ; 10 livrés puis consommés au chantier ; 2 encore portés et 18 restés au gisement, soit 30 conservés. Le bâtisseur achève le puits ; un autre PNJ y boit. La soif du bûcheron déclenchée après la construction trouve une réponse. Portée : ce village-test avec chantier ouvert et pierre disponible, pas une économie complète.

## PLY

PLAYER_EFFECT : UNKNOWN — aucune preuve joueur ou lecture visuelle revendiquée.

## ECARTS

- n°43 ouvert : sous-ensemble du transport JS, bois du bûcheron livré directement au chantier. La logistique de dépôts et la scierie ne sont pas portées.
- n°30 précisé : son absence de livraison vers chantier est partiellement levée par n°43 ; ses autres limites demeurent.

## INTEGRATION_RISK

- OIKOS a rendu son worktree propre ; son commit `f123159a` n'est pas ancêtre du `main` observé à `640fa3e8`. L'intégrateur doit encore juger l'empilement. Aucun fichier OIKOS n'a été modifié par ERGON ; son accusé de réception du contrat écrit reste inconnu.
- La racine canonique contient `download.png` et `Build/` non suivis, exclus de cette mission.
- La construction du puits suppose le stock de pierre disponible ; la livraison autonome de pierre est déjà une autre extension opt-in et n'est pas attribuée à ERGON.
- Aucun script PIE dédié : les commandes de l'hôte ne configurent pas ce scénario sans porteur opt-in, qui masquerait la contribution du bûcheron. L'automatisation C++ en éditeur couvre le mécanisme et le scénario du simulateur ; elle ne constitue pas une preuve PIE ou joueur.
- Si `main` avance, comparer les arbres et rejouer `finish` avant admission. Pas de merge manuel.

## STOP

Arrêt après mécanisme, scénario, contrôles locaux et passation `finish`. Pas de scierie, d'intérieur domestique, de politique de production générale ni de revendication PLAYER_EFFECT.
