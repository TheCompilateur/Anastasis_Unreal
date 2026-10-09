#pragma once

#include "CoreMinimal.h"

class UWorld;
namespace AnastasisVillage { class FVillage; struct FBuilding; }

/**
 * ARCHITECTURE_SCALE_001 (architecture-crusade-001) : le catalogue FONCTIONNEL des batiments du village,
 * separe de leur apparence. Un archetype dit ce qu'un batiment permet (dormir, stocker, travailler, entrer),
 * ou sont sa porte, son foyer et son emprise ; le mesh n'en est que la forme. Les nombres viennent du
 * generateur (tools/unreal/create-village-architecture.py, rapport docs/unreal/architecture/architecture-kit-001.json),
 * en cm, repere local : pivot au centre de la parcelle, z = 0 = cour, +Y = acces.
 *
 * Rien ici n'ecrit dans la simulation. L'etat VIVANT (proprietaire, occupants, etat, age) vient de
 * AnastasisVillage::FBuilding et se compose avec l'archetype dans FBuildingRecord.
 */
namespace AnastasisArchitecture
{
	enum class EVariant : uint8
	{
		HousePoor,
		HouseMedium,
		HouseFarm,
		Storehouse,
		Well,
		Workshop,
		Chapel,
		/** ma-cabane-001 : la cabane du joueur, une piece levee seul (type de simulation `cabin`). */
		Cabin,
		Count
	};

	struct FRoom
	{
		const TCHAR* Name;
		const TCHAR* Use;
		double AreaM2;
		double FloorCm;
		double CeilingCm;
	};

	struct FConstructionMaterials
	{
		int32 Wood = 0;
		int32 Stone = 0;
		int32 Tile = 0;
	};

	struct FArchetype
	{
		EVariant Variant;
		const TCHAR* Id;
		/** Type de la simulation qui peut porter cet archetype (house, granary, well) ; vide = laboratoire seulement. */
		const TCHAR* SimType;
		int32 Tier;
		const TCHAR* BodyMesh;
		const TCHAR* FootingMesh;
		/** Emprise de l'assise, cm locaux (min X, min Y, max X, max Y). */
		FBox2D Footprint;
		double RidgeCm;
		/** Seuil de la porte principale (cm locaux, z = sol de la piece) et sa baie. */
		FVector DoorLocal;
		double DoorWidthCm;
		double DoorClearCm;
		/** Ou l'on se tient pour entrer : devant la porte ou le portail, cote acces. */
		FVector EntryLocal;
		bool bHasHearth;
		FVector HearthLocal;
		int32 SleepCapacity;
		int32 StorageCapacity;
		int32 WorkSlots;
		TArray<FRoom> Rooms;
		FConstructionMaterials Materials;
		/** Journees de travail par an pour tenir le batiment (toiture, enduit, torchis). */
		double MaintenanceDaysPerYear;
	};

	/** Convention d'echelle (ARCHITECTURE_SCALE_001, section 4). */
	inline constexpr double HumanCm = 170.0;
	inline constexpr double ParcelCm = 2000.0;
	inline constexpr double ParcelMarginCm = 100.0;
	inline constexpr double DoorClearMinCm = 185.0;
	inline constexpr double DoorWidthMinCm = 85.0;

	const FArchetype& Get(EVariant Variant);
	TConstArrayView<FArchetype> All();

	/**
	 * Repli d'un batiment SANS biographie : la phase de la reference seule (1-2 pauvre, 3-4 moyenne, 5-6 ferme)
	 * pour une maison, un archetype pour le grenier et le puits. Aucune graine. La forme d'une maison prise par
	 * un foyer vient de AnastasisSettlement::ProgramFor (metier du fondateur, taille du foyer). Faux pour un type inconnu.
	 */
	bool ChooseVariant(const FString& SimType, int32 HousePhase, const FString& BuildingId, EVariant& OutVariant);

	/** Mediane d'echantillons d'altitude (cm) : le niveau de la cour terrassee (ARCH-10). */
	double PadLevel(TArray<double> Samples);

	/** Points d'echantillonnage du terrain sous l'emprise, dans le repere local (grille N x N). */
	TArray<FVector2D> FootprintSamples(const FArchetype& Archetype, int32 PerSide = 5);

	/** L'archetype compose avec ce que la simulation sait du batiment, a un jour donne. */
	struct FBuildingRecord
	{
		FString Id;
		FString BuildingType;
		const FArchetype* Archetype = nullptr;
		FString Owner;
		int32 Capacity = 0;
		int32 Occupants = 0;
		int32 Inside = 0;
		int32 StorageCapacity = 0;
		int32 Stored = 0;
		int32 WorkSlots = 0;
		int32 Rooms = 0;
		FConstructionMaterials Materials;
		double MaintenanceDaysPerYear = 0.0;
		int32 AgeDays = 0;
		int32 VacantDays = 0;
		/** 0..1 : achevement du chantier, entame par l'abandon. */
		double StructuralIntegrity = 0.0;
		/** 0..1 : 1 = entretenu, decroit avec les jours vides (meme courbe que l'usure visible). */
		double Condition = 1.0;
	};

	FBuildingRecord Describe(const AnastasisVillage::FVillage& Village, const AnastasisVillage::FBuilding& Building,
		EVariant Variant, int32 Day);

	/** Une ligne de journal `ANASTASIS_ARCH record ...` (preuves, inspection). */
	FString ToLogLine(const FBuildingRecord& Record);
}
