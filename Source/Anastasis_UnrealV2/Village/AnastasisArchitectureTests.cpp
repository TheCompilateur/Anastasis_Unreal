#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Village/AnastasisArchitecture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisArchitectureTest
{
	using namespace AnastasisArchitecture;

	bool IsDwelling(const FArchetype& A)
	{
		return FCString::Strcmp(A.SimType, TEXT("house")) == 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisArchitectureScaleTest,
	"Anastasis.Village.Architecture.Echelle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisArchitectureScaleTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisArchitecture;
	TestEqual(TEXT("huit archetypes"), All().Num(), static_cast<int32>(EVariant::Count));
	const double Limit = ParcelCm * 0.5 - ParcelMarginCm;
	for (const FArchetype& A : All())
	{
		const FString Name(A.Id);
		// ARCH-09 : la maisonnee tient dans sa parcelle de 20 m, ruelle comprise.
		TestTrue(Name + TEXT(" : emprise dans la parcelle"),
			A.Footprint.Min.X >= -Limit && A.Footprint.Min.Y >= -Limit && A.Footprint.Max.X <= Limit && A.Footprint.Max.Y <= Limit);
		// La parcelle n'est plus vide : l'emprise couvre au moins le cinquieme de la case (l ancienne maison : 2,8 %).
		const double Share = A.Footprint.GetArea() / (ParcelCm * ParcelCm);
		// Le puits, et la cabane du joueur, petite par mandat (ma-cabane-001), ne remplissent pas leur parcelle.
		if (A.Variant != EVariant::Well && A.Variant != EVariant::Cabin)
		{
			TestTrue(FString::Printf(TEXT("%s : occupe sa parcelle (%.0f %%)"), *Name, Share * 100.0), Share >= 0.20);
		}
		if (A.DoorWidthCm > 0.0)
		{
			// ARCH-01 / ARCH-02 : un adulte de 170 cm passe debout, une charge a la main.
			TestTrue(FString::Printf(TEXT("%s : porte %.0f cm libres >= %.0f"), *Name, A.DoorClearCm, DoorClearMinCm),
				A.DoorClearCm >= DoorClearMinCm);
			TestTrue(FString::Printf(TEXT("%s : porte %.0f cm de large"), *Name, A.DoorWidthCm), A.DoorWidthCm >= DoorWidthMinCm);
			TestTrue(Name + TEXT(" : la porte depasse l'humain de 15 cm"), A.DoorClearCm >= HumanCm + 15.0);
		}
		for (const FRoom& Room : A.Rooms)
		{
			// ARCH-03 : 225 cm sous plafond.
			TestTrue(FString::Printf(TEXT("%s/%s : %.0f cm sous plafond"), *Name, Room.Name, Room.CeilingCm - Room.FloorCm),
				Room.CeilingCm - Room.FloorCm >= 225.0);
			if (FCString::Strstr(Room.Use, TEXT("foyer")))
			{
				// ARCH-08 : une piece a foyer fait au moins 16 m2 et son foyer existe.
				TestTrue(FString::Printf(TEXT("%s/%s : %.1f m2"), *Name, Room.Name, Room.AreaM2), Room.AreaM2 >= 16.0);
				TestTrue(Name + TEXT(" : foyer place"), A.bHasHearth);
			}
		}
		if (AnastasisArchitectureTest::IsDwelling(A))
		{
			TestTrue(Name + TEXT(" : faitage au-dessus de deux humains"), A.RidgeCm >= 2.0 * HumanCm);
			TestTrue(Name + TEXT(" : on y dort"), A.SleepCapacity >= 3);
			TestTrue(Name + TEXT(" : on y range"), A.StorageCapacity > 0);
			TestTrue(Name + TEXT(" : le foyer est sous le toit, dans l'emprise"),
				A.Footprint.IsInside(FVector2D(A.HearthLocal.X, A.HearthLocal.Y)));
		}
		TestTrue(Name + TEXT(" : l'entree est cote acces (+Y)"), A.EntryLocal.Y > 0.0 || A.Variant == EVariant::Chapel);
		TestTrue(Name + TEXT(" : l'entree est dans la parcelle"), FMath::Abs(A.EntryLocal.X) <= ParcelCm * 0.5 && FMath::Abs(A.EntryLocal.Y) <= ParcelCm * 0.5);
		TestTrue(Name + TEXT(" : materiaux de construction"), A.Materials.Wood + A.Materials.Stone > 0);
	}
	// Les tiers se lisent par la taille : pauvre < moyenne <= ferme.
	TestTrue(TEXT("pauvre plus petite que moyenne"), Get(EVariant::HousePoor).Footprint.GetArea() < Get(EVariant::HouseMedium).Footprint.GetArea());
	TestTrue(TEXT("moyenne pas plus grande que ferme"), Get(EVariant::HouseMedium).Footprint.GetArea() <= Get(EVariant::HouseFarm).Footprint.GetArea());
	TestTrue(TEXT("la maison moyenne a deux niveaux"), Get(EVariant::HouseMedium).RidgeCm > 1.8 * Get(EVariant::HousePoor).RidgeCm);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisArchitectureReportTest,
	"Anastasis.Village.Architecture.Rapport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisArchitectureReportTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisArchitecture;
	// Le catalogue C++ et le generateur disent la meme chose : emprise, porte, foyer, faitage.
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs/unreal/architecture/architecture-kit-001.json"));
	FString Text;
	if (!TestTrue(TEXT("rapport du generateur present : ") + Path, FFileHelper::LoadFileToString(Text, *Path)))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	if (!TestTrue(TEXT("rapport lisible"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid()))
	{
		return false;
	}
	TMap<FString, TSharedPtr<FJsonObject>> ByName;
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("buildings")))
	{
		ByName.Add(V->AsObject()->GetStringField(TEXT("name")), V->AsObject());
	}
	for (const FArchetype& A : All())
	{
		FString Mesh(A.BodyMesh);
		Mesh = FPaths::GetBaseFilename(Mesh.Left(Mesh.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd)));
		const TSharedPtr<FJsonObject>* Found = ByName.Find(Mesh);
		if (!TestNotNull(FString::Printf(TEXT("%s : %s au rapport"), A.Id, *Mesh), Found))
		{
			continue;
		}
		const TSharedPtr<FJsonObject>& B = *Found;
		const TArray<TSharedPtr<FJsonValue>>& Foot = B->GetArrayField(TEXT("footprint"));
		TestTrue(FString::Printf(TEXT("%s : emprise identique"), A.Id),
			FMath::IsNearlyEqual(Foot[0]->AsNumber(), A.Footprint.Min.X, 1.0) && FMath::IsNearlyEqual(Foot[1]->AsNumber(), A.Footprint.Min.Y, 1.0)
			&& FMath::IsNearlyEqual(Foot[2]->AsNumber(), A.Footprint.Max.X, 1.0) && FMath::IsNearlyEqual(Foot[3]->AsNumber(), A.Footprint.Max.Y, 1.0));
		const TArray<TSharedPtr<FJsonValue>>& Bounds = B->GetArrayField(TEXT("bounds"));
		TestTrue(FString::Printf(TEXT("%s : faitage identique"), A.Id),
			FMath::IsNearlyEqual(Bounds[1]->AsArray()[2]->AsNumber(), A.RidgeCm, 1.0));
		const TArray<TSharedPtr<FJsonValue>>& Hearths = B->GetArrayField(TEXT("hearths"));
		TestEqual(FString::Printf(TEXT("%s : foyer"), A.Id), Hearths.Num() > 0, A.bHasHearth);
		if (A.bHasHearth && Hearths.Num() > 0)
		{
			const TArray<TSharedPtr<FJsonValue>>& H = Hearths[0]->AsArray();
			TestTrue(FString::Printf(TEXT("%s : foyer au meme endroit"), A.Id),
				FVector::Dist(FVector(H[0]->AsNumber(), H[1]->AsNumber(), H[2]->AsNumber()), A.HearthLocal) <= 1.5);
		}
		const TArray<TSharedPtr<FJsonValue>>& Entry = B->GetArrayField(TEXT("entry"));
		TestTrue(FString::Printf(TEXT("%s : entree identique"), A.Id),
			FVector::Dist(FVector(Entry[0]->AsNumber(), Entry[1]->AsNumber(), Entry[2]->AsNumber()), A.EntryLocal) <= 1.5);
		bool bDoor = A.DoorWidthCm <= 0.0;
		for (const TSharedPtr<FJsonValue>& D : B->GetArrayField(TEXT("doors")))
		{
			const TSharedPtr<FJsonObject> O = D->AsObject();
			const TArray<TSharedPtr<FJsonValue>>& At = O->GetArrayField(TEXT("at"));
			bDoor |= FMath::IsNearlyEqual(O->GetNumberField(TEXT("width")), A.DoorWidthCm, 0.5)
				&& FMath::IsNearlyEqual(O->GetNumberField(TEXT("clear")), A.DoorClearCm, 0.5)
				&& FVector::Dist(FVector(At[0]->AsNumber(), At[1]->AsNumber(), At[2]->AsNumber()), A.DoorLocal) <= 1.5;
		}
		TestTrue(FString::Printf(TEXT("%s : porte principale au rapport"), A.Id), bDoor);
		TestEqual(FString::Printf(TEXT("%s : rapport sans erreur d'echelle"), A.Id), B->GetArrayField(TEXT("errors")).Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisArchitectureVariantTest,
	"Anastasis.Village.Architecture.Variante",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisArchitectureVariantTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisArchitecture;
	EVariant V;
	TestTrue(TEXT("puits"), ChooseVariant(TEXT("well"), 1, TEXT("building-1"), V) && V == EVariant::Well);
	TestTrue(TEXT("grenier"), ChooseVariant(TEXT("granary"), 1, TEXT("building-2"), V) && V == EVariant::Storehouse);
	TestTrue(TEXT("cabane"), ChooseVariant(TEXT("cabin"), 1, TEXT("building-4"), V) && V == EVariant::Cabin);
	TestFalse(TEXT("type inconnu"), ChooseVariant(TEXT("tavern"), 1, TEXT("building-3"), V));
	// settlement-morphogenesis-001 : plus de graine. Deux maisons de meme phase ont la meme forme de repli,
	// quel que soit leur identifiant ; seule la phase de la reference fait grandir.
	for (int32 I = 0; I < 30; ++I)
	{
		const FString Id = FString::Printf(TEXT("building-%d"), I);
		EVariant A;
		TestTrue(TEXT("maison"), ChooseVariant(TEXT("house"), 1, Id, A));
		TestEqual(TEXT("phase 1 : la meme forme pour tous (pas de tirage)"), static_cast<int32>(A), static_cast<int32>(EVariant::HousePoor));
	}
	EVariant P3;
	EVariant P6;
	ChooseVariant(TEXT("house"), 3, TEXT("building-1"), P3);
	ChooseVariant(TEXT("house"), 6, TEXT("building-1"), P6);
	TestEqual(TEXT("phase 3 : deux niveaux"), static_cast<int32>(P3), static_cast<int32>(EVariant::HouseMedium));
	TestEqual(TEXT("phase 6 : ferme"), static_cast<int32>(P6), static_cast<int32>(EVariant::HouseFarm));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisArchitecturePadTest,
	"Anastasis.Village.Architecture.Terrasse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisArchitecturePadTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisArchitecture;
	TestEqual(TEXT("sans echantillon"), PadLevel({}), 0.0);
	TestEqual(TEXT("mediane impaire"), PadLevel({5.0, 1.0, 3.0}), 3.0);
	TestEqual(TEXT("mediane paire"), PadLevel({4.0, 1.0, 3.0, 2.0}), 2.5);
	// Une pente reguliere de 10 % sous 15 m : la cour se pose au milieu, ni sur le point haut ni sur le bas,
	// et l'assise de 480 cm couvre le cote aval.
	const FArchetype& Farm = Get(EVariant::HouseFarm);
	TArray<double> Slope;
	for (const FVector2D& P : FootprintSamples(Farm, 5))
	{
		Slope.Add(0.10 * P.Y);
	}
	const double Pad = PadLevel(Slope);
	TestTrue(TEXT("25 echantillons"), Slope.Num() == 25);
	TestTrue(TEXT("pente : cour au milieu"), FMath::Abs(Pad) < 1.0);
	TestTrue(TEXT("pente : soutenement suffisant a l'aval"), Pad - FMath::Min(Slope) < 480.0);
	return true;
}

#endif
