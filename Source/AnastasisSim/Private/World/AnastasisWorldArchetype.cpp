#include "World/AnastasisWorldArchetype.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimMath.h"

namespace AnastasisWorldArchetype
{
	namespace
	{
		FKnobs Base(EId Id)
		{
			FKnobs K;
			K.Id = Id;
			switch (Id)
			{
			case EId::Vale:
				K.SeaLowFrac = 0.10; K.ValleyDig = 1.0; K.BasinDig = 1.0; K.ChannelDig = 1.0; K.LakeDig = 1.0;
				K.MinWaterBody = 8; K.ForestT = 0.61; K.FieldT = 0.35; K.Highland = 0.61; K.RockT = 0.45; K.RuinT = 0.75;
				K.RidgeBoost = 0.06; K.PlateauFlat = 0.38; K.MoistBias = 0.02; K.RiparianBoost = 0.08;
				K.ClearRadius = 9; K.ExtentW = 96; K.ExtentH = 96;
				break;
			case EId::Delta:
				K.SeaLowFrac = 0.18; K.ValleyDig = 1.35; K.BasinDig = 1.7; K.ChannelDig = 1.25; K.LakeDig = 1.45;
				K.MinWaterBody = 6; K.ForestT = 0.63; K.FieldT = 0.41; K.Highland = 0.65; K.RockT = 0.50; K.RuinT = 0.76;
				K.RidgeBoost = 0.04; K.PlateauFlat = 0.45; K.MoistBias = 0.0; K.RiparianBoost = 0.08;
				K.ClearRadius = 9; K.ExtentW = 112; K.ExtentH = 96;
				break;
			case EId::Plateau:
				K.SeaLowFrac = 0.035; K.ValleyDig = 0.35; K.BasinDig = 0.25; K.ChannelDig = 0.55; K.LakeDig = 0.4;
				K.MinWaterBody = 10; K.ForestT = 0.625; K.FieldT = 0.37; K.Highland = 0.58; K.RockT = 0.44; K.RuinT = 0.72;
				K.RidgeBoost = 0.05; K.PlateauFlat = 0.55; K.MoistBias = -0.07; K.RiparianBoost = 0.04;
				K.ClearRadius = 10; K.ExtentW = 112; K.ExtentH = 112;
				break;
			case EId::Highland:
				K.SeaLowFrac = 0.07; K.ValleyDig = 1.15; K.BasinDig = 0.7; K.ChannelDig = 1.1; K.LakeDig = 0.75;
				K.MinWaterBody = 8; K.ForestT = 0.615; K.FieldT = 0.33; K.Highland = 0.48; K.RockT = 0.34; K.RuinT = 0.70;
				K.RidgeBoost = 0.16; K.PlateauFlat = 0.08; K.MoistBias = -0.05; K.RiparianBoost = 0.04;
				K.ClearRadius = 9; K.ExtentW = 88; K.ExtentH = 96;
				break;
			case EId::Deepwood:
				K.SeaLowFrac = 0.075; K.ValleyDig = 0.9; K.BasinDig = 0.85; K.ChannelDig = 0.9; K.LakeDig = 0.9;
				K.MinWaterBody = 8; K.ForestT = 0.59; K.FieldT = 0.35; K.Highland = 0.60; K.RockT = 0.46; K.RuinT = 0.74;
				K.RidgeBoost = 0.07; K.PlateauFlat = 0.22; K.MoistBias = -0.01; K.RiparianBoost = 0.06;
				K.ClearRadius = 11; K.ExtentW = 104; K.ExtentH = 104;
				break;
			}
			return K;
		}

		double ArchetypeJitter(uint32 Seed)
		{
			const double N = AnastasisJs::Sin(static_cast<double>(Seed) * 0.00117 + 19.17) * 43758.5453;
			return N - AnastasisJs::Floor(N);
		}
	}

	EId PickId(uint32 Seed)
	{
		const uint32 H = AnastasisJs::Imul(Seed ^ (Seed >> 16), 0x45d9f3bu);
		const uint32 H2 = AnastasisJs::Imul(H ^ (H >> 13), 0x119de1f3u);
		return static_cast<EId>(H2 % static_cast<uint32>(Count));
	}

	FKnobs Resolve(uint32 Seed)
	{
		FKnobs BaseKnobs = Base(PickId(Seed));
		const double J = ArchetypeJitter(Seed);
		FKnobs Out = BaseKnobs;
		Out.SeaLowFrac = AnastasisMath::Clamp(BaseKnobs.SeaLowFrac + J * 0.025 - 0.012, 0.02, 0.24);
		Out.ForestT = AnastasisMath::Clamp(BaseKnobs.ForestT + (J - 0.5) * 0.02, 0.54, 0.70);
		Out.FieldT = AnastasisMath::Clamp(BaseKnobs.FieldT + (J - 0.5) * 0.025, 0.22, 0.42);
		Out.MoistBias = BaseKnobs.MoistBias + (J - 0.5) * 0.025;
		Out.ClearRadius = static_cast<int32>(AnastasisJs::Round(AnastasisMath::Clamp(BaseKnobs.ClearRadius + (J - 0.5) * 2.2, 5.0, 12.0)));
		return Out;
	}

	const TCHAR* IdName(EId Id)
	{
		switch (Id)
		{
		case EId::Vale: return TEXT("vale");
		case EId::Delta: return TEXT("delta");
		case EId::Plateau: return TEXT("plateau");
		case EId::Highland: return TEXT("highland");
		case EId::Deepwood: return TEXT("deepwood");
		}
		return TEXT("vale");
	}
}
