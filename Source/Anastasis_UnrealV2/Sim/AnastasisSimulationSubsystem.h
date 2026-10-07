#pragma once

#include "CoreMinimal.h"
#include "Sim/AnastasisSimulation.h"
#include "Sim/AnastasisTimeWarp.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "Village/AnastasisVillagePresentation.h"
#include "AnastasisSimulationSubsystem.generated.h"

class APawn;

/**
 * Pompe Unreal du tick de simulation. Possede FAnastasisSimulation.
 * Tick seulement dans les mondes game/PIE — pas l'editeur.
 */
UCLASS()
class UAnastasisSimulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	void ResetCanonical(uint32 Seed);
	const FString& GetSettlementSiteReport() const { return SettlementSiteReport; }
	void LogStatus() const;

	/**
	 * VILLAGER_PNG_001 -- le village du lancement : `anastasis.Village.StartVillagers` habitants
	 * (12 par defaut, 0 = village vide) autour du premier puits, poses par SeedFirstWell au debut
	 * de partie pour que le jeu ne s'ouvre pas sur un monde vide. Vrai tant qu'il est intact.
	 */
	bool HasStartVillage() const { return bStartVillage; }

	/**
	 * Premier batiment (mission first-building-001) : pose un puits sur la case
	 * libre la plus proche de (TileX, TileY) et `NpcCount` habitants autour, a
	 * soifs echelonnees. Rend l'identifiant du puits, vide si rien n'a pu etre pose.
	 * Tout passe par la simulation ; la presentation suit au prochain Sync.
	 */
	FString SeedFirstWell(int32 NpcCount, int32 TileX, int32 TileY);

	/**
	 * La maison (mission house-rest-001) : deux maisons pres de (TileX, TileY),
	 * la premiere a un proprietaire, la seconde libre ; `NpcCount` habitants,
	 * dont un proprietaire et les autres sans toit, abrites par
	 * `assignSheltersDaily`. Rend l'identifiant de la maison possedee.
	 */
	FString SeedFirstHouse(int32 NpcCount, int32 TileX, int32 TileY);

	/**
	 * Le grenier (mission granary-eat-001) : un grenier pres de (TileX, TileY),
	 * rempli de `Food` portions, et `NpcCount` habitants SANS TOIT a moins de 7
	 * cases (ils le voient), faims echelonnees. Sans toit parce que, dans la
	 * reference, un habitant qui a un foyer va manger chez lui (HOUSE -> eat).
	 */
	FString SeedFirstGranary(int32 NpcCount, int32 Food, int32 TileX, int32 TileY);

	/**
	 * ARCHITECTURE_SCALE_001 (architecture-crusade-001) : un hameau pour juger l'espace bati -- un puits,
	 * `Houses` maisons en grappe autour (une parcelle libre entre deux au plus pres), un grenier rempli,
	 * `NpcCount` habitants, les premiers proprietaires, les autres abrites. Ne change aucune regle de la
	 * simulation : c'est un scenario de preuve comme FirstHouse. Rend le nombre de batiments poses.
	 */
	int32 SeedArchitectureHamlet(int32 Houses, int32 NpcCount, int32 TileX, int32 TileY);

	/** Une ligne `ANASTASIS_ARCH record` par batiment : archetype et etat vivant (AnastasisArchitecture::Describe). */
	void LogArchitecture() const;
	bool SeedFoodSupply();

	/**
	 * Le fermier (mission gather-deliver-001) : le champ genere le plus proche de
	 * (TileX, TileY), un grenier VIDE a 3-4 cases (seuil qui atteint le champ),
	 * `FarmerCount` fermiers poses au seuil et embauches au grenier. Ils voient le
	 * champ ; la table de la reference les envoie cueillir puis livrer.
	 */
	FString SeedFirstFarmer(int32 FarmerCount, int32 TileX, int32 TileY);
	const FString& GetFarmerGranaryId() const { return FarmerGranaryId; }

	/**
	 * Le chantier (mission build-001) : un chantier `Type` ouvert pres de (TileX, TileY)
	 * sur la premiere case libre, devis livre sur place si `bDelivered`, et
	 * `BuilderCount` batisseurs poses a son seuil. L'ouverture par les habitants et
	 * les livraisons ne sont pas portees : c'est l'hote qui ouvre.
	 */
	FString SeedFirstSite(const FString& Type, int32 BuilderCount, bool bDelivered, int32 TileX, int32 TileY);
	const FString& GetFirstSiteId() const { return FirstSiteId; }
	FIntPoint GetFarmerField() const { return FarmerField; }

	/** Reflete les batiments de la simulation en acteurs. Appele a chaque Tick. */
	int32 SyncVillagePresentation();
	const FAnastasisVillagePresentation& GetVillagePresentation() const { return VillagePresentation; }

	FAnastasisSimulation& GetSimulation() { return Simulation; }
	const FAnastasisSimulation& GetSimulation() const { return Simulation; }

	/**
	 * TIME_WARP_001 -- avance instantanee (Anastasis.Sim.Advance) : `Seconds` simulees dans cette
	 * frame, presentation resynchronisee, temoin informe (tout ce temps est oisif pour le village).
	 * Rend le nombre de Tick consommes.
	 */
	int32 AdvanceBy(double Seconds);

	/** Ce que le village a vu du joueur (presence, jours oisifs). Voir AnastasisTimeWarp::FWitness. */
	const AnastasisTimeWarp::FWitness& GetWitness() const { return Witness; }

	/** Secondes simulees par seconde reelle, lissees : l'acceleration obtenue, pas la demandee. */
	double GetEffectiveRate() const { return EffectiveRate; }

	/** Vrai si la derniere frame acceleree a ete coupee par anastasis.Sim.WarpBudgetMs. */
	bool WasWarpBudgetCut() const { return bWarpBudgetCut; }

	/**
	 * player-minimal-001 -- direction de marche imposee par une commande (agents, preuves), en axes de
	 * la simulation. Tant qu'elle tient, l'entree du pawn est ignoree ; (0, 0) la rend au pawn.
	 */
	void SetScriptedDrive(double DX, double DY);

	/** Le pawn local suit-il l'habitant incarne (anastasis.Player.Pawn) ? */
	bool IsPawnBound() const { return BoundPawn.IsValid(); }

private:
	void DrawOverlay() const;
	/** Ligne JOUEUR de l'overlay : habitant incarne, presence, reputation, jours oisifs. */
	void DrawPlayerOverlay() const;
	/** Avant les pas : la direction du pawn (ou la commande) devient celle du corps incarne. */
	void ApplyPlayerInput();
	/**
	 * Le temoin TIME_WARP_001 regarde l'habitant incarne, et lui seul : sans joueur il ne compte rien.
	 * Il repart des valeurs de la personne quand on en change, et les lui reecrit apres chaque pas.
	 */
	void ObservePlayerTime(double SimSeconds, double Multiplier);
	/** Apres la presentation : pawn pose sur le corps incarne, sa carte cachee (on ne se voit pas). */
	void PlacePlayerPawn();
	/** Le pawn retrouve sa marche Unreal. */
	void UnbindPawn();
	void LogDayIfChanged();
	/**
	 * Un scenario explicite (FirstWell, FirstHouse, FirstGranary, FirstFarmer, FoodSupply) REMPLACE le
	 * village du lancement : simulation remise a zero sur la meme graine, acteurs de presentation
	 * retires. Les preuves PIE des autres missions retrouvent donc exactement leur etat d'avant. Sans
	 * village de lancement intact, ne fait rien : deux scenarios s'empilent comme avant.
	 */
	void ReplaceStartVillage();
	void TryStartVillage(float DeltaTime);
	/** Equipe un habitant du village initial d'un foyer et d'un travail reels. */
	void SeedOpeningHousehold();
	/** Un chantier initial fini par les habitants existants, sans creer de PNJ ni modifier AnastasisSim. */
	void SeedOpeningConstruction();
	/** Affecte des colons encore libres au grenier accessible du village initial. */
	void SeedOpeningWorkforce();
	/** Attribue la maison achevee a un de ses bâtisseurs capable d'en atteindre l'acces. */
	void AssignCompletedOpeningHome();
	bool bPendingStartVillage = false;
	double StartVillageWait = 0.0;
	FString SettlementSiteReport = TEXT("{\"status\":\"not_started\"}");

	FAnastasisSimulation Simulation;
	FAnastasisVillagePresentation VillagePresentation;
	FString FarmerGranaryId;
	FString FirstSiteId;
	FString OpeningSiteId;
	FString OpeningWorkId;
	FIntPoint FarmerField = FIntPoint(-1, -1);
	int32 LoggedDay = 0;
	/** True only after OnWorldBeginPlay. Tests ResetCanonical without the engine ticker. */
	bool bPumpFromEngineTick = false;
	bool bStartVillage = false;
	/** Miroir de l'accumulateur de la simulation : fraction du pas en cours, pour interpoler les cartes. */
	double PresentationAccumulator = 0.0;
	/** Voie acceleree (anastasis.Sim.Warp != 1). */
	AnastasisTimeWarp::FWarpPump WarpPump;
	AnastasisTimeWarp::FWitness Witness;
	double EffectiveRate = 0.0;
	bool bWarpBudgetCut = false;

	/** player-minimal-001. La personne que le temoin regarde ; vide sans joueur. */
	FString WitnessPersonId;
	TWeakObjectPtr<APawn> BoundPawn;
	bool bScriptedDrive = false;
	FVector2D ScriptedDrive = FVector2D::ZeroVector;
};

/**
 * Lecteurs de debug pour les scripts de preuve Python (tools/unreal/*-pie.py).
 * Python ne sait pas atteindre un sous-systeme de monde : USubsystemBlueprintLibrary
 * est reservee aux noeuds Blueprint internes. Lecture seule, rien ne decide ici.
 */
UCLASS()
class UAnastasisSimulationDebugLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Temps de simulation (s) de l'hote de ce monde, -1 sans hote. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static double GetSimulationTime(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetSettlementSiteStatus(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FVector GetSettlementGroundPoint(const UObject* WorldContextObject, double SimX, double SimY);

	/** Phase commune du village ("night", "dawn"...), vide sans hote. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetVillagePhase(const UObject* WorldContextObject);

	/** Habitants dedans ce batiment (`npc.inside`), -1 sans hote. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static int32 CountInside(const UObject* WorldContextObject, const FString& BuildingId);

	/**
	 * Etat d'un habitant pour les preuves : "goal|activity|insideBuilding|insideGoal|sheltersTaken",
	 * vide sans hote ou sans cet habitant. Lecture seule.
	 */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetNpcState(const UObject* WorldContextObject, const FString& NpcId);

	/** Stock physique de nourriture d'un batiment, -1 sans hote ou sans batiment. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static int32 GetFoodStock(const UObject* WorldContextObject, const FString& BuildingId);

	/** Total des repas confirmes par les habitants presents, -1 sans hote. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static int32 CountMealsTaken(const UObject* WorldContextObject);

	/** Read-only finite-food accounting for PIE evidence. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetFoodSupplyStatus(const UObject* WorldContextObject);

	/**
	 * Etat du fermier pose par FirstFarmer, en JSON : temps, nourriture aux champs
	 * (tout le monde, etat vivant), sacs, stocks, repas, livraisons, but du premier
	 * fermier, positions Unreal du grenier, du champ et du fermier.
	 */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetGatherStatus(const UObject* WorldContextObject);

	/**
	 * Etat du chantier ouvert par FirstSite, en JSON : temps, progres et pieces,
	 * devis / pose / stock du site, bras inscrits, pieces posees par les habitants,
	 * but et session du premier batisseur, positions Unreal du chantier et du batisseur.
	 */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetBuildStatus(const UObject* WorldContextObject);

	/** Capacites et resultats observes du village courant ; les buts sont des intentions instantanees. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetBuildingCapacityStatus(const UObject* WorldContextObject);

	/**
	 * VILLAGER_PNG_001, en JSON : nombre d'habitants simules et de cartes, puis une ligne par
	 * habitant avec sa carte (portrait, pieds, cachee, en miroir), "look":"" s'il n'en a pas.
	 * Lecture seule ; `{}` sans hote.
	 */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetVillagerCards(const UObject* WorldContextObject);

	/** Premier habitant du village initial : etat simule et usage Smart Object, pour une preuve PIE. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetOpeningLifeStatus(const UObject* WorldContextObject);

	/**
	 * TIME_WARP_001, en JSON : temps et jour simules, Warp / Speed / TimeScale demandes, acceleration
	 * obtenue (secondes simulees par seconde reelle), coupe budget, presence et jours oisifs du temoin.
	 * `{}` sans hote.
	 */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetTimeWarpStatus(const UObject* WorldContextObject);

	/**
	 * player-minimal-001, en JSON : habitant incarne (vide en observateur), position, but, activite,
	 * presence, reputation, jours oisifs, pawn lie, et combien d'habitants le voient a leur portee de
	 * compagnon. `{}` sans hote.
	 */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetPlayerStatus(const UObject* WorldContextObject);
};
