# AnastasisCore — archive du projet legacy 5.7

Code récupéré de `Anastasis_Unreal` (Unreal Engine 5.7, module `AnastasisCore`)
avant la suppression définitive du projet legacy, le 2026-09-12.

**Ce n'est pas du code vivant.** Il ne compile pas dans ce projet et n'est pas
référencé par `Anastasis_UnrealV2`. Il est conservé parce qu'il n'existait nulle
part ailleurs : `AnastasisSim` l'a remplacé fonctionnellement, mais sans reprendre
ces fichiers.

## Contenu

| Fichier | Rôle |
|---|---|
| `AnastasisWorldContract.{h,cpp}` | contrat de monde portable |
| `AnastasisTimeControl.{h,cpp}` | contrôle du temps de simulation |
| `AnastasisActorKinematics.{h,cpp}` | cinématique d'acteur |
| `AnastasisInfrastructure.{h,cpp}` | infrastructure |
| `AnastasisNumeric.h` | primitives numériques |
| `Private/Tests/AnastasisCoreTests.cpp` | tests d'automation du module |
| `AnastasisCore.Build.cs` | définition du module |
| `Anastasis_Unreal.uproject.reference` | `.uproject` legacy, pour la déclaration du module |

Les 12 fichiers ont été vérifiés par SHA-256 contre la source avant suppression.

## Provenance

`AnastasisCore` était le contrat portable sans dépendance Engine du projet 5.7.
Son successeur est `Source/AnastasisSim/`, qui porte la parité avec le simulateur JS.
Consulter ces fichiers pour retrouver une intention de conception, pas pour les réintégrer tels quels.
