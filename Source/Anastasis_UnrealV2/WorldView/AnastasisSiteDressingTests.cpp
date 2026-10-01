#include "WorldView/AnastasisSiteDressing.h"
#include "WorldView/AnastasisHumanGeography.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisSiteDressingReadsTheLand, "Anastasis.Sites.ReadsTheLand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisSiteDressingReadsTheLand::RunTest(const FString&)
{
	using namespace AnastasisSiteDressing;

	const FRead Pass = Read(42.0, 33.0);
	TestTrue(TEXT("le col est un passage"), Pass.bPassage && Pass.Passage > 0.8);
	TestFalse(TEXT("le col n'est pas une riviere"), Pass.bRiparian);

	const FRead Valley = Read(53.0, 53.0);
	TestTrue(TEXT("la grande vallee est un pre"), Valley.bMeadow);
	TestFalse(TEXT("le centre de la vallee n'est pas le col"), Valley.bPassage);

	const FRead Ridge = Read(71.0, 26.0);
	TestFalse(TEXT("la crete sud-est reste hors du pre"), Ridge.bMeadow);
	TestFalse(TEXT("la crete sud-est n'est pas le col"), Ridge.bPassage);

	TestTrue(TEXT("le col refuse la foret"), ShouldOmitForest(Pass, 1u));
	TestFalse(TEXT("la crete garde ses arbres"), ShouldOmitForest(Ridge, 1u));

	int32 Kept = 0;
	for (uint32 Seed = 0; Seed < 130u; ++Seed)
	{
		if (!ShouldOmitForest(Valley, Seed))
		{
			++Kept;
		}
	}
	TestTrue(TEXT("le pre garde quelques arbres"), Kept > 0);
	TestTrue(TEXT("le pre n'en garde qu'une minorite"), Kept < 20);

	const AnastasisHumanGeography::FSample Crest = AnastasisHumanGeography::Evaluate(42.0, 33.0, 8.0);
	TestTrue(TEXT("la hauteur du col suit toujours la courbe"), FMath::IsNearlyEqual(Crest.Height, 5.4, 0.15));
	TestTrue(TEXT("le poids de passage est expose"), Crest.PassageWeight > 0.8);

	TArray<FProp> Props;
	AppendCompositions(Props);
	TestTrue(TEXT("quelques compositions, pas un tapis"), Props.Num() >= 30 && Props.Num() <= 80);
	for (const FProp& Prop : Props)
	{
		TestTrue(TEXT("l'echelle reste sous le plafond des objets"), Prop.Scale > 0.4f && Prop.Scale < 9.5f);
		TestFalse(TEXT("aucune composition dans le futur etablissement"), IsFutureSettlement(Prop.X, Prop.Y));
		TestNotNull(TEXT("chaque composition nomme un mesh"), Prop.Mesh);
	}
	return true;
}

#endif
