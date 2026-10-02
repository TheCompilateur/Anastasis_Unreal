// Le chantier — un batiment se monte piece par piece, un coup de batisseur a la fois.
//
// Reference commitee `fee66ae`. Ce qui est porte ici est pur, et prouve par
// vecteurs (`Anastasis.Sim.Parite.Chantier`) :
//
//   sim/npc.js                buildScore (branche « un chantier est ouvert »),
//                             pickBuildSite (score d'un chantier)
//   sim/collectivePriorities  collectiveBuildingNeedScore : 85 des qu'un chantier est ouvert
//   sim/simulation.js         buildCost / costMultiplier, siteCanPlacePiece, consumeSiteMaterials
//   sim/constructionPieces.js CONSTRUCTION_BLUEPRINT (22 pieces), placeConstructionPieces
//   sim/craftWork.js          profil `build`, swingPeriodFor, CRAFT_ARRIVE_BY_CRAFT.build
//   sim/craftToolSwitch.js    craftToolSwitchSeconds
//   sim/content.js            BUILDINGS[type].cost, BUILD_COST_GROWTH ; metiers builder
//   life/skills.js            competence `craft` teintee par le trait (createNpc)
//
// L'etat — chantiers, stock du site, session de coups — vit dans le village :
// Village/AnastasisVillage.h.

#pragma once

#include "CoreMinimal.h"
#include "Work/AnastasisGather.h"

namespace AnastasisBuild
{
	inline const TCHAR* const JobBuilder = TEXT("builder");
	inline const TCHAR* const GoalBuild = TEXT("build");
	inline const TCHAR* const CraftBuild = TEXT("build");

	/** `CONSTRUCTION_PIECE_TOTAL` : piquets, cordeau, fondation, poteaux, lisse, murs, chevrons, sous-toiture, toit. */
	inline constexpr int32 PieceTotal = 22;

	/** `collectiveBuildingNeedScore` quand au moins un chantier est ouvert. */
	inline constexpr double NeedWithActiveSite = 85.0;

	/** `CRAFT_PROFILES.build` et `CRAFT_ARRIVE_BY_CRAFT.build`. */
	inline constexpr double BaseSwingPeriod = 0.62;
	inline constexpr double MinSwingPeriod = 0.46;
	inline constexpr double SkillPeriodFactor = 0.06;
	inline constexpr int32 SwingsPerAction = 1;
	inline constexpr double ArriveSeconds = 0.4;

	/** `CRAFT_TOOL_SWITCH_SEC`. */
	inline constexpr double ToolSwitchSeconds = 0.48;

	/** `gainSkill(npc, 0.004)` a chaque piece posee ; domaine du but `build` : `craft`. */
	inline constexpr double BuildSkillGain = 0.004;

	/** `progressBuildWork` : au-dela de 0,75 tuile du seuil, on marche encore. */
	inline constexpr double SiteReachDistance = 0.75;

	/** `SITE_STOCK_PROFILE.cap`. */
	inline constexpr int32 SiteWoodCap = 80;
	inline constexpr int32 SiteStoneCap = 60;

	/** `BUILD_COST_GROWTH`. */
	inline constexpr int32 CostFreePerType = 2;
	inline constexpr double CostPerExistingOfType = 0.012;
	inline constexpr double CostMaxMultiplier = 2.2;

	/** `workConstruction` : moral +1 par piece, +4 a l'achevement. */
	inline constexpr double PieceMorale = 1.0;
	inline constexpr double CompletionMorale = 4.0;

	/** Devis en bois et pierre (`BUILDINGS[type].cost`, apres `costMultiplier`). */
	struct FBuildCost
	{
		int32 Wood = 0;
		int32 Stone = 0;
	};

	/** `BUILDINGS[type].cost` pour les types portes ; faux pour un type inconnu. */
	ANASTASISSIM_API bool BaseCost(const FString& Type, FBuildCost& Out);

	/** `buildCost(type)` : ceil(base x costMultiplier), multiplicateur selon les acheves du type. Sans scierie : pas de planches. */
	ANASTASISSIM_API FBuildCost BuildCost(const FString& Type, int32 CompletedOfType);

	/** `costMultiplier(type)`. */
	ANASTASISSIM_API double CostMultiplier(int32 CompletedOfType);

	/** `JOBS[job].traitBias.build`. */
	ANASTASISSIM_API double JobTraitBiasBuild(const FString& JobId);

	/**
	 * `buildScore(sim, npc)` quand un chantier est ouvert : besoin 85, liquidite 1,
	 * `85 x trait.build x job.traitBias.build + builderFit + jobPriority(build)` ;
	 * plans, colonisation, brief de chantier et biais collectif : nuls ici.
	 */
	ANASTASISSIM_API double BuildScoreActiveSite(double TraitBuild, const FString& JobId);

	/** `BUILD_WAGE` : le salaire d'une journee de chantier (content.js). */
	inline constexpr int32 BuildWage = 15;
	/** `FOUNDATION.wageGold` / `FOUNDATION.inKindWood`. */
	inline constexpr int32 FoundationWageGold = 4;
	inline constexpr int32 FoundationInKindWood = 4;

	/**
	 * `buildScore` sans les biais de plan, de colonisation et de brief :
	 * `needFloor x liquidity x trait.build x job.traitBias.build + builderFit + jobPriority(build)`.
	 */
	ANASTASISSIM_API double BuildScoreFromNeed(double NeedTimesLiquidity, double TraitBuild, const FString& JobId);

	/** `swingPeriodFor(npc, "build")`, sans coup de main ni technique. */
	ANASTASISSIM_API double SwingPeriod(double Skill, int32 SwingsDone, double Energy);

	/** `craftToolSwitchSeconds(from, to)` ; `From` vide = pas de session precedente. */
	ANASTASISSIM_API double CraftToolSwitchSeconds(const FString& From, const FString& To);

	/** `createNpc` : `skills.craft *= 0.88 + trait.build x 0,12`, x 1,18 pour l'artisan (competence de depart 1). */
	ANASTASISSIM_API double TintedCraftSkill(const AnastasisGather::FTrait& Trait);

	/** `pickBuildSite` : le score d'un chantier vu par un habitant (sans memoire d'acces). */
	ANASTASISSIM_API double SiteScore(double Distance, bool bSessionHere, bool bOwnSite, bool bPieceReady, bool bFarmDone, bool bFarmSite);

	/** Materiaux d'un chantier : devis, deja poses, stock physique sur le site (rien n'y est reserve). */
	struct FSiteMaterials
	{
		int32 NeedWood = 0;
		int32 NeedStone = 0;
		int32 ConsumedWood = 0;
		int32 ConsumedStone = 0;
		int32 StockWood = 0;
		int32 StockStone = 0;
	};

	/** La part d'une ressource pour une piece : max(1, ceil(reste x n / pieces restantes)). */
	ANASTASISSIM_API int32 PieceShare(int32 StillNeeded, int32 PiecesPlaced, int32 PieceCount = 1);

	/** `siteCanPlacePiece(building, 1)`. */
	ANASTASISSIM_API bool SiteCanPlacePiece(const FSiteMaterials& M, int32 PiecesPlaced);

	/** `consumeSiteMaterials(building, 1)` : faux (et rien ne bouge) si la piece n'est pas posable. */
	ANASTASISSIM_API bool ConsumeSiteMaterials(FSiteMaterials& M, int32 PiecesPlaced);

	/** `placeConstructionPieces(building, 1)` : vrai si une piece a ete posee ; progres = posees / 22, 1 a la derniere. */
	ANASTASISSIM_API bool PlaceConstructionPiece(int32& InOutPiecesPlaced, double& OutProgress);

	/** `CONSTRUCTION_BLUEPRINT[i].kind` (0..21). */
	ANASTASISSIM_API const TCHAR* PieceKind(int32 Index);
}
