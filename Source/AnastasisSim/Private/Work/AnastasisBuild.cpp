#include "Work/AnastasisBuild.h"

#include "Core/AnastasisJsNumeric.h"

namespace AnastasisBuild
{
	namespace
	{
		// `CONSTRUCTION_BLUEPRINT` : l'ordre de pose.
		const TCHAR* const Blueprint[PieceTotal] = {
			TEXT("stake"), TEXT("stake"), TEXT("stake"), TEXT("stake"), TEXT("cord"), TEXT("foundation"),
			TEXT("post"), TEXT("post"), TEXT("post"), TEXT("post"), TEXT("rail"),
			TEXT("wall"), TEXT("wall"), TEXT("wall"), TEXT("wall"), TEXT("wall"),
			TEXT("rafter"), TEXT("rafter"), TEXT("underRoof"), TEXT("roof"), TEXT("roof"), TEXT("roof"),
		};

		// `CRAFT_TOOL_SWITCH_PAIRS` : les changements d'outil qui se lisent.
		const TCHAR* const SwitchPairs[][2] = {
			{ TEXT("chop"), TEXT("saw") }, { TEXT("saw"), TEXT("chop") },
			{ TEXT("forge"), TEXT("workshop") }, { TEXT("workshop"), TEXT("forge") },
			{ TEXT("forge"), TEXT("maintain") }, { TEXT("maintain"), TEXT("forge") },
			{ TEXT("build"), TEXT("maintain") }, { TEXT("maintain"), TEXT("build") },
			{ TEXT("build"), TEXT("workshop") },
			{ TEXT("tan"), TEXT("workshop") }, { TEXT("workshop"), TEXT("tan") },
		};

		int32 StillNeeded(int32 Need, int32 Consumed)
		{
			return FMath::Max(0, Need - Consumed);
		}
	}

	bool BaseCost(const FString& Type, FBuildCost& Out)
	{
		// `BUILDINGS[type].cost` (batiments/catalog.js), dans l'ordre { wood, stone }.
		if (Type == TEXT("well")) { Out = { 10, 18 }; return true; }
		if (Type == TEXT("house")) { Out = { 24, 8 }; return true; }
		if (Type == TEXT("granary")) { Out = { 26, 16 }; return true; }
		Out = FBuildCost();
		return false;
	}

	double CostMultiplier(int32 CompletedOfType)
	{
		return FMath::Min(CostMaxMultiplier, 1.0 + FMath::Max(0, CompletedOfType - CostFreePerType) * CostPerExistingOfType);
	}

	FBuildCost BuildCost(const FString& Type, int32 CompletedOfType)
	{
		FBuildCost Base;
		BaseCost(Type, Base);
		const double Mul = CostMultiplier(CompletedOfType);
		FBuildCost Cost;
		Cost.Wood = static_cast<int32>(FMath::CeilToDouble(Base.Wood * Mul));
		Cost.Stone = static_cast<int32>(FMath::CeilToDouble(Base.Stone * Mul));
		// Planches : seulement avec une scierie et un stock de planches. Ni l'un ni l'autre ici.
		return Cost;
	}

	double JobTraitBiasBuild(const FString& JobId)
	{
		// `JOBS[job].traitBias.build` (metiers/catalog.js).
		if (JobId == JobBuilder) return 1.35;
		if (JobId == AnastasisGather::JobFarmer) return 0.8;
		return 1.0;
	}

	double BuildScoreActiveSite(double TraitBuild, const FString& JobId)
	{
		// needFloor = max(85, 0, 0) ; liquidite 1 (chantier actif).
		const double BuilderFit = JobId == JobBuilder ? 18.0
			: (JobId == TEXT("artisan") || JobId == TEXT("blacksmith")) ? 6.0 : 0.0;
		return NeedWithActiveSite * 1.0 * TraitBuild * JobTraitBiasBuild(JobId)
			+ BuilderFit
			+ AnastasisGather::JobPriority(JobId, GoalBuild)
			+ 0.0  // planBias : pas de plan
			+ 0.0  // colonizationBuildBias : pas de lisiere chaude
			+ 0.0  // colonySiteBuildBias : pas de brief de chantier (ouverture non portee)
			+ 0.0; // collectiveGoalBias : pas de colonie
	}

	double SwingPeriod(double Skill, int32 SwingsDone, double Energy)
	{
		const double S = AnastasisJs::NumberOr(Skill, 0.7);
		double Period = FMath::Max(MinSwingPeriod, BaseSwingPeriod - S * SkillPeriodFactor);
		Period *= AnastasisGather::CraftFatiguePeriodMul(SwingsDone, Energy);
		// techniqueWorkPeriodMultiplier : 1 sans technique.
		return Period;
	}

	double CraftToolSwitchSeconds(const FString& From, const FString& To)
	{
		if (From.IsEmpty() || To.IsEmpty() || From == To) return 0.0;
		for (const auto& Pair : SwitchPairs)
		{
			if (From == Pair[0] && To == Pair[1]) return ToolSwitchSeconds;
		}
		return ToolSwitchSeconds * 0.72;
	}

	double TintedCraftSkill(const AnastasisGather::FTrait& Trait)
	{
		double Craft = 1.0 * (0.88 + Trait.Build * 0.12);
		if (FCString::Strcmp(Trait.Label, TEXT("artisan")) == 0) Craft *= 1.18;
		return Craft;
	}

	double SiteScore(double Distance, bool bSessionHere, bool bOwnSite, bool bPieceReady, bool bFarmDone, bool bFarmSite)
	{
		double Score = 80.0 - FMath::Min(80.0, Distance);
		// accessFailurePenalty : memoire d'acces non portee, 0.
		if (bSessionHere) Score += 28.0;
		if (bOwnSite) Score += 12.0;
		if (bPieceReady) Score += 56.0;
		else Score -= 48.0;
		if (!bFarmDone && bFarmSite) Score += 50.0;
		else if (!bFarmDone && !bFarmSite) Score -= 24.0;
		return Score;
	}

	int32 PieceShare(int32 Still, int32 PiecesPlaced, int32 PieceCount)
	{
		const int32 Remaining = FMath::Max(1, PieceTotal - PiecesPlaced);
		const int32 N = FMath::Max(1, PieceCount);
		return FMath::Max(1, static_cast<int32>(FMath::CeilToDouble(static_cast<double>(Still) * N / Remaining)));
	}

	bool SiteCanPlacePiece(const FSiteMaterials& M, int32 PiecesPlaced)
	{
		// Object.entries(materialsNeeded) : bois puis pierre.
		const int32 StillWood = StillNeeded(M.NeedWood, M.ConsumedWood);
		if (M.NeedWood > 0 && StillWood > 0 && M.StockWood < PieceShare(StillWood, PiecesPlaced)) return false;
		const int32 StillStone = StillNeeded(M.NeedStone, M.ConsumedStone);
		if (M.NeedStone > 0 && StillStone > 0 && M.StockStone < PieceShare(StillStone, PiecesPlaced)) return false;
		return true;
	}

	bool ConsumeSiteMaterials(FSiteMaterials& M, int32 PiecesPlaced)
	{
		if (!SiteCanPlacePiece(M, PiecesPlaced)) return false;
		auto Debit = [PiecesPlaced](int32 Need, int32& Consumed, int32& Stock)
		{
			if (Need <= 0) return;
			const int32 Still = StillNeeded(Need, Consumed);
			if (Still <= 0) return;
			// `debitStock` : min(disponible, part).
			const int32 Taken = FMath::Min(FMath::Max(0, Stock), PieceShare(Still, PiecesPlaced));
			Stock -= Taken;
			Consumed += Taken;
		};
		Debit(M.NeedWood, M.ConsumedWood, M.StockWood);
		Debit(M.NeedStone, M.ConsumedStone, M.StockStone);
		return true;
	}

	bool PlaceConstructionPiece(int32& InOutPiecesPlaced, double& OutProgress)
	{
		const int32 Before = FMath::Clamp(InOutPiecesPlaced, 0, PieceTotal);
		const int32 N = FMath::Min(PieceTotal, Before + 1);
		InOutPiecesPlaced = N;
		OutProgress = N >= PieceTotal ? 1.0 : static_cast<double>(N) / PieceTotal;
		return N > Before;
	}

	const TCHAR* PieceKind(int32 Index)
	{
		return Blueprint[FMath::Clamp(Index, 0, PieceTotal - 1)];
	}
}
