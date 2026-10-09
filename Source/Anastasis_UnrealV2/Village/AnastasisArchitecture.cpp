#include "Village/AnastasisArchitecture.h"

#include "Village/AnastasisBuildingMetabolism.h"
#include "Village/AnastasisVillage.h"

namespace AnastasisArchitecture
{
	namespace
	{
		TArray<FArchetype> BuildCatalogue()
		{
			// Valeurs relevees dans docs/unreal/architecture/architecture-kit-001.json (generateur
			// create-village-architecture.py, recette architecture-crusade-001-v1). Le test
			// Anastasis.Village.Architecture.Rapport les relit et refuse toute derive.
			TArray<FArchetype> C;
			C.Add({EVariant::HousePoor, TEXT("house_poor"), TEXT("house"), 1,
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_House_Poor_01.SM_Arch_House_Poor_01"),
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_House_Poor_01_Footing.SM_Arch_House_Poor_01_Footing"),
				FBox2D(FVector2D(-515, -615), FVector2D(611, 685)), 434.0,
				FVector(102, 75, 18), 96.0, 192.0, FVector(102, 210, 0),
				true, FVector(-60, -397, 58), 3, 20, 2,
				{{TEXT("piece"), TEXT("vie+sommeil+foyer"), 22.7, 18.0, 264.0}},
				{24, 8, 0}, 6.0});
			C.Add({EVariant::HouseMedium, TEXT("house_medium"), TEXT("house"), 2,
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_House_Medium_01.SM_Arch_House_Medium_01"),
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_House_Medium_01_Footing.SM_Arch_House_Medium_01_Footing"),
				FBox2D(FVector2D(-679, -869), FVector2D(739, 888)), 815.0,
				FVector(0, -10, 14), 140.0, 212.0, FVector(340, 245, 0),
				true, FVector(-404, -420, 340), 6, 60, 4,
				{{TEXT("salle"), TEXT("vie+foyer+veillee"), 34.0, 300.0, 554.0},
				 {TEXT("chambre"), TEXT("sommeil"), 24.2, 300.0, 554.0},
				 {TEXT("rez"), TEXT("etable+reserve"), 47.0, 14.0, 278.0}},
				{30, 40, 12}, 4.0});
			C.Add({EVariant::HouseFarm, TEXT("house_farm"), TEXT("house"), 3,
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_House_Farm_01.SM_Arch_House_Farm_01"),
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_House_Farm_01_Footing.SM_Arch_House_Farm_01_Footing"),
				FBox2D(FVector2D(-888, -888), FVector2D(888, 888)), 809.0,
				FVector(60, 780, 0), 260.0, 215.0, FVector(60, 640, 0),
				true, FVector(-490, -650, 340), 8, 120, 4,
				{{TEXT("salle"), TEXT("vie+foyer+veillee"), 29.4, 300.0, 554.0},
				 {TEXT("chambre"), TEXT("sommeil"), 20.9, 300.0, 554.0},
				 {TEXT("rez"), TEXT("reserve"), 40.0, 14.0, 278.0},
				 {TEXT("grange"), TEXT("etable+fenil"), 42.8, 0.0, 260.0}},
				{52, 70, 14}, 8.0});
			C.Add({EVariant::Storehouse, TEXT("storehouse"), TEXT("granary"), 1,
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_Storehouse_01.SM_Arch_Storehouse_01"),
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_Storehouse_01_Footing.SM_Arch_Storehouse_01_Footing"),
				FBox2D(FVector2D(-850, -798), FVector2D(888, 783)), 780.0,
				FVector(-140, 192, 12), 240.0, 260.0, FVector(-140, 410, 0),
				false, FVector::ZeroVector, 0, AnastasisVillage::GranaryFoodCap, 1,
				{{TEXT("reserve"), TEXT("stock commun"), 66.3, 12.0, 300.0}},
				{40, 50, 16}, 5.0});
			C.Add({EVariant::Well, TEXT("well"), TEXT("well"), 1,
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_Well_01.SM_Arch_Well_01"),
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_Well_01_Footing.SM_Arch_Well_01_Footing"),
				FBox2D(FVector2D(-325, -490), FVector2D(306, 322)), 282.0,
				FVector(0, 150, 0), 0.0, 0.0, FVector(0, 260, 0),
				false, FVector::ZeroVector, 0, 0, 1, {}, {10, 18, 0}, 2.0});
			C.Add({EVariant::Workshop, TEXT("workshop"), TEXT(""), 1,
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_Workshop_01.SM_Arch_Workshop_01"),
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_Workshop_01_Footing.SM_Arch_Workshop_01_Footing"),
				FBox2D(FVector2D(-505, -453), FVector2D(505, 413)), 460.0,
				FVector(0, 260, 4), 680.0, 290.0, FVector(0, 460, 0),
				false, FVector::ZeroVector, 0, 10, 2,
				{{TEXT("atelier"), TEXT("forge"), 40.3, 4.0, 300.0}},
				{20, 36, 10}, 5.0});
			C.Add({EVariant::Chapel, TEXT("chapel"), TEXT(""), 1,
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_Chapel_01.SM_Arch_Chapel_01"),
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_Chapel_01_Footing.SM_Arch_Chapel_01_Footing"),
				FBox2D(FVector2D(-850, -463), FVector2D(649, 463)), 1040.0,
				FVector(552, 0, 10), 140.0, 280.0, FVector(820, 0, 0),
				false, FVector::ZeroVector, 0, 0, 1,
				{{TEXT("nef"), TEXT("culte+assemblee"), 55.1, 10.0, 559.0}},
				{30, 90, 20}, 4.0});
			// ma-cabane-001 : la cabane du joueur. Une piece de 16,8 m2 a foyer, un dormeur, levee seul : plus petite
			// que sa parcelle (17,7 %), c'est voulu (« Elle doit etre petite », Alexandre, 2026-10-09).
			C.Add({EVariant::Cabin, TEXT("cabin"), TEXT("cabin"), 1,
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_Cabin_01.SM_Arch_Cabin_01"),
				TEXT("/Game/Anastasis/VillageArchitecture/SM_Arch_Cabin_01_Footing.SM_Arch_Cabin_01_Footing"),
				FBox2D(FVector2D(-391, -429), FVector2D(432, 432)), 399.0,
				FVector(0, 225, 18), 96.0, 192.0, FVector(0, 360, 0),
				true, FVector(110, -229, 58), 1, 4, 1,
				{{TEXT("piece"), TEXT("vie+sommeil+foyer"), 16.8, 18.0, 247.0}},
				{8, 2, 0}, 3.0});
			// dormir-couche-001 : banquettes et volume habite des logis, releves du meme rapport (`benches`, `interior`).
			auto Dwelling = [&C](EVariant V, TArray<FBench> Benches, const FBox& Interior)
			{
				C[static_cast<int32>(V)].Benches = MoveTemp(Benches);
				C[static_cast<int32>(V)].Interior = Interior;
			};
			Dwelling(EVariant::HousePoor, {{FBox2D(FVector2D(-290, -440), FVector2D(-206, -58)), 64.0}},
				FBox(FVector(-295, -445, 18), FVector(295, 25, 265)));
			Dwelling(EVariant::HouseMedium, {{FBox2D(FVector2D(-437, -683), FVector2D(54, -603)), 346.0},
				{FBox2D(FVector2D(92, -683), FVector2D(457, -593)), 346.0}},
				FBox(FVector(-450, -690, 14), FVector(450, -70, 555)));
			Dwelling(EVariant::HouseFarm, {{FBox2D(FVector2D(-757, -723), FVector2D(-324, -643)), 346.0},
				{FBox2D(FVector2D(-286, -723), FVector2D(37, -633)), 346.0}},
				FBox(FVector(-770, -730, 14), FVector(30, -130, 555)));
			Dwelling(EVariant::Cabin, {{FBox2D(FVector2D(-230, -270), FVector2D(-146, 102)), 64.0}},
				FBox(FVector(-235, -275, 18), FVector(235, 175, 248)));
			check(C.Num() == static_cast<int32>(EVariant::Count));
			for (int32 I = 0; I < C.Num(); ++I)
			{
				check(static_cast<int32>(C[I].Variant) == I);
			}
			return C;
		}

		const TArray<FArchetype>& Catalogue()
		{
			static const TArray<FArchetype> Data = BuildCatalogue();
			return Data;
		}
	}

	const FArchetype& Get(EVariant Variant)
	{
		return Catalogue()[FMath::Clamp(static_cast<int32>(Variant), 0, static_cast<int32>(EVariant::Count) - 1)];
	}

	TConstArrayView<FArchetype> All()
	{
		return Catalogue();
	}

	bool ChooseVariant(const FString& SimType, int32 HousePhase, const FString& BuildingId, EVariant& OutVariant)
	{
		if (SimType == AnastasisVillage::WellType)
		{
			OutVariant = EVariant::Well;
			return true;
		}
		if (SimType == AnastasisVillage::GranaryType)
		{
			OutVariant = EVariant::Storehouse;
			return true;
		}
		if (SimType == AnastasisVillage::CabinType)
		{
			OutVariant = EVariant::Cabin;
			return true;
		}
		if (SimType != AnastasisVillage::HouseType)
		{
			return false;
		}
		// Phase de la reference seule : 1-2 noyau, 3-4 extension, 5-6 maison amelioree. La forme d'une maison
		// prise par un foyer vient de sa biographie (AnastasisSettlement::ProgramFor) ; ceci n'est que le repli
		// d'une maison sans histoire. Aucune graine : settlement-morphogenesis-001 a retire le tirage par identifiant.
		const int32 Tier = FMath::Clamp(HousePhase, 1, 6) <= 2 ? 0 : (HousePhase <= 4 ? 1 : 2);
		OutVariant = Tier == 0 ? EVariant::HousePoor : (Tier == 1 ? EVariant::HouseMedium : EVariant::HouseFarm);
		return true;
	}

	TArray<FSleepSpot> SleepSpots(const FArchetype& A, int32 Count)
	{
		TArray<FSleepSpot> Out;
		if (Count <= 0 || A.Benches.IsEmpty()) return Out;
		// Une rangee de couchages le long d'un axe : autant de places de 185 cm qu'il en tient, centrees dans leur part.
		auto Row = [&Out, Count](bool bAlongX, double From, double To, double Across, double Z, bool bOnBench)
		{
			const double L = To - From;
			const int32 N = FMath::FloorToInt32(L / SleeperLengthCm);
			if (N <= 0) return;
			const double Slot = L / N;
			for (int32 I = 0; I < N && Out.Num() < Count; ++I)
			{
				const double S0 = From + I * Slot + 0.5 * (Slot - SleeperLengthCm);
				const double S1 = S0 + SleeperLengthCm;
				FSleepSpot Spot;
				Spot.Feet = bAlongX ? FVector(S0, Across, Z) : FVector(Across, S0, Z);
				Spot.Head = bAlongX ? FVector(S1, Across, Z) : FVector(Across, S1, Z);
				Spot.bOnBench = bOnBench;
				Out.Add(Spot);
			}
		};
		for (const FBench& B : A.Benches)
		{
			const FVector2D Size = B.Box.GetSize();
			const bool bAlongX = Size.X >= Size.Y;
			const FVector2D C = B.Box.GetCenter();
			// Une banquette plus courte qu'un dormeur porte quand meme un dormeur (jambes repliees).
			const double From = bAlongX ? B.Box.Min.X : B.Box.Min.Y;
			const double To = FMath::Max(bAlongX ? B.Box.Max.X : B.Box.Max.Y, From + SleeperLengthCm);
			Row(bAlongX, From, To, bAlongX ? C.Y : C.X, B.TopCm, true);
		}
		if (Out.Num() >= Count || !A.Interior.IsValid) return Out;
		// Des nattes au sol, rang apres rang le long de la premiere banquette, vers le milieu de la piece.
		const FBench& B = A.Benches[0];
		const FVector2D Size = B.Box.GetSize();
		const bool bAlongX = Size.X >= Size.Y;
		const double Floor = B.TopCm - 46.0;
		const double BenchMid = bAlongX ? B.Box.GetCenter().Y : B.Box.GetCenter().X;
		const double BenchHalf = 0.5 * (bAlongX ? Size.Y : Size.X);
		const double RoomMid = bAlongX ? A.Interior.GetCenter().Y : A.Interior.GetCenter().X;
		const double Dir = RoomMid >= BenchMid ? 1.0 : -1.0;
		const double Lo = (bAlongX ? A.Interior.Min.Y : A.Interior.Min.X) + 35.0;
		const double Hi = (bAlongX ? A.Interior.Max.Y : A.Interior.Max.X) - 35.0;
		// Dans le long de la banquette seulement : une cloison peut couper la piece au-dela (maison a etage).
		const double From = bAlongX ? B.Box.Min.X : B.Box.Min.Y;
		const double To = FMath::Max(bAlongX ? B.Box.Max.X : B.Box.Max.Y, From + SleeperLengthCm);
		for (int32 R = 0; Out.Num() < Count; ++R)
		{
			const double Across = BenchMid + Dir * (BenchHalf + 50.0 + 75.0 * R);
			if (Across < Lo || Across > Hi) break;
			Row(bAlongX, From, To, Across, Floor, false);
		}
		return Out;
	}

	double PadLevel(TArray<double> Samples)
	{
		if (Samples.Num() == 0)
		{
			return 0.0;
		}
		Samples.Sort();
		const int32 Mid = Samples.Num() / 2;
		return Samples.Num() % 2 ? Samples[Mid] : 0.5 * (Samples[Mid - 1] + Samples[Mid]);
	}

	TArray<FVector2D> FootprintSamples(const FArchetype& Archetype, int32 PerSide)
	{
		TArray<FVector2D> Out;
		PerSide = FMath::Max(PerSide, 2);
		const FBox2D& B = Archetype.Footprint;
		for (int32 J = 0; J < PerSide; ++J)
		{
			for (int32 I = 0; I < PerSide; ++I)
			{
				// En retrait de 10 % : le bord du soutenement peut s'enterrer, c'est le coeur qui fixe la cour.
				const double U = 0.1 + 0.8 * I / (PerSide - 1);
				const double V = 0.1 + 0.8 * J / (PerSide - 1);
				Out.Add(FVector2D(FMath::Lerp(B.Min.X, B.Max.X, U), FMath::Lerp(B.Min.Y, B.Max.Y, V)));
			}
		}
		return Out;
	}

	FBuildingRecord Describe(const AnastasisVillage::FVillage& Village, const AnastasisVillage::FBuilding& Building,
		EVariant Variant, int32 Day)
	{
		const FArchetype& A = Get(Variant);
		FBuildingRecord R;
		R.Id = Building.Id;
		R.BuildingType = Building.Type;
		R.Archetype = &A;
		R.Owner = Building.Owner;
		R.Inside = Village.InsideOf(Building.Id).Num();
		R.WorkSlots = A.WorkSlots;
		R.Rooms = A.Rooms.Num();
		R.Materials = A.Materials;
		R.MaintenanceDaysPerYear = A.MaintenanceDaysPerYear;
		if (Building.Type == AnastasisVillage::HouseType || Building.Type == AnastasisVillage::CabinType)
		{
			// La capacite qui compte est celle de la simulation ; l'archetype dit combien de couchages il offre.
			R.Capacity = Village.ShelterCapacity(Building);
			R.Occupants = Village.CountShelterOccupants(Building.Id);
			R.VacantDays = Building.IsCompleted() ? AnastasisVillage::VacantAgeDays(Building, Day) : 0;
			if (Building.CompletedDay >= 0)
			{
				R.VacantDays = FMath::Min(R.VacantDays, FMath::Max(0, Day - Building.CompletedDay));
			}
		}
		R.StorageCapacity = Building.Type == AnastasisVillage::GranaryType ? AnastasisVillage::GranaryFoodCap : A.StorageCapacity;
		R.Stored = Building.FoodPhysical;
		R.AgeDays = Building.CompletedDay >= 0 ? FMath::Max(0, Day - Building.CompletedDay) : 0;
		const double Neglect = AnastasisMetabolism::NeglectForDays(R.VacantDays);
		R.Condition = 1.0 - Neglect;
		R.StructuralIntegrity = FMath::Clamp(Building.Progress, 0.0, 1.0) * (1.0 - 0.5 * Neglect);
		return R;
	}

	FString ToLogLine(const FBuildingRecord& R)
	{
		return FString::Printf(
			TEXT("ANASTASIS_ARCH record id=%s type=%s archetype=%s tier=%d owner=%s capacity=%d occupants=%d inside=%d storage=%d/%d rooms=%d work=%d materials=w%d/s%d/t%d maintenance=%.1f age=%d vacant=%d condition=%.2f integrity=%.2f"),
			*R.Id, *R.BuildingType, R.Archetype ? R.Archetype->Id : TEXT("-"), R.Archetype ? R.Archetype->Tier : 0,
			R.Owner.IsEmpty() ? TEXT("-") : *R.Owner, R.Capacity, R.Occupants, R.Inside, R.Stored, R.StorageCapacity,
			R.Rooms, R.WorkSlots, R.Materials.Wood, R.Materials.Stone, R.Materials.Tile, R.MaintenanceDaysPerYear,
			R.AgeDays, R.VacantDays, R.Condition, R.StructuralIntegrity);
	}
}
