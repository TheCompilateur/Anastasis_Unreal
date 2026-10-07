#include "Life/AnastasisWeatherBehavior.h"

#include "Life/AnastasisVillageRhythm.h"

namespace AnastasisWeatherBehavior
{
namespace
{
	double Clamp100(double V)
	{
		return V < 0.0 ? 0.0 : V > 100.0 ? 100.0 : V;
	}
}

FSimWeather ReadSimWeather(const uint32 Seed, const int32 Day, const double Time)
{
	// `((sim.time % DAY_LENGTH) + DAY_LENGTH) % DAY_LENGTH / DAY_LENGTH` : fmod comme le `%` de JS.
	const double L = AnastasisRhythm::DayLength;
	const double DayFrac = FMath::IsFinite(Time)
		? FMath::Fmod(FMath::Fmod(Time, L) + L, L) / L
		: AnastasisWeather::NoHour;
	// `sim.day || 1`.
	const AnastasisWeather::FWeather W = AnastasisWeather::WeatherAt(Seed, Day != 0 ? Day : 1, nullptr, DayFrac);
	FSimWeather Out;
	Out.Rain = W.Rain;
	Out.Snow = W.Snow;
	Out.Wind = W.Wind;
	Out.Cover = W.Cover;
	Out.Season = W.Season;
	Out.Clearing = W.Clearing;
	return Out;
}

bool IsOutdoorWorker(const FString& JobId)
{
	return JobId == TEXT("farmer") || JobId == TEXT("herder") || JobId == TEXT("fisherman")
		|| JobId == TEXT("woodcutter") || JobId == TEXT("quarryman") || JobId == TEXT("guard")
		|| JobId == TEXT("butcher");
}

bool IsRainExposedGoal(const FString& Goal)
{
	return Goal == TEXT("gatherWood") || Goal == TEXT("gatherStone") || Goal == TEXT("gatherFood")
		|| Goal == TEXT("build") || Goal == TEXT("helpFarm") || Goal == TEXT("explore")
		|| Goal == TEXT("maintain");
}

double WeatherGoalBias(const FSimWeather& Weather, const FString& JobId, const FString& Goal)
{
	using namespace GoalWeights;
	// `if (!npc || !goal || !weather) return 0`.
	if (Goal.IsEmpty())
	{
		return 0.0;
	}
	double Bias = 0.0;
	const double Rain = Weather.Rain;
	const double Snow = Weather.Snow;
	const double Wind = Weather.Wind;
	const AnastasisWeather::ESeason Season = Weather.Season;
	const bool bOutdoor = IsOutdoorWorker(JobId);

	if (Rain >= RainThreshold)
	{
		const double T = FMath::Min(1.0, (Rain - RainThreshold) / 0.55);
		if (Goal == TEXT("explore")) Bias += RainExplore * T;
		if (Goal == TEXT("build")) Bias += RainBuild * T;
		if (Goal.StartsWith(TEXT("gather"), ESearchCase::CaseSensitive))
		{
			Bias += bOutdoor ? RainGather * T * 0.35 : RainGather * T;
		}
		if (Goal == TEXT("craft")) Bias += RainCraft * T;
		if (Goal == TEXT("rest")) Bias += RainRest * T;
		if (Goal == GoalShelterRain) Bias += 16.0 * T + (Rain >= Shelter::RainHeavy ? 22.0 : 0.0);
		if (Goal == TEXT("socialize") || Goal == TEXT("visitFamily") || Goal == TEXT("relax"))
		{
			Bias += RainSocial * T;
		}
		if (Goal == TEXT("maintain")) Bias += RainMaintain * T;
	}

	const bool bWinter = Season == AnastasisWeather::ESeason::Winter;
	if (Snow >= SnowThreshold || bWinter)
	{
		const double Sn = bWinter
			? FMath::Max(0.45, FMath::Min(1.0, Snow / 0.7 + 0.35))
			: FMath::Min(1.0, Snow / 0.7);
		if (Goal == TEXT("explore")) Bias += SnowExplore * Sn;
		if (Goal == TEXT("gatherFood") || Goal == TEXT("helpFarm")) Bias += WinterFood * Sn;
		if (Goal == TEXT("rest")) Bias += WinterRest * Sn;
		if (Goal == TEXT("buy") || Goal == TEXT("eat")) Bias += WinterBuy * Sn;
	}

	if (Season == AnastasisWeather::ESeason::Autumn)
	{
		if (Goal == TEXT("gatherFood") || Goal == TEXT("helpFarm")) Bias += AutumnFood;
		if (Goal == TEXT("deliver")) Bias += AutumnDeliver;
		if (Goal == TEXT("sell")) Bias += AutumnSell;
	}

	if (Season == AnastasisWeather::ESeason::Spring)
	{
		if (Goal == TEXT("explore")) Bias += SpringExplore;
		if (Goal == TEXT("gatherWood")) Bias += SpringGatherWood;
	}

	if (Wind >= WindThreshold)
	{
		const double W = FMath::Min(1.0, (Wind - WindThreshold) / 0.35);
		if (Goal == TEXT("explore")) Bias += WindExplore * W;
		if (Goal == TEXT("build")) Bias += WindBuild * W;
	}

	return Bias;
}

bool ShouldSeekRainShelter(const double Rain, const bool bInside, const FString& Goal, const FString& JobId,
	const double Time, const double CooldownUntil)
{
	if (bInside) return false;
	// `isHeavyRain` : `(rain || 0) >= rainHeavy`.
	if (!(Rain >= Shelter::RainHeavy)) return false;
	// `shelterCooldownUntil != null && (sim.time || 0) < shelterCooldownUntil`.
	if (CooldownUntil >= 0.0 && Time < CooldownUntil) return false;
	return IsRainExposedGoal(Goal) || IsOutdoorWorker(JobId);
}

double ShelterRainScore(const double Rain, const bool bInside, const FString& Goal, const FString& JobId,
	const double Time, const double CooldownUntil)
{
	if (!ShouldSeekRainShelter(Rain, bInside, Goal, JobId, Time, CooldownUntil)) return 0.0;
	const double Exposed = IsRainExposedGoal(Goal) ? 32.0 : 14.0;
	const double OutdoorJob = IsOutdoorWorker(JobId) ? 12.0 : 0.0;
	return 24.0 + Rain * 42.0 + Exposed + OutdoorJob;
}

double ShelterRainDuration(const double Rain)
{
	// `readSimWeather(sim).rain || SHELTER_RAIN.rainHeavy` : une pluie nulle vaut l'orage naissant.
	const double R = (Rain != 0.0 && !FMath::IsNaN(Rain)) ? Rain : Shelter::RainHeavy;
	const double T = FMath::Min(1.0, FMath::Max(0.0, (R - Shelter::RainHeavy) / 0.45));
	return Shelter::MinSeconds + T * (Shelter::MaxSeconds - Shelter::MinSeconds);
}

void ApplyRainExposure(const double Rain, const bool bInside, const FString& Goal, const double Dt,
	double& InOutEnergy, double& InOutHealth, const double CanopyCover)
{
	if (!(Dt > 0.0)) return;
	if (Rain < Shelter::RainHeavy) return;
	if (bInside || Goal == GoalShelterRain) return;
	// ecart n°41: partial interception beneath an actually embodied crown. Heavy rain
	// still reaches the ground, and the original thresholds remain unchanged.
	const double ShelterFraction = 0.20 * FMath::Clamp(FMath::IsFinite(CanopyCover) ? CanopyCover : 0.0, 0.0, 1.0);
	InOutEnergy = Clamp100(InOutEnergy - Shelter::EnergyDrainPerSec * Rain * Dt * (1.0 - ShelterFraction));
	if (Rain >= Shelter::HealthDrainRain)
	{
		InOutHealth = Clamp100(InOutHealth - Shelter::HealthDrainPerSec * Dt * (1.0 - ShelterFraction));
	}
}

double RainSpeedFactor(const double DailyRain, const FString& Goal, const FString& JobId)
{
	if (!(DailyRain > 0.2)) return 1.0;
	// Ce bloc a SA definition du travailleur d'exterieur, distincte d'`isOutdoorWorker` :
	// sans bucheron ni carrier, mais avec tout but `gather*`.
	const bool bOutdoorWorker = JobId == TEXT("farmer") || JobId == TEXT("herder") || JobId == TEXT("fisherman")
		|| JobId == TEXT("butcher") || JobId == TEXT("guard") || Goal.StartsWith(TEXT("gather"), ESearchCase::CaseSensitive);
	const bool bShelterGoal = Goal == TEXT("rest") || Goal == TEXT("eat") || Goal == TEXT("eatTogether")
		|| Goal == TEXT("socialize") || Goal == TEXT("visitFamily") || Goal == GoalShelterRain;
	if (Goal == GoalShelterRain) return 1.0 + DailyRain * 0.28;
	if (!bOutdoorWorker && bShelterGoal) return 1.0 + DailyRain * 0.18;
	if (!bOutdoorWorker) return 0.96;
	return 1.0;
}
}
