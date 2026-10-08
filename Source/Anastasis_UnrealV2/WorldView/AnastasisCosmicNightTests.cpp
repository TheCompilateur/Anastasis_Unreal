#include "Misc/AutomationTest.h"
#include "WorldView/AnastasisCosmicNight.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisCosmicCalendar,
	"Anastasis.Sky.Cosmic.Calendar", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisCosmicCalendar::RunTest(const FString&)
{
	using namespace AnastasisCosmicNight;
	int32 ShowerNights = 0;
	int32 StrangeNights = 0;
	for (int32 Day = 1; Day <= 240; ++Day)
	{
		const FNight A = Plan(12345u, Day);
		const FNight B = Plan(12345u, Day);
		TestEqual(TEXT("same seed/day gives same kind"), static_cast<int32>(A.Kind), static_cast<int32>(B.Kind));
		TestEqual(TEXT("same seed/day gives same count"), A.MeteorCount, B.MeteorCount);
		ShowerNights += A.Kind == EKind::Meteors ? 1 : 0;
		StrangeNights += A.Kind == EKind::Veil ? 1 : 0;
		if (A.Kind == EKind::Veil)
		{
			TestEqual(TEXT("strange nights do not also stage a meteor shower"), A.MeteorCount, 0);
		}
	}
	TestTrue(TEXT("meteors recur but do not dominate the calendar"), ShowerNights >= 18 && ShowerNights <= 55);
	TestTrue(TEXT("strange nights are rarer than meteor nights"), StrangeNights >= 1 && StrangeNights < ShowerNights);
	TestEqual(TEXT("the same night survives midnight"), EveningDayFor(42, 23.5), EveningDayFor(43, 2.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisCosmicVisibility,
	"Anastasis.Sky.Cosmic.Visibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisCosmicVisibility::RunTest(const FString&)
{
	using namespace AnastasisCosmicNight;
	const FInstant Clear = Evaluate(12345u, 42, 23.0, -22.0, 0.0, 0.0, 2);
	const FInstant Covered = Evaluate(12345u, 42, 23.0, -22.0, 1.0, 1.0, 2);
	const FInstant Noon = Evaluate(12345u, 42, 12.0, 45.0, 0.0, 0.0, 2);
	TestEqual(TEXT("clear night has full visibility"), Clear.Visibility, 1.0f);
	TestTrue(TEXT("strange veil is visible during its window"), Clear.VeilStrength > 0.0f);
	TestEqual(TEXT("clouds conceal scheduled veil"), Covered.VeilStrength, 0.0f);
	TestEqual(TEXT("day conceals scheduled veil"), Noon.VeilStrength, 0.0f);

	int32 ActiveSamples = 0;
	for (int32 Sample = 0; Sample <= 7000; ++Sample)
	{
		const double Hour = 21.0 + Sample / 1000.0;
		const FInstant S = Evaluate(12345u, 42, Hour, -20.0, 0.0, 0.0, 1);
		if (S.MeteorStrength > 0.01f)
		{
			++ActiveSamples;
			TestTrue(TEXT("meteor directions remain above the horizon"), S.MeteorStart.Z > 0.0 && S.MeteorEnd.Z > 0.0);
		}
	}
	TestTrue(TEXT("forced meteor night contains observable intervals"), ActiveSamples > 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisCosmicComet,
	"Anastasis.Sky.Cosmic.Comet", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisCosmicComet::RunTest(const FString&)
{
	using namespace AnastasisCosmicNight;
	int32 Onset = 0;
	for (int32 Day = 1; Day <= 480; ++Day)
	{
		const FNight Night = Plan(12345u, Day);
		if (Night.Kind == EKind::Comet && Night.CometDayIndex == 0)
		{
			Onset = Day;
			break;
		}
	}
	if (!TestTrue(TEXT("seeded calendar contains a comet onset"), Onset > 0))
	{
		return false;
	}
	FVector PreviousHead = FVector::ZeroVector;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FNight Night = Plan(12345u, Onset + Index);
		TestEqual(TEXT("comet persists four evenings"), static_cast<int32>(Night.Kind), static_cast<int32>(EKind::Comet));
		TestEqual(TEXT("same comet retains onset"), Night.CometStartDay, Onset);
		TestEqual(TEXT("comet advances one night"), Night.CometDayIndex, Index);
		TestEqual(TEXT("comet does not stage a meteor shower"), Night.MeteorCount, 0);
		const FInstant Clear = Evaluate(12345u, Onset + Index, 23.0, -22.0, 0.0, 0.0);
		TestTrue(TEXT("clear comet is visible and above horizon"),
			Clear.CometStrength > 0.1f && Clear.CometHead.Z > 0.0 && Clear.CometTail.Z > 0.0);
		if (Index > 0)
		{
			TestTrue(TEXT("comet shifts against the stars overnight"),
				FVector::DotProduct(PreviousHead, Clear.CometHead) < 0.999);
		}
		PreviousHead = Clear.CometHead;
	}
	const FInstant AfterMidnight = Evaluate(12345u, Onset + 1, 2.0, -22.0, 0.0, 0.0);
	TestEqual(TEXT("comet evening survives midnight"), AfterMidnight.Night.CometStartDay, Onset);
	TestEqual(TEXT("comet evening index survives midnight"), AfterMidnight.Night.CometDayIndex, 0);
	TestEqual(TEXT("cloud hides comet without erasing calendar"),
		Evaluate(12345u, Onset, 23.0, -22.0, 1.0, 0.0).CometStrength, 0.0f);
	TestEqual(TEXT("day hides comet without erasing calendar"),
		Evaluate(12345u, Onset, 12.0, 45.0, 0.0, 0.0).CometStrength, 0.0f);
	return true;
}

#endif
