#pragma once

#include "CoreMinimal.h"
#include "Sim/AnastasisSimulation.h"
#include "Sim/AnastasisTimeWarp.h"
#include "Sim/AnastasisVillageChronicle.h"
#include "Sim/AnastasisValmireFounders.h"
#include "Sim/AnastasisNotebook.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "Village/AnastasisVillagePresentation.h"
#include "AnastasisSimulationSubsystem.generated.h"

class APawn;
class AAnastasisWorldEmbodiment;
class SWidget;

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
	 * Le village du lancement pose sur une case deja choisie : premier puits, `NpcCount` habitants, foyer et
	 * grenier d'ouverture, chantier d'ouverture (si anastasis.Village.OpeningConstruction), bras du grenier.
	 * C'est la sequence que le debut de partie lance apres l'arpentage du site ; un test la lance sans rendu.
	 * Rend l'identifiant du puits, vide si rien n'a pu etre pose.
	 */
	FString SeedStartVillage(int32 NpcCount, int32 TileX, int32 TileY);

	/**
	 * familles-feu-001 (ecart n°44) -- les fondateurs de Valmire poses par le village du lancement quand
	 * anastasis.Village.Founders vaut 1 : quatre familles et le moine, dans l'ordre de pose. Vide sinon.
	 */
	const TArray<AnastasisFounders::FFounder>& GetFounders() const { return Founders; }

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

	/** SETTLEMENT_MORPHOGENESIS_001 : biographies (`ANASTASIS_SETTLEMENT bio`) et passage / sentiers (`ANASTASIS_SETTLEMENT traffic`). */
	void LogSettlement() const;
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
	 * CHRONIQUE_VILLAGE_001 -- la chronique du village, tenue a chaque frame et a chaque tranche d'un saut
	 * (anastasis.Chronicle.Enabled). Remise a zero avec la simulation (ResetCanonical, scenario qui remplace
	 * le village du lancement). Ne lit que la simulation.
	 */
	const AnastasisChronicle::FVillageChronicle& GetChronicle() const { return Chronicle; }

	/**
	 * Ecrit la chronique en texte (UTF-8) dans Saved/Chronicle/ : `FileName` s'il est donne, sinon
	 * chronique-<graine>-jour-<jour>.txt. Rend le chemin complet, vide en cas d'echec.
	 */
	FString WriteChronicle(const FString& FileName = FString()) const;

	/** memoire-decisions-001 -- le carnet du joueur incarne : ce qu'il a entendu, de qui, version par version. */
	const AnastasisNotebook::FPlayerNotebook& GetNotebook() const { return Notebook; }

	/** Ecrit le carnet dans Saved/Chronicle/ (carnet-<graine>-jour-<jour>.txt par defaut). Rend le chemin, vide en echec. */
	FString WriteNotebook(const FString& FileName = FString()) const;

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
	/** Premiere scene sociale : incarnation d'un chef de famille et ouverture de son chantier si necessaire. */
	void StartHelpScene();
	/** Demande adressee a la personne regardee et a portee de voix. */
	void AskFocusedHelp();
	/** Meme action que E, avec cible explicite pour les preuves instrumentales. */
	void AskHelpToId(const FString& PersonId);
	void ChooseHelpSceneWork();
	void StopHelpSceneWork();
	void ToggleHelpNotebook();
	bool IsHelpSceneActive() const { return bHelpSceneActive; }
	bool HasHelpPanel() const { return HelpPanel.IsValid(); }
	/**
	 * arrivant-seul-001 : ce que le panneau dit a l'habitant arrive seul, sans famille (le debut de partie, player-start-002) :
	 * comment marcher, ou lire ses buts, comment regler le temps. Jamais une touche de la scene d'entraide (E, F, X, J) : elle
	 * n'existe que pour qui a ouvert cette scene.
	 */
	static FString ArrivalHelpText(int32 Day, const FString& Name);

	/** Le pawn local suit-il l'habitant incarne (anastasis.Player.Pawn) ? */
	bool IsPawnBound() const { return BoundPawn.IsValid(); }
	/** ma-cabane-001 : le corps du joueur est-il pose dans sa cabane ? */
	bool IsPawnIndoors() const { return bPawnIndoors; }
	bool IsPawnLying() const { return bPawnLying; }

	/**
	 * player-minimal-001 -- un habitant ordinaire arrive et il est incarne (FVillage::ArriveAsPlayer) ; tuile
	 * negative = settlement + (2, 3). Le seul chemin d'arrivee : `Anastasis.Player.Arrive` et le debut de partie
	 * (player-start-001) passent tous deux ici. Rend l'id, vide si refuse.
	 */
	FString ArrivePlayer(double TileX, double TileY, const TCHAR* Why);

	/** player-start-001 -- vrai si l'habitant-joueur a ete incarne au debut de la partie (anastasis.Player.AutoArrive). */
	bool HasArrivedAtStart() const { return bArrivedAtStart; }

	/** Le pawn local lie a l'habitant incarne (nul sinon). */
	const APawn* GetBoundPawn() const { return BoundPawn.Get(); }

	/**
	 * SAVE_STATE_001 -- sauve la partie dans Saved/SaveGames/<Slot>.sav : l'etat complet de la simulation
	 * (FAnastasisSimulation::SaveState) et les verrous de l'hote. Rend faux sans simulation en cours.
	 */
	bool SaveGameToSlot(const FString& Slot, FString& OutMessage);

	/**
	 * Recharge une partie : la simulation est remplacee (refus = rien n'a change), les verrous de l'hote
	 * reprennent, la presentation est retiree puis refaite depuis l'etat relu. Le monde exterieur est
	 * relu depuis le scenario note a la sauvegarde.
	 */
	bool LoadGameFromSlot(const FString& Slot, FString& OutMessage);

	/** JSON du dernier Save / Load (slot, octets, empreinte, presentation refaite). `{}` avant. */
	const FString& GetSaveStatus() const { return SaveStatus; }

	/** Le scenario exterieur charge par Anastasis.Geo.Load (chemin tel que donne, relatif a Content/), pour qu'une sauvegarde sache le relire. */
	void NoteGeoScenarioPath(const FString& Path) { GeoScenarioPath = Path; }

	/** Lit et valide un scenario exterieur, chemin relatif a Content/ ou absolu (AnastasisSimulationGeo.cpp) ; journalise chaque erreur. */
	static bool ReadGeoScenarioFile(const FString& Path, AnastasisGeo::FScenario& Out);
	/** Charge le monde exterieur (chemin relatif a Content/ ; vide = anastasis.Geo.ScenarioPath). Faux si refuse, chaque erreur au log. */
	bool LoadGeoScenario(const FString& Given);
	/** arrivants-001 (ecart n°53) : Valmire fondee, les groupes qui viendront et le monde d'ou ils viennent (anastasis.Geo.AutoLoad). */
	void OpenValmireToTheWorld();

private:
	void DrawOverlay() const;
	void EnsureHelpPanel();
	FString HelpPanelText() const;
	bool FindHelpFocus(FString& OutSiteId, FString& OutPersonId) const;
	TSharedPtr<SWidget> HelpPanel;
	FString HelpFeedback;
	bool bHelpSceneActive = false;
	bool bHelpNotebookOpen = false;
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
	/**
	 * player-start-001 -- Play = debut du jeu : une fois le village du lancement pose, l'habitant-joueur y arrive
	 * (meme chemin qu'Anastasis.Player.Arrive), si anastasis.Player.AutoArrive ou anastasis.Visual.Mode 2.
	 */
	void TryAutoArrive();
	/** Oriente la camera du pawn vers le puits du village, une fois, a la premiere pose apres l'arrivee. */
	void FacePawnTowardVillage(APawn& Pawn);
	bool bArrivedAtStart = false;
	bool bFaceVillagePending = false;
	void LogDayIfChanged();
	/** CHRONIQUE_VILLAGE_001 : la chronique lit la simulation, si anastasis.Chronicle.Enabled. */
	void ObserveChronicle();
	/** familles-feu-001 : la chronique s'ouvre sur les familles, puis le premier soir au feu. */
	void TellFounding(const AnastasisFounders::FScenario& Scenario);
public:
	/**
	 * geopolitical-world-001 (ecart n°38) -- ce que le monde exterieur a produit depuis le dernier
	 * appel, une ligne ANASTASIS_GEO par evenement. Appele a chaque pas de l'hote et apres chaque
	 * commande Anastasis.Geo.*.
	 */
	void LogGeoEvents();
private:
	/**
	 * Un scenario explicite (FirstWell, FirstHouse, FirstGranary, FirstFarmer, FoodSupply) REMPLACE le
	 * village du lancement : simulation remise a zero sur la meme graine, acteurs de presentation
	 * retires. Les preuves PIE des autres missions retrouvent donc exactement leur etat d'avant. Sans
	 * village de lancement intact, ne fait rien : deux scenarios s'empilent comme avant.
	 */
	void ReplaceStartVillage();
	void TryStartVillage(float DeltaTime);
	void BindRainCanopy();
	/**
	 * opening-in-sim-001 : dit ce que la simulation a decide pour le village d'ouverture
	 * (`FVillage::SeedOpeningVillage`, ecart n°40), avec les lignes de log d'avant ; retient le chantier.
	 */
	void LogOpeningReport(const AnastasisVillage::FOpeningReport& Opening);
	/** Dit une fois l'issue de la maison d'ouverture, attribuee par la simulation elle-meme. */
	void LogOpeningHome();
	bool bOpeningHomeLogged = false;
	bool bPendingStartVillage = false;
	double StartVillageWait = 0.0;
	FString SettlementSiteReport = TEXT("{\"status\":\"not_started\"}");

	FAnastasisSimulation Simulation;
	/** CHRONIQUE_VILLAGE_001. Observation seule : n'ecrit jamais dans Simulation. */
	AnastasisChronicle::FVillageChronicle Chronicle;
	/** memoire-decisions-001. Lecture seule, comme la chronique. */
	AnastasisNotebook::FPlayerNotebook Notebook;
	/** familles-feu-001 : les fondateurs poses, dans l'ordre de pose. */
	TArray<AnastasisFounders::FFounder> Founders;
	TWeakObjectPtr<AAnastasisWorldEmbodiment> RainCanopyActor;
	FAnastasisVillagePresentation VillagePresentation;
	FString FarmerGranaryId;
	FString FirstSiteId;
	FIntPoint FarmerField = FIntPoint(-1, -1);
	/** SAVE_STATE_001 : chemin du scenario exterieur charge (vide sans monde exterieur). */
	FString GeoScenarioPath;
	FString SaveStatus = TEXT("{}");
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
	/** ma-cabane-001 : le corps du joueur est-il dans sa cabane (pour ne journaliser que le passage de la porte) ? */
	bool bPawnIndoors = false;
	/** dormir-couche-001 : le corps du pawn est couche ; sa pose debout (relative a la capsule), a rendre au reveil. */
	bool bPawnLying = false;
	bool bHasPawnMeshPose = false;
	FTransform PawnMeshPose;
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
	/** Host-bound crown coverage at a simulation position; read-only, 0 without embodiment. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static double GetRainCanopyCover(const UObject* WorldContextObject, double SimX, double SimY);

	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetSettlementSiteStatus(const UObject* WorldContextObject);

	/** CHRONIQUE_VILLAGE_001 : la chronique du village en francais (texte complet), vide sans simulation. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetChronicleText(const UObject* WorldContextObject);

	/** CHRONIQUE_VILLAGE_001 : resume JSON de la chronique (jours clos, lignes par type, habitants, morts). */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetChronicleStatus(const UObject* WorldContextObject);

	/** memoire-decisions-001 : le carnet du joueur (texte complet), vide sans simulation. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetNotebookText(const UObject* WorldContextObject);

	/** arrivants-001 : les groupes venus par la route et chaque conseil du soir, en JSON (AnastasisArrivals::StatusJson). */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetArrivalsStatus(const UObject* WorldContextObject);

	/** memoire-decisions-001 : resume JSON du carnet (choses entendues, histoires, conteurs, legendes). */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetNotebookStatus(const UObject* WorldContextObject);

	/** memoire-decisions-001 : ecrit le carnet dans Saved/Chronicle/ et rend le chemin, vide sans simulation. */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString WriteNotebook(const UObject* WorldContextObject, const FString& FileName);

	/** CHRONIQUE_VILLAGE_001 : ecrit la chronique dans Saved/Chronicle/ et rend le chemin, vide sans simulation. */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString WriteChronicle(const UObject* WorldContextObject, const FString& FileName);

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

	/**
	 * SETTLEMENT_MORPHOGENESIS_001 : passage, sentiers et biographie des batiments (JSON). Lu par
	 * settlement-morphogenesis-pie.py ; rien n'est ecrit.
	 */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetSettlementStatus(const UObject* WorldContextObject);

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

	/**
	 * ma-cabane-001, en JSON : la cabane du joueur (chantier, pieces, batisseurs, proprietaire), son foyer, s'il est dedans
	 * et a quoi, ou se tient son corps (monde et repere de la cabane), la porte, l'entree, et qui d'autre y loge ou y est
	 * entre. `{}` sans hote, `"site":""` sans cabane.
	 */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetCabinStatus(const UObject* WorldContextObject);

	/**
	 * dormir-couche-001, en JSON : chaque habitant dedans (logis, activite, couche ou cache, pieds et tete du corps
	 * couche, angle du corps avec l'horizontale), le joueur (pawn couche), et chaque logis (exposition d'interieur,
	 * foyer, dormeurs). `{}` sans hote.
	 */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetSleepStatus(const UObject* WorldContextObject);

	/** Etat instrumental de la scene et des demandes, sans pretendre juger la lisibilite de l'image. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetHelpSceneStatus(const UObject* WorldContextObject);
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static void StartHelpScene(const UObject* WorldContextObject);
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static void AskHelpToId(const UObject* WorldContextObject, const FString& PersonId);

	/**
	 * geopolitical-world-001, en JSON : scenario, jour, verite par noeud, exposition du village, savoir
	 * du village, groupes d'arrivants, paquets en route ; plus `actors` (habitants simules). `{}` sans hote.
	 */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetGeoStatus(const UObject* WorldContextObject);

	/** geopolitical-world-001 : `Anastasis.Geo.Trace` en texte (arrivees, routes, cause, source). Vide sans hote. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetGeoTrace(const UObject* WorldContextObject, const FString& NodeId, const FString& Pressure);

	/**
	 * SAVE_STATE_001, en JSON : empreinte d'etat de la simulation (hexadecimal), temps, jour, habitants et
	 * batiments simules, acteurs de presentation, et le resultat du dernier Anastasis.Sim.Save / Load.
	 * `{}` sans hote.
	 */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetSaveStatus(const UObject* WorldContextObject);
};
