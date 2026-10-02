#include "WorldView/AnastasisTrunkContact.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTrunkContactPlan, "Anastasis.GroundContact.Skirt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTrunkContactPlan::RunTest(const FString&)
{
	using namespace AnastasisTrunkContact;
	TArray<FVector> Trunks;
	Trunks.Add(FVector(1000, 2000, 40));
	Trunks.Add(FVector(4000, 2000, 350));
	Trunks.Add(FVector(8000, 5000, 900));
	TArray<FPatch> A, B;
	FReport ReportA, ReportB;
	FString Error;
	TestTrue(TEXT("plan"), Build(Trunks, 12345u, A, ReportA, Error));
	TestTrue(TEXT("repeat"), Build(Trunks, 12345u, B, ReportB, Error));
	TestEqual(TEXT("same count"), A.Num(), B.Num());
	TestTrue(TEXT("a real trunk grows a skirt"), A.Num() > 0);
	TestTrue(TEXT("cap holds"), A.Num() <= 6000);
	TestEqual(TEXT("kinds add up"), ReportA.Litter + ReportA.Moss, ReportA.Patches);
	int32 OnTiny = 0;
	for (int32 I = 0; I < A.Num(); ++I)
	{
		TestTrue(TEXT("same patch"), A[I].Position.Equals(B[I].Position, 1.0e-4));
		TestEqual(TEXT("same kind"), A[I].Kind, B[I].Kind);
		const FVector& Home = Trunks[A[I].Trunk];
		const double Distance = FVector2D::Distance(A[I].Position, FVector2D(Home.X, Home.Y));
		TestTrue(TEXT("sits at the foot, not at the drip line"), Distance >= 40.0 - 1.0 && Distance <= 140.0 + 1.0);
		TestTrue(TEXT("moss stays low"), A[I].Kind != 1 || A[I].ScaleZ < 0.25);
		TestTrue(TEXT("litter stays flatter than it is wide"), A[I].Kind != 0 || A[I].ScaleZ < A[I].ScaleXY * 0.25);
		OnTiny += A[I].Trunk == 0;
	}
	TestEqual(TEXT("a sapling crown grows no skirt"), OnTiny, 0);
	TArray<FVector> Many;
	Many.Reserve(4000);
	for (int32 I = 0; I < 4000; ++I) Many.Add(FVector(I * 30.0, 0.0, 400.0));
	TestTrue(TEXT("dense plan"), Build(Many, 7u, A, ReportA, Error));
	TestEqual(TEXT("global cap"), A.Num(), 6000);
	TArray<FVector> Empty;
	TestTrue(TEXT("empty"), Build(Empty, 1u, A, ReportA, Error));
	TestEqual(TEXT("empty stays empty"), A.Num(), 0);
	Trunks[1].X = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("rejects a broken trunk"), Build(Trunks, 1u, A, ReportA, Error));
	TestEqual(TEXT("no partial plan"), A.Num(), 0);
	return true;
}
#endif
