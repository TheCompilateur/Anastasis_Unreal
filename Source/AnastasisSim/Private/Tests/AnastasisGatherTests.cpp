#include "Misc/AutomationTest.h"

#include "Village/AnastasisVillage.h"
#include "Work/AnastasisGather.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisGatherParity
{
	namespace Vecteurs
	{
#include "AnastasisGatherVectors.inl"
	}

	double GatherFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	uint64 GatherToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	/** Les huit besoins que la declaration fixe (gather.mjs, `habitant`). */
	AnastasisNeeds::FNeeds FixedNeeds(double Hunger)
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = Hunger;
		N.Thirst = 10.0;
		N.Energy = 90.0;
		N.Social = 90.0;
		N.Leisure = 90.0;
		N.Hygiene = 90.0;
		N.Health = 95.0;
		N.Morale = 60.0;
		return N;
	}
}

/**
 * La boucle du fermier — compare BIT A BIT a la reference executee (`fee66ae`) :
 * scores de recolte et de livraison, fin de tache, trait, envie de travailler,
 * pression morale, rythme et rendement des coups, saison, poste dans la
 * parcelle, apprentissage. Vecteurs : tools/migration/parity/gather.mjs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisParityGatherTest,
	"Anastasis.Sim.Parite.Recolte",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisParityGatherTest::RunTest(const FString&)
{
	using AnastasisGatherParity::GatherFromBits;
	using AnastasisGatherParity::GatherToBits;
	using AnastasisGatherParity::FixedNeeds;
	using namespace AnastasisGatherParity::Vecteurs;
	namespace G = AnastasisGather;
	int32 Failures = 0;
	int32 Checked = 0;

	auto CheckD = [&](const TCHAR* Case, int32 I, const TCHAR* Field, double Got, uint64 Expected)
	{
		++Checked;
		if (GatherToBits(Got) != Expected)
		{
			++Failures;
			AddError(FString::Printf(TEXT("%s[%d].%s : %.17g attendu %.17g"), Case, I, Field, Got, GatherFromBits(Expected)));
		}
	};
	auto CheckI = [&](const TCHAR* Case, int32 I, const TCHAR* Field, int32 Got, int32 Expected)
	{
		++Checked;
		if (Got != Expected)
		{
			++Failures;
			AddError(FString::Printf(TEXT("%s[%d].%s : %d attendu %d"), Case, I, Field, Got, Expected));
		}
	};

	for (int32 I = 0; I < UE_ARRAY_COUNT(WorkScoresVectors); ++I)
	{
		const FWorkScoresVector& V = WorkScoresVectors[I];
		const FString Id = UTF8_TO_TCHAR(V.A3);
		const G::FTrait& Trait = G::TraitAt(V.A4);
		const double Hunger = GatherFromBits(V.A1Bits);
		const double Believed = G::BelievedFoodPresumed(Id);
		CheckD(TEXT("WorkScores"), I, TEXT("believed"), Believed, V.AttenduBelievedBits);
		CheckD(TEXT("WorkScores"), I, TEXT("food"), G::ResourceScoreFood(V.A0, Believed, G::JobFarmer, Trait.Gather, V.A2, Hunger), V.AttenduFoodBits);
		CheckD(TEXT("WorkScores"), I, TEXT("deliver"), G::DeliveryScoreDepot(G::JobFarmer, V.A2), V.AttenduDeliverBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(CompletionVectors); ++I)
	{
		const FCompletionVector& V = CompletionVectors[I];
		const FString Goal = UTF8_TO_TCHAR(V.A0);
		// Fermier au grenier : la nourriture est a la fois la charge et la charge du depot.
		const bool bCritical = AnastasisVillage::NeedsCritical(FixedNeeds(GatherFromBits(V.A3Bits)));
		const FString SessionGoal = V.A2 != 0 ? FString(G::GoalGatherFood) : FString();
		CheckD(TEXT("Completion"), I, *Goal, G::CompletionBias(Goal, V.A1, V.A1, SessionGoal, bCritical), V.AttenduBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(TraitBiasVectors); ++I)
	{
		const FTraitBiasVector& V = TraitBiasVectors[I];
		const FString Goal = UTF8_TO_TCHAR(V.A1);
		CheckD(TEXT("TraitBias"), I, *Goal, G::TraitGoalBias(G::TraitAt(V.A0), Goal), V.AttenduBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(SkillTintVectors); ++I)
	{
		const FSkillTintVector& V = SkillTintVectors[I];
		CheckD(TEXT("SkillTint"), I, TEXT("gather"), G::TintedGatherSkill(G::TraitAt(V.A0)), V.AttenduGatherBits);
		CheckD(TEXT("SkillTint"), I, TEXT("trade"), G::TintedTradeSkill(G::TraitAt(V.A0)), V.AttenduTradeBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(WorkWillVectors); ++I)
	{
		const FWorkWillVector& V = WorkWillVectors[I];
		AnastasisNeeds::FNeeds N;
		N.Hunger = GatherFromBits(V.A0Bits);
		N.Thirst = GatherFromBits(V.A1Bits);
		N.Energy = GatherFromBits(V.A2Bits);
		N.Social = GatherFromBits(V.A3Bits);
		N.Leisure = 100.0;
		N.Hygiene = 100.0;
		N.Health = GatherFromBits(V.A4Bits);
		N.Morale = GatherFromBits(V.A5Bits);
		CheckD(TEXT("WorkWill"), I, TEXT("factor"), G::WorkWillFactor(N), V.AttenduBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(MoralVectors); ++I)
	{
		const FMoralVector& V = MoralVectors[I];
		AnastasisNeeds::FNeeds N = FixedNeeds(GatherFromBits(V.A2Bits));
		N.Morale = GatherFromBits(V.A3Bits);
		CheckD(TEXT("Moral"), I, TEXT("effectiveWork"), G::MoralEffectiveWork(N, V.A0, V.A1), V.AttenduWorkBits);
		CheckD(TEXT("Moral"), I, TEXT("socialMul"), G::MoralSocialMul(N.Morale, V.A0, V.A1), V.AttenduSocialBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(SwingVectors); ++I)
	{
		const FSwingVector& V = SwingVectors[I];
		const double Skill = GatherFromBits(V.A0Bits);
		CheckD(TEXT("Swing"), I, TEXT("period"), G::SwingPeriodFarm(Skill, V.A1, GatherFromBits(V.A2Bits)), V.AttenduPeriodBits);
		CheckI(TEXT("Swing"), I, TEXT("yield"), G::YieldPerSwingFarm(Skill), V.AttenduYield);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(SeasonVectors); ++I)
	{
		const FSeasonVector& V = SeasonVectors[I];
		CheckI(TEXT("Season"), I, TEXT("amount"), G::FieldSeasonGatherAmount(V.A0, V.A1), V.Attendu);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(FieldPostVectors); ++I)
	{
		const FFieldPostVector& V = FieldPostVectors[I];
		const FString Id = UTF8_TO_TCHAR(V.A0);
		const int32 Preferred = G::PreferredFieldPostIndex(Id, V.A1, V.A2);
		const uint32 Claimed = V.A3 >= 0 ? (1u << V.A3) : 0u;
		double X = 0.0;
		double Y = 0.0;
		G::FieldPostWorld(V.A1, V.A2, G::FieldWorkPostIndex(Preferred, Claimed), X, Y);
		CheckI(TEXT("FieldPost"), I, TEXT("preferred"), Preferred, V.AttenduPreferred);
		CheckD(TEXT("FieldPost"), I, TEXT("x"), X, V.AttenduXBits);
		CheckD(TEXT("FieldPost"), I, TEXT("y"), Y, V.AttenduYBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(LearnVectors); ++I)
	{
		const FLearnVector& V = LearnVectors[I];
		double Skill = GatherFromBits(V.A0Bits);
		double Domain = GatherFromBits(V.A1Bits);
		G::GainDomainSkill(Skill, Domain, GatherFromBits(V.A2Bits));
		CheckD(TEXT("Learn"), I, TEXT("skill"), Skill, V.AttenduSkillBits);
		CheckD(TEXT("Learn"), I, TEXT("domain"), Domain, V.AttenduDomainBits);
		CheckD(TEXT("Learn"), I, TEXT("bias"), G::SkillGoalBias(Domain), V.AttenduBiasBits);
	}

	AddInfo(FString::Printf(TEXT("Recolte : %d valeurs comparees, %d ecarts"), Checked, Failures));
	return Failures == 0;
}

#endif
