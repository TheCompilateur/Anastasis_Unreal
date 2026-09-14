// Trois archetypes, pas quarante.
//
// Ces fabriques ne sont pas le catalogue du village : elles sont la preuve que le
// contrat FAnastasisBuildingSpec suffit a decrire un lieu reel, et le materiel du
// prototype vertical. Un quatrieme archetype ne prouverait rien de plus ; il
// coutera trois lignes le jour ou la simulation aura besoin de la ferme.
//
// Chacun expose PLUSIEURS activites, ce qui est le point : un batiment n'est pas
// un usage unique. Le puits se boit et s'y puise ; l'atelier se travaille et s'y
// depose. Un PNJ qui cherche « ou deposer » et un PNJ qui cherche « ou
// travailler » peuvent viser le meme batiment sans se disputer la meme place.
//
// Rien ici ne fixe une quantite de production, un temps de travail ou un besoin
// satisfait. Le nombre de lits ou de postes est un parametre que la simulation
// fournit -- c'est elle qui sait combien de gens vivent la.

#pragma once

#include "CoreMinimal.h"
#include "Village/AnastasisVillageInteraction.h"

namespace AnastasisVillageArchetypes
{
	/**
	 * Maison : dormir.
	 *
	 * @param BedCount nombre de couches, decide par la simulation (taille du foyer)
	 *
	 * Les couches sont un seul SlotId « bed » de capacite BedCount : la simulation
	 * demande « un lit chez Duguay », pas « le lit numero 2 ». L'Occurrence de
	 * l'adresse retournee dit laquelle a ete attribuee, si cela l'interesse.
	 */
	ANASTASIS_UNREALV2_API FAnastasisBuildingSpec MakeHouse(FName BuildingId, const FTransform& Transform, int32 BedCount = 2);

	/**
	 * Puits : boire, et puiser.
	 *
	 * @param RimCount nombre de places autour de la margelle
	 *
	 * Deux activites sur une meme structure, et deliberement sur des slots
	 * distincts : celui qui remplit une cruche pour le village n'occupe pas la
	 * place de celui qui boit. Puiser est Activity.Storage.Take -- prendre dans
	 * une reserve -- parce que l'eau d'un puits est une reserve du monde, et que
	 * creer un `Activity.FetchWater` separe aurait double l'ontologie pour un cas.
	 */
	ANASTASIS_UNREALV2_API FAnastasisBuildingSpec MakeWell(FName BuildingId, const FTransform& Transform, int32 RimCount = 3);

	/**
	 * Atelier : travailler, et deposer.
	 *
	 * @param StationCount nombre de postes de travail
	 *
	 * Le metier exerce a ces postes n'apparait pas ici. Un atelier ANASTASIS est
	 * un lieu ou l'on travaille ; QUOI s'y produit est une decision economique,
	 * donc une decision de la simulation.
	 */
	ANASTASIS_UNREALV2_API FAnastasisBuildingSpec MakeWorkshop(FName BuildingId, const FTransform& Transform, int32 StationCount = 2);
}
