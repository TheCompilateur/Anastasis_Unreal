#include "WorldView/AnastasisPonticWaterMicro.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPonticWaterMicroHabitat,
	"Anastasis.PonticWaterMicro.HabitatSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPonticWaterMicroHabitat::RunTest(const FString&)
{
	AnastasisMicroEcology::FPlan Bank;
	for (int32 I = 0; I < 300; ++I)
	{
		for (int32 Role = 0; Role < 3; ++Role)
		{
			AnastasisMicroEcology::FPlacement P;
			P.Ground = FVector(100.0 * I, 300.0 * Role, 40.0);
			P.AboveWater = 40.0;
			P.SlopeDegrees = 8.0;
			P.Role = Role == 0 ? AnastasisMicroEcology::ERole::BankReed
				: Role == 1 ? AnastasisMicroEcology::ERole::BankPebble : AnastasisMicroEcology::ERole::BankTuft;
			P.Pocket = Role == 0 ? AnastasisMicroEcology::EPocket::Vegetated
				: Role == 1 ? AnastasisMicroEcology::EPocket::Rocky : AnastasisMicroEcology::EPocket::Muddy;
			Bank.Instances.Add(P);
		}
	}
	AnastasisPonticWaterMicro::FPlan A, Repeat;
	AnastasisMicroEcology::FInputs Habitat;
	Habitat.SampleHeight = [](double, double, double& Z) { Z = 40.0; return true; };
	Habitat.SampleWaterHeight = [](double, double, double& Z) { Z = 0.0; return true; };
	AnastasisPonticWaterMicro::Build(Bank, Habitat, 12345u, A);
	AnastasisPonticWaterMicro::Build(Bank, Habitat, 12345u, Repeat);
	TestTrue(TEXT("horsetail present"), A.Counts[0] > 0);
	TestTrue(TEXT("coltsfoot present"), A.Counts[1] > 0);
	TestTrue(TEXT("frog present"), A.Counts[2] > 0);
	TestEqual(TEXT("repeat count"), A.Instances.Num(), Repeat.Instances.Num());
	bool bSame = A.Instances.Num() == Repeat.Instances.Num();
	for (int32 I = 0; bSame && I < A.Instances.Num(); ++I)
	{
		bSame = A.Instances[I].Ground.Equals(Repeat.Instances[I].Ground, 0.0)
			&& A.Instances[I].Kind == Repeat.Instances[I].Kind
			&& A.Instances[I].Yaw == Repeat.Instances[I].Yaw;
	}
	TestTrue(TEXT("deterministic placements"), bSame);
	TestEqual(TEXT("counts match placements"), A.Counts[0] + A.Counts[1] + A.Counts[2], A.Instances.Num());
	bool bClearFrogs = true;
	for (const AnastasisPonticWaterMicro::FPlacement& Frog : A.Instances)
	{
		if (Frog.Kind != AnastasisPonticWaterMicro::EKind::Frog) continue;
		for (const AnastasisPonticWaterMicro::FPlacement& Plant : A.Instances)
		{
			if (Plant.Kind == AnastasisPonticWaterMicro::EKind::Frog) continue;
			bClearFrogs &= FVector::DistSquared2D(Frog.Ground, Plant.Ground) >= 150.0 * 150.0;
		}
	}
	TestTrue(TEXT("frogs clear of new plants"), bClearFrogs);
	bool bClearSourceProps = true;
	for (const AnastasisPonticWaterMicro::FPlacement& P : A.Instances)
	{
		for (const AnastasisMicroEcology::FPlacement& Source : Bank.Instances)
		{
			bClearSourceProps &= FVector::DistSquared2D(P.Ground, Source.Ground) >= 85.0 * 85.0;
		}
	}
	TestTrue(TEXT("new assets clear of original bank props"), bClearSourceProps);
	for (AnastasisMicroEcology::FPlacement& P : Bank.Instances)
	{
		P.AboveWater = -10.0;
	}
	AnastasisPonticWaterMicro::FPlan Submerged;
	AnastasisPonticWaterMicro::Build(Bank, Habitat, 12345u, Submerged);
	TestEqual(TEXT("no underwater life"), Submerged.Instances.Num(), 0);
	return true;
}

#endif
