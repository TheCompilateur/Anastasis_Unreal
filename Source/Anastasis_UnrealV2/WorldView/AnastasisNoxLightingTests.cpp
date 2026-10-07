#include "Misc/AutomationTest.h"
#include "WorldView/AnastasisNoxLighting.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisNoxNightContract,
	"Anastasis.Atmosphere.Nox.NightContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisNoxNightContract::RunTest(const FString&)
{
	using namespace AnastasisNoxLighting;
	const FNight Legacy = Resolve(0, 0.3f, -1.0, 0.0f, 1.0f);
	TestEqual(TEXT("off restores existing full moon even under test pins"), Legacy.MoonLux, 0.3f);
	TestEqual(TEXT("off restores existing night exposure"), Legacy.ExposureEV100, -1.0);

	for (int32 Profile = 1; Profile <= 3; ++Profile)
	{
		const FNight Full = Resolve(Profile, 0.3f, -1.0, 1.0f, 0.0f);
		const FNight Overcast = Resolve(Profile, 0.3f, -1.0, 1.0f, 1.0f);
		const FNight Moonless = Resolve(Profile, 0.3f, -1.0, 0.0f, 0.0f);
		TestEqual(TEXT("clear full moon keeps the measured source lux"), Full.MoonLux, 0.3f);
		TestTrue(TEXT("cloud lowers the actual moon light"), Overcast.MoonLux < Full.MoonLux);
		TestEqual(TEXT("moonless has no directional moon energy"), Moonless.MoonLux, 0.0f);
		TestTrue(TEXT("moonless exposure cannot turn absence of moon into a gray day"),
			Moonless.ExposureEV100 > Full.ExposureEV100);
		TestEqual(TEXT("sunlit exposure stays exact"), ExposureForSunElevation(14.0, Moonless.ExposureEV100, 30.0), 14.0);
		TestEqual(TEXT("calibrated sunset stays exact"), ExposureForSunElevation(5.0, Moonless.ExposureEV100, -8.0), 5.0);
		TestEqual(TEXT("deep night reaches bounded target"), ExposureForSunElevation(-1.0, Moonless.ExposureEV100, -20.0),
			Moonless.ExposureEV100);
	}
	return true;
}

#endif
