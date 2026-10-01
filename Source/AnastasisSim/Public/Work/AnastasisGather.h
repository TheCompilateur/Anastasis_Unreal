// Recolte et livraison — la boucle du fermier dont le poste est le grenier.
//
// Reference commitee `fee66ae`. Ce qui est porte ici est pur, et prouve par
// vecteurs (`Anastasis.Sim.Parite.Recolte`) :
//
//   sim/npc.js        resourceScore (branche nourriture), deliveryScore (branche
//                     depot), completionBias, traitGoalBias, jobPriority,
//                     workplaceGoalBias (poste au grenier), mealPathBlocked,
//                     survivalWorkFactor, shouldHaulGatherLoad
//   ai/memory.js      presumedNoise, believedStock (sans souvenir de marche)
//   life/needs.js     workWillFactor (paliers)
//   ai/moralPressure  moralPressure (sans colonie, sans deuil)
//   sim/craftWork.js  swingPeriodFor, yieldPerSwing (profil `farm`)
//   sim/craftFatigue  craftFatigueOf
//   sim/fieldCrops.js fieldSeasonFromDay, fieldSeasonGatherAmount
//   sim/fieldWorkPosts preferredFieldPostIndex, fieldPostWorld, nearestFieldPostIndex
//   life/skills.js    gainDomainSkill, skillGoalBias
//   sim/content.js    TRAITS ; metiers/catalog.js farmer, settler
//
// L'habitant n'a ni ambition, ni technique, ni nature tiree au sort : sa
// nature est moyenne (corps, esprit, coeur a 1, sans qualite ni defaut). Ce
// que ces absences annulent est ecrit au point d'usage.
//
// L'etat — gisements connus, session de coups, sac, livraison — vit dans le
// village : Village/AnastasisVillage.h.

#pragma once

#include "CoreMinimal.h"
#include "Life/AnastasisNeeds.h"

namespace AnastasisGather
{
	/** Metiers portes. La chaine est le vocabulaire de la reference. */
	inline const TCHAR* const JobSettler = TEXT("settler");
	inline const TCHAR* const JobFarmer = TEXT("farmer");

	inline const TCHAR* const GoalGatherFood = TEXT("gatherFood");
	inline const TCHAR* const GoalDeliver = TEXT("deliver");

	/** `FOOD_TARGET_FLOOR`, `FOOD_RESERVE_DAYS`. */
	inline constexpr double FoodTargetFloor = 55.0;
	inline constexpr double FoodReserveDays = 6.0;

	/** `PERCEPTION.presumedStock.food`. */
	inline constexpr double PresumedFoodStock = 40.0;

	/** `GOAL_AI`. */
	inline constexpr double HaulCompletion = 28.0;
	inline constexpr double HaulAbandonPenalty = 24.0;
	inline constexpr int32 HeavyLoadAt = 6;
	inline constexpr double SessionCompletion = 26.0;
	inline constexpr double SessionAbandonPenalty = 16.0;
	inline constexpr double TraitPush = 24.0;

	/**
	 * `shouldHaulGatherLoad` : « if (load > 9) return true » dans la reference
	 * commitee `fee66ae`. La copie de travail de la reference porte, NON
	 * commitee, un passage a 11 : ce portage suit le commit.
	 */
	inline constexpr int32 HaulLoadAbove = 9;

	/** `deliver()` : lot maximal vers son propre depot. */
	inline constexpr int32 DepotMaxBatch = 12;

	/** `CRAFT_PROFILES.farm`. */
	inline constexpr double FarmBaseSwingPeriod = 0.72;
	inline constexpr double FarmMinSwingPeriod = 0.52;
	inline constexpr double FarmSkillPeriodFactor = 0.07;
	/** `CRAFT_ARRIVE_BY_CRAFT.farm` : ancrage avant le premier coup. */
	inline constexpr double FarmArriveSeconds = 0.36;

	/** `gainSkill` : par coup de recolte, par livraison. */
	inline constexpr double GatherSkillGain = 0.008;
	inline constexpr double DeliverSkillGain = 0.002;

	/** `SKILL`. */
	inline constexpr double SkillCap = 2.6;
	inline constexpr double SkillFloor = 0.2;

	/** `PERCEPTION` : balayage des gisements. */
	inline constexpr double ScanMove = 1.5;
	inline constexpr int32 SpotCapacity = 26;
	inline constexpr double HearsayPenalty = 5.0;

	/** `FIELD_WORK_POSTS` : huit postes par parcelle. */
	inline constexpr int32 FieldPostCount = 8;

	/** `TRAITS` de content.js, dans l'ordre. */
	struct FTrait
	{
		const TCHAR* Label;
		double Build;
		double Trade;
		double Gather;
		double Explore;
	};
	inline constexpr int32 TraitCount = 6;
	/** 3 = « gardien » (build 1, trade 0,8, gather 1, explore 1). */
	inline constexpr int32 DefaultTraitIndex = 3;
	ANASTASISSIM_API const FTrait& TraitAt(int32 Index);

	/** `jobPriority(npc, goal)` — rang dans `job.priority`, 18 - rang * 2,5. */
	ANASTASISSIM_API double JobPriority(const FString& JobId, const FString& Goal);

	/** `job.traitBias.gather`. */
	ANASTASISSIM_API double JobTraitBiasGather(const FString& JobId);

	/** `presumedNoise(npcId, resource)` — FNV-1a, dans [0,85 ; 1,15). */
	ANASTASISSIM_API double PresumedNoise(const FString& NpcId, const FString& Resource);

	/** `believedStock(sim, npc).food` sans souvenir de marche : 40 * bruit. */
	ANASTASISSIM_API double BelievedFoodPresumed(const FString& NpcId);

	/**
	 * `resourceScore(sim, npc, "food", believed)` — sans chantier ni colonie,
	 * sans ambition (planBias 0). ActorCount = `sim.actors.length`.
	 */
	ANASTASISSIM_API double ResourceScoreFood(int32 ActorCount, double Believed, const FString& JobId, double TraitGather, int32 InventoryFood, double Hunger);

	/**
	 * `deliveryScore`, branche depot : `48 + charge * 5 + jobPriority`. Rend 0
	 * sans charge de la ressource du depot (la branche « charge generale » ne
	 * sert qu'a qui porte autre chose que ce que son poste accepte).
	 */
	ANASTASISSIM_API double DeliveryScoreDepot(const FString& JobId, int32 DepotLoad);

	/**
	 * `completionBias(sim, npc, goal)` — Load = `inventoryLoad`, DepotLoad = charge
	 * de la ressource du poste, SessionGoal = but de la session de coups (vide sans).
	 */
	ANASTASISSIM_API double CompletionBias(const FString& Goal, int32 Load, int32 DepotLoad, const FString& SessionGoal, bool bNeedsCritical);

	/** `traitGoalBias(npc, goal)`. */
	ANASTASISSIM_API double TraitGoalBias(const FTrait& Trait, const FString& Goal);

	/** `createNpc` : competence de domaine teintee par le trait (competences fournies a 1). */
	ANASTASISSIM_API double TintedGatherSkill(const FTrait& Trait);
	ANASTASISSIM_API double TintedTradeSkill(const FTrait& Trait);

	/**
	 * `workplaceGoalBias(sim, npc, goal)` pour un poste au GRENIER (depot de
	 * nourriture, jamais ferme ici). Les +9 de trajet (`workCommutePos`) : entretien,
	 * apprentissage, livraison (le grenier porte `storage` au catalogue).
	 */
	ANASTASISSIM_API double GranaryWorkplaceGoalBias(const FString& JobId, const FString& Goal, int32 FoodCarried);

	/** `workWillFactor(npc)` — adulte : 1 / 0,7 / 0,45, 0 si un besoin est critique. */
	ANASTASISSIM_API double WorkWillFactor(const AnastasisNeeds::FNeeds& Needs);

	/**
	 * `moralPressure(sim, npc).effectiveWork` — sans colonie (moral collectif 50),
	 * sans deuil, sans rush. MarketFood = `sim.market.stock.food`.
	 */
	ANASTASISSIM_API double MoralEffectiveWork(const AnastasisNeeds::FNeeds& Needs, int32 MarketFood, int32 Day);

	/**
	 * `moralPressure(sim, npc).socialMul` : moral bas -> chercher compagnie (x1,12, x1,08),
	 * famine et froid -> moins (x0,92, x0,94) ; borne [0,5 ; 1,2]. Sans deuil.
	 */
	ANASTASISSIM_API double MoralSocialMul(double Morale, int32 MarketFood, int32 Day);

	/** `mealPathBlocked`, sans memoire d'echec de repas (non portee). */
	ANASTASISSIM_API bool MealPathBlocked(double Hunger, int32 InventoryFood, double BelievedFood);

	/** `survivalWorkFactor(goal, workFactor, mealBlocked)`. */
	ANASTASISSIM_API double SurvivalWorkFactor(const FString& Goal, double WorkFactor, bool bMealBlocked);

	/** `shouldHaulGatherLoad` — sans rush, sans hub, pour la nourriture. */
	ANASTASISSIM_API bool ShouldHaulGatherLoad(int32 Load);

	/** `craftFatigueOf(npc).periodMul`. */
	ANASTASISSIM_API double CraftFatiguePeriodMul(int32 SwingsDone, double Energy);

	/** `swingPeriodFor(npc, "farm")` — sans technique ni coup de main. */
	ANASTASISSIM_API double SwingPeriodFarm(double Skill, int32 SwingsDone, double Energy);

	/** `yieldPerSwing(npc, "farm")` — 2, ou 3 des la competence 1,35. */
	ANASTASISSIM_API int32 YieldPerSwingFarm(double Skill);

	/** `fieldSeasonFromDay(day)` : 0 printemps, 1 ete, 2 automne, 3 hiver. */
	ANASTASISSIM_API int32 FieldSeasonFromDay(int32 Day);

	/** `fieldSeasonGatherAmount(base, day)`. */
	ANASTASISSIM_API int32 FieldSeasonGatherAmount(int32 Base, int32 Day);

	/** `preferredFieldPostIndex(npc, tile)`. */
	ANASTASISSIM_API int32 PreferredFieldPostIndex(const FString& NpcId, int32 TileX, int32 TileY);

	/** `fieldPostWorld(tile, index)`. */
	ANASTASISSIM_API void FieldPostWorld(int32 TileX, int32 TileY, int32 Index, double& OutX, double& OutY);

	/** `nearestFieldPostIndex(tile, x, y)`. */
	ANASTASISSIM_API int32 NearestFieldPostIndex(int32 TileX, int32 TileY, double X, double Y);

	/**
	 * `fieldWorkTarget`, le choix d'index : le poste prefere, sinon le premier
	 * libre en tournant ; tous pris -> le prefere. ClaimedMask : bit i = poste i tenu.
	 */
	ANASTASISSIM_API int32 FieldWorkPostIndex(int32 Preferred, uint32 ClaimedMask);

	/** `natureLearnFactor` d'une nature moyenne (esprit 1). */
	ANASTASISSIM_API double NeutralLearnFactor();

	/**
	 * `gainDomainSkill(npc, amount, goal)` — competence plate et competence du
	 * domaine du but (cueillette pour gatherFood, marche pour deliver).
	 */
	ANASTASISSIM_API void GainDomainSkill(double& InOutSkill, double& InOutDomainSkill, double Amount);

	/** `skillGoalBias` pour un but dont le domaine vaut DomainSkill. */
	ANASTASISSIM_API double SkillGoalBias(double DomainSkill);
}
