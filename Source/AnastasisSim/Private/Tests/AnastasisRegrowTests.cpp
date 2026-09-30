#include "Misc/AutomationTest.h"

#include "Work/AnastasisFields.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisRegrowParity
{
	namespace Vecteurs
	{
#include "AnastasisRegrowVectors.inl"
	}

	double RegrowFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	uint64 RegrowToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	AnastasisWorld::ECropId CropOf(const FString& Name)
	{
		using AnastasisWorld::ECropId;
		if (Name == TEXT("grain")) return ECropId::Grain;
		if (Name == TEXT("greens")) return ECropId::Greens;
		if (Name == TEXT("fruit")) return ECropId::Fruit;
		if (Name == TEXT("fallow")) return ECropId::Fallow;
		return ECropId::None;
	}

	const TCHAR* CropName(AnastasisWorld::ECropId Crop)
	{
		using AnastasisWorld::ECropId;
		switch (Crop)
		{
		case ECropId::Grain: return TEXT("grain");
		case ECropId::Greens: return TEXT("greens");
		case ECropId::Fruit: return TEXT("fruit");
		case ECropId::Fallow: return TEXT("fallow");
		default: return TEXT("");
		}
	}
}

/**
 * La repousse des champs — compare a `Simulation.regrowFieldsDaily` EXECUTEE
 * (`fee66ae`, extraction propre) sur une tuile : a-t-elle repousse, et que
 * porte-t-elle apres. Vecteurs : tools/migration/parity/regrow.mjs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisParityRegrowTest,
	"Anastasis.Sim.Parite.Repousse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisParityRegrowTest::RunTest(const FString&)
{
	using namespace AnastasisRegrowParity;
	using namespace AnastasisRegrowParity::Vecteurs;
	int32 Failures = 0;
	int32 Grown = 0;

	for (int32 I = 0; I < UE_ARRAY_COUNT(RegrowDailyVectors); ++I)
	{
		const FRegrowDailyVector& V = RegrowDailyVectors[I];
		AnastasisWorld::FTile Tile;
		Tile.X = V.A0;
		Tile.Y = V.A1;
		Tile.Type = AnastasisWorld::ETileType::Field;
		Tile.Resource = FString(UTF8_TO_TCHAR(V.A3)) == TEXT("food") ? AnastasisWorld::EResource::Food : AnastasisWorld::EResource::None;
		Tile.Amount = V.A4;
		Tile.CropId = CropOf(UTF8_TO_TCHAR(V.A5));
		Tile.Fertility = RegrowFromBits(V.A6Bits);
		const bool bGrown = AnastasisFields::RegrowTileDaily(Tile, V.A2);
		Grown += bGrown ? 1 : 0;
		const FString Resource = Tile.Resource == AnastasisWorld::EResource::Food ? TEXT("food") : TEXT("");
		const FString WantResource = UTF8_TO_TCHAR(V.AttenduResource);
		const FString WantCrop = UTF8_TO_TCHAR(V.AttenduCrop);
		if (bGrown != (V.AttenduGrown != 0) || Resource != WantResource || Tile.Amount != V.AttenduAmount || CropName(Tile.CropId) != WantCrop)
		{
			++Failures;
			AddError(FString::Printf(TEXT("RegrowDaily[%d] (%d,%d) jour %d : %s %s/%d/%s attendu %s %s/%d/%s"),
				I, V.A0, V.A1, V.A2,
				bGrown ? TEXT("repousse") : TEXT("rien"), *Resource, Tile.Amount, CropName(Tile.CropId),
				V.AttenduGrown ? TEXT("repousse") : TEXT("rien"), *WantResource, V.AttenduAmount, *WantCrop));
		}
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(RegenAmountVectors); ++I)
	{
		const FRegenAmountVector& V = RegenAmountVectors[I];
		const int32 Amount = AnastasisFields::RegenAmount(AnastasisFields::FieldRegenPerDay, V.A0);
		const double Chance = AnastasisFields::DailyChance(V.A0);
		if (Amount != V.AttenduAmount || RegrowToBits(Chance) != V.AttenduChanceBits)
		{
			++Failures;
			AddError(FString::Printf(TEXT("RegenAmount[%d] jour %d : %d / %.17g attendu %d / %.17g"),
				I, V.A0, Amount, Chance, V.AttenduAmount, RegrowFromBits(V.AttenduChanceBits)));
		}
	}

	AddInfo(FString::Printf(TEXT("Repousse : %d + %d vecteurs, %d tuiles repoussees, %d ecarts"),
		static_cast<int32>(UE_ARRAY_COUNT(RegrowDailyVectors)), static_cast<int32>(UE_ARRAY_COUNT(RegenAmountVectors)), Grown, Failures));
	return Failures == 0;
}

#endif
