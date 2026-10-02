#include "Misc/AutomationTest.h"
#include "WorldView/AnastasisSkyPassage.h"
#include "WorldView/AnastasisSkyClock.h"
#include "WorldView/AnastasisAtmosphereProfile.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPassageExposure, "Anastasis.Sky.Passage.ExposureSafety", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPassageExposure::RunTest(const FString&)
{
	using namespace AnastasisSkyPassage;
	const auto* P = UAnastasisAtmosphereProfile::CreateCodeDefaults(GetTransientPackage());
	int32 Violations = 0;
	int32 LegacyViolations = 0;
	for (int32 Day : {1, 31, 61, 91})
	{
		for (double Speed : {0.0375, 1.0, 4.0, 64.0, 1000.0, -4.0})
		{
			double Time = AnastasisSkyClock::SimTimeFor(Day, 3.0);
			double EV = AnastasisSkyClock::Evaluate(*P, Time, 12345).ExposureEV100;
			double Legacy = EV;
			for (int32 Frame = 0; Frame < 1200; ++Frame)
			{
				const double Dt = Frame == 600 ? 2.0 : 1.0 / 30.0;
				Time = FMath::Max(0.0, Time + Speed * Dt);
				const double Target = AnastasisSkyClock::Evaluate(*P, Time, 12345).ExposureEV100;
				const double Next = Exposure(EV, Target, Dt, P->MaxExposureChangePerSecond);
				if (!FMath::IsFinite(Next) || Next < Target - 1e-9
					|| EV - Next > P->MaxExposureChangePerSecond * FMath::Min(Dt, 0.1) + 1e-9) ++Violations;
				Legacy = AnastasisSkyClock::AdaptExposure(Legacy, Target, Dt, P->MaxExposureChangePerSecond);
				if (Legacy < Target - 0.5) ++LegacyViolations;
				EV = Next;
			}
		}
	}
	TestEqual(TEXT("no positive exposure mismatch; bounded dark adaptation including stalls"), Violations, 0);
	TestTrue(TEXT("negative control: previous policy fails this temporal contract"), LegacyViolations > 0);
	TestEqual(TEXT("observed dawn jump cannot retain night sensitivity"), Exposure(-1.0, 13.25, 1.6, 3.0), 13.25);
	TestEqual(TEXT("a stalled frame cannot spend seconds of dark adaptation at once"), Exposure(14.0, -1.0, 10.0, 3.0), 13.7, 1e-9);
	AddInfo(FString::Printf(TEXT("PASSAGE_EXPOSURE violations=%d legacy_overexposure_samples=%d"), Violations, LegacyViolations));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPassageRelay, "Anastasis.Sky.Passage.SurfaceRelay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPassageRelay::RunTest(const FString&)
{
	using namespace AnastasisSkyPassage;
	const auto* P = UAnastasisAtmosphereProfile::CreateCodeDefaults(GetTransientPackage());
	const double SF = P->PassageSunFullElevation, MF = P->PassageMoonFullElevation;
	const FRelay Horizon = Resolve(0.0, 0.0, SF, MF);
	TestEqual(TEXT("zero sun surface energy at the priority switch"), Horizon.Sun, 0.0);
	TestEqual(TEXT("zero moon surface energy at the priority switch"), Horizon.Moon, 0.0);
	TestEqual(TEXT("sky owns the interval"), Horizon.Twilight, 1.0);
	TestEqual(TEXT("full day remains authored"), Resolve(45, -45, SF, MF).Sun, 1.0);
	TestEqual(TEXT("full night remains authored"), Resolve(-45, 45, SF, MF).Moon, 1.0);
	TestEqual(TEXT("a hidden moon cannot light surfaces"), Resolve(-20, -5, SF, MF).Moon, 0.0);
	int32 Bad = 0;
	FRelay Previous = Resolve(-90, 90, SF, MF);
	for (int32 I = -8999; I <= 9000; ++I)
	{
		const double E = I * 0.01;
		const FRelay R = Resolve(E, -E, SF, MF);
		if (R.Sun < 0 || R.Moon < 0 || R.Twilight < 0
			|| FMath::Abs(R.Sun + R.Moon + R.Twilight - 1.0) > 1e-9
			|| FMath::Abs(R.Sun - Previous.Sun) > 0.01 || FMath::Abs(R.Moon - Previous.Moon) > 0.01
			|| (E < 0 && R.Sun != 0) || (E > 0 && R.Moon != 0)) ++Bad;
		Previous = R;
	}
	TestEqual(TEXT("continuous bounded partition over full horizon sweep"), Bad, 0);
	return true;
}

#endif
