// Village — les batiments fonctionnels et les habitants qui s'en servent.
//
// Tranche de `src/sim/simulation.js` + `src/sim/npc.js` + `src/sim/navGrid.js`
// + `src/life/villageRhythm.js` + `src/life/domestic.js` + `src/ai/algorithmic/*`
// + `src/sim/transport/stockLedger.js`, limitee a trois boucles completes :
//
//   PUITS  (first-building-001)             MAISON  (house-rest-001)
//   la soif monte                           la fatigue monte ; la nuit tombe
//   `drink` gagne                           `rest` gagne (phaseBias : ~+91 la nuit avec un toit)
//   puits le plus proche -> seuil           foyer, abri, logement libre -> seuil
//   A* + marche                             A* + marche
//   une seconde sur place, satisfyDrink     ENTREE : npc.inside ; tickNeeds branche sommeil ;
//                                           a `until`, satisfyRest puis SORTIE
//   npc.thirst baisse                       npc.energy remonte, selon la qualite du lit
//
//   GRENIER (granary-eat-001) — Noûs, le chemin ACTIF PAR DEFAUT de la reference
//   la faim monte ; Noûs (toutes les 2,2 s) score manger / chercher / travailler...
//   `eat` gagne (table complete + biais Noûs + porte de commit)
//   RESERVATION d'une portion dans le stock du grenier connu (croyance vue a 7 tuiles)
//   cible : couches rythme (mealPlace) et domestique — le FOYER passe avant le grenier
//   ENTREE (2,1 s) ; tickNeeds branche repas ; a `until` : confirmMeal (stock -1),
//   inventaire +1 -1, satisfyEat
//   building.stock.food.physical baisse ; npc.hunger baisse
//
// FOOD SUPPLY (food-supply-001), extension opt-in : ActivateFoodSource lit une
// tuile Food generee et ouvre un registre fini (la generation reste immuable).
// Perception locale -> gatherFood (3 s, 2 portions) -> inventaire -> deliver
// au seuil du grenier -> CreditFood accepte / debit du sac -> repas reserve.
// Ecarts explicites : pas de craft session / outil / metier / progression JS ;
// scores bornes de collecte/livraison (85 et 100+5*sac), pas de regeneration.
// Ce circuit est une extension deterministe, PAS un portage de trajectoire JS.
// Quand une source est active, manger dedans exige une reservation : le repas
// a vide de la reference ne doit pas masquer une rupture d'approvisionnement.
//
//   CUEILLIR ET LIVRER (gather-deliver-001) — le fermier dont le poste est le grenier
//   il VOIT les champs (perceive : gisements, 7 tuiles, apres 1,5 tuile de marche)
//   table : `gatherFood` calculee (resourceScore + faim * 0,15, facteur de travail,
//   poste +22, fin de tache, trait, competence) ; cible : le gisement dont il se SOUVIENT
//   A* + marche ; session de coups (0,36 s d'ancrage, periode 0,52-0,72 s, fatigue)
//   chaque coup : tile.amount -= 2 ou 3 (saison) ; npc.inventory.food += autant
//   sac > 9 : beginHaulToDepot -> `deliver` vers un seuil du grenier (son poste)
//   au seuil, DEHORS (depot de son poste) ; 1 s ; deliver() : jusqu'a 12, dans la capacite
//   building.stock.food.physical monte ; le sac se vide ; il redecide
//
// La maison apporte le mecanisme d'INTERIEUR (enterBuilding / updateInside /
// exitBuilding) que reutiliseront manger, se soulager, se detendre, socialiser.
// Elle apporte aussi le foyer (npc.home, npc.shelter, proprietaire, capacite)
// et le rythme du jour, sans lequel personne ne se couche.
//
// ECARTS DECLARES — ce qui n'est pas la reference, et pourquoi :
// (Registre qui fait autorite : Source/AnastasisSim/ECARTS.md. Un ecart nouveau s'y declare,
// pas ici ; ce bloc garde le detail des n° 1 a 18.)
//
//  1. Table de decision : les 25 lignes de adultScores, dans l'ordre de la
//     reference, triees de facon stable. `eat`, `rest`, `drink` sont calculees
//     (needGoalScores + jobPriority / bonus puits + phaseBias) ; chaque but NON
//     porte vaut `UnportedGoalsFloor` (42) + son vrai `phaseBias` (pour un fermier,
//     `gatherFood` et `deliver` sont calculees : ecart n°10). Noûs biaise
//     TOUTES les lignes (applyAlgorithmicScoreBias) puis la porte de commit peut
//     forcer `eat` ou `rest`. Un but non porte qui gagne donne `observer`. Sans `goalNoise`
//     (le bruit consomme `sim.rng()` dans l'ordre de TOUTE la table), sans
//     `statusBias` (misere = or <= 2 et sans toit : l'or n'existe pas encore),
//     sans mode de vie, district, age, memoire, prevision de survie. La meteo
//     (`weatherGoalBias`) est portee : ecart n°17.
//  2. Pas de reconsideration aleatoire (`sim.rng() < chance`) ni de collant de
//     but (`goalStickinessBonus`) : un habitant qui a une cible la garde jusqu'a
//     l'arrivee, l'echec ou la disparition ; il redecide des qu'il n'en a plus.
//  3. Points d'acces sans intention urbaine (sim/urban/intent.js, vague 5) :
//     l'anneau 1 oriente vers le camp, qui est exactement le repli de la reference.
//  4. Pilotage reduit : pas de file de porte (crowdNav), pas d'hesitation, pas
//     de facteur de vitesse, pas de contournement local ; escalade anti-blocage
//     en trois paliers ; pas de verrou de seuil domestique en route.
//  5. Cadence LOD (`consumeNpcSimulationCadence`) : BRANCHEE en tete d'`UpdateNpc` des
//     qu'une vue est posee (`SetSimulationView`, budget-cadence-001) — near a chaque
//     tick, medium (dedans, ou < 42) 10 Hz, far 1 Hz, invisible 0,25 Hz, temps accumule
//     dans `_simBudgetAccum`. Reste de l'ecart : SANS vue posee (tests d'assemblage, jeu
//     sans camera branchee), chaque habitant est « proche du point de vue » ; la
//     reference, elle, a toujours une vue ((0, 0) sans camera). Pression toujours 0.
//     La cadence de pensee reste celle de Noûs (2,2 s ; 0,55 s en besoin critique),
//     avec la bascule de phase qui force une pensee, comme la reference.
//  6. RemoveBuilding n'existe pas dans la reference — elle ne demolit jamais.
//  7. Foyer minimal : `assignHomeToHousehold` sans famille (le proprietaire seul),
//     `assignSheltersDaily` a minuit ; pas d'achat de maison (or), pas
//     d'agrandissement (phase 1, capacite 3), pas d'hospitalite, pas de dortoir.
//     `redirectDomesticDoorFailure` bascule sur `explore`, non porte : ici `observer`.
//  8. Tous les habitants sont des adultes sans metier de garde, sans famille, sans
//     mode de vie. `jobPriority(rest)` vaut 18 - 2,5 = 15,5 pour TOUS les metiers
//     du catalogue (rest y est toujours au rang 1) : la constante est reprise telle quelle.
//     Idem `jobPriority(eat)` = 18 (rang 0 partout).
//  9. Noûs, ce qui n'est pas porte : pas d'or ni de marche (buy_food ne gagne
//     jamais ; `believedStock` = 0), pas de danger (flee ne gagne jamais), pas
//     d'oubli des croyances (`staleAfterDays`), pas de mendicite (`begForFood`),
//     pas de memoire d'echec (`noteGoalFailure`), pas d'agregat `market.stock`,
//     pas de gisements (`spots`). Le stock ne porte que la nourriture.
//     COMPORTEMENT DE LA REFERENCE, reproduit tel quel : un habitant qui a un
//     foyer ou un abri est envoye CHEZ LUI pour `eat` (couches rythme et
//     domestique), meme si sa reservation est au grenier ; il y mange « a vide »
//     (la branche repas de tickNeeds baisse sa faim), et sa reservation expire.
// 10. Metiers : `settler` (defaut) et `farmer`, embauche par AssignWorkplace au
//     grenier (le marche de l'emploi n'est pas porte). Les lignes `gatherFood` et
//     `deliver` ne sont CALCULEES que pour un fermier dont le poste est un grenier
//     acheve ; pour les autres elles restent au plancher, et gagner donne `observer` :
//     un sans-metier qui cueille finit par `sell` (or, marche), non porte.
//     L'habitant n'a ni ambition, ni plan, ni technique ; sa nature est MOYENNE
//     (corps, esprit, coeur 1, sans qualite) et son trait est « gardien », ses
//     competences valent 1 avant la teinte du trait : ce sont des options
//     legales de createNpc, pas des tirages.
// 11. Recolte : pas de rate de coup (`rollCraftMiss` tire `sim.rng`), pas
//     d'exploration quand il ne connait aucun gisement (`exploreTarget` tire
//     `sim.rng`) : sans gisement connu il vaque. Pas de rumeurs (on-dit), pas de
//     danger, pas de repousse des champs (`regrowFieldsDaily`). La reference
//     ecrit dans `sim.tiles` ; ici le monde genere reste IMMUABLE et le village
//     tient l'etat vivant des tuiles touchees (LiveTileAt) — memes lectures, memes
//     valeurs. La tuile epuisee reste un champ, en jachere ; le rendu ne suit pas.
// 12. Livraison : seulement a SON depot (le grenier de son poste). Sans poste,
//     la reference livre au marche ou vend : non porte, il vaque. Le stock de
//     marche (`sim.market.stock.food`, lu par la pression morale) est la somme
//     des stocks physiques, recalculee a la lecture — la reference la reconstruit
//     a chaque repas confirme et chaque livraison, les seules mutations ici.
// 15. Socialiser (social-relax-001) : la cible est `socialPos` (le premier batiment
//     acheve qui rassemble — `dailyMorale` > 0 : le puits), le gain social court
//     tant que le but est `socialize`, et l'action est, sans personne a portee, la
//     branche SANS COMPAGNON de `socialize()` (`satisfySocial(14)`, moral +1) ; la
//     branche avec compagnon et les couches liens / memoire de la cible : n°16. Sans
//     puits, la reference vise la place (routes) puis le marche : ici, le point
//     d'acces pres de l'origine.
//     Se detendre : complet — chez soi (foyer, sinon abri ouvert), dedans 4,8 s ;
//     sans toit, dehors ; `hearthInviteScore` (scene de foyer) vaut 0.
// 16. Liens et rumeurs (bonds-rumors-001) : la branche AVEC compagnon de
//     `socialize()` — compagnon choisi dans la grille du tick (ordre de la reference),
//     porte de conversation, relations et paliers (moodlet newFriend), fiches de
//     personnes et theorie de l'esprit, porte de parole (hash FNV), session qui FIGE
//     les deux habitants, rumeurs de GISEMENTS (on-dit), oubli quotidien a la place
//     de la reference dans la file de minuit (`memory`). Non porte, et pourquoi :
//       - le generateur de texte des repliques : le REFUS (seul effet de jeu) est
//         evalue au premier tirage (`attempt` = 0) ; la boucle de re-tirage contre les
//         redites depend du texte ;
//       - `shareRumors` hors gisements (croyance de marche, puits, lits, dangers,
//         acces bloques, savoir negatif, fiches colportees, episodes) : le portage
//         n'a pas ces croyances, l'echange serait vide ou invente ;
//       - les visites de voisinage, les conseils d'aine, les rencontres quotidiennes
//         (`runSocialEncounters`), les frictions (`updateConflictsDaily`) ;
//       - les tirages (`sim.rng`) des rumeurs viennent d'un flux propre au village,
//         deterministe : la trajectoire JS n'est pas promise au tirage pres.
//       - l'ancre du regard (`npc.target = partenaire`) est relachee a la fin de la
//         session : la reference repense sa cible a chaque pensee, ce portage seulement
//         sans cible (ecart n°2), et l'ancre deviendrait une destination durable.
// 14. Cohabitation avec l'extension food-supply (non fidele, voir plus haut) : elle
//     ne s'applique qu'aux habitants qui ne sont PAS le fermier d'un grenier ; une
//     tuile ouverte par ActivateFoodSource n'a qu'une verite, son registre fini.
// 17. Meteo (village-weather-001). Porte : `readSimWeather` (graine de la SIMULATION, posee
//     par l'hote), `weatherGoalBias` ajoute a CHAQUE ligne de la table (`score.weather`),
//     la ligne `shelterRain` calculee (`shelterRainScore`, sans `goalNoise`), la porte
//     d'orage de `commitGoalChoice` (lacher un but expose pour `shelterRain`, memoriser le
//     but a reprendre), `shelterRainAccess`, l'entree (son POSTE a 3,4 tuiles —
//     `workplaceAcceptsIndoorGoal` accepte toujours `shelterRain`, le fermier s'abrite donc au
//     grenier ; sinon `afford` : foyer, logement, civique — le PUITS est civique dans la
//     reference, il abrite donc ; un grenier qui n'est pas son poste, groupe food, non),
//     la duree d'abri, la recuperation sous l'auvent, `performShelterRain` (energie +14,
//     moral +2, delai de grace 18 s, reprise du but expose), `applyRainExposure`, et le bloc
//     pluie de `movementSpeedFactor` (le reste de ce facteur n'est pas porte : ecart n°4).
//     Non porte : `bestKnownBed` (le foyer en tient lieu), la taverne (absente), le biais
//     `shelterRain` de la prevision de survie, `weatherGoalLabel` (inspecteur). Apres l'abri,
//     un but repris qui n'est pas expose devient `craft` dans la reference : non porte, donc
//     `observer`. Parite : `Anastasis.Sim.Parite.MeteoHabitants` (vecteurs) et
//     `Anastasis.Sim.MeteoHabitants.*` (formules non exportees, boucle assemblee).
// 18. Chantier (build-001) : un batiment inacheve monte piece par piece (22) sous les
//     coups des batisseurs ; chaque piece consomme sa part du devis dans le stock du
//     site ; a la derniere, le batiment est acheve et sert. La ligne `build` est
//     calculee pour TOUS les adultes des qu'un chantier est ouvert (besoin collectif
//     85, comme la reference) ; sans chantier, elle reste au plancher des buts non
//     portes. Non porte, et pourquoi :
//       - l'OUVERTURE d'un chantier par un habitant (`tryOpenNewConstruction` : choix
//         collectif du type, emplacement, salaire d'ouverture, apport initial) : c'est
//         l'hote qui ouvre (`OpenSite`), devis livre sur place ou non ;
//       - les LIVRAISONS au chantier (`requestSiteDeliveries`, porteurs) : un chantier
//         a sec attend ; le bois et la pierre ne se recoltent pas encore ;
//       - la memoire d'echec et de danger d'un chantier bloque, le relais de pieces
//         (rendu), les episodes, la reputation, l'annonce, les postes ouverts par le
//         batiment acheve, la restitution du reliquat (aucun depot de bois ni de pierre) ;
//       - le rate de coup (ecart n°11) et le bruit de but (ecart n°1), comme ailleurs.
// 13. Reference : `fee66ae`, commitee. Sa copie de travail porte, NON commitee,
//     `load > 11` au lieu de `load > 9` pour rentrer livrer : non suivi.
//
// Parite bit a bit : prouvee pour les besoins, le rythme, la qualite du repos et
// la decision Noûs (Anastasis.Sim.Parite.Besoins / .Rythme / .Nous). La boucle assemblee est
// deterministe, elle n'est PAS la trajectoire JS — l'ecart 1 suffit a l'interdire.

#pragma once

#include "CoreMinimal.h"
#include "Ai/AnastasisNous.h"
#include "Core/AnastasisRng.h"
#include "Core/AnastasisSimBudget.h"
#include "Core/AnastasisSpatialGrid.h"
#include "Life/AnastasisBonds.h"
#include "Life/AnastasisLifestyle.h"
#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Life/AnastasisWeatherBehavior.h"
#include "Work/AnastasisBuild.h"
#include "Work/AnastasisGather.h"
#include "World/AnastasisEntityTable.h"
#include "World/AnastasisNavGrid.h"
#include "World/AnastasisPathfinding.h"

namespace AnastasisWorld { struct FWorld; }

namespace AnastasisVillage
{
	using FPoint = AnastasisPath::FPoint;

	/** Types de batiment que ce portage sait poser. Chaine = vocabulaire de la reference. */
	inline const TCHAR* const WellType = TEXT("well");
	inline const TCHAR* const HouseType = TEXT("house");
	inline const TCHAR* const GranaryType = TEXT("granary");

	/** Buts portes. `observer` est l'etat initial de createNpc. */
	inline const TCHAR* const GoalObserver = TEXT("observer");
	inline const TCHAR* const GoalDrink = TEXT("drink");
	inline const TCHAR* const GoalRest = TEXT("rest");
	inline const TCHAR* const GoalEat = TEXT("eat");
	/** Portes pour tous (social-relax-001) : la solitude et l'ennui ont un remede. */
	inline const TCHAR* const GoalSocialize = TEXT("socialize");
	inline const TCHAR* const GoalRelax = TEXT("relax");
	/** Portes pour un fermier dont le poste est un grenier (ecart n°10). */
	inline const TCHAR* const GoalGatherFood = AnastasisGather::GoalGatherFood;
	inline const TCHAR* const GoalDeliver = AnastasisGather::GoalDeliver;
	/** Porte pour tous (village-weather-001) : sous l'orage, lacher le travail pour un toit. */
	inline const TCHAR* const GoalShelterRain = AnastasisWeatherBehavior::GoalShelterRain;
	/**
	 * `PLAYER_IDLE_GOAL` de decisionProvider.js (player-minimal-001) : l'habitant incarne sans commande
	 * attend ; Nous ne choisit jamais pour lui. Aucun effet materiel.
	 */
	inline const TCHAR* const GoalIdle = TEXT("idle");

	/**
	 * player-minimal-001 -- le joueur est un habitant (reference : docs/PLAYER_AS_HABITANT.md).
	 *
	 * `STANDING` de life/standing.js pour la reputation. Le reste est une EXTENSION, demande
	 * d'Alexandre (TIME_WARP_001) : un joueur qui accelere le temps ne fait rien aux yeux du village.
	 * L'hote lui passe sa PRESENCE (1 = vu, 0 = invisible) et le temps ou il a ete oisif ; les
	 * habitants voient une personne a presence p jusqu'a p x leur portee, plus du tout sous
	 * MinPresenceSeen, et l'oublient sous RememberPresenceMin. L'oisivete est un acte comme les
	 * autres : un merite negatif, permanent, comme le vol de la reference.
	 */
	namespace Standing
	{
		/** `STANDING.base`, `initStanding`. */
		inline constexpr double Base = 50.0;
		/** `STANDING.driftToBase` : part de l'ecart a la cible rattrapee chaque minuit. */
		inline constexpr double DriftToBase = 0.4;
		/** EXTENSION : merite perdu par jour oisif vu du village (`theftLoss` vaut 14). */
		inline constexpr double IdleMeritPerDay = 4.0;
		/** EXTENSION : sous cette presence, plus personne ne la voit. */
		inline constexpr double MinPresenceSeen = 0.05;
		/** EXTENSION : sous cette presence, on ne pense plus a aller la chercher. */
		inline constexpr double RememberPresenceMin = 0.25;
		/** EXTENSION : poids de l'ecart de reputation dans l'envie de lui parler. */
		inline constexpr double AffinityWeight = 0.4;
		/** `STANDING.buildGain` : merite par batiment acheve (`deeds.built`). */
		inline constexpr double BuildGain = 3.0;
	}

	/**
	 * player-goals-001 -- la main du joueur (decisionProvider.js, `decideAsPlayer`). Le joueur ne choisit
	 * pas « ce qu'il veut » : il choisit dans la table que Nous aurait lue, sous les memes verrous.
	 * L'intention DURE jusqu'a ce qu'il la retire ; elle CEDE pour une decision, sans disparaitre, et
	 * dit pourquoi.
	 */
	namespace PlayerDecision
	{
		/** `BUILD_SITE_LOCK` (npc.js) : au-dela, le corps passe devant l'intention. */
		inline constexpr double HungerRelease = 92.0;
		inline constexpr double ThirstRelease = 88.0;
		inline constexpr double EnergyRelease = 12.0;
		/** `REFUSAL` de decisionProvider.js. */
		inline const TCHAR* const RefusalNotInTable = TEXT("hors-table");
		inline const TCHAR* const RefusalLocked = TEXT("verrou");
		inline const TCHAR* const RefusalBody = TEXT("le-corps-parle");
		/** Lignes gardees pour l'affichage des buts possibles (`playerGoalOptions(limit)`). */
		inline constexpr int32 OptionsKept = 8;
	}

	/** `sim.playerGoalChoice` : le but pose, combien de decisions il a tenu, combien il a cede, et pourquoi. */
	struct FPlayerGoalChoice
	{
		FString Goal;
		int32 Holds = 0;
		int32 Yields = 0;
		FString CedingFor;
	};

	/** `npc.playerRefusal` : le dernier refus oppose (« tu voulais batir, tu bois d'abord »). */
	struct FPlayerRefusal
	{
		FString Wanted;
		FString Reason;
		FString Applied;
		int32 Day = 0;
	};

	/** Une ligne de la table du joueur a sa derniere decision : un but porte et son score. */
	struct FPlayerGoalOption
	{
		FString Goal;
		double Score = 0.0;
	};

	/** `NPC_AI` de npc.js (chemin classique) — conserve pour `aiThinkStagger`. */
	inline constexpr double ThinkEvery = 0.12;
	inline constexpr double ThinkEveryCritical = 0.045;

	/** `jobPriority(npc, "eat") * 0.35` : eat est au rang 0 de chaque metier du catalogue. */
	inline constexpr double EatJobPriorityBias = 18.0 * 0.35;

	/** `DEPOT_PROFILES.granary.cap.food`. */
	inline constexpr int32 GranaryFoodCap = 300;

	/** `PERCEPTION` de ai/memory.js. */
	inline constexpr double PerceptionRadius = 7.0;
	inline constexpr double PerceptionScanInterval = 9.0;

	/** `MEAL_RESERVATION` de mealReservation.js. */
	namespace Meal
	{
		inline constexpr double TtlSeconds = 45.0;
		inline constexpr double TtlMaxSeconds = 180.0;
		inline constexpr double AbsoluteMaxSeconds = 120.0;
		inline constexpr int32 MaxRenewals = 3;
		inline constexpr double RenewalSeconds = 35.0;
		inline constexpr double ProgressStallSeconds = 18.0;
		inline constexpr double InteractionRadius = 2.2;
	}

	/** Rayon de puisage autour du CENTRE d'un puits (`WELL_REACH`). */
	inline constexpr double WellReach = 2.5;
	/** Seuil de berge qui vaut point d'eau (`SHORE_REACH`). */
	inline constexpr double ShoreReach = 0.3;
	/** `reachedMoveTarget` : arrivee a moins de 0,75 tuile. */
	inline constexpr double ArrivalDistance = 0.75;

	/** `NPC_UNSTICK` de npc.js — la porte du foyer. */
	inline constexpr double DoorWaitSeconds = 2.8;
	inline constexpr double DoorApproachSeconds = 5.5;
	inline constexpr double DoorAccessRadius = 1.05;

	/** `buildingNearActor(actor, 3.2)` dans buildingForIndoorAction. */
	inline constexpr double IndoorBuildingRadius = 3.2;

	/**
	 * ECART DECLARE n°1 — le score d'un but non porte AVANT le rythme.
	 *
	 * 42 = `urgeScore(thirstUrge)`, la valeur a laquelle la reference dit que la
	 * soif « deborde largement les scores de metier (souvent 40-90) ». Chaque but
	 * non porte recoit ensuite son vrai `phaseBias` : le matin le travail pese
	 * 42 + 62, la nuit se soulager pese 42 + 35. Ce n'est pas une valeur de la
	 * reference ; c'est la place qu'occupent les buts qui ne sont pas encore portes.
	 */
	inline constexpr double UnportedGoalsFloor = 42.0;

	/** `jobPriority(npc, "rest") * 0.35` : rest est au rang 1 de chaque metier du catalogue. */
	inline constexpr double RestJobPriorityBias = (18.0 - 1.0 * 2.5) * 0.35;

	/** `HOUSE_PHASES[0].capacity` — une maison de phase 1 loge trois personnes. */
	inline constexpr int32 HousePhaseOneCapacity = 3;

	/** Enregistrement de batiment — les champs d'`addBuilding` que les boucles lisent. */
	struct FBuilding
	{
		FString Id;
		FString Type;
		/** Case du batiment (entiere dans la reference ; le centre est x + 0,5). */
		double X = 0.0;
		double Y = 0.0;
		/** 1 = acheve. Un batiment inacheve n'est ni compte, ni puise, ni habite. */
		double Progress = 1.0;
		int32 CreatedDay = 0;
		/** Maison : identifiant du proprietaire (`npc-N`), vide si libre. */
		FString Owner;
		/** Maison : `housePhase` (1..6) ; la capacite en decoule. */
		int32 HousePhase = 1;
		/** Seuils persistes : le PNJ vise une porte, jamais le centre bloque. */
		TArray<FPoint> AccessPoints;
		/** `building.stock.food` — `{physical, reserved}`, reserved <= physical. Grenier seulement. */
		int32 FoodPhysical = 0;
		int32 FoodReserved = 0;

		/** Chantier : pieces posees (0..22), devis / consomme / stock du site, qui l'a ouvert. */
		int32 PiecesPlaced = 0;
		bool bHasMaterials = false;
		AnastasisBuild::FSiteMaterials Materials;
		FString BuilderId;
		/** `building.workers[npcId]` : pieces posees par chacun, dans l'ordre d'arrivee. */
		TArray<TPair<FString, int32>> Workers;
		/** `completedDay`, `completedById` : -1 / vide tant qu'il n'est pas acheve sous les coups. */
		int32 CompletedDay = -1;
		FString CompletedById;

		bool IsCompleted() const { return Progress >= 1.0; }
		int32 FoodAvailable() const { return FMath::Max(0, FoodPhysical - FoodReserved); }
	};

	/** Finite food ledger over an immutable generated tile. Only this ledger is consumed.
	 * No regrowth; reactivating a tile never refills it. */
	struct FFoodSource
	{
		int32 TileIndex = INDEX_NONE;
		FPoint Position;
		int32 Initial = 0;
		int32 Remaining = 0;
	};

	/** Une portion reservee (`sim.mealReservations.byId[...]`). */
	struct FMealReservation
	{
		FString Id;
		FString NpcId;
		/** Vide pour une reservation d'inventaire. */
		FString BuildingId;
		/** "colony" ou "inventory". */
		FString Source;
		int32 Amount = 1;
		double CreatedAt = 0.0;
		double ExpiresAt = 0.0;
		double AbsoluteExpiresAt = 0.0;
		int32 Renewals = 0;
		double LastProgressAt = 0.0;
		double LastDistance = 0.0;
	};

	/** `npc.hungerAction` — la machine a etats du repas. */
	struct FHungerAction
	{
		FString State = TEXT("pending");
		FString TargetId;
		double StartedAt = 0.0;
		FString LastFailure;
		double CooldownUntil = 0.0;
		double Progress = 0.0;
		FString ReservationId;
		FString SourceBuildingId;
		FString ExcludedType;
	};

	/** Une croyance de stock (`mind.beliefs.knownStocks[key]`) — ce que l'habitant CROIT. */
	struct FStockBelief
	{
		FString Key;
		FString Resource;
		FString BuildingId;
		FString Kind;
		double X = 0.0;
		double Y = 0.0;
		double EstimatedAmount = 0.0;
		double Confidence = 0.0;
		int32 Day = 0;
	};

	/** Un gisement dont l'habitant se souvient (`mind.spots[key]`). */
	struct FResourceSpot
	{
		/** `"x,y"` — la cle de la reference. */
		FString Key;
		/** Centre de la tuile (x + 0,5). */
		double X = 0.0;
		double Y = 0.0;
		/** "food", "wood", "stone". */
		FString Resource;
		int32 Amount = 0;
		int32 Day = 0;
		bool bHearsay = false;
		/** On-dit : qui l'a dit, la source d'origine, combien de bouches, quand. */
		FString SourceId;
		FString OriginalSourceId;
		int32 HopCount = 0;
		int32 ReceivedDay = 0;
		double ReceivedAt = 0.0;
	};

	/** `INFORM_RESOURCE_SPOT` — un gisement qu'un habitant raconte a un autre. */
	struct FSpotAct
	{
		FString SourceId;
		FString Key;
		FString Resource;
		double X = 0.0;
		double Y = 0.0;
		int32 Amount = 0;
		int32 Day = 0;
		int32 HopCount = 0;
		FString OriginalSourceId;
	};

	/** `npc.lastTalk` — le dernier propos (le texte n'est pas porte). */
	struct FLastTalk
	{
		bool bValid = false;
		FString WithId;
		double At = 0.0;
		/** `detail === "refuse"`. */
		bool bRefuse = false;
	};

	/** `npc.talkFatigue[otherId]`. */
	struct FTalkFatigue
	{
		FString Id;
		int32 Count = 0;
		double At = 0.0;
	};

	/** Deux habitants vus par `bumpRelation` (tests de parite). */
	struct FBondPair
	{
		double RelAB = 0.0;
		double RelBA = 0.0;
		double MoraleA = 50.0;
		double MoraleB = 50.0;
		TArray<AnastasisBonds::FMoodlet> MoodletsA;
		TArray<AnastasisBonds::FMoodlet> MoodletsB;
	};

	/** `npc.workSession` — la session de coups de metier, ancree a une tuile. */
	struct FWorkSession
	{
		bool bActive = false;
		FString CraftId;
		int32 TileX = 0;
		int32 TileY = 0;
		/** `postIndex` : poste pris dans la parcelle, -1 = pas encore. */
		int32 PostIndex = -1;
		double ArrivedAt = 0.0;
		double NextSwingAt = 0.0;
		int32 SwingsDone = 0;
		/** < 0 = null. */
		double LastSwingAt = -1.0;
		/** Chantier : le batiment travaille (`buildingId`), coups accumules pour une piece. */
		FString BuildingId;
		int32 ActionAcc = 0;
	};

	/** `npc.inside` — present pendant qu'un habitant est DANS un batiment. */
	struct FInside
	{
		bool bActive = false;
		FString BuildingId;
		FString Activity;
		FString Goal;
		double EnteredAt = 0.0;
		double Until = 0.0;
		double ExitX = 0.0;
		double ExitY = 0.0;
	};

	/**
	 * Pourquoi un habitant a choisi ce qu'il fait — OBSERVATION, pas etat.
	 * N'entre pas dans l'empreinte ; sert au debug et aux tests.
	 */
	struct FDecisionTrace
	{
		double Time = -1.0;
		FString Phase;
		AnastasisNeeds::FNeedGoalScores NeedScores;
		/** Ligne `rest` : needs.rest + jobPriority + phaseBias. */
		double RestRowScore = 0.0;
		/** Ligne `drink` : needs.drink + bonus puits + phaseBias. */
		double DrinkRowScore = 0.0;
		/** Lignes `socialize` et `relax`, rythme et biais compris, apres biais Noûs. */
		double SocializeRowScore = 0.0;
		double RelaxRowScore = 0.0;
		/** Ligne `build` (calculee seulement quand un chantier est ouvert). */
		double BuildRowScore = 0.0;
		/** Ligne `eat` : needs.eat + jobPriority + phaseBias, puis biais Noûs. */
		double EatRowScore = 0.0;
		/** Lignes calculees d'un fermier (ecart n°10), apres biais Noûs ; NaN sinon ou retiree. */
		double GatherRowScore = std::numeric_limits<double>::quiet_NaN();
		double DeliverRowScore = std::numeric_limits<double>::quiet_NaN();
		/** Les memes lignes AVANT le biais Noûs : adultScores seul. */
		double GatherRowTable = std::numeric_limits<double>::quiet_NaN();
		double DeliverRowTable = std::numeric_limits<double>::quiet_NaN();
		/** Facteur de travail (`moralPressure.effectiveWork * phase.work * ...`), NaN hors fermier. */
		double WorkFactor = std::numeric_limits<double>::quiet_NaN();
		/** Tete de table apres tri, avant la porte de commit (peut etre un but non porte). */
		FString TableWinner;
		/** Decision Noûs au moment du choix, et ce que la porte a fait. */
		FString NousType;
		double NousScore = 0.0;
		double NousUrgency = 0.0;
		FString CommitGate;
		/** Meilleur but non porte a cette phase, et son score (plancher + phaseBias). */
		FString FloorGoal;
		double FloorScore = 0.0;
		FString Winner;
		/** Batiment vise par ce choix, vide si aucun. */
		FString BuildingId;
		/** D'ou vient la cible : well, shore, home, shelter, housing, open-shelter, settlement, none. */
		FString TargetSource;
		/** Meteo lue a ce choix (`readSimWeather`) et la ligne `shelterRain` (score + biais + rythme). */
		double WeatherRain = 0.0;
		double ShelterRowScore = 0.0;
		/** La porte d'orage de commitGoalChoice a-t-elle remplace le gagnant ? */
		bool bStormGate = false;
	};

	/** Ce que les lignes de travail d'un fermier partagent a une decision. */
	struct FWorkRowContext
	{
		/** `believedStock(sim, npc).food`. */
		double Believed = 0.0;
		bool bMealBlocked = false;
		/** `workFactor` d'adultScores, avant `survivalWorkFactor`. */
		double WorkFactor = 0.0;
	};

	/** Habitant — les champs de createNpc que les boucles lisent ou ecrivent. */
	struct FNpc
	{
		FString Id;
		double X = 0.0;
		double Y = 0.0;
		double Speed = 4.0;
		AnastasisNeeds::FNeeds Needs;

		FString Goal = GoalObserver;
		FString Activity;
		/** `npc.target` : un POINT, jamais un batiment. */
		bool bHasTarget = false;
		FPoint Target;
		double WorkTimer = 0.0;
		/** `npc.aiThinkAt` ; < 0 = null. */
		double AiThinkAt = -1.0;
		int32 FailedActions = 0;

		/** `npc.home` / `npc.shelter` — des IDENTIFIANTS, pas des objets. */
		FString HomeId;
		FString ShelterId;
		FInside Inside;
		double DoorStuckAt = 0.0;
		double DoorApproachAt = 0.0;

		// Navigation (`npc.navigation` + champs de chemin).
		TArray<FPoint> Path;
		int32 PathStep = 0;
		bool bHasPathGoal = false;
		FPoint PathGoal;
		bool bPathFailed = false;
		double PathCooldown = 0.0;
		FString NavTargetKey;
		int32 NavVersion = -1;
		/** `navigation.destBuildingId` — une reference PAR IDENTIFIANT. */
		FString DestBuildingId;
		double StuckTimer = 0.0;
		int32 StuckStage = 0;

		/** `npc.inventory.food`. */
		int32 InventoryFood = 0;
		int32 GatheredFood = 0;
		int32 DeliveredFood = 0;
		int32 FoodSourceIndex = INDEX_NONE;
		/** Locally perceived source quantities; zero means observed exhausted. */
		TMap<int32, int32> KnownFoodSources;

		/** `npc.jobId`, `npc.workplace` (PAR IDENTIFIANT). */
		FString JobId = AnastasisGather::JobSettler;
		FString WorkplaceId;
		/** Index dans `TRAITS` ; « gardien » par defaut (ecart n°10). */
		int32 TraitIndex = AnastasisGather::DefaultTraitIndex;
		/** `npc.skill` et `npc.skills.gather`, `npc.skills.trade` (teintes par le trait). */
		double Skill = 1.0;
		double SkillGather = 1.0;
		double SkillTrade = 1.0;
		/** `skills.craft` : le domaine du but `build`. */
		double SkillCraft = 1.0;
		/** `npc.buildBinding` : le chantier auquel il s'est engage. */
		FString BuildBinding;
		/** Observation : `deeds.workedConstruction` (pieces posees), `deeds.built` (achevements). */
		int32 PiecesPlaced = 0;
		int32 BuildingsCompleted = 0;
		/** `mind.spots`, dans l'ordre d'insertion. */
		TArray<FResourceSpot> Spots;
		/** `mind.scanX`, `mind.scanY` : ou il se tenait au dernier balayage des tuiles. */
		double ScanX = -999.0;
		double ScanY = -999.0;
		FWorkSession WorkSession;
		/** Observation : nombre de livraisons faites (les quantites : GatheredFood, DeliveredFood). */
		int32 Deliveries = 0;
		FHungerAction HungerAction;
		/** `mind.beliefs.knownStocks`, dans l'ordre d'insertion (celui d'un objet JS). */
		TArray<FStockBelief> KnownStocks;
		double LastScan = -999.0;
		/** `npc.villagePhase` — la bascule force une pensee. */
		FString VillagePhase;
		double GoalSince = 0.0;
		/** `npc._algoDebug` : la decision Noûs courante et son contexte. */
		bool bHasAlgoDecision = false;
		AnastasisNous::FDecision AlgoDecision;
		AnastasisNous::FFoodContext AlgoContext;
		bool bAlgoInertiaKeep = false;
		FString AlgoInertiaReason;
		FString AlgoMappedGoal;

		/** Actes accomplis. Observation (la reference les compte ailleurs). */
		int32 DrinksTaken = 0;
		int32 RestsTaken = 0;
		int32 MealsTaken = 0;
		int32 SocialsTaken = 0;
		int32 RelaxesTaken = 0;
		/** `deeds.sheltered` : abris pris sous l'orage. */
		int32 SheltersTaken = 0;

		/** `npc.shelterResumeGoal` : le travail expose a reprendre apres l'abri ; vide = null. */
		FString ShelterResumeGoal;
		/** `npc.shelterCooldownUntil` ; < 0 = absent. */
		double ShelterCooldownUntil = -1.0;

		/**
		 * `npc._simBudgetAccum` : le temps accumule par la cadence du budget hors de la
		 * bande near (simulationBudget.js). La cle n'existe dans la reference qu'apres le
		 * premier passage hors near ; `bHasSimBudgetAccum` en tient la presence.
		 */
		double SimBudgetAccum = 0.0;
		bool bHasSimBudgetAccum = false;

		/**
		 * `npc.phenotype` et `npc.conditioning` (needs-wiring-001) : ce que lisent les cinq
		 * facteurs des besoins (`AnastasisNeeds::NeedFactorsFor`). Le phenotype ne change pas
		 * de la vie ; le conditionnement avance en fin de besoins (`TickNeedsConditioning`).
		 * Non poses : habitant median, facteurs 1, conditionnement immobile -- c'est le cas de
		 * tout habitant cree par le C++ (ecart n°8).
		 */
		TOptional<AnastasisGenome::FPhenotype> Phenotype;
		TOptional<AnastasisConditioning::FConditioning> Conditioning;

		/**
		 * `npc.lifestyle` (lifestyle-wiring-001) : le mode de vie et son score de regularite,
		 * tenu une fois par jour (`LifestyleDailyUpdate`). Non pose : habitant cree par le C++,
		 * sans mode de vie, et aucun tirage pour lui en donner un (ecart n°8).
		 */
		TOptional<AnastasisLifestyle::FLifestyle> Lifestyle;

		/** `npc.relations` (ordre d'insertion), `mind.people`, `mind.tom`, `npc.moodlets`. */
		TArray<TPair<FString, double>> Relations;
		TArray<AnastasisBonds::FPersonRow> People;
		TArray<AnastasisBonds::FTomEntry> Tom;
		TArray<AnastasisBonds::FMoodlet> Moodlets;
		/** Conversation : dernier propos, fatigue par interlocuteur, session en cours. */
		FLastTalk LastTalk;
		TArray<FTalkFatigue> TalkFatigue;
		FString TalkWithId;
		double TalkUntil = 0.0;
		int32 TalkTurn = 0;
		int32 TalkMaxTurns = 0;
		FString TalkStarterId;
		double TalkNextAt = 0.0;
		/** `talkChainTopic || talkChainCue` : seule leur presence compte (porte de parole). */
		bool bTalkChain = false;
		/** La cible est l'ancre du regard posee par la conversation (relachee a la fin). */
		bool bTalkAnchor = false;
		/** `npc.socialSeekId` : la personne memorisee vers qui il marche. */
		FString SocialSeekId;
		/** Observation : conversations engagees (compagnon), gisements entendus / racontes. */
		int32 TalksWithCompanion = 0;
		int32 RumorsHeard = 0;
		int32 RumorsShared = 0;
		FDecisionTrace LastDecision;

		/** `npc.reputation` (`initStanding`). Hors empreinte : les actes qui la font ne sont pas portes. */
		double Reputation = Standing::Base;
		/**
		 * EXTENSION (player-minimal-001) : combien le village voit cette personne, et le temps simule
		 * ou il l'a vue ne rien faire. Seul l'habitant incarne s'en ecarte (l'hote les pose) : pour tout
		 * autre, 1 et 0, et les regles qui les lisent rendent au bit pres ce qu'elles rendaient.
		 */
		double Presence = 1.0;
		double IdleSeconds = 0.0;

		/** `livingHome(npc)` = home || shelter. */
		const FString& LivingHomeId() const { return !HomeId.IsEmpty() ? HomeId : ShelterId; }
	};

	/**
	 * L'etat du village et ses regles. Lie a un monde genere, qu'il ne possede
	 * pas : l'hote de simulation garde le monde et le village cote a cote.
	 */
	class ANASTASISSIM_API FVillage
	{
	public:
		/** Lie le village a un monde et reconstruit la grille de navigation. Vide les tables. */
		void Bind(const AnastasisWorld::FWorld& InWorld);
		bool IsBound() const { return World != nullptr; }

		/** `sim.settlement` : l'origine vers laquelle les portes s'ouvrent. Centre du monde par defaut. */
		void SetSettlement(double InX, double InY) { Settlement = { InX, InY }; }
		FPoint GetSettlement() const { return Settlement; }

		/**
		 * `addBuilding(type, x, y)` — identifiant `building-N`, case bloquee, cout
		 * infini, seuils calcules, version de navigation incrementee.
		 * Rend l'identifiant, vide si la case est hors bornes ou deja bloquee.
		 */
		FString AddBuilding(const FString& Type, int32 TileX, int32 TileY, double Progress = 1.0, int32 Day = 0);

		/**
		 * EXTENSION — la reference ne demolit jamais un batiment.
		 *
		 * Garanties : la case redevient ce que le terrain dit, la version de
		 * navigation change, et AUCUN habitant ne garde une reference vers lui —
		 * ceux qui le visaient perdent cible, chemin et but ; ceux qui etaient
		 * DEDANS en sortent par leur seuil d'entree ; ceux dont c'etait le foyer
		 * ou l'abri le perdent.
		 */
		bool RemoveBuilding(const FString& Id);

		/** `spawnNpc`, reduit : position, besoins et vitesse fournis, identifiant `npc-N`, sans toit. */
		FString SpawnNpc(double InX, double InY, const AnastasisNeeds::FNeeds& Needs, double Speed = 4.0);

		/** `actors.splice` — decalage, l'ordre des autres survit. */
		bool RemoveNpc(const FString& Id);

		// --- Le joueur est un habitant (player-minimal-001, simulation.js « Incarnation ») ---------
		//
		// Une seule verite : `PlayerPersonId`. Aucun `bIsPlayer` dissemine. Vide = observateur, et
		// alors rien de ce qui suit n'est lu : la simulation est celle d'avant, au bit pres.

		/** `sim.playerPersonId` ; vide en mode observateur. */
		const FString& GetPlayerPersonId() const { return PlayerPersonId; }
		bool IsPlayer(const FNpc& Npc) const { return !PlayerPersonId.IsEmpty() && Npc.Id == PlayerPersonId; }
		/** `playerActor()` : l'habitant incarne, ou nullptr. */
		const FNpc* PlayerActor() const;
		FNpc* PlayerActor();

		/** `incarnate(personId)` : prendre la main sur un habitant vivant. Faux s'il n'existe pas. */
		bool Incarnate(const FString& NpcId);
		/** `release()` : rendre la main a Nous. Rend l'identifiant relache. */
		FString Release();
		/**
		 * `arriveAsPlayer` : un habitant ordinaire arrive par `spawnNpc`, puis est incarne. Sans
		 * position, `settlement + (2, 3)` comme la reference, deplace sur le premier sol libre.
		 * `spawnNpc` ne tire ici aucun aleatoire : le flux du monde est intact sans flux joueur.
		 * Rend l'identifiant, vide sans monde ou sans sol libre.
		 */
		FString ArriveAsPlayer(double InX = -1.0, double InY = -1.0);

		/**
		 * `setPlayerMovementInput(dx, dy)` : direction de marche du corps incarne, normalisee ;
		 * (0, 0) l'arrete. Transitoire, jamais sauvegardee. Faux sans joueur ou sans direction.
		 */
		bool SetPlayerMovementInput(double DX, double DY);
		FPoint GetPlayerDrive() const { return PlayerDrive; }

		/**
		 * EXTENSION (TIME_WARP_001) : l'hote dit ce que le village a vu du joueur depuis le dernier
		 * appel -- sa presence, et le temps simule ou il n'a rien fait. Sans joueur, rien.
		 */
		void ObservePlayer(double Presence, double IdleSecondsDelta);

		/**
		 * EXTENSION : un habitant voit-il `Other` a la distance D, pour une portee donnee ? Presence 1 :
		 * exactement `D <= Range`, le test d'avant. Sinon `D <= Range x presence`, et rien sous
		 * Standing::MinPresenceSeen.
		 */
		static bool Sees(const FNpc& Other, double D, double Range);
		/**
		 * EXTENSION : ce que la reputation de `Other` ajoute a l'envie de lui parler. Seulement pour le
		 * joueur : entre habitants, la reference ne s'en sert pas ici, et le portage reste au bit pres.
		 */
		double ReputationAffinity(const FNpc& Other) const;

		/**
		 * `updateReputationDaily`, appele a minuit : `reputation += (cible - reputation) x 0,4`,
		 * cible = `base + merite`. Merites portes : batiments acheves (`deeds.built x buildGain`) et,
		 * EXTENSION, l'oisivete du joueur (negative). Un habitant sans acte a pour cible 50 et y reste.
		 */
		void UpdateReputationDaily();

		// --- La main du joueur (player-goals-001, simulation.js « La main du joueur ») ------------

		/**
		 * `choosePlayerGoal(goal)` : pose une intention qui dure jusqu'a ce que le joueur la retire
		 * (`Goal` vide). Ne mute rien d'autre : la decision reste prise au point de decision, a la
		 * prochaine pensee, ou un verrou peut encore la refuser. Faux sans joueur incarne.
		 */
		bool ChoosePlayerGoal(const FString& Goal);
		/** `playerGoalStanding()` : l'intention posee, ou nullptr. */
		const FPlayerGoalChoice* GetPlayerGoalChoice() const { return bHasPlayerChoice ? &PlayerChoice : nullptr; }
		/** `playerRefusal()` : le dernier refus, ou nullptr. */
		const FPlayerRefusal* GetPlayerRefusal() const { return bHasPlayerRefusal ? &PlayerRefusal : nullptr; }
		/**
		 * `playerGoalOptions` : les buts portes que le joueur pourrait commettre, tries, tels que la
		 * table les donnait a sa DERNIERE decision. Lecture pure : rien n'est recalcule ici (la reference
		 * a paye deux fois une lecture d'interface qui mutait l'habitant).
		 */
		const TArray<FPlayerGoalOption>& GetPlayerGoalOptions() const { return PlayerOptions; }

		/**
		 * `assignHomeToHousehold`, branche sans famille : la maison appartient a
		 * l'habitant, elle devient son foyer, moral +12. Refuse si ce n'est pas une
		 * maison achevee libre (ou deja a lui).
		 */
		bool AssignHome(const FString& NpcId, const FString& HouseId);

		/** `assignSheltersDaily` — appele par l'hote a minuit. Rend le nombre d'abrites. */
		int32 AssignSheltersDaily();

		/**
		 * Embauche (ecart n°10) : `npc.jobId`, `npc.workplace`. Seul le fermier au
		 * grenier acheve est accepte — le catalogue y admet steward, farmer, porter.
		 */
		bool AssignWorkplace(const FString& NpcId, const FString& JobId, const FString& BuildingId);

		/** Fermier dont le poste est un grenier acheve : ses lignes gatherFood / deliver sont calculees. */
		bool IsGranaryWorker(const FNpc& Npc) const;

		/**
		 * `npc.jobId` sans poste (ecart n°18) : le batisseur de la reference n'a pas
		 * besoin d'un batiment pour batir. Refuse un metier inconnu du portage.
		 */
		bool SetJob(const FString& NpcId, const FString& JobId);

		/**
		 * Ouverture d'un chantier par l'hote (ecart n°18) : `addBuilding(type, x, y,
		 * { progress: 0, materialsNeeded: buildCost(type) })`. `bDelivered` : le devis
		 * entier est deja dans le stock du site (borne par sa capacite). Rend l'id.
		 */
		FString OpenSite(const FString& Type, int32 TileX, int32 TileY, bool bDelivered);

		/** `activeConstructions()` : les batiments inacheves, dans l'ordre du tableau. */
		TArray<const FBuilding*> ActiveSites() const;

		/** Ajoute au stock d'un chantier (borne par sa capacite) ; rend la quantite entree. */
		int32 CreditSiteMaterials(const FString& BuildingId, int32 Wood, int32 Stone);

		/** `sim.market.stock.food` : la somme des stocks physiques (ecart n°12). */
		int32 MarketFood() const;

		/**
		 * La tuile telle qu'elle est MAINTENANT : la generation, puis ce que la recolte
		 * en a fait (`tile.amount -= taken`, `depleteTile`). Le monde genere reste
		 * immuable ; le village tient l'etat vivant des tuiles touchees. Une tuile
		 * ouverte par l'extension food-supply lit son registre (une seule verite).
		 */
		AnastasisWorld::FTile LiveTileAt(int32 TileX, int32 TileY) const;

		/** `for (const npc of this.actors) updateNpc(this, npc, dt)` — Time = temps de sim APRES avance. */
		void UpdateActors(double Time, double Dt);

		/**
		 * `regrowFieldsDaily` sur l'etat vivant des tuiles (Work/AnastasisFields.h).
		 * Appele par l'hote en tete des travaux de minuit. Une tuile ouverte par
		 * l'extension food-supply n'est jamais regarnie (elle se declare sans repousse).
		 * Rend le nombre de tuiles qui ont repousse.
		 */
		int32 RegrowFieldsDaily(int32 Day);
		/** Observation : nourriture ajoutee aux champs par la repousse depuis Bind. */
		int64 GetRegrownFood() const { return RegrownFood; }

		/**
		 * Travail `memory` de la file de minuit : `forgetStale` (gisements de plus de
		 * 14 jours) et `forgetStalePeople` (fiches de plus de 26 jours), pour chacun.
		 */
		void ForgetStaleDaily(int32 Day);

		/** Graine du flux de tirages du village (rumeurs). Defaut fixe ; l'hote la pose. */
		void SetRngSeed(uint32 Seed) { VillageRng = FAnastasisRng(Seed); }

		/**
		 * Meteo (village-weather-001). `readSimWeather` lit `weatherAt(sim.seed, sim.day, dayFrac)` :
		 * le village a besoin de la graine de la SIMULATION (pas celle de ses tirages), posee par
		 * l'hote au Reset. Sans hote (les tests d'assemblage), PAS de meteo : un ciel d'ete sec,
		 * ou chaque terme meteo vaut exactement 0 — le comportement d'avant, au bit pres.
		 */
		void SetWeatherSeed(uint32 Seed) { WeatherSeed = Seed; bWeatherSeeded = true; }
		uint32 GetWeatherSeed() const { return WeatherSeed; }

		/**
		 * Cadence du budget (budget-cadence-001) : `pinSimulationView(budget, x, y)`. Une fois
		 * la vue posee, chaque habitant passe d'abord par `consumeNpcSimulationCadence`, comme
		 * la premiere instruction d'`updateNpc` : near (< 18 cases de la vue) a chaque tick,
		 * medium (dedans, ou < 42) a 10 Hz, far a 1 Hz, invisible a 0,25 Hz, avec le temps
		 * accumule dans `_simBudgetAccum`.
		 *
		 * ECART DECLARE n°5 (reste) — sans vue posee (tests d'assemblage, jeu sans camera branchee),
		 * PAS de cadence : chaque habitant tourne a chaque tick. La reference, elle, a toujours
		 * une vue ((0, 0) par defaut) : c'est l'hote qui doit la poser.
		 */
		void SetSimulationView(double InX, double InY)
		{
			BudgetDirector.ViewX = InX;
			BudgetDirector.ViewY = InY;
			BudgetDirector.bViewPinned = true;
			bSimulationView = true;
		}
		bool HasSimulationView() const { return bSimulationView; }
		const AnastasisBudget::FDirector& GetBudgetDirector() const { return BudgetDirector; }

		/**
		 * `sim.forceWeather` de la reference (« Hook verifies / debug ») : impose l'etat lu par les
		 * habitants, a la fois a l'heure (decisions) et pour la journee (vitesse de marche).
		 * Pour les tests et le debug ; la simulation normale n'y touche jamais.
		 */
		void SetForcedWeather(const AnastasisWeatherBehavior::FSimWeather& Weather) { bForcedWeather = true; ForcedWeather = Weather; }
		void ClearForcedWeather() { bForcedWeather = false; }
		bool HasForcedWeather() const { return bForcedWeather; }

		/** La meteo que les habitants lisent maintenant (`readSimWeather`), et la pluie journaliere de la marche. */
		AnastasisWeatherBehavior::FSimWeather CurrentWeather() const;
		double DailyRain() const;

		/** `relationOf(npc, otherId)`. */
		static double RelationOf(const FNpc& Npc, const FString& OtherId);

		/** `isTalking(npc, sim)`. */
		bool IsTalking(const FNpc& Npc) const;

		/** `countBuildings(type)` — acheves seulement. */
		int32 CountBuildings(const FString& Type) const;

		/** `shelterCapacity` — maison : capacite de sa phase ; sinon `housing` du catalogue. */
		int32 ShelterCapacity(const FBuilding& Building) const;

		/** `countShelterOccupants` — habitants dont c'est le foyer OU l'abri (pas ceux qui sont dedans). */
		int32 CountShelterOccupants(const FString& BuildingId) const;

		/** Habitants physiquement dedans en ce moment (`npc.inside.buildingId`). */
		TArray<FString> InsideOf(const FString& BuildingId) const;

		/** `sleepQuality(npc)`. */
		static double SleepQualityOf(const FNpc& Npc);

		/** `creditStock(building, "food", n)` — respecte la capacite. Rend la quantite ajoutee. */
		int32 CreditFood(const FString& BuildingId, int32 Amount);

		/** Activate an existing generated Food tile, once; no injected stock. */
		bool ActivateFoodSource(int32 TileX, int32 TileY);
		const TArray<FFoodSource>& GetFoodSources() const { return FoodSources; }

		/** Registre des reservations, dans l'ordre d'insertion. */
		const TArray<FMealReservation>& GetMealReservations() const { return MealReservations; }
		const FMealReservation* FindMealReservation(const FString& NpcId) const;

		/** `perceive(sim, npc, true)` — balayage force (utilise par SpawnNpc et les tests). */
		void PerceiveNow(const FString& NpcId);

		/**
		 * Harnais (sim-digest-emitter-001) : reprend un etat lu d'une sauvegarde JS
		 * (Harness/AnastasisJsSave.h) sur un village lie au monde lu, comme
		 * `deserialize` le fait cote JS. Les batiments gardent leur identifiant,
		 * leurs champs et leurs seuils SAUVEGARDES (case bloquee, cout infini, version
		 * de navigation incrementee, comme AddBuilding) ; les habitants sont repris
		 * tels quels, sans perception (le JS ne percoit pas au chargement) ; le
		 * registre des repas et les compteurs d'identifiants aussi. Refuse si le
		 * village n'est pas lie, s'il a deja des entites, ou si un batiment sort de
		 * la carte.
		 */
		bool RestoreForHarness(const TArray<FBuilding>& InBuildings, const TArray<FNpc>& InActors,
			const TArray<FMealReservation>& InMeals, int32 InMealSeq, int32 InNextBuildingId, int32 InNextNpcId, FString& OutError);

		/** Tuiles touchees depuis Bind (recolte, repousse) : index -> etat vivant. Lecture seule. */
		const TMap<int32, AnastasisWorld::FTile>& GetLiveTiles() const { return LiveTiles; }
		/** `ledger.seq` du registre des repas. */
		int32 GetMealSeq() const { return MealSeq; }

		const TArray<FBuilding>& GetBuildings() const { return Buildings.GetItems(); }
		const TArray<FNpc>& GetActors() const { return Actors.GetItems(); }
		const FBuilding* FindBuilding(const FString& Id) const { return Buildings.FindById(Id); }
		const FNpc* FindNpc(const FString& Id) const { return Actors.FindById(Id); }
		/** Acces d'ecriture pour les tests et les outils de debug (besoins imposes). */
		FNpc* FindNpcMutable(const FString& Id) { return Actors.FindById(Id); }

		const AnastasisNav::FNavGrid& GetNavGrid() const { return Nav; }
		int32 GetNavVersion() const { return NavVersion; }

		/** `footBlockedAt(floor(x), floor(y))` — eau, bati, arbres debout. */
		bool IsFootBlocked(double InX, double InY) const;

		/** `isBlocked(x, y)` — eau et bati seulement (enterBuilding / exitBuilding). */
		bool IsBlocked(double InX, double InY) const;

		/** `atDrinkSpot` : eau, berge, ou moins de 2,5 tuiles du centre d'un puits acheve. */
		bool AtDrinkSpot(double InX, double InY) const;

		/**
		 * Habitants qui se servent de ce batiment en ce moment : ceux qui le visent
		 * (`destBuildingId`) pour boire ou dormir, et ceux qui sont dedans. Derive,
		 * jamais stocke — la reference n'a pas de liste d'occupants.
		 */
		TArray<FString> UsersOf(const FString& BuildingId) const;

		/** Projection canonique (FStateWriter) : batiments puis acteurs, dans l'ordre. */
		uint64 Digest() const;

	private:
		void UpdateNpc(FNpc& Npc, double Dt);
		void ChooseGoal(FNpc& Npc);
		bool FoodSupplyTarget(FNpc& Npc, FPoint& Out, FString& Source);
		bool PerformFoodSupply(FNpc& Npc);
		bool HasKnownFoodSource(const FNpc& Npc) const;
		const FBuilding* KnownFoodDepot(const FNpc& Npc) const;
		void CommitGoal(FNpc& Npc, const FString& Next, FDecisionTrace& Trace);
		bool AssignTarget(FNpc& Npc, FDecisionTrace& Trace);

		// Perception (ai/memory.js, branche nourriture).
		void Perceive(FNpc& Npc, bool bForce);
		AnastasisNous::FFoodContext PerceiveFoodContext(const FNpc& Npc) const;
		int32 Day() const;

		// Noûs (ai/algorithmic).
		void ComputeAlgorithmicDecision(FNpc& Npc);
		void ApplyAlgorithmicScoreBias(FNpc& Npc, TArray<TPair<FString, double>>& Rows);
		FString ApplyAlgorithmicCommitGate(FNpc& Npc, const FString& Next, const FString& Previous, FString& OutGate);
		void OnAlgorithmicGoalCommitted(FNpc& Npc, const FString& Previous, const FString& Next);
		void TickAlgorithmicNpc();
		/** Rend 1 (mange), 0 (echec), -1 (en route : `null` de la reference). */
		int32 TryAlgorithmicEat(FNpc& Npc);
		int32 RunHungerActionStep(FNpc& Npc, bool bAtFoodAccess, const FString& SourceBuildingId);
		void FailHungerAction(FNpc& Npc, const FString& Reason, const FString& ExcludedType);
		void CancelHungerAction(FNpc& Npc, const FString& Reason);
		bool Eat(FNpc& Npc);

		// Reservations (mealReservation.js).
		bool ReserveMeal(FNpc& Npc, const FString& BuildingId, double TravelSeconds, double TtlSeconds, FString& OutReason);
		FMealReservation* GetMealReservation(const FNpc& Npc);
		bool ReleaseMeal(FNpc& Npc, const FString& Reason);
		bool ConfirmMeal(FNpc& Npc, FString& OutReason, FString& OutSourceId);
		FString MaybeRenewMealReservation(FNpc& Npc);
		int32 ExpireMealReservations();
		bool MealSourceAccessPoint(FNpc& Npc, FPoint& Out, bool& bOutBuilding);
		bool IsNpcAtMealSource(FNpc& Npc);

		bool EatTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		const FBuilding* MealPlace(const FNpc& Npc) const;
		void Act(FNpc& Npc, double Dt);
		bool Perform(FNpc& Npc);
		void RedirectAfterFailure(FNpc& Npc);
		void RedirectDomesticDoorFailure(FNpc& Npc);

		AnastasisRhythm::FPhaseSubject PhaseSubjectOf(const FNpc& Npc) const;
		bool IsNight() const;

		const FBuilding* NearestWell(double FromX, double FromY) const;
		bool DrinkTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		bool RestTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		const FBuilding* NearestHousing(const FNpc& Npc) const;
		const FBuilding* FindOpenShelter(const FNpc& Npc) const;
		bool IsEnterableHousing(const FBuilding& Building) const;
		const FBuilding* NearLivingHome(const FNpc& Npc) const;

		bool TryEnterIndoorAction(FNpc& Npc);
		const FBuilding* BuildingForIndoorAction(const FNpc& Npc, const FString& Goal) const;
		/** `buildingForIndoorAction(actor, "shelterRain")` : batiment a portee qui abrite (`afford`). */
		const FBuilding* ShelterBuildingNearActor(const FNpc& Npc) const;
		/** `shelterRainAccess(sim, npc)` : foyer, poste, batiment couvert, sinon la place. */
		bool ShelterRainTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		/** `performShelterRain(sim, npc)`. */
		bool PerformShelterRain(FNpc& Npc);
		const FBuilding* BuildingNearActor(const FNpc& Npc, double Radius) const;
		bool EnterBuilding(FNpc& Npc, const FBuilding& Building, const FString& InActivity, double Duration);
		void UpdateInside(FNpc& Npc);
		bool ExitBuilding(FNpc& Npc);

		bool BuildingAccessPoint(FBuilding& Building, FNpc* Actor, FPoint& Out, const FPoint* Exclude = nullptr);
		bool BuildingAccessPointById(const FString& BuildingId, FNpc* Actor, FPoint& Out);
		bool PickBuildingAccessPoint(FBuilding& Building, const FNpc* Actor, FPoint& Out, const FPoint* Exclude);
		const TArray<FPoint>& EnsureBuildingAccessPoints(FBuilding& Building);
		TArray<FPoint> ComputeBuildingAccessPoints(const FBuilding& Building) const;
		bool AccessPointNear(double PosX, double PosY, const FNpc* Actor, FPoint& Out) const;
		bool NearestFreePoint(double InX, double InY, int32 MaxRadius, FPoint& Out) const;
		TMap<FIntPoint, double> LocalOccupancy(int32 CX, int32 CY, int32 Radius, const FNpc* Ignored) const;
		bool IsFreeCell(int32 TX, int32 TY) const;

		bool ReachedMoveTarget(const FNpc& Npc, const FPoint& Target) const;
		void MoveActor(FNpc& Npc, const FPoint& Target, double Dt);
		FPoint NextWaypoint(FNpc& Npc, const FPoint& Target, double Dt);
		void ResolveStuckActor(FNpc& Npc, const FPoint& Target);
		void ClearNavigation(FNpc& Npc);

		// Recolte et livraison (npc.js, craftWork.js, fieldWorkPosts.js, memory.js).
		bool IsPortedGoalFor(const FNpc& Npc, const FString& Goal) const;
		// Liens et paroles (bonds.js, talk.js, socialMemory.js).
		FNpc* PickSocialCompanion(FNpc& Npc, double MaxDistance);
		bool IsSociallyAvailable(const FNpc& Other) const;
		bool CanStartTalk(const FNpc& A, const FNpc& B) const;
		int32 TalkFatigueLevel(const FNpc& A, const FNpc& B) const;
		int32 VillageEmitCount() const;
		void SocializeWithCompanion(FNpc& Npc, FNpc& Other);
		void BumpRelation(FNpc& A, FNpc& B, double DeltaA, double DeltaB);
		void NoteMeeting(FNpc& A, FNpc& B);
		void RecordTalk(FNpc& Speaker, FNpc& Listener, bool bContinue);
		bool BeginTalkSession(FNpc& Speaker, FNpc& Listener, bool bContinue);
		/** `holdTalkAct` : vrai si l'habitant est fige en conversation ce tick. */
		bool HoldTalk(FNpc& Npc);
		void AdvanceTalkTurn(FNpc& Driver, FNpc& Partner);
		static void ClearTalkSession(FNpc& Npc);
		bool BondSocialTarget(FNpc& Npc, FPoint& InOutTarget);
		bool RememberedSocialTarget(FNpc& Npc, FPoint& InOutTarget);
		FString PickRememberedSeekFor(const FNpc& Npc) const;
		void ExchangeSpotRumors(FNpc& A, FNpc& B);

		/** Lignes `socialize` / `relax` d'adultScores (Phase, rythme et pression morale compris). */
		double SocialRowScore(const FNpc& Npc, const FString& Goal, double NeedScore, double PhaseBias) const;
		/** `sim.socialPos(npc)` : le premier batiment acheve qui rassemble, sinon l'origine. */
		bool SocialPos(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		bool SocializeTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		bool RelaxTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		/** Ligne `gatherFood` ou `deliver` d'adultScores pour un fermier, rythme compris. */
		double WorkRowScore(const FNpc& Npc, const FString& Goal, double PhaseBias, const FWorkRowContext& Work) const;
		void ScanTiles(FNpc& Npc, int32 CX, int32 CY, bool bForce);
		bool GatherTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		bool DeliverTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		/** `progressCraftGather` : 1 = working, 2 = done, 0 = false. */
		int32 ProgressCraftGather(FNpc& Npc);

		// Chantier (npc.js progressBuildWork, simulation.js workConstruction).
		/** `progressBuildWork` : 0 = echec, 1 = au travail, 2 = fini. */
		int32 ProgressBuildWork(FNpc& Npc, double Dt);
		bool WorkConstruction(FBuilding& Site, FNpc& Npc);
		FBuilding* BoundBuildSite(FNpc& Npc);
		FBuilding* PickBuildSite(FNpc& Npc);
		static bool SitePieceReady(const FBuilding& Site);
		void EnsureBuildSession(FNpc& Npc, const FBuilding& Site);
		/** `constructionAccessPoint(npc)` : le seuil du premier chantier ouvert. */
		bool ConstructionAccessPoint(FNpc& Npc, FPoint& OutTarget);
		/** La ligne `build` d'adultScores quand un chantier est ouvert. */
		double BuildRowScore(const FNpc& Npc, double PhaseBias, const FWorkRowContext& Work) const;
		/** `goalForWorkSession(npc)` : le but du metier de la session en cours. */
		static FString SessionGoalOf(const FNpc& Npc);
		/** Index de la premiere tuile de la ressource dans le 3 x 3 de l'habitant, -1 sinon. */
		int32 ResourceTileNear(const FNpc& Npc, AnastasisWorld::EResource Resource) const;
		AnastasisWorld::FTile LiveTile(int32 Index) const;
		/** `tile.amount -= taken` sur l'etat vivant (ou le registre food-supply). */
		void TakeFromTile(int32 Index, int32 Taken);
		void EnsureCraftSession(FNpc& Npc, int32 TileX, int32 TileY);
		FPoint FieldWorkTarget(FNpc& Npc, const AnastasisWorld::FTile& Tile);
		uint32 ClaimedFieldPosts(const FNpc& Npc, int32 TileX, int32 TileY) const;
		void DepleteTile(int32 Index);
		void BeginHaulToDepot(FNpc& Npc);
		bool Deliver(FNpc& Npc);
		static void ClearWorkSession(FNpc& Npc);

		/** L'habitant incarne : pensee sans Nous (but `idle`), marche directe. */
		void UpdatePlayer(FNpc& Npc, double Dt);
		/** `drivePlayerActor` : un pas dans la direction humaine, memes collisions que MoveActor. */
		void DrivePlayer(FNpc& Npc, double Dt);
		/**
		 * `decideAsPlayer` : le point de decision unique, appele par ChooseGoal APRES le tri, l'eligibilite
		 * et les verrous. Rend le but a commettre, `idle` quand l'humain attend. `bLocked` : un verrou a
		 * impose `Next` (orage, livraison en cours).
		 */
		FString DecideAsPlayer(FNpc& Npc, const TArray<TPair<FString, double>>& Rows, const FString& Next, bool bLocked);
		/** `cede` : l'intention cede pour CETTE decision, sans disparaitre ; le refus dit pourquoi. */
		FString CedePlayerGoal(const FString& Reason);
		/** `bodyOverrides` : faim, soif ou fatigue au-dela des seuils du verrou de chantier. */
		static bool BodyOverrides(const FNpc& Npc);
		/** EXTENSION : `Goal` soigne-t-il le besoin qui parle (boire / soif, manger / faim, dormir / fatigue) ? */
		static bool IsRemedyFor(const FNpc& Npc, const FString& Goal);
		/**
		 * Le but est-il vraiment dans la table du joueur : porte pour lui, et calcule (pas un plancher) --
		 * `build` sans chantier ouvert n'y est pas.
		 */
		bool IsPlayerTableGoal(const FNpc& Npc, const FString& Goal) const;
		/** L'humain attend : `idle`, sans cible, sans travail, sans repas reserve. */
		void CommitPlayerIdle(FNpc& Npc);
		/** Direction, intention, refus et options oublies (incarnation, liberation, retrait). */
		void ResetPlayerHand();

		FString PlayerPersonId;
		FPoint PlayerDrive;
		bool bHasPlayerChoice = false;
		FPlayerGoalChoice PlayerChoice;
		/** Le choix a change depuis la derniere decision : la prochaine pensee le relit. */
		bool bPlayerChoiceDirty = false;
		bool bHasPlayerRefusal = false;
		FPlayerRefusal PlayerRefusal;
		TArray<FPlayerGoalOption> PlayerOptions;

		const AnastasisWorld::FWorld* World = nullptr;
		/** Tuiles touchees par la recolte : index -> etat vivant. Ecrit seulement par TakeFromTile / DepleteTile. */
		TMap<int32, AnastasisWorld::FTile> LiveTiles;
		int64 RegrownFood = 0;
		/** `sim.spatial` : reconstruite une fois par tick, au debut de la boucle des habitants. */
		AnastasisSpatialGrid::FGrid Grid;
		/** `sim.life.recentVillageEmits` : instants des paroles de rue. */
		TArray<double> RecentVillageEmits;
		FAnastasisRng VillageRng = FAnastasisRng(0x6a09e667u);
		AnastasisNav::FNavGrid Nav;
		int32 NavVersion = 0;
		FPoint Settlement;
		int32 NextBuildingId = 0;
		int32 NextNpcId = 0;
		/** `sim.time` du tick en cours (pose par UpdateActors). */
		double Now = 0.0;

		/** Meteo : graine de la simulation, forcage de test, et l'etat lu au debut du tick. */
		uint32 WeatherSeed = 0;
		bool bWeatherSeeded = false;
		bool bForcedWeather = false;
		AnastasisWeatherBehavior::FSimWeather ForcedWeather;
		AnastasisWeatherBehavior::FSimWeather TickWeather;
		double TickDailyRain = 0.0;
		/** `sim.simulationBudget` : la vue et la pression ; `bSimulationView` = cadence active (ecart n°5). */
		AnastasisBudget::FDirector BudgetDirector;
		bool bSimulationView = false;
		/** `sim.mealReservations` : ordre d'insertion = ordre de `Object.keys`. */
		TArray<FMealReservation> MealReservations;
		TArray<FFoodSource> FoodSources;
		int32 MealSeq = 0;
		double ReservationSweepAt = 0.0;
		TAnastasisEntityTable<FBuilding> Buildings;
		TAnastasisEntityTable<FNpc> Actors;
	};

	/** `canStartTalk` sur des champs (tests) : cooldown de paire puis fatigue. */
	ANASTASISSIM_API bool CanStartTalkFor(double Now, bool bALast, double ALastAt, bool bBLast, double BLastAt,
		int32 FatigueAB, double FatigueABAt, int32 FatigueBA, double FatigueBAAt);

	/** `bumpRelation(sim, a, b, dA, dB)` + `noteBondStageCross` (moodlet newFriend). */
	ANASTASISSIM_API void BumpRelationPair(FBondPair& Pair, double DeltaA, double DeltaB, double Now);

	/** `createInformResourceSpotActs(sim, source, target, { limit })` ; R01 = le tirage `sim.rng()`. */
	ANASTASISSIM_API TArray<FSpotAct> CreateInformSpotActs(const TArray<FResourceSpot>& Source, const TArray<FResourceSpot>& Target,
		const FString& SourceId, double R01, int32 Limit);

	/** `commitHearsayResourceSpot` puis `trimResourceSpotMemory`. Rend vrai si le gisement entre. */
	ANASTASISSIM_API bool CommitHearsaySpot(TArray<FResourceSpot>& Target, const FSpotAct& Act, int32 Day, double Time);

	/** `aiThinkStagger` — decalage FNV-1a de l'identifiant, dans [0, thinkEvery). */
	ANASTASISSIM_API double AiThinkStagger(const FString& Id);

	/** `needsCritical`. */
	ANASTASISSIM_API bool NeedsCritical(const AnastasisNeeds::FNeeds& Needs);

	/** `BUILDINGS[type].housing` — places de logement du catalogue (0 = pas un logement). */
	ANASTASISSIM_API int32 HousingOfType(const FString& Type);

	/** `BUILDINGS[type].group === "food"` (grenier). */
	ANASTASISSIM_API bool IsFoodGroupType(const FString& Type);

	/** `computeMealTtlSeconds` ; TravelSeconds NaN = trajet inconnu. */
	ANASTASISSIM_API double ComputeMealTtlSeconds(double TravelSeconds, double Base = Meal::TtlSeconds);

	/** Buts de la table adulte qui n'ont PAS de boucle portee (ecart n°1). */
	ANASTASISSIM_API const TArray<FString>& UnportedGoals();
}
