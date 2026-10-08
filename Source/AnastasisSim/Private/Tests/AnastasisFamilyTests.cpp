// familles-feu-001 (ecart n°44) -- identite et foyers poses par l'hote : `createNpc` (name, familyName,
// gender, age, familyId) et `createFamily` / `leavePreviousFamily` (adults, dependents), comme donnees seules.

#include "Misc/AutomationTest.h"
#include "Life/AnastasisNeeds.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace AnastasisFamilyTest
{
	using namespace AnastasisVillage;

	AnastasisWorld::FWorld MakeWorld()
	{
		AnastasisWorld::FWorld W;
		W.W = 32;
		W.H = 32;
		W.Tiles.SetNum(1024);
		for (int32 I = 0; I < W.Tiles.Num(); ++I)
		{
			W.Tiles[I].X = I % 32;
			W.Tiles[I].Y = I / 32;
			W.Tiles[I].Alt = 0.5;
		}
		return W;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFamilyIdentityTest, "Anastasis.Sim.Famille.Identite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFamilyIdentityTest::RunTest(const FString&)
{
	using namespace AnastasisFamilyTest;
	const AnastasisWorld::FWorld W = MakeWorld();
	FVillage V;
	V.Bind(W);
	const FString A = V.SpawnNpc(12.5, 12.5, AnastasisNeeds::FNeeds());

	// Sans hote, rien : le harnais ne voit ni nom ni foyer.
	const FNpc* Npc = V.FindNpc(A);
	TestTrue(TEXT("SpawnNpc seul : pas de nom"), Npc->Name.IsEmpty() && Npc->FamilyName.IsEmpty() && Npc->Gender.IsEmpty());
	TestEqual(TEXT("SpawnNpc seul : age 0"), Npc->Age, 0.0);
	TestTrue(TEXT("SpawnNpc seul : pas de foyer"), Npc->FamilyId.IsEmpty() && V.GetFamilies().IsEmpty());

	const uint64 ParityBefore = V.Digest();
	const uint64 StateBefore = V.StateDigest();
	TestTrue(TEXT("identite posee"), V.SetIdentity(A, TEXT("Georgios"), TEXT("la famille de Georgios"), TEXT("male"), 50.0));
	TestEqual(TEXT("nom"), V.FindNpc(A)->Name, FString(TEXT("Georgios")));
	TestEqual(TEXT("sexe"), V.FindNpc(A)->Gender, FString(TEXT("male")));
	TestEqual(TEXT("age"), V.FindNpc(A)->Age, 50.0);
	TestFalse(TEXT("habitant inconnu"), V.SetIdentity(TEXT("npc-99"), TEXT("X"), TEXT("Y"), TEXT("male"), 1.0));
	// L'identite est de l'etat (STATE_ORACLE_001) ; la projection de parite JS, figee, ne la lit pas.
	TestNotEqual(TEXT("l'empreinte d'etat voit l'identite"), V.StateDigest(), StateBefore);
	TestEqual(TEXT("la projection de parite ne bouge pas"), V.Digest(), ParityBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFamilyHouseholdTest, "Anastasis.Sim.Famille.Foyer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFamilyHouseholdTest::RunTest(const FString&)
{
	using namespace AnastasisFamilyTest;
	const AnastasisWorld::FWorld W = MakeWorld();
	FVillage V;
	V.Bind(W);
	const FString Father = V.SpawnNpc(12.5, 12.5, AnastasisNeeds::FNeeds());
	const FString Son = V.SpawnNpc(13.5, 12.5, AnastasisNeeds::FNeeds());
	const FString Ward = V.SpawnNpc(14.5, 12.5, AnastasisNeeds::FNeeds());

	const FString First = V.AddFamily(TEXT("la famille de Georgios"));
	const FString Second = V.AddFamily(TEXT("la maison de Niketas"));
	TestEqual(TEXT("identifiants de foyer"), First, FString(TEXT("family-0")));
	TestEqual(TEXT("second foyer"), Second, FString(TEXT("family-1")));
	TestTrue(TEXT("le pere, adulte"), V.JoinFamily(Father, First, true, TEXT("chef")));
	TestTrue(TEXT("le fils, dependant"), V.JoinFamily(Son, First, false, TEXT("fils")));
	TestTrue(TEXT("le pupille, chez Niketas"), V.JoinFamily(Ward, Second, false, TEXT("pupille")));
	TestFalse(TEXT("foyer inconnu"), V.JoinFamily(Father, TEXT("family-9"), true, TEXT("chef")));

	const FVillage::FFamily* F = V.FindFamily(First);
	if (!TestNotNull(TEXT("foyer retrouve"), F)) return false;
	TestEqual(TEXT("nom du foyer"), F->Name, FString(TEXT("la famille de Georgios")));
	TestEqual(TEXT("un adulte"), F->Adults.Num(), 1);
	TestEqual(TEXT("un dependant"), F->Dependents.Num(), 1);
	TestEqual(TEXT("le fils porte le foyer"), V.FindNpc(Son)->FamilyId, First);
	TestEqual(TEXT("et son role"), V.FindNpc(Son)->KinRole, FString(TEXT("fils")));

	// `leavePreviousFamily` : rejoindre un foyer quitte le precedent.
	TestTrue(TEXT("le pupille change de foyer"), V.JoinFamily(Ward, First, false, TEXT("pupille")));
	TestEqual(TEXT("plus personne chez Niketas"), V.FindFamily(Second)->Dependents.Num(), 0);
	TestEqual(TEXT("deux dependants chez Georgios"), V.FindFamily(First)->Dependents.Num(), 2);

	// Un absent quitte son foyer.
	TestTrue(TEXT("le fils s'en va"), V.RemoveNpc(Son));
	TestFalse(TEXT("le foyer ne le garde pas"), V.FindFamily(First)->Dependents.Contains(Son));
	TestEqual(TEXT("le pere reste"), V.FindFamily(First)->Adults.Num(), 1);
	return true;
}

#endif
