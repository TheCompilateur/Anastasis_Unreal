// Monde exterieur, V1 : la geopolitique comme pression qui se propage (geopolitical-world-001).
//
// EXTENSION — ecart n°38. La reference JS n'a ni monde exterieur, ni puissances, ni rumeurs qui
// voyagent : tout ce module est nouveau. Il ne s'active que si un hote charge un scenario
// (`FAnastasisSimulation::GetGeo().Load`) ; aucun scenario du harnais ne le fait, et sans scenario
// la simulation est celle d'avant, au bit pres.
//
// Ce que le module modelise, et rien d'autre :
//
//   EVENEMENT DU MONDE (choc)  ->  PAQUETS DE PRESSION  ->  ROUTES (delai, attenuation)  ->  NOEUDS
//                                                                                 -> NOEUD DU VILLAGE
//                                                                                 -> EXPOSITION
//                                                                                 -> ADAPTATEUR LOCAL
//   et, a part :
//   EVENEMENT DU MONDE  ->  PAQUETS D'INFORMATION (plus rapides, deformes)  ->  SAVOIR DU VILLAGE
//
// Le village ne lit jamais la verite du monde : il lit son exposition (ce qui l'a atteint) et son
// savoir (ce qu'on lui a dit, ou ce qu'il a vecu). La verite reste interrogeable pour le debug.
//
// Loi des donnees : le C++ porte la physique (unites, delais, attenuation, determinisme). La
// situation historique (lieux, routes, puissances, chocs, et leur source) vit dans un scenario JSON,
// avec sa provenance. Rien d'historique n'est ecrit en dur ici.
//
// Temps : le jour de simulation (`FAnastasisSimulation::GetDay`). Pas de pas plus fin, pas d'horloge
// murale, pas de tirage aleatoire : meme scenario + memes interventions = meme suite, au bit pres.
//
// Unites : toute pression est une INTENSITE normalisee dans [0, 1] (0 = rien ne pese, 1 = la pire
// situation que le canal peut decrire). Deux pressions independantes se combinent en « ou bruite » :
// P = 1 - (1 - P)(1 - m), qui reste dans [0, 1] et ne double jamais une menace deja totale.

#pragma once

#include "CoreMinimal.h"

namespace AnastasisJson { struct FValue; }

namespace AnastasisGeo
{
	/** Les canaux materiels. Peu nombreux, chacun avec un sens causal clair (voir `PressureMeaning`). */
	enum class EPressure : uint8
	{
		/** Echanges coupes ou rares : marchands absents, prix, biens introuvables. */
		TradeDisruption,
		/** Danger sur les chemins et aux champs : raids, enlevements, bandes. */
		Insecurity,
		/** Gens en mouvement vers le village : fuyards, familles, captifs evades. */
		Migration,
		/** Troupes presentes ou en marche : requisitions, passages, peur. */
		Military,
		/** Prelevement exige : impot, tribut, corvee, part de recolte. */
		Extraction,
		Num
	};
	inline constexpr int32 PressureCount = static_cast<int32>(EPressure::Num);

	ANASTASISSIM_API const TCHAR* PressureName(EPressure Pressure);
	/** Phrase courte : ce que la pression veut dire pour un village. */
	ANASTASISSIM_API const TCHAR* PressureMeaning(EPressure Pressure);
	ANASTASISSIM_API bool ParsePressure(const FString& Name, EPressure& Out);

	/** Statut historique d'un objet du scenario (HP-001 §1). */
	enum class EHistoricalStatus : uint8
	{
		Verified,
		Plausible,
		Abstraction,
		Unknown,
	};
	ANASTASISSIM_API const TCHAR* StatusName(EHistoricalStatus Status);
	ANASTASISSIM_API bool ParseStatus(const FString& Name, EHistoricalStatus& Out);

	/** Pourquoi cet objet existe. Tout objet du scenario en porte une. */
	struct FProvenance
	{
		/** Document source (ex. `HP-001`, `BW85`) ; `designer` pour une abstraction de jeu. */
		FString SourceId;
		/** Page, section, ou note. */
		FString SourceRef;
		EHistoricalStatus Status = EHistoricalStatus::Unknown;
		/** [0, 1] : la confiance que le scenario accorde a cet objet. */
		double Confidence = 0.0;
		FString Note;
	};

	/** Regles d'un canal materiel. Valeurs dans le scenario, jamais en dur. */
	struct FChannelRules
	{
		/** Part de l'ecart a la base qui SURVIT a un jour : P = base + (P - base) * Retention. [0, 1]. */
		double RetentionPerDay = 0.85;
		/** Multiplie le temps de trajet d'une route (jours) : < 1 = plus rapide que le pas de route. > 0. */
		double SpeedFactor = 1.0;
		/** Part de l'intensite qui franchit un relais (noeud intermediaire). [0, 1]. */
		double HopAttenuation = 0.8;
		/** Sous ce seuil, un paquet ne repart plus. ]0, 1]. */
		double MinMagnitude = 0.03;
	};

	/** Regles de l'information. Elle voyage a part, plus vite, et se deforme en route. */
	struct FInformationRules
	{
		double SpeedFactor = 0.5;
		/** Fiabilite gardee a chaque relais. [0, 1]. */
		double ReliabilityPerHop = 0.85;
		/** Grossissement par relais : rapportee = vraie * (1 + Exaggeration * relais), bornee a 1. >= 0. */
		double ExaggerationPerHop = 0.15;
		/** Sous cette fiabilite, une nouvelle ne repart plus. ]0, 1]. */
		double MinReliability = 0.2;
	};

	/** Passage d'une pression migratoire a des personnes (le pont macro -> micro). */
	struct FMigrationRules
	{
		/** Personnes par unite d'intensite migratoire qui atteint le village. >= 0. */
		double PersonsPerUnit = 10.0;
		/** Plafond d'un groupe d'arrivants. >= 0. */
		int32 MaxPersonsPerBatch = 6;
		/** Sous ce seuil, une vague n'amene personne. [0, 1]. */
		double MinMagnitude = 0.05;
	};

	/** Influence d'un acteur sur un noeud : plusieurs dimensions, jamais un proprietaire. Chacune dans [0, 1]. */
	struct FInfluence
	{
		double Political = 0.0;
		double Military = 0.0;
		double Trade = 0.0;
		double Administrative = 0.0;
	};

	struct FNode
	{
		FString Id;
		FString Label;
		/** Region, Settlement, Port, Pass, TradeHub, PoliticalCenter, Pasture… : une etiquette, pas un enum. */
		FString Type;
		TArray<FString> Tags;
		double PopulationWeight = 0.0;
		double EconomicWeight = 0.0;
		double StrategicWeight = 0.0;
		/** Niveau de repos de chaque canal, vers lequel la pression revient. */
		double Baseline[PressureCount] = {};
		FProvenance Provenance;
	};

	struct FRoute
	{
		FString Id;
		FString From;
		FString To;
		/** Jours de trajet au pas de route (avant le facteur de vitesse du canal). > 0. */
		double TravelDays = 1.0;
		/** Part de chaque canal que la route transmet. [0, 1]. */
		double Transmission[PressureCount] = { 1.0, 1.0, 1.0, 1.0, 1.0 };
		double InformationTransmission = 1.0;
		/** Plafond d'intensite migratoire et militaire qu'un paquet peut porter sur cette route. [0, 1]. */
		double Capacity = 1.0;
		/** Danger propre de la route : amplifie l'insecurite qui la traverse, par (1 + BaseRisk). [0, 1]. */
		double BaseRisk = 0.0;
		bool bEnabled = true;
		bool bBidirectional = true;
		FProvenance Provenance;
	};

	struct FActor
	{
		FString Id;
		FString Label;
		TArray<FString> Tags;
		/** Noeud -> influence. Ordre du scenario. */
		TArray<TPair<FString, FInfluence>> InfluenceByNode;
		FProvenance Provenance;
	};

	struct FEmission
	{
		EPressure Pressure = EPressure::TradeDisruption;
		double Magnitude = 0.0;
	};

	/** Une cause exterieure. Generique : un choc n'a pas de code a lui, il a des emissions. */
	struct FShockDef
	{
		FString Id;
		FString Label;
		FString SourceNode;
		/** Acteur responsable, ou vide. */
		FString ActorId;
		/** Cause mere (un choc peut en deriver un autre), ou vide. */
		FString ParentCauseId;
		int32 StartDay = 1;
		/** Tant qu'il dure, le noeud source garde au moins l'intensite emise. >= 1. */
		int32 DurationDays = 1;
		TArray<FEmission> Emissions;
		/** Intensite de la nouvelle emise (0 = l'evenement ne se raconte pas). [0, 1]. */
		double InformationMagnitude = 0.0;
		TArray<FString> CauseTags;
		FProvenance Provenance;
	};

	struct FScenario
	{
		FString Id;
		FString Label;
		/** Le noeud ou vit le village. Obligatoire. */
		FString VillageNodeId;
		/** Ce que le jour 1 represente (texte, avec sa provenance). */
		FString CalendarNote;
		FChannelRules Channels[PressureCount];
		FInformationRules Information;
		FMigrationRules Migration;
		/** Une administration forte amortit l'insecurite qui arrive : * (1 - Damping * reach max). [0, 1]. */
		double AdministrationDamping = 0.5;
		/** Plafond de relais d'un paquet (garde contre l'explosion). >= 1. */
		int32 MaxHops = 6;
		TArray<FNode> Nodes;
		TArray<FRoute> Routes;
		TArray<FActor> Actors;
		TArray<FShockDef> InitialShocks;
		FProvenance Provenance;

		const FNode* FindNode(const FString& NodeId) const;
		const FRoute* FindRoute(const FString& RouteId) const;
		const FActor* FindActor(const FString& ActorId) const;
	};

	/** Lit un scenario JSON (format : `docs/unreal/GEOPOLITICAL_WORLD_001.md`). Faux et erreurs sinon. */
	ANASTASISSIM_API bool ParseScenario(const FString& Json, FScenario& Out, TArray<FString>& OutErrors);

	/**
	 * Rejette ce qui ne peut pas etre simule : identifiants en double, references vers rien, temps
	 * negatifs, valeurs hors bornes, village sans ancre, chocs mal dates. Ne repare jamais.
	 */
	ANASTASISSIM_API bool ValidateScenario(const FScenario& Scenario, TArray<FString>& OutErrors);

	/** Valide un choc contre un scenario (utilise aussi par l'injection). */
	ANASTASISSIM_API bool ValidateShock(const FScenario& Scenario, const FShockDef& Shock, TArray<FString>& OutErrors);

	// --- Etat vivant -----------------------------------------------------------------------------

	/** Un paquet de pression en route. Toujours rattache a sa cause racine et a son paquet parent. */
	struct FPressurePacket
	{
		FString Id;
		/** Seq de creation : l'ordre de traitement a jour egal. */
		int64 Seq = 0;
		FString RootCauseId;
		FString ParentPacketId;
		EPressure Pressure = EPressure::TradeDisruption;
		double Magnitude = 0.0;
		FString FromNode;
		FString ToNode;
		/** Vide pour l'emission a la source. */
		FString RouteId;
		int32 DepartureDay = 0;
		int32 ArrivalDay = 0;
		int32 Hop = 0;
	};

	/** Une nouvelle en route. Elle sait ce qu'elle raconte, pas ce qui est vrai. */
	struct FInformationPacket
	{
		FString Id;
		int64 Seq = 0;
		FString RootCauseId;
		FString ParentPacketId;
		FString OriginNode;
		FString FromNode;
		FString ToNode;
		FString RouteId;
		int32 CreationDay = 0;
		int32 DepartureDay = 0;
		int32 ArrivalDay = 0;
		int32 Hop = 0;
		TArray<FString> SubjectTags;
		/** Ce que la nouvelle dit de l'ampleur. */
		double ReportedMagnitude = 0.0;
		double Reliability = 1.0;
		/** `route` (racontee de proche en proche), `origin` (sur place). */
		FString SourceType;
	};

	/** Une arrivee de pression a un noeud : la piece de base de toute trace causale. */
	struct FArrival
	{
		FString PacketId;
		FString NodeId;
		EPressure Pressure = EPressure::TradeDisruption;
		double Magnitude = 0.0;
		double Before = 0.0;
		double After = 0.0;
		int32 Day = 0;
	};

	/** Ce que le village sait. Une entree par nouvelle recue ou par pression vecue. */
	struct FKnownReport
	{
		FString Id;
		/** Paquet (information ou pression) qui l'a apportee. */
		FString PacketId;
		FString CauseId;
		int32 LearnedDay = 0;
		int32 EventDay = 0;
		FString OriginNode;
		TArray<FString> SubjectTags;
		double ReportedMagnitude = 0.0;
		double Reliability = 0.0;
		/** `rumor` (nouvelle), `experience` (vecu au village). */
		FString SourceType;
	};

	/** Un groupe d'arrivants : le geopolitique le cree, le village l'instancie. */
	struct FMigrationBatch
	{
		FString Id;
		FString CauseId;
		FString PacketId;
		FString OriginNode;
		int32 Day = 0;
		int32 Persons = 0;
		/** `pending` (a admettre), `admitted`, `blocked` (aucun sol libre, ou pas de village). */
		FString Status;
		/** Identifiants des habitants crees par l'adaptateur local. */
		TArray<FString> AdmittedIds;
	};

	/** Ce que le village subit. Petit, stable, la seule porte vers les systemes locaux. */
	struct FVillageExposure
	{
		int32 Day = 0;
		double Pressure[PressureCount] = {};
		/** Les dernieres entrees du savoir du village, de la plus recente a la plus ancienne. */
		TArray<FKnownReport> RecentInformation;
	};

	/** Une ligne de trace : d'une arrivee locale jusqu'a la cause et sa source. */
	struct FTraceStep
	{
		FString PacketId;
		FString FromNode;
		FString ToNode;
		FString RouteId;
		int32 DepartureDay = 0;
		int32 ArrivalDay = 0;
		double Magnitude = 0.0;
	};

	struct FCauseTrace
	{
		FArrival Arrival;
		/** Du paquet arrive vers l'emission a la source. */
		TArray<FTraceStep> Steps;
		FString RootCauseId;
		FString RootLabel;
		FString RootActorId;
		FProvenance RootProvenance;
	};

	class ANASTASISSIM_API FGeoWorld
	{
	public:
		/**
		 * Charge un scenario valide au jour `Day` : bases, influences, chocs initiaux en attente.
		 * Faux (et monde vide) si le scenario ne valide pas.
		 */
		bool Load(const FScenario& InScenario, int32 Day, TArray<FString>& OutErrors);
		void Unload();
		bool IsLoaded() const { return bLoaded; }
		const FScenario& GetScenario() const { return Scenario; }
		int32 GetDay() const { return Day; }

		/**
		 * Ajoute un choc. Identifiant vide = `shock-N`. Un choc qui commence avant ou au jour courant
		 * emet tout de suite (au jour courant) ; sinon a son jour. Rend l'identifiant, vide si refuse.
		 */
		FString InjectShock(const FShockDef& Shock, TArray<FString>& OutErrors);

		/** Ouvre ou ferme une route. Les paquets deja en route arrivent quand meme ; aucun nouveau n'y part. */
		bool SetRouteEnabled(const FString& RouteId, bool bEnabled);

		/** Fait passer les jours jusqu'a `ToDay` compris, un par un. Sans effet si `ToDay` <= jour courant. */
		void AdvanceToDay(int32 ToDay);

		// --- Lecture : verite du monde (debug) ---
		double GetTruePressure(const FString& NodeId, EPressure Pressure) const;
		FInfluence GetInfluence(const FString& ActorId, const FString& NodeId) const;
		const TArray<FPressurePacket>& GetPressureInTransit() const { return PressureQueue; }
		const TArray<FInformationPacket>& GetInformationInTransit() const { return InformationQueue; }
		const TArray<FArrival>& GetArrivals() const { return Arrivals; }
		const TArray<FShockDef>& GetShocks() const { return Shocks; }
		bool IsShockActive(const FShockDef& Shock) const;

		// --- Lecture : ce que le village a (la seule porte pour les systemes locaux) ---
		const FVillageExposure& GetVillageExposure() const { return Exposure; }
		const TArray<FKnownReport>& GetVillageKnowledge() const { return Knowledge; }
		const TArray<FMigrationBatch>& GetMigrationBatches() const { return Batches; }

		/** Indices des groupes `pending`, dans l'ordre d'arrivee. */
		TArray<int32> PendingMigration() const;
		/** L'adaptateur local rend compte : identifiants crees, ou `blocked`. */
		void RecordAdmission(int32 BatchIndex, const TArray<FString>& AdmittedIds);

		/** Les arrivees de `Pressure` au noeud, de la plus recente a la plus ancienne, remontees a leur cause. */
		TArray<FCauseTrace> TraceCause(const FString& NodeId, EPressure Pressure, int32 MaxArrivals = 8) const;

		/** Evenements lisibles produits depuis le dernier appel (pour les logs de l'hote). */
		TArray<FString> DrainEvents();

		// --- Persistance ---
		/** Tout l'etat vivant, en JSON. Le scenario n'y est pas : il se recharge depuis sa source. */
		FString SaveState() const;
		/** Recharge un etat sur le scenario qui l'a produit (meme identifiant exige). */
		bool LoadState(const FScenario& InScenario, const FString& Json, TArray<FString>& OutErrors);
		/** Empreinte de l'etat (determinisme). */
		uint64 Digest() const;

		// --- Debug textuel ---
		FString DescribeStatus() const;
		FString DescribeNode(const FString& NodeId) const;
		FString DescribeExposure() const;
		FString DescribeTrace(const FString& NodeId, EPressure Pressure) const;
		/** Etat complet en JSON pour les preuves (vrai etat + exposition + savoir + groupes). */
		FString StatusJson() const;

	private:
		void StepDay(int32 D);
		void ApplyDecay();
		void EmitShock(int32 ShockIndex, int32 D);
		void ProcessArrivals(int32 D);
		void ArrivePressure(const FPressurePacket& Packet, int32 D);
		void ArriveInformation(const FInformationPacket& Packet, int32 D);
		void PropagatePressure(const FPressurePacket& Packet, int32 D);
		void PropagateInformation(const FInformationPacket& Packet, int32 D);
		void RefreshExposure();
		void AddKnowledge(FKnownReport Report);
		void Event(const FString& Line);
		int32 NodeIndex(const FString& NodeId) const;
		const FShockDef* FindShock(const FString& ShockId) const;
		int32 TravelDays(const FRoute& Route, double SpeedFactor) const;
		double MaxAdministrativeReach(const FString& NodeId) const;
		/** Routes sortantes de `NodeId` (sens direct ou retour d'une route a double sens), ordre du scenario. */
		void OutgoingRoutes(const FString& NodeId, TArray<TPair<int32, FString>>& Out) const;
		void ResetRuntime();

		bool bLoaded = false;
		FScenario Scenario;
		int32 Day = 0;
		/** [noeud][canal]. */
		TArray<TArray<double>> NodePressure;
		/** Etat d'ouverture courant des routes (le scenario donne l'initial). */
		TArray<bool> RouteEnabled;
		TArray<FShockDef> Shocks;
		TArray<bool> ShockEmitted;
		TArray<FPressurePacket> PressureQueue;
		TArray<FInformationPacket> InformationQueue;
		/** Tous les paquets de pression crees : la memoire de la trace causale. */
		TArray<FPressurePacket> PressureArchive;
		TMap<FString, int32> PressureArchiveIndex;
		TArray<FArrival> Arrivals;
		/** Cle `cause|canal|noeud` (ou `cause|info|noeud`) deja atteinte : un paquet ne repasse jamais. */
		TSet<FString> Visited;
		TArray<FKnownReport> Knowledge;
		TArray<FMigrationBatch> Batches;
		FVillageExposure Exposure;
		TArray<FString> PendingEvents;
		int64 NextSeq = 1;
		int32 NextShockId = 1;
		int32 NextPacketId = 1;
		int32 NextInformationId = 1;
		int32 NextReportId = 1;
		int32 NextBatchId = 1;
	};
}
