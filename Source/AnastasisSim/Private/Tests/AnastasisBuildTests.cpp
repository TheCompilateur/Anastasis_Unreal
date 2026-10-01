#include "Misc/AutomationTest.h"

#include "Village/AnastasisVillage.h"
#include "Work/AnastasisBuild.h"
#include "Work/AnastasisGather.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisBuildParity
{
	namespace Vecteurs
	{
#include "AnastasisBuildVectors.inl"
	}

	double BuildFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	uint64 BuildToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisBuildParityTest,
	"Anastasis.Sim.Parite.Chantier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisBuildParityTest::RunTest(const FString&)
{
	using namespace AnastasisBuildParity;
	namespace B = AnastasisBuild;
	namespace G = AnastasisGather;
	int32 Checked = 0;
	int32 Failed = 0;
	auto Expect = [&](bool bOk, const FString& What)
	{
		++Checked;
		if (!bOk)
		{
			++Failed;
			if (Failed <= 20) AddError(What);
		}
	};

	for (const auto& V : Vecteurs::CostVectors)
	{
		const B::FBuildCost Cost = B::BuildCost(ANSI_TO_TCHAR(V.A0), V.A1);
		const double Mul = B::CostMultiplier(V.A1);
		Expect(Cost.Wood == V.AttenduWood && Cost.Stone == V.AttenduStone && BuildToBits(Mul) == V.AttenduMulBits,
			FString::Printf(TEXT("Cost %hs x%d : %d/%d mul %.17g, attendu %d/%d"), V.A0, V.A1, Cost.Wood, Cost.Stone, Mul, V.AttenduWood, V.AttenduStone));
	}

	for (const auto& V : Vecteurs::JobsVectors)
	{
		const FString Job = ANSI_TO_TCHAR(V.A0);
		const double Priority = G::JobPriority(Job, B::GoalBuild);
		const double Bias = B::JobTraitBiasBuild(Job);
		Expect(BuildToBits(Priority) == V.AttenduPriorityBits && BuildToBits(Bias) == V.AttenduTraitBiasBits,
			FString::Printf(TEXT("Jobs %s : priorite %.17g biais %.17g"), *Job, Priority, Bias));
	}

	for (const auto& V : Vecteurs::TraitsVectors)
	{
		const G::FTrait& Trait = G::TraitAt(V.A0);
		const double Craft = B::TintedCraftSkill(Trait);
		Expect(BuildToBits(Trait.Build) == V.AttenduBuildBits && BuildToBits(Craft) == V.AttenduCraftBits,
			FString::Printf(TEXT("Traits %d : build %.17g craft %.17g, attendu craft %.17g"), V.A0, Trait.Build, Craft, BuildFromBits(V.AttenduCraftBits)));
	}

	for (const auto& V : Vecteurs::SwingVectors)
	{
		const double Period = B::SwingPeriod(BuildFromBits(V.A0Bits), V.A1, BuildFromBits(V.A2Bits));
		Expect(BuildToBits(Period) == V.AttenduBits,
			FString::Printf(TEXT("Swing %.17g / %d / %.17g : %.17g, attendu %.17g"), BuildFromBits(V.A0Bits), V.A1, BuildFromBits(V.A2Bits), Period, BuildFromBits(V.AttenduBits)));
	}

	for (const auto& V : Vecteurs::ToolSwitchVectors)
	{
		const double Seconds = B::CraftToolSwitchSeconds(ANSI_TO_TCHAR(V.A0), ANSI_TO_TCHAR(V.A1));
		Expect(BuildToBits(Seconds) == V.AttenduBits, FString::Printf(TEXT("ToolSwitch %hs -> %hs : %.17g"), V.A0, V.A1, Seconds));
	}

	for (const auto& V : Vecteurs::PiecesVectors)
	{
		int32 Placed = V.A0;
		double Progress = 0.0;
		const bool bMoved = B::PlaceConstructionPiece(Placed, Progress);
		const FString Kind = bMoved ? FString(B::PieceKind(Placed - 1)) : FString();
		Expect(Placed == V.AttenduPlaced && BuildToBits(Progress) == V.AttenduProgressBits && Kind == ANSI_TO_TCHAR(V.AttenduKind) && bMoved == (V.AttenduMoved != 0),
			FString::Printf(TEXT("Pieces %d : %d %.17g %s"), V.A0, Placed, Progress, *Kind));
	}

	for (const auto& V : Vecteurs::MaterialsVectors)
	{
		B::FSiteMaterials M;
		M.NeedWood = V.A1;
		M.NeedStone = V.A2;
		M.ConsumedWood = V.A3;
		M.ConsumedStone = V.A4;
		M.StockWood = V.A5;
		M.StockStone = V.A6;
		const bool bOk = B::ConsumeSiteMaterials(M, V.A0);
		Expect(bOk == (V.AttenduOk != 0) && M.ConsumedWood == V.AttenduConsumedWood && M.ConsumedStone == V.AttenduConsumedStone
			&& M.StockWood == V.AttenduStockWood && M.StockStone == V.AttenduStockStone,
			FString::Printf(TEXT("Materials poses %d devis %d/%d conso %d/%d stock %d/%d : %d -> conso %d/%d stock %d/%d"),
				V.A0, V.A1, V.A2, V.A3, V.A4, V.A5, V.A6, bOk ? 1 : 0, M.ConsumedWood, M.ConsumedStone, M.StockWood, M.StockStone));
	}

	for (const auto& V : Vecteurs::CompletionVectors)
	{
		const FString Goal = ANSI_TO_TCHAR(V.A0);
		const FString Craft = ANSI_TO_TCHAR(V.A1);
		const FString SessionGoal = Craft == TEXT("build") ? FString(B::GoalBuild) : Craft == TEXT("farm") ? FString(G::GoalGatherFood) : FString();
		AnastasisNeeds::FNeeds N;
		N.Hunger = BuildFromBits(V.A3Bits);
		N.Thirst = 10.0;
		N.Energy = 90.0;
		N.Social = 90.0;
		N.Leisure = 90.0;
		N.Hygiene = 90.0;
		N.Health = 95.0;
		N.Morale = 60.0;
		// Sans poste : pas de depot, la charge du depot est nulle.
		const double Bias = G::CompletionBias(Goal, V.A2, 0, SessionGoal, AnastasisVillage::NeedsCritical(N));
		Expect(BuildToBits(Bias) == V.AttenduBits,
			FString::Printf(TEXT("Completion %s / %s / sac %d / faim %.3g : %.17g, attendu %.17g"), *Goal, *Craft, V.A2, N.Hunger, Bias, BuildFromBits(V.AttenduBits)));
	}

	AddInfo(FString::Printf(TEXT("%d vecteurs, %d ecarts"), Checked, Failed));
	return Failed == 0;
}

#endif
