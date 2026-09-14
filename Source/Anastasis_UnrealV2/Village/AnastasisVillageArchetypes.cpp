#include "Village/AnastasisVillageArchetypes.h"

#include "Village/AnastasisVillageTags.h"

namespace
{
	/**
	 * Ajoute un slot dont les Count places sont reparties en arc, chacune tournee
	 * vers le centre du batiment.
	 *
	 * Un seul SlotId porte la capacite -- la simulation demande « un lit », pas « le
	 * lit numero 2 » -- mais chaque occurrence recoit sa propre pose, sinon les N
	 * places se superposeraient et deux PNJ occuperaient le meme point.
	 *
	 * Les distances sont en centimetres, unite monde d'Unreal. Ce sont des valeurs
	 * de prototype : elles donnent une geometrie lisible en debug et des distances
	 * distinctes entre places, ce dont les tests ont besoin pour prouver que le tri
	 * par distance trie vraiment. Quand les meshes arriveront, ces poses viendront
	 * des points d'attache des modeles, pas d'ici.
	 */
	void AddArcSlot(
		FAnastasisBuildingSpec& Spec,
		const FName SlotId,
		const FGameplayTag& ActivityTag,
		const int32 Count,
		const double RingRadius,
		const double StartAngleDegrees,
		const double AngleStepDegrees)
	{
		if (Count < 1)
		{
			return;
		}

		FAnastasisInteractionSlotSpec& Slot = Spec.Slots.AddDefaulted_GetRef();
		Slot.SlotId = SlotId;
		Slot.ActivityTag = ActivityTag;
		Slot.Capacity = Count;
		Slot.OccurrencePoses.Reserve(Count);

		for (int32 Occurrence = 0; Occurrence < Count; ++Occurrence)
		{
			const double AngleRadians = FMath::DegreesToRadians(StartAngleDegrees + AngleStepDegrees * Occurrence);
			const FVector Offset(RingRadius * FMath::Cos(AngleRadians), RingRadius * FMath::Sin(AngleRadians), 0.0);

			// Tournee vers le centre du batiment.
			Slot.OccurrencePoses.Emplace((-Offset).Rotation(), Offset);
		}

		// Renseigne aussi la pose simple : elle ne sert pas a l'enregistrement puisque
		// OccurrencePoses la supplante, mais elle garde le spec lisible en inspection.
		Slot.LocalOffset = Slot.OccurrencePoses[0].GetLocation();
		Slot.LocalRotation = Slot.OccurrencePoses[0].Rotator();
	}
}

namespace AnastasisVillageArchetypes
{
	FAnastasisBuildingSpec MakeHouse(const FName BuildingId, const FTransform& Transform, const int32 BedCount)
	{
		FAnastasisBuildingSpec Spec;
		Spec.BuildingId = BuildingId;
		Spec.BuildingTag = AnastasisVillageTags::Building_House;
		Spec.Transform = Transform;

		// Les couches s'alignent d'un cote de la piece : un arc serre, pas un cercle.
		AddArcSlot(Spec, TEXT("bed"), AnastasisVillageTags::Activity_Sleep, FMath::Max(1, BedCount), 150.0, 0.0, 25.0);

		return Spec;
	}

	FAnastasisBuildingSpec MakeWell(const FName BuildingId, const FTransform& Transform, const int32 RimCount)
	{
		FAnastasisBuildingSpec Spec;
		Spec.BuildingId = BuildingId;
		Spec.BuildingTag = AnastasisVillageTags::Building_Well;
		Spec.Transform = Transform;

		// Boire au bord : les places se repartissent sur la moitie accessible de la margelle.
		AddArcSlot(Spec, TEXT("rim"), AnastasisVillageTags::Activity_Drink, FMath::Max(1, RimCount), 120.0, 0.0, 45.0);

		// Puiser : une seule place, a l'oppose, donc exclusive. Deux seaux dans le
		// meme puits au meme instant est une image, pas une mecanique.
		AddArcSlot(Spec, TEXT("draw"), AnastasisVillageTags::Activity_Storage_Take, 1, 120.0, 180.0, 0.0);

		return Spec;
	}

	FAnastasisBuildingSpec MakeWorkshop(const FName BuildingId, const FTransform& Transform, const int32 StationCount)
	{
		FAnastasisBuildingSpec Spec;
		Spec.BuildingId = BuildingId;
		Spec.BuildingTag = AnastasisVillageTags::Building_Workshop;
		Spec.Transform = Transform;

		AddArcSlot(Spec, TEXT("station"), AnastasisVillageTags::Activity_Work, FMath::Max(1, StationCount), 200.0, 90.0, 35.0);

		// La reserve de l'atelier : deposer ce qui a ete produit.
		AddArcSlot(Spec, TEXT("store"), AnastasisVillageTags::Activity_Storage_Deposit, 1, 200.0, 270.0, 0.0);

		return Spec;
	}
}
