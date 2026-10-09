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
//     Bruit (sim-rng-001) : `goalNoise` est porte en fonction pure et la table de ses
//     dix-sept tirages (`Ai/AnastasisGoalNoise.h`), prouves contre les decisions
//     MESUREES de la reference (`Parite.BruitDeBut`) — mais PAS BRANCHE. Le releve
//     (`docs/migration/phase3/P3_RNG_RELEVE.md`) montre qu'avant ses quatorze bruits,
//     chaque decision adulte tire deja `exploreTarget` (2 a 8 tirages selon la memoire
//     des cases) : brancher la table seule coderait un faux ordre. Suite :
//     perception-explore-001.
//  2. Reconsideration aleatoire PORTEE (reconsider-001 : `sim.rng() < chance` a chaque
//     pensee avec cible, phase personnelle, quart de travail) et collant de but
//     (`goalStickinessBonus`) porte. Reste : deux fins d'action relachent la cible que la
//     reference garderait (chantier fini, ancre sociale).
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
//       - les tirages (`sim.rng`) des rumeurs viennent de `VillageRng`. Depuis sim-rng-001
//         c'est le flux PARTAGE de la reference (`makeRng(seed)`), etat lisible et posable
//         (`GetSimRngState` / `SetSimRngState`, pour `save.rng`). La trajectoire JS n'est
//         pourtant pas promise au tirage pres : la reference tire bien plus sur ce flux
//         (decision, `exploreTarget`, rate de coup, `npc.js` l. 893 ; releve
//         `P3_RNG_RELEVE.md`), et le C++ pas encore — la position dans le flux diverge.
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
//     pluie de `movementSpeedFactor` (le reste du facteur : nav-wiring-001).
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
//       - le bruit de but (ecart n°1), comme ailleurs. Le rate de coup est tire depuis
//         chat-on-haul-001 (`RollCraftMiss`, profil `build`).
// 13. Reference : `fee66ae`, commitee. Sa copie de travail porte, NON commitee,
//     `load > 11` au lieu de `load > 9` pour rentrer livrer : non suivi.
//
// Parite bit a bit : prouvee pour les besoins, le rythme, la qualite du repos et
// la decision Noûs (Anastasis.Sim.Parite.Besoins / .Rythme / .Nous). La boucle assemblee est
// deterministe, elle n'est PAS la trajectoire JS — l'ecart 1 suffit a l'interdire.

#pragma once

#include "CoreMinimal.h"
#include "Ai/AnastasisGoalNoise.h"
#include "Life/AnastasisReconsider.h"
#include "Life/AnastasisWorkShift.h"
#include "Ai/AnastasisNous.h"
#include "World/AnastasisTraffic.h"
#include "World/AnastasisExplore.h"
#include "World/AnastasisSoilWater.h"
#include "Core/AnastasisRng.h"
#include "Core/AnastasisSimBudget.h"
#include "Core/AnastasisSpatialGrid.h"
#include "Life/AnastasisBonds.h"
#include "Life/AnastasisEpisodes.h"
#include "Life/AnastasisLifestyle.h"
#include "Life/AnastasisNature.h"
#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Life/AnastasisWeatherBehavior.h"
#include "Village/AnastasisPlanner.h"
#include "Work/AnastasisBuild.h"
#include "Work/AnastasisGather.h"
#include "World/AnastasisEntityTable.h"
#include "World/AnastasisNavGrid.h"
#include "World/AnastasisNavService.h"
#include "World/AnastasisPathfinding.h"
#include "World/AnastasisWorld.h"

namespace AnastasisWorld { struct FWorld; }
namespace AnastasisArchive { class FStateArchive; }

namespace AnastasisVillage
{
	using FPoint = AnastasisPath::FPoint;

	/** Types de batiment que ce portage sait poser. Chaine = vocabulaire de la reference. */
	inline const TCHAR* const WellType = TEXT("well");
	inline const TCHAR* const HouseType = TEXT("house");
	inline const TCHAR* const GranaryType = TEXT("granary");

	/** `MORTALITY.famineHunger` (life/mortality.js). */
	inline constexpr double MortalityFamineHunger = 88.0;

	/** Buts portes. `observer` est l'etat initial de createNpc. */
	inline const TCHAR* const GoalObserver = TEXT("observer");
	/** Soigner une parcelle de champ (help-farm-001). */
	inline const TCHAR* const GoalHelpFarm = TEXT("helpFarm");
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
	/** `NPC_UNSTICK.pathFailStreakMax` (nav-wiring-001) : trois A* en echec de suite, la cible est abandonnee. */
	inline constexpr int32 PathFailStreakMax = 3;
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
		/**
		 * `building.stock` tel que la reference le tient (planner-wiring-001) : toutes les cases, dans
		 * l'ordre des cles. La case `food` est aussi tenue par `FoodPhysical` / `FoodReserved`, et celles
		 * d'un chantier par `Materials.Stock*` : la vue du planificateur les reprend, sa recopie les y rend.
		 */
		bool bHasPlannerStock = false;
		TArray<TPair<FString, AnastasisPlanner::FStockSlot>> PlannerStock;
		/** `building.stock.food` — `{physical, reserved}`, reserved <= physical. Grenier seulement. */
		int32 FoodPhysical = 0;
		int32 FoodReserved = 0;
		/**
		 * `building.laborToday` (act-gate-001) : les gestes de travail du jour notes sur ce poste
		 * (`notePlaceUse`), remis a zero chaque jour par la production quotidienne (non portee).
		 * `bHasLaborToday` : la cle existe (la reference ne l'ecrit qu'au premier geste).
		 */
		double LaborToday = 0.0;
		bool bHasLaborToday = false;

		/** Chantier : pieces posees (0..22), devis / consomme / stock du site, qui l'a ouvert. */
		int32 PiecesPlaced = 0;
		bool bHasMaterials = false;
		AnastasisBuild::FSiteMaterials Materials;
		FString BuilderId;
		/**
		 * ecart n°50 (valmire-grows-001) : l'habitant qui a trace ce batiment commun quand le village l'a
		 * decide, et pourquoi (lisible). Vides pour un chantier ouvert par l'hote, un scenario ou une famille.
		 */
		FString OpenedById;
		FString OpenCause;
		/** `building.workers[npcId]` : pieces posees par chacun, dans l'ordre d'arrivee. */
		TArray<TPair<FString, int32>> Workers;
		/**
		 * memoire-decisions-001 (ecart n°48) -- la maison d'une famille : le foyer qui l'a voulue, ceux qui ont le
		 * droit d'y travailler (la famille et ceux qui ont dit oui ; vide = tout le monde, comme la reference),
		 * et ceux a qui l'on a deja demande.
		 */
		FString OwnerFamilyId;
		TArray<FString> AllowedBuilders;
		TArray<FString> AskedIds;
		/** `completedDay`, `completedById` : -1 / vide tant qu'il n'est pas acheve sous les coups. */
		int32 CompletedDay = -1;
		FString CompletedById;

		/**
		 * `building.vacantSinceDay` (collectivePriorities.js, `stampHouseVacant`) : jour ou la maison
		 * est devenue libre, -1 = `null` (occupee). Pose par AddBuilding, RemoveNpc, AssignHome, et par le
		 * planificateur (`housingVacancySnapshot`, planner-wiring-001, via sa vue) ; lu et projete par le harnais.
		 */
		int32 VacantSinceDay = -1;

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

	/** Une ligne de `npc.goalExplain.top` : un des trois premiers buts, et sa cause dominante. */
	struct FGoalExplainEntry
	{
		FString Goal;
		/** Arrondis au dixieme (`Math.round(x * 10) / 10`). */
		double Score = 0.0;
		FString Cause;
		FString CauseKey;
		double CauseValue = 0.0;
	};

	/** `npc.goalExplain` (`captureGoalExplain`, ai/explainGoal.js) : pourquoi ce but, au commit. */
	struct FGoalExplain
	{
		double At = 0.0;
		/** `scores[0].goal` ; vide = `null` (table vide). */
		FString Goal;
		TArray<FGoalExplainEntry> Top;
		FString Line;
	};

	/** `npc.streetDecision` (`stampStreetDecision`, ai/streetSignal.js) : la fenetre d'une bascule. */
	struct FStreetDecision
	{
		double At = 0.0;
		double Until = 0.0;
		FString Goal;
		/** Vide = `null`. */
		FString From;
		double Margin = 0.0;
		bool bChanged = false;
		bool bTight = false;
		FString Cause;
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
		/** Tirages `sim.rng` de la decision (perception-explore-001) : `exploreTarget`, puis les bruits. */
		int32 ExploreDraws = 0;
		/** `survivalForecastBias` et `spatialRiskBiasMap` (resource-targets-001), but -> biais, cles a 0 comprises. */
		TMap<FString, double> ForecastBias;
		TMap<FString, double> SpatialRiskBias;
		/** Chaque ligne juste avant et juste apres l'ajout des deux cartes (spatial-risk-test-001). */
		TMap<FString, double> RowsBeforeRisk;
		TMap<FString, double> RowsAfterRisk;
		int32 NoiseDraws = 0;
		/** Le bruit tire pour chaque ligne qui en a tire un (but -> valeur). */
		TMap<FString, double> RowNoise;
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
		/** Le verrou de quart a tenu le but precedent (reconsider-001). */
		bool bShiftLock = false;
		/** `goalStickinessBonus` ajoute a la ligne du but en cours (0 si aucun). */
		double Stickiness = 0.0;
		/** Passe collective (collective-pass-001) : urgence, plancher et rush appliques, par ligne. */
		TMap<FString, double> CollectiveDelta;
	};

	/**
	 * Ce que le planificateur collectif (`collectivePriorities.js`) dit a la decision d'UN habitant
	 * (collective-pass-001). Rempli par le module du planificateur ; vide (tout a 0, faux) tant
	 * qu'il n'est pas branche : la table garde alors ses bits.
	 */
	struct FCollectiveDecision
	{
		/** `collectiveGoalBias(sim, goal)` : additif DANS la formule de certaines lignes. */
		TMap<FString, double> GoalBias;
		/** `collectiveGoalFloor(sim, goal)` : plancher apres `workFactor`. */
		TMap<FString, double> GoalFloor;
		/** `collectiveUrgencyBiasMap(sim, npc)` : additif sur chaque ligne, propre a l'habitant. */
		TMap<FString, double> UrgencyBias;
		/** `isWoodBootstrapDraftee(sim, npc)` : plancher `bootstrapWoodFloor` (165) sur `gatherWood`. */
		bool bWoodBootstrapDraftee = false;
		/** `isFoodRush(sim)`. */
		bool bFoodRush = false;
		/** `farmStaffingGap(sim).gap` : postes de ferme achevee sans fermier. */
		int32 FarmStaffingGap = 0;
		/** `sim.buildingNeedScore()` : la pression a batir du planificateur (le `need` de `buildScore`). */
		double BuildingNeedScore = 0.0;
		/**
		 * Ce que `buildScore` lit de la colonie (planner-wiring-001) : vrai quand le village a une
		 * colonie ; `bBuildIdle` = le retour 0 (ni besoin, ni lisiere, ni manque en attente) ;
		 * sinon `needFloor x liquidity`.
		 */
		bool bHasColony = false;
		bool bBuildIdle = false;
		double BuildNeedTerm = 0.0;

		double BiasOf(const FString& Goal) const { const double* V = GoalBias.Find(Goal); return V ? *V : 0.0; }
		double FloorOf(const FString& Goal) const { const double* V = GoalFloor.Find(Goal); return V ? *V : 0.0; }
		double UrgencyOf(const FString& Goal) const { const double* V = UrgencyBias.Find(Goal); return V ? *V : 0.0; }
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

		// Service de navigation et pas de marche (nav-wiring-001).
		/** `actor.pathFailStreak`. */
		int32 PathFailStreak = 0;
		/** `navigation.requestedAt`. */
		double NavRequestedAt = 0.0;
		/** `navigation.awaitingPath`. */
		bool bAwaitingPath = false;
		/**
		 * `navigation.path` / `navigation.pathIndex` : des COPIES de `actor.path` / `actor.pathStep`, recopiees
		 * seulement par `syncNavigationFromActor`. Entre deux synchronisations, la reference les laisse
		 * en retard (un `npc.path = null` ne les touche pas) : la projection les montre tels quels.
		 */
		TArray<FPoint> NavPath;
		int32 NavPathIndex = 0;
		/** `navigation.doorQueueRole` (vide = null) et `doorQueueRank`. */
		FString DoorQueueRole;
		int32 DoorQueueRank = 0;
		/** `navigation.stuckTicks`. */
		int32 StuckTicks = 0;
		/** `actor.lastMoveDir`, pose au premier segment marche. */
		bool bHasLastMoveDir = false;
		FPoint LastMoveDir;
		/** `actor.hesitationTimer` / `hesitationCooldown` : absents tant que `moveActor` n'a pas tourne. */
		bool bHasHesitation = false;
		double HesitationTimer = 0.0;
		double HesitationCooldown = 0.0;
		/** `actor.trafficTimer`. */
		double TrafficTimer = 0.0;

		/** `npc.inventory.food`. */
		int32 InventoryFood = 0;
		/** Extension opt-in de portage materiel : charge conservee entre source et chantier. ecart n°18. */
		int32 MaterialCarry = 0;
		AnastasisWorld::EResource MaterialResource = AnastasisWorld::EResource::None;
		int32 MaterialSourceIndex = INDEX_NONE;
		double MaterialRetryAt = 0.0;
		int32 MaterialsDelivered = 0;
		/** Confirmed wood removed from live tiles; no depot transfer yet (ecart n°30). */
		int32 InventoryWood = 0;
		int32 GatheredWood = 0;
		int32 GatheredFood = 0;
		int32 DeliveredFood = 0;
		int32 FoodSourceIndex = INDEX_NONE;
		/** Locally perceived source quantities; zero means observed exhausted. */
		TMap<int32, int32> KnownFoodSources;

		/**
		 * familles-feu-001 (ecart n°44) -- l'identite d'un habitant : `npc.name`, `npc.familyName`, `npc.gender`
		 * (« male » / « female »), `npc.age` (annees) et `npc.familyId` de createNpc. Ce ne sont que des donnees :
		 * aucune decision ne les lit encore (ni stade de vie, ni lien de parente, ni enfant). Vides et 0 pour un
		 * habitant cree par SpawnNpc seul, comme le reste du harnais. `KinRole` (chef, epouse, fils, frere,
		 * pupille, engage...) est une EXTENSION : la reference ne range pas un frere ni un engage dans un foyer.
		 */
		FString Name;
		FString FamilyName;
		FString Gender;
		double Age = 0.0;
		FString FamilyId;
		FString KinRole;

		/**
		 * memoire-decisions-001 (ecart n°47) -- `npc.chronicle` d'ai/episodes.js : ce que l'habitant a vecu, vu
		 * ou entendu, avec la deformation de chaque bouche. Vide tant que rien ne lui arrive.
		 */
		AnastasisEpisodes::FChronicle Chronicle;

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
		/**
		 * Le dernier coup rate (chat-on-haul-001, `craftMiss.js`) : `npc.craftMissAt` et
		 * `npc.craftMiss = { kind, craftId, at }`. Absents = 0 et vide (la reference lit `|| 0`).
		 */
		double CraftMissAt = 0.0;
		FString CraftMissKind;
		FString CraftMissCraftId;
		double CraftMissStampAt = 0.0;
		/** Observation : nombre de livraisons faites (les quantites : GatheredFood, DeliveredFood). */
		int32 Deliveries = 0;
		FHungerAction HungerAction;
		/** `mind.beliefs.knownStocks`, dans l'ordre d'insertion (celui d'un objet JS). */
		TArray<FStockBelief> KnownStocks;
		double LastScan = -999.0;
		/**
		 * `mind.cells` (perception-explore-001) : les regions 8x8 ou l'habitant s'est
		 * tenu lors d'une perception (`markCell`), par index `floor(y/8) * cols + floor(x/8)`.
		 * Jamais oubliees. `exploreTarget` saute une region connue.
		 */
		TSet<int32> KnownCells;
		/** `mind.cellCount` — tenu par `markCell`, egal a `KnownCells.Num()` hors reprise. */
		int32 CellCount = 0;
		/** `npc.villagePhase` — la bascule force une pensee. */
		FString VillagePhase;
		double GoalSince = 0.0;
		/**
		 * `npc.phaseChangedAt` (reconsider-001) : l'instant de la derniere bascule de la phase
		 * PERSONNELLE (`syncVillagePhase`). Vide = `null` (jamais bascule) : c'est la valeur
		 * « absent » que le lecteur pose quand la sauvegarde n'a pas le champ.
		 */
		TOptional<double> PhaseChangedAt;
		/** `npc.workShift` (reconsider-001) — NON sauvegarde par la reference : absent au chargement. */
		AnastasisWorkShift::FWorkShift WorkShift;
		/** `npc.activitySince` : l'heure du dernier changement d'activite (`setActivity`). */
		double ActivitySince = 0.0;
		/**
		 * Ce que la premiere pensee ecrit (premiere-pensee-001). Chaque cle apparait dans la
		 * reference au moment ou elle est ecrite pour la premiere fois ; le C++ la tient des ce moment.
		 */
		TOptional<FGoalExplain> GoalExplain;
		TOptional<FStreetDecision> StreetDecision;
		/** `npc.hungerAction` cree (`ensureHungerAction`, des la premiere decision Noûs). */
		bool bHasHungerAction = false;
		/** `npc.nocturnalIntent`, pose par `phaseBias` a chaque ligne de la table. */
		TOptional<bool> NocturnalIntent;
		/** `npc.buildBinding` ecrit (au commit : `null` hors `build`, sinon `bindBuildSite`). */
		bool bHasBuildBinding = false;
		/** `npc.socialSeekId` ecrit (`resolveNpcDestination`, a chaque cible). */
		bool bHasSocialSeekId = false;
		/** `npc.mind.failures` cree (`ensureFailures` dans `failureTargetBiasMap`) ; toujours vide ici (ecart n°9). */
		bool bHasFailureStore = false;
		/** `skills.care` (lu pour `skillGoalBias` des buts sociaux). */
		double SkillCare = 0.0;
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
		/**
		 * `deeds.helped` (help-farm-001) : soins de parcelle donnes. Observation : le lecteur ne
		 * projette pas `deeds` (ses autres compteurs non plus).
		 */
		int32 DeedsHelped = 0;

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
		/**
		 * `npc.nature` (lifestyle-decision-001) : lue de la sauvegarde. Un habitant cree par le C++ n'en a pas ;
		 * la reference lui en tirerait une dans le flux de secours : il garde la nature moyenne (ecart n°10).
		 */
		TOptional<AnastasisNature::FNature> Nature;
		/**
		 * `npc.gold` (lifestyle-decision-001), lu pour `statusBias`. L'or ne bouge pas ici (ecart n°12). Un
		 * habitant cree par le C++ n'en a pas : `createNpc` lui en donnerait 10 a 29, ni pauvre ni aise.
		 */
		TOptional<double> Gold;

		/**
		 * `npc.placeMemory` (act-gate-001) : un lieu par batiment frequente, dans l'ordre de premiere
		 * visite (`Object.values` le parcourt ainsi, et le favori garde le premier a score egal).
		 * Tenu par `FVillage::NotePlaceUse` (`simulation.js` `notePlaceUse`).
		 */
		struct FPlaceEntry
		{
			FString BuildingId;
			FString Type;
			double Score = 0.0;
			double Work = 0.0;
			double Social = 0.0;
			double Home = 0.0;
			double Talk = 0.0;
			double Drink = 0.0;
			double Activity = 0.0;
			double Crisis = 0.0;
			double DecayDay = 0.0;
			double LastDay = 0.0;
			/** `entry.lifestyle` : points par mode de vie, ordre d'insertion. */
			TArray<TPair<FString, double>> Lifestyle;
		};
		TArray<FPlaceEntry> PlaceEntries;
		/** `placeMemory.favoriteBuildingId` ; vide = `null`. */
		FString FavoriteBuildingId;

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
	 * EXTENSION -- ecart n°46 (save-history-001) : la biographie d'un batiment, observee par la simulation.
	 *
	 * Ce que la simulation sait d'un batiment au present (proprietaire, dormeurs, phase) ne dit pas son
	 * HISTOIRE : qui l'a fonde, pour quel foyer, quand il a change de mains, combien de nuits il a ete
	 * plein. SETTLEMENT_MORPHOGENESIS_001 l'ecrivait dans la presentation Unreal, image par image ; elle
	 * fixe la forme du batiment (le programme de son fondateur), et elle n'est pas derivable d'un
	 * instantane (un fondateur mort ne s'y lit plus). Elle vit donc ici, observee a chaque pas : elle entre
	 * dans l'empreinte d'etat et dans la sauvegarde, et un saut de temps n'en perd aucune transition.
	 * Aucune regle ne la lit : elle n'ecrit rien d'autre qu'elle-meme. La forme (le programme) se deduit
	 * dans l'hote Unreal, de `FormHousePhase`, `FounderJob` et `FounderHousehold`.
	 */
	enum class EBiographyEvent : uint8
	{
		Seen,         // le batiment apparait (chantier ou pose directe)
		Completed,    // le chantier s'acheve
		Founded,      // un foyer le prend pour la premiere fois : la forme se fixe
		OwnerChanged, // il passe a un autre foyer
		OwnerLost,    // son proprietaire disparait (mort, depart) : la maison se vide
		Crowded,      // premiere nuit ou le foyer remplit la maison (pression d'agrandissement)
		Vacated,      // plus personne n'y dort
		Reoccupied,   // quelqu'un y dort de nouveau
	};

	ANASTASISSIM_API const TCHAR* BiographyEventName(EBiographyEvent Kind);

	struct FBiographyEvent
	{
		int32 Day = 0;
		EBiographyEvent Kind = EBiographyEvent::Seen;
		FString Detail;
	};

	struct FBuildingBiography
	{
		FString Id;
		FString Type;
		int32 CellX = 0;
		int32 CellY = 0;
		int32 FirstSeenDay = 0;
		/** Jour d'achevement (FBuilding::CompletedDay, sinon le premier jour vu acheve). -1 = chantier. */
		int32 CompletedDay = -1;
		/** Jour ou un foyer l'a prise (ou, hors maison, ou elle s'est achevee) : la forme est fixe depuis. -1 = jamais. */
		int32 FoundedDay = -1;
		FString Founder;
		FString FounderJob;
		int32 FounderHousehold = 0;
		/** Phase de la maison quand sa forme a ete lue : a la premiere vue, puis a la fondation. */
		int32 FormHousePhase = 0;
		bool bFormFixed = false;
		FString Owner;
		int32 Occupants = 0;
		int32 PeakOccupants = 0;
		/** Nuits ou le foyer remplissait la maison (`occupants >= capacite - 1`, la pression de `resolveHouseUpgrades`). */
		int32 CrowdedDays = 0;
		int32 OwnerChanges = 0;
		int32 VacancyEpisodes = 0;
		bool bWasOccupied = false;
		int32 LastObservedDay = -1;
		TArray<FBiographyEvent> Events;

		int32 AgeDays(int32 Day) const { return CompletedDay < 0 ? 0 : FMath::Max(0, Day - CompletedDay); }
	};

	/**
	 * EXTENSION — ecart n°40 (opening-in-sim-001). Ce que `FVillage::SeedOpeningVillage` a decide pour le
	 * village d'ouverture du jeu Unreal. Un rapport, pas un etat : l'etat est dans les habitants et les
	 * batiments (foyer, poste, metier, porteur), plus le verrou `OpeningSiteId` du village.
	 */
	struct FOpeningReport
	{
		/** Au moins un colon : le foyer et le poste du premier ont ete cherches. */
		bool bHousehold = false;
		FString ResidentId;
		FString HomeId;
		FString WorkId;
		/** Chantier demande et au moins deux colons : il a ete cherche. */
		bool bConstructionTried = false;
		/** Vide si aucun chantier atteignable n'a ete trouve. */
		FString SiteId;
		int32 SiteX = -1;
		int32 SiteY = -1;
		TArray<FString> Builders;
		FString CourierId;
		int32 StockWood = 0;
		int32 StockStone = 0;
		/** Le premier colon est embauche au grenier : les colons libres qui l'atteignent l'y rejoignent. */
		bool bWorkforce = false;
		TArray<FString> Farmers;
	};

	/** Ou en est l'attribution de la maison d'ouverture achevee (ecart n°40). */
	enum class EOpeningHomeStatus : uint8
	{
		/** Aucun chantier d'ouverture dans ce village (harnais, scenarios explicites). */
		None,
		/** Chantier ouvert, maison pas encore achevee. */
		Pending,
		/** Achevee et donnee au batisseur sans toit le plus proche par le chemin. */
		Assigned,
		/** Achevee, mais aucun batisseur sans toit ne l'atteint. */
		Unassigned,
		/** Le chantier a disparu avant d'etre acheve. */
		SiteGone,
	};

	struct FOpeningHomeOutcome
	{
		EOpeningHomeStatus Status = EOpeningHomeStatus::None;
		FString SiteId;
		FString NpcId;
		/** `sim.time` de la resolution (0 tant qu'en attente). */
		double Time = 0.0;
	};

	ANASTASISSIM_API const TCHAR* OpeningHomeStatusName(EOpeningHomeStatus Status);

	/**
	 * L'etat du village et ses regles. Lie a un monde genere, qu'il ne possede
	 * pas : l'hote de simulation garde le monde et le village cote a cote.
	 */
	class ANASTASISSIM_API FVillage : public AnastasisNavService::INavServiceHost
	{
	public:
		// --- Hote du service de navigation (nav-wiring-001) ---------------------
		double GetTime() const override { return Now; }
		/** `sim.speedScale` : le harnais tourne a 1. */
		double GetSpeedScale() const override { return 1.0; }
		int32 GetNavVersion() const override { return NavVersion; }
		const AnastasisPath::INavSource& GetNavSource() const override;
		AnastasisNavService::FNavAgent* FindLiveAgent(const FString& Id) override;
		/** `data.navVersion` (harnais) : la version de navigation sauvee, et le cache `navCache`. */
		void RestoreNavigationForHarness(int32 InNavVersion,
			const TArray<TPair<FString, AnastasisNavService::FNavCacheEntry>>& InCache);

		/** Lie le village a un monde et reconstruit la grille de navigation. Vide les tables. */
		void Bind(const AnastasisWorld::FWorld& InWorld);
		bool IsBound() const { return World != nullptr; }

		/** `sim.settlement` : l'origine vers laquelle les portes s'ouvrent. Centre du monde par defaut. */
		void SetSettlement(double InX, double InY) { Settlement = { InX, InY }; }
		FPoint GetSettlement() const { return Settlement; }
		/** RouteCost-001: optional Unreal gameplay extension. JS parity harness keeps the reference's uniform travel time. */
		void SetTerrainTravelCostEnabled(bool bEnabled) { bTerrainTravelCostEnabled = bEnabled; }
		/** Host-only coverage from actually embodied crowns; unset in portable parity runs. */
		void SetRainCanopyCover(TFunction<double(double, double)> InCover) { RainCanopyCover = MoveTemp(InCover); }
		double GetRainCanopyCover(double X, double Y) const { return RainCanopyCover ? RainCanopyCover(X, Y) : 0.0; }
		bool IsTerrainTravelCostEnabled() const { return bTerrainTravelCostEnabled; }
		/** Extension ecart n°18 : un porteur explicite travaille sur les chantiers secs. Vide = inactif. */
		void SetMaterialCourier(const FString& NpcId) { MaterialCourierId = NpcId; }
		const FString& GetMaterialCourierId() const { return MaterialCourierId; }
		const FString& GetMaterialCourier() const { return MaterialCourierId; }

		/**
		 * `settlement.marketDx/marketDy` (build-decision-001) : le site reserve au marche, relatif au
		 * camp. Non pose = `undefined` cote reference : `ensureMarketOffset` prend alors son repli.
		 */
		void SetMarketOffset(const TOptional<double>& Dx, const TOptional<double>& Dy) { MarketDx = Dx; MarketDy = Dy; }
		/** `plannedMarketPos()` : le camp + le decalage du marche, garde hors du camp. */
		FPoint PlannedMarketPos() const;
		/**
		 * `marketAccessPoint(actor)` : le seuil du marche ; sans marche bati (aucun dans ce village),
		 * `accessPointNear(plannedMarketPos(), actor)`.
		 */
		bool MarketAccessPoint(const FNpc* Actor, FPoint& Out) const;

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

		/**
		 * EXTENSION — ecart n°38 (geopolitical-world-001). Un groupe venu du dehors arrive au village :
		 * `Count` habitants ordinaires (`spawnNpc`, besoins par defaut d'un arrivant), poses sur le
		 * premier sol libre d'un anneau a `Radius` cases du camp, a partir de l'angle `StartAngle` (rad).
		 * Aucun tirage : la position ne depend que du terrain et des arguments. Rend les identifiants
		 * crees (moins que `Count` si le sol libre manque, vide sans monde).
		 */
		TArray<FString> AdmitExternalArrivals(int32 Count, double Radius, double StartAngle);

		/**
		 * EXTENSION — ecart n°40 (opening-in-sim-001). Le village d'ouverture du jeu Unreal, une fois le puits
		 * et les colons poses : le premier colon recoit une maison et un poste au grenier qu'il atteint
		 * (pres du champ de nourriture le plus proche qu'il atteint) ; si `bOpenConstruction`, un chantier
		 * de maison sec est ouvert pres d'un colon, ses un ou deux batisseurs atteignent son seuil, le
		 * premier porte les materiaux ; puis deux colons libres qui atteignent le grenier y sont embauches.
		 * Tout par chemin reel (`findPath`), dans l'ordre des habitants, sans tirage. `Day` date les
		 * batiments. Pose le verrou lu par `AssignCompletedOpeningHome`.
		 */
		FOpeningReport SeedOpeningVillage(int32 Day, bool bOpenConstruction);

		/**
		 * EXTENSION — ecart n°40. La regle de la maison d'ouverture, lue a la fin de chaque `UpdateActors` :
		 * des que le chantier d'ouverture est acheve, une seule tentative le donne au batisseur sans toit
		 * qui en atteint un seuil par le chemin le plus court (premier dans l'ordre des habitants a
		 * egalite), puis le verrou tombe. Sans verrou (tout village du harnais) : ne fait rien.
		 */
		void AssignCompletedOpeningHome();
		/** Le chantier d'ouverture en attente d'achevement ; vide sinon. */
		const FString& GetOpeningSiteId() const { return OpeningSiteId; }
		const FOpeningHomeOutcome& GetOpeningHome() const { return OpeningHome; }

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
		/** Bucheron : le bois coupe peut etre livre a un chantier ouvert, jamais credite a distance. */
		bool WoodDeliveryTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		bool DeliverWoodToSite(FNpc& Npc);

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
		/** Experimental field reservoir (ecart n°37). Off by default for JS parity. */
		void SetSoilWaterEnabled(bool bEnabled)
		{
			if (bSoilWaterEnabled != bEnabled) { bSoilWaterEnabled = bEnabled; SoilWaterByTile.Reset(); }
		}
		bool IsSoilWaterEnabled() const { return bSoilWaterEnabled; }
		bool GetSoilWaterAt(int32 TileX, int32 TileY, double& OutStored) const;

		/** Une mort, telle que le village la retient (hors empreinte). */
		struct FDeath
		{
			FString NpcId;
			FString Cause;
			int32 Day = 0;
		};

		/**
		 * MORTALITY_001 -- `causeOfDeath`, branche « vitalite epuisee » seule (ecart n°28) : la cause
		 * d'un habitant dont la sante est <= 0 (`de soif`, `de faim`, `d'epuisement`, `de faiblesse`,
		 * seuils de la reference), vide tant qu'il vit. Aucun tirage.
		 */
		static FString CauseOfDeath(const FNpc& Npc);

		/**
		 * `updateMortalityDaily` reduit a la mort certaine (ecart n°28) : parcours a l'envers, tout
		 * habitant de sante <= 0 est retire (`RemoveNpc` : maison liberee, repas rendu), puis oublie
		 * de ses semblables (`forgetTheDead`). Appele a minuit par l'hote. Rend le nombre de morts.
		 * Ne tire jamais dans le flux aleatoire.
		 */
		int32 UpdateMortalityDaily();
		const TArray<FDeath>& GetDeaths() const { return DeathLog; }

		/**
		 * familles-feu-001 (ecart n°44) -- un foyer, forme de `createFamily` (life/household.js, sim/life.js) :
		 * `adults`, `dependents`, `homeId`. `Name` (« la maison du Scribe ») est une EXTENSION : la reference nomme
		 * le foyer par le `familyName` de ses membres. Pas de `births`, `lastBirthDay`, ni de couple forme par la
		 * simulation : un foyer se pose seulement par l'hote (fondation).
		 */
		struct FFamily
		{
			FString Id;
			FString Name;
			TArray<FString> Adults;
			TArray<FString> Dependents;
			FString HomeId;
		};

		/** Pose un foyer vide et rend son identifiant (`family-N`). */
		FString AddFamily(const FString& Name);
		/** Range un habitant dans un foyer, adulte ou dependant, avec son role ; le retire de son foyer precedent. */
		bool JoinFamily(const FString& NpcId, const FString& FamilyId, bool bAdult, const FString& KinRole);
		/** Nom, nom de famille, sexe (« male » / « female ») et age d'un habitant. Ne change aucune decision. */
		bool SetIdentity(const FString& NpcId, const FString& InName, const FString& InFamilyName, const FString& InGender, double InAge);
		const TArray<FFamily>& GetFamilies() const { return Families; }
		const FFamily* FindFamily(const FString& Id) const;

		// --- Memoire episodique (memoire-decisions-001, ecart n°47 ; ai/episodes.js) -----------------------

		/** Les options de `recordEpisode(sim, npc, kind, options)`. */
		struct FEpisodeOptions
		{
			/** L'autre habitant implique (`about`) : son identifiant ; son nom est lu sur lui. */
			FString AboutId;
			FString Note;
			double Detail = 0.0;
			double Intensity = 1.0;
			TOptional<double> Weight;
			/** Un fait vecu par plusieurs partage une racine ; vide = l'identifiant du souvenir. */
			FString RootId;
		};

		/**
		 * `recordEpisode` : ce qui arrive a cet habitant et qu'il n'oubliera pas de sitot. Rend l'identifiant du
		 * souvenir, vide s'il le savait deja (comme la reference, un souvenir aussitot evince par la capacite compte).
		 */
		FString RecordEpisode(const FString& NpcId, const FString& Kind, const FEpisodeOptions& Options);
		/** `recordWitnesses` : ceux qui etaient assez pres retiennent la scene, moins fort ; quatre au plus. Rend leur nombre. */
		int32 RecordWitnesses(const FString& SubjectId, const FString& Kind, const FEpisodeOptions& Options, double Radius = 6.0);
		/**
		 * EXTENSION (ecart n°47) -- un habitant raconte a un autre UN souvenir precis, de vive voix (le premier soir
		 * au feu) : la meme deformation que `tellEpisodes` (`retell`, puis `createGossipEpisode` et sa reception),
		 * sans le tirage du choix ni celui de l'envie de repeter. Rend vrai si l'autre l'a retenu.
		 */
		bool TellEpisode(const FString& FromId, const FString& ToId, const FString& EpisodeId);
		/** `fadeEpisodes` pour chacun : le travail `memory` de minuit. */
		void FadeEpisodesDaily(int32 Day);
		/** `applyEpisodeFeelings` : ce que l'on a vecu avec quelqu'un teinte la relation. Phase « relations » de la vie du soir. */
		void ApplyEpisodeFeelingsDaily();
		// --- Decider de batir, demander de l'aide (memoire-decisions-001, ecart n°48 ; Bible §29) -------------

		/** Une reponse a une demande d'aide : qui a demande a qui, pour quel chantier, oui ou non, et la raison dominante. */
		struct FHelpAnswer
		{
			int32 Day = 0;
			FString FromId;
			FString ToId;
			FString SiteId;
			bool bAccepted = false;
			/** dette_rendue, amitie, voisin ; refus_rendu, dette, son_toit, occupe, faible, inconnu. */
			FString Reason;
		};
		/** Resultat d'une demande adressee pendant la journee. Un rejet de precondition ne change aucun etat. */
		struct FHelpRequestResult
		{
			bool bValid = false;
			FString InvalidReason;
			FHelpAnswer Answer;
			/** Rang stable dans HelpLog, zero si la demande n'a pas ete entendue. */
			int32 RequestId = 0;
		};
		static constexpr double HelpSpeakingRange = 6.0;
		/** ecart n°48 : meme verbe pour chaque habitant, avec rencontre physique et reponse immediate. */
		FHelpRequestResult AskHelp(const FString& FromId, const FString& ToId, const FString& SiteId);

		/** Combien de personnes un chef de famille va voir par jour. */
		static constexpr int32 HelpAsksPerDay = 2;

		/**
		 * Le soir (phase « vie » de minuit) : une famille sans maison decide d'en batir une (une famille par soir) ;
		 * chaque famille qui batit va demander de l'aide a deux personnes de plus ; une maison achevee revient a
		 * sa famille, et son chef se souvient de qui l'a aidee. Sans foyer pose (le harnais), ne fait rien.
		 */
		void UpdateFamilyHousesDaily();

		/** `evaluateRequest` (Bible §29) : la somme des raisons, et la plus forte dans le sens de la reponse. */
		FHelpAnswer EvaluateHelp(const FNpc& Asker, const FNpc& Asked, const FBuilding& Site) const;

		/**
		 * « Seul pour un abri, a plusieurs pour une vraie maison » (Alexandre, 2026-10-08) : la maison d'une famille
		 * monte a moitie par les siens ; le toit attend qu'un aidant hors de la famille ait dit oui.
		 */
		static constexpr double FamilyRoofAt = 0.5;
		/** Le toit attend : personne hors de la famille n'y a encore travaille, ni n'a dit oui. */
		bool AwaitsHelp(const FBuilding& Site) const;
		/**
		 * Pour cet habitant : un membre de la famille attend qu'un aidant soit venu poser ses pieces (dire oui ne
		 * suffit pas) ; un aidant, lui, n'attend personne.
		 */
		bool AwaitsHelpFor(const FBuilding& Site, const FNpc& Npc) const;

		/** Ce chantier admet-il cet habitant ? (Vide : tout le monde.) */
		static bool CanBuildAt(const FBuilding& Site, const FString& NpcId) { return Site.AllowedBuilders.IsEmpty() || Site.AllowedBuilders.Contains(NpcId); }

		const TArray<FHelpAnswer>& GetHelpLog() const { return HelpLog; }

		/** `episodeGoalBias(npc, goal)` : borne, nul sans souvenir. */
		double EpisodeGoalBiasOf(const FNpc& Npc, const FString& Goal) const { return AnastasisEpisodes::GoalBias(Npc.Chronicle, Goal); }
		/** Observation : nourriture ajoutee aux champs par la repousse depuis Bind. */
		int64 GetRegrownFood() const { return RegrownFood; }

		/**
		 * Travail `memory` de la file de minuit : `forgetStale` (gisements de plus de
		 * 14 jours) et `forgetStalePeople` (fiches de plus de 26 jours), pour chacun.
		 */
		void ForgetStaleDaily(int32 Day);

		// --- Passage et sentiers (`sim.traffic`, settlement-morphogenesis-001) ------------------

		/** `decayFootTraffic` (trafficDecay.js), a minuit avant les logements. Fidele. Rend les cases touchees. */
		int32 DecayTrafficDaily();

		/**
		 * `updateRoadEvolutionDaily`, branche sentier de desir seule (ecart n°42) : une case foulee
		 * au-dela de 14 passages accumule un effort ; apres 18 nuits (plus pour un champ ou une foret)
		 * elle devient un sentier (`Road`, classe `path`), dont le cout de marche tombe a 0,86. Ne fait
		 * rien tant que l'hote n'a pas active l'extension (`SetRoadEvolutionEnabled`). Rend les sentiers poses.
		 */
		int32 UpdateRoadEvolutionDaily(int32 Day);

		/**
		 * ecart n°50 (valmire-grows-001) -- le village decide de ses batiments communs. Active par l'hote
		 * seulement, pour le village du lancement (le village C++ nu, le harnais et les scenarios restent a
		 * faux). Chaque soir (`UpdateCommonBuildingsDaily`, travail `collective` de la file de minuit), s'il
		 * manque un grenier (aucun, ou tous pleins) ou un puits (trop d'ames par puits), un habitant en trace
		 * l'emplacement et le chantier s'ouvre sec. Les maisons restent l'affaire des familles (ecart n°48).
		 */
		void SetGrowthEnabled(bool bEnabled);
		bool IsGrowthEnabled() const { return bGrowthEnabled; }
		/** ecart n°50 : la decision du soir ; rend l'identifiant du chantier ouvert, ou vide. */
		FString UpdateCommonBuildingsDaily(int32 InDay);
		/** Chantiers communs que le village a ouverts de lui-meme depuis Bind. */
		int32 GetGrowthSitesOpened() const { return GrowthSitesOpened; }
		/** La derniere decision de `pickCollectiveBuilding` (type vide = aucun), pour les rapports. */
		const FString& GetLastBuildDecision() const { return LastBuildDecision; }

		/** Activation de l'ecart n°42 : l'hote Unreal seul ; le village C++ nu reste a faux (parite). */
		void SetRoadEvolutionEnabled(bool bEnabled) { bRoadEvolutionEnabled = bEnabled; }
		bool IsRoadEvolutionEnabled() const { return bRoadEvolutionEnabled; }

		/**
		 * ecart n°46 -- la biographie des batiments : observee a la fin de chaque `UpdateActors` quand l'hote
		 * l'active (le village C++ nu reste a faux : le harnais n'en ecrit aucune). `ObserveBiographies` est
		 * aussi appelable directement (tests). Rend le nombre d'evenements ecrits.
		 */
		void SetBiographyEnabled(bool bEnabled) { bBiographyEnabled = bEnabled; }
		bool IsBiographyEnabled() const { return bBiographyEnabled; }
		int32 ObserveBiographies(int32 Day);
		const TMap<FString, FBuildingBiography>& GetBiographies() const { return Biographies; }
		const FBuildingBiography* FindBiography(const FString& BuildingId) const { return Biographies.Find(BuildingId); }

		/** `trafficAt(x, y)` : passage accumule (f32 relu en double), 0 hors bornes. */
		double TrafficAt(int32 TileX, int32 TileY) const;
		const TArray<float>& GetTraffic() const { return Traffic; }
		/** Sentiers nes du passage : index de case -> classe, jour de naissance, passage a la naissance. */
		const TMap<int32, AnastasisTraffic::FRoadTile>& GetRoads() const { return Roads; }
		/** Effort de defrichage en cours (`roadBuildEffort`) : index de case -> jours accumules. */
		const TMap<int32, double>& GetRoadEfforts() const { return RoadEfforts; }
		/** Observation : passages enregistres depuis Bind. */
		int64 GetPassageCount() const { return PassageCount; }

		/** Graine du flux de tirages du village (rumeurs). Defaut fixe ; l'hote la pose. */
		void SetRngSeed(uint32 Seed) { VillageRng = FAnastasisRng(Seed); }

		/**
		 * `sim.rng` (sim-rng-001) : `VillageRng` EST le flux partage de la reference,
		 * `this.rng = makeRng(this.seed)` (simulation.js l. 1022) — l'hote le seme avec
		 * la graine de la simulation (`SetRngSeed`), comme la reference.
		 *
		 * Ce qui tire dessus cote C++ aujourd'hui : les actes de parole sur les gisements
		 * (`createInformResourceSpotActs`, speechActs.js l. 62), dans `ExchangeSpotRumors`
		 * et `SocializeWithCompanion` — et rien d'autre. La table de decision ne tire pas
		 * (ecart n°1) ; ce que la reference y tire est releve dans
		 * `docs/migration/phase3/P3_RNG_RELEVE.md`.
		 *
		 * L'etat est celui de `sim.rng.state()` / `setState()` : ce que la sauvegarde JS
		 * range dans `save.rng` (save.js l. 66) et que `deserialize` restaure (l. 168).
		 */
		uint32 GetSimRngState() const { return VillageRng.GetState(); }
		void SetSimRngState(uint32 State) { VillageRng.SetState(State); }

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
		 * `chooseGoal(sim, npc)` tout de suite (perception-explore-001) : pour les tests qui
		 * rejouent une decision mesuree de la reference et comptent ses tirages `sim.rng`.
		 */
		void ChooseGoalNow(const FString& NpcId);

		/**
		 * La chance de reconsideration de `updateNpc` (reconsider-001) pour cet habitant, a cet
		 * instant, avec ce `thinkDt` : `committedReconsiderChance(phaseReconsiderChance(
		 * needsReconsiderChance(npc, dt)))`. Lecture pure : ne tire pas, ne synchronise pas la phase.
		 * Pour les tests qui rejouent les tirages mesures de la reference.
		 */
		double ReconsiderChanceNow(const FString& NpcId, double ThinkDt) const;

		/**
		 * `rollCraftMiss(sim, npc, craftId)` (craftMiss.js l. 75, chat-on-haul-001) : si la porte
		 * l'ouvre et que la chance est positive, UN tirage du flux partage ; rate si tirage < chance,
		 * et le rate est estampille sur l'habitant (`stampCraftMiss`). Generique : `CraftId` est le
		 * profil (`farm`, `build`, `chop`, `quarry`, `tend`…). Maitrise des techniques : 0 (ecart n°10).
		 */
		bool RollCraftMiss(FNpc& Npc, const FString& CraftId);

		/**
		 * `applyCraftMissRecovery(npc, sim, craftId, swingPeriodFor)` : `Period` est
		 * `swingPeriodFor(npc, craftId)` calcule AVANT d'incrementer les coups de la session.
		 */
		void ApplyCraftMissRecovery(FNpc& Npc, double Period);

		/** La meme chance a l'instant `At` (tests de rejeu : l'instant mesure, pas l'horloge du village). */
		double ReconsiderChanceAt(const FNpc& Npc, double At, double ThinkDt) const;

		/** La phase PERSONNELLE de l'habitant maintenant (`villagePhaseFor(sim, npc)`). */
		AnastasisRhythm::EPhase PersonalPhaseOf(const FNpc& Npc) const;

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

		/**
		 * Projection canonique de PARITE JS (FStateWriter) : batiments puis acteurs, dans l'ordre. Perimetre
		 * fige, une partie de l'etat seulement : ce n'est pas un oracle d'egalite d'etat. Pour savoir si deux
		 * villages sont le meme, ou si quelque chose a ecrit dedans : StateDigest().
		 */
		uint64 Digest() const;

		/**
		 * STATE_ORACLE_001 -- empreinte d'ETAT : tout ce qui decide du futur du village (sim.rng, meteo, joueur,
		 * compteurs, memoire, liens...). Oracle des tests de determinisme et de non-ecriture ; pas la parite.
		 * Ce qui n'y entre pas est classe dans tools/migration/state-fields.json.
		 */
		uint64 StateDigest() const;

		/**
		 * SAVE_STATE_001 -- le parcours d'etat du village (Core/AnastasisStateArchive.h) : le meme hache
		 * (`StateDigest`), sauve et relit. Appele par FAnastasisSimulation::SaveState / LoadState ; en
		 * lecture, le village doit etre lie au monde (`Bind`) et `AfterStateLoaded` suit.
		 */
		void ArchiveState(AnastasisArchive::FStateArchive& Ar);

		/** Apres une lecture : vide les caches classes `cache:` (state-fields.json) pour qu'ils se refassent. */
		void AfterStateLoaded();


	private:
		void UpdateNpc(FNpc& Npc, double Dt);
		bool ProgressMaterialCourier(FNpc& Npc, double Dt);
		void ChooseGoal(FNpc& Npc);
		/**
		 * La passe collective de fin d'`adultScores` (npc.js l. 1205-1258) : urgence collective, plancher
		 * collectif apres `workFactor` (corvee de bois comprise), puis rush famine.
		 */
		void ApplyCollectivePass(const FNpc& Npc, const FCollectiveDecision& Collective,
			TArray<TPair<FString, double>>& Rows, FDecisionTrace& Trace) const;
	public:
		/**
		 * Ce que le planificateur dit pour cet habitant. Sans planificateur branche : vide. Un test
		 * peut le remplacer (`CollectiveDecisionOverride`).
		 */
		FCollectiveDecision CollectiveDecisionOf(const FNpc& Npc);
		/**
		 * La colonie d'une sauvegarde reprise par le harnais (planner-wiring-001) : tresor et moral,
		 * priorites collectives, rapport de stock, charte, doctrine, stock du marche, rayon degage du camp.
		 * Un village cree par le C++ n'en a pas : il ne consulte pas le planificateur (ecart n°27).
		 */
		void RestoreColonyForHarness(const AnastasisPlanner::FColonyState& InColony, const AnastasisPlanner::FOrderedMap& InMarketStock,
			const TOptional<double>& InSettlementClearRadius, const TOptional<double>& InTreasury);
		/** `colony.treasury` (le `liquidity` de `buildScore`) ; vide sans colonie. */
		TOptional<double> GetColonyTreasury() const { return ColonyTreasury; }
		bool HasColony() const { return bHasColony; }
		const AnastasisPlanner::FColonyState& GetColony() const { return Colony; }
		/** La vue du village que lit le planificateur (`sim` vu par `collectivePriorities.js`). */
		AnastasisPlanner::FPlannerVillage BuildPlannerView();
		/** Ce que le planificateur a ecrit dans la vue, rendu au village, dans l'ordre de sa fiche. */
		void WritePlannerView(const AnastasisPlanner::FPlannerVillage& View);
		TFunction<FCollectiveDecision(const FNpc&)> CollectiveDecisionOverride;
	private:
		bool FoodSupplyTarget(FNpc& Npc, FPoint& Out, FString& Source);
		bool PerformFoodSupply(FNpc& Npc);
		bool HasKnownFoodSource(const FNpc& Npc) const;
		const FBuilding* KnownFoodDepot(const FNpc& Npc) const;
		void CommitGoal(FNpc& Npc, const FString& Next, FDecisionTrace& Trace, const TArray<TPair<FString, double>>* Scores = nullptr);
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
		/**
		 * `rhythmBias + statusBias + lifestyleBias` (adultScores, le terme `score.rhythm_status_lifestyle`)
		 * pour un but : la phase personnelle, le statut, le mode de vie a l'heure du village.
		 */
		double RhythmStatusLifestyle(const FNpc& Npc, AnastasisRhythm::EPhase Phase, const AnastasisRhythm::FPhaseSubject& Subject, const FString& Goal) const;
		/** `statusBias(npc, goal)` : misere (moral, faim, or sans toit) ou aisance (or, reputation). */
		static double StatusBias(const FNpc& Npc, const FString& Goal);
		/** `natureGoalBias(npc, goal)` ; sans nature, 0 (nature moyenne). */
		static double NatureGoalBiasOf(const FNpc& Npc, const FString& Goal);
		/** `setActivity(sim, npc, activity)` : l'activite, et l'heure ou elle a change. */
		void SetActivity(FNpc& Npc, const FString& Activity);
		/** `captureGoalExplain(sim, npc, scores, extrasFor)` : les trois premieres lignes et leur cause. */
		void CaptureGoalExplain(FNpc& Npc, const TArray<TPair<FString, double>>& Scores);
		/** `stampStreetDecision(sim, npc, scores, previousGoal)`. */
		void StampStreetDecision(FNpc& Npc, const TArray<TPair<FString, double>>& Scores, const FString& PreviousGoal);
		void FailHungerAction(FNpc& Npc, const FString& Reason, const FString& ExcludedType);
		/** ecart n°48 : une maison de famille achevee lui revient ; le chef retient qui l'a aidee. */
		void SettleFamilyHouse(FBuilding& Site);
		/** `shareEpisodes(sim, a, b)` dans `spreadRumorExchange` : chacun raconte, dans cet ordre. Rend le nombre retenu. */
		int32 ShareEpisodes(FNpc& A, FNpc& B);
		/** `tellEpisodes(sim, from, to)`. */
		int32 TellEpisodes(FNpc& From, FNpc& To);
		/** `createGossipEpisode` puis `commitReceivedEpisodeBelief` : l'autre retient la version qu'on lui a racontee. */
		bool ReceiveEpisode(FNpc& From, FNpc& To, const AnastasisEpisodes::FEpisode& Source, const AnastasisEpisodes::FEpisode& Retold);
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
		/** `waitingActivity` de la reference : le `kind` que `notePlaceUse` recoit en attendant. */
		FString JsWaitingActivity(const FNpc& Npc) const;
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
		void UpdateInside(FNpc& Npc, double Dt);
		/**
		 * `notePlaceUse(actor, kind, amount)` (act-gate-001) : le batiment ou l'habitant agit
		 * (dedans, sinon le plus proche, sinon son poste pour un geste de travail) entre dans sa
		 * memoire des lieux ; un geste de travail compte au `laborToday` du poste.
		 */
		void NotePlaceUse(FNpc& Npc, const FString& Kind, double Amount);
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
		/** `npc.path = null ; npc.pathCooldown = Cooldown` — ce que la reference ecrit a la plupart de ses sites. */
		static void DropPath(FNpc& Npc, double Cooldown);
		/** `syncNavigationFromActor(actor)`. */
		static void SyncNavigationFromActor(FNpc& Npc);
		/** `requestPath(sim, actor, target, options)` par le service ; rend vrai la ou le JS rend un chemin. */
		bool RequestPathFor(FNpc& Npc, const FPoint& Target, bool bAllowBlockedTarget);
		/** `processNavQueue(sim)`. */
		void ProcessNavQueue();
		/** `steerAroundBlock(actor, target)`. */
		FPoint SteerAroundBlock(const FNpc& Npc, const FPoint& Target) const;
		/** `doorQueueWaypoint(sim, actor, target, waypoint)` (crowdNav.js) : pose le role et le rang. */
		FPoint DoorQueueWaypoint(FNpc& Npc, const FPoint& Target, const FPoint& Waypoint);
		/** `movementSpeedFactor(sim, actor, target)`. */
		double MovementSpeedFactor(FNpc& Npc, const FPoint* Target);
		/** `recordPassage(actor)` : un passage de plus (plafond 180, f32), ni sur l'eau ni sur un batiment ; toutes les 0,85 s de marche. */
		void RecordPassage(const FNpc& Npc);
		/** Copie l'habitant dans sa vue du service, et l'inverse. */
		static void FillNavAgent(const FNpc& Npc, AnastasisNavService::FNavAgent& Agent);
		static void WriteBackNavAgent(const AnastasisNavService::FNavAgent& Agent, FNpc& Npc);
		void FlushNavAgents();

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
		/**
		 * `recordTalk(sim, speaker, listener, options)`. `KindOverride` = `options.kind` (sinon le lien),
		 * `AmbientChance` = `options.ambientChance` de la porte d'impulsion (`shouldSpeakNow`).
		 */
		void RecordTalk(FNpc& Speaker, FNpc& Listener, bool bContinue,
			TOptional<AnastasisBonds::EBondKind> KindOverride = {}, double AmbientChance = AnastasisBonds::SpeakWorthAmbientChance);
		/** `maybeChatOnHaul(sim, npc)` (npc.js l. 5503) : la causette au depot, apres une livraison. */
		void MaybeChatOnHaul(FNpc& Npc);
		/**
		 * `tellSpots(sim, from, to)` (memory.js l. 722) : un sens de `shareRumors`, actes crees PUIS
		 * deposes avant l'autre sens. Rend le nombre de gisements appris.
		 */
		int32 TellSpots(FNpc& From, FNpc& To);
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
		double SocialRowScore(const FNpc& Npc, const FString& Goal, double NeedScore, double PhaseBias, double Noise = 0.0) const;
		/** `sim.socialPos(npc)` : le premier batiment acheve qui rassemble, sinon l'origine. */
		bool SocialPos(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		bool SocializeTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		bool RelaxTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		/** Ligne `gatherFood` ou `deliver` d'adultScores pour un fermier, rythme compris. */
		double WorkRowScore(const FNpc& Npc, const FString& Goal, double PhaseBias, const FWorkRowContext& Work, double Noise = 0.0) const;
		void ScanTiles(FNpc& Npc, int32 CX, int32 CY, bool bForce);
		bool GatherTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		bool DeliverTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		/** `progressCraftGather` : 1 = working, 2 = done, 0 = false. */
		int32 ProgressCraftGather(FNpc& Npc);

		// Chantier (npc.js progressBuildWork, simulation.js workConstruction).
		/** `progressBuildWork` : 0 = echec, 1 = au travail, 2 = fini. */
		int32 ProgressBuildWork(FNpc& Npc, double Dt);
		/** ecart n°50 : le batiment commun qui manque (grenier, puits), et pourquoi ; vide si rien ne manque. */
		FString PickCommonBuilding(FString& OutCause);
		/** ecart n°50 : `findBuildSpot` (repli de la reference : 80 tirages autour du village). */
		bool FindBuildSpot(const FString& Type, FIntPoint& OutSpot);
		/** `constructionOpenSlots`, le grenier tenant lieu de ferme (ecart n°50). */
		int32 GrowthOpenSlots() const;
		bool WorkConstruction(FBuilding& Site, FNpc& Npc);
		FBuilding* BoundBuildSite(FNpc& Npc);
		FBuilding* PickBuildSite(FNpc& Npc);
		static bool SitePieceReady(const FBuilding& Site);
		void EnsureBuildSession(FNpc& Npc, const FBuilding& Site);
		/** `constructionAccessPoint(npc)` : le seuil du premier chantier ouvert. */
		bool ConstructionAccessPoint(FNpc& Npc, FPoint& OutTarget);
		/** La ligne `build` d'adultScores quand un chantier est ouvert. */
		double BuildRowScore(const FNpc& Npc, double PhaseBias, const FWorkRowContext& Work, double Noise = 0.0,
			const FCollectiveDecision& Collective = FCollectiveDecision()) const;

		/**
		 * `exploreTarget(sim, npc)` sur le flux partage (perception-explore-001) : la cible,
		 * et combien de tirages elle a coute. Voir `World/AnastasisExplore.h`.
		 */
		AnastasisExplore::FExploreResult ExploreTargetFor(const FNpc& Npc);

		/** Le monde de l'exploration : `IsBlocked`, `IsFootBlocked`, le centre du village. */
		AnastasisExplore::FExploreWorld ExploreWorld() const;

		/**
		 * Les conditions des trois bruits conditionnels de la table (`Ai/AnastasisGoalNoise.h`) :
		 * une ferme ou un batiment `nourrir` acheve ; un batiment `fabriquer` ou qui produit des
		 * outils acheve ; une famille a visiter.
		 */
		bool NoiseConditionHolds(const FNpc& Npc, AnastasisGoalNoise::ENoiseCondition Condition) const;
		/** `goalForWorkSession(npc)` : le but du metier de la session en cours. */
		static FString SessionGoalOf(const FNpc& Npc);
		/** Index de la premiere tuile de la ressource dans le 3 x 3 de l'habitant, -1 sinon. */
		int32 ResourceTileNear(const FNpc& Npc, AnastasisWorld::EResource Resource) const;
		AnastasisWorld::FTile LiveTile(int32 Index) const;
		/** `tile.amount -= taken` sur l'etat vivant (ou le registre food-supply). */
		void TakeFromTile(int32 Index, int32 Taken);
		/**
		 * `ensureCraftSession(sim, npc, craftId, tile)` : la session de coups ancree sur une tuile.
		 * Changer de metier (`farm` <-> `tend`) coute `craftToolSwitchSeconds`, puis `craftArriveSeconds`.
		 */
		void EnsureCraftSession(FNpc& Npc, int32 TileX, int32 TileY, const TCHAR* CraftId = TEXT("farm"));
		/**
		 * `sim.findTendFieldNear(origin, 8)` (help-farm-001) : la parcelle la plus utile a soigner autour
		 * d'un point ; `Actor` exclu du compte des travailleurs (nul quand l'origine est un batiment).
		 * Rend l'index de la tuile, ou INDEX_NONE.
		 */
		int32 FindTendFieldNear(double OriginX, double OriginY, const FNpc* Actor) const;
		/** `findTendFieldNear(npc) || findTendFieldNear(workplace acheve)`. */
		int32 FindTendFieldFor(const FNpc& Npc) const;
		/** `progressTendWork(sim, npc)` (help-farm-001) : 0 = echec, 1 = au travail, 2 = fini. */
		int32 ProgressTendWork(FNpc& Npc);
		/** La ligne `helpFarm` d'`adultScores` : `helpFarmScore * wf("helpFarm")`, puis la chaine des biais. */
		double HelpFarmRowScore(const FNpc& Npc, double PhaseBias, const FWorkRowContext& Work, double Noise,
			const FCollectiveDecision& Collective) const;
		/** `assignTarget` pour `helpFarm` : la parcelle faible, sinon `farmPos`. */
		bool HelpFarmTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		/** Bucheron qui n'est pas le porteur de materiaux : la recolte (ecart n°30) ne double jamais le porteur. */
		bool IsWoodHarvester(const FNpc& Npc) const;
		bool WoodTarget(const FNpc& Npc, FPoint& OutTarget, FString& OutSource) const;
		int32 ProgressWoodGather(FNpc& Npc);
		double WoodRowScore(const FNpc& Npc, double PhaseBias, const FWorkRowContext& Work, double Noise) const;
		FPoint FieldWorkTarget(FNpc& Npc, const AnastasisWorld::FTile& Tile);

		// --- Prevision de survie et risque spatial d'adultScores (resource-targets-001,
		// AnastasisVillageSpatialRisk.cpp). Ordre des appels = celui de la reference : leurs
		// `buildingAccessPoint` filtrent les seuils (paresseux) et posent `DestBuildingId`.

		/** `survivalForecastBias(sim, npc)` : replis manger, boire, dormir, puis la carte. */
		TArray<TPair<FString, double>> SurvivalForecastBiasFor(FNpc& Npc, bool bMealBlocked);
		/** `spatialRiskBiasMap(sim, npc)` : replis dormir, manger, boire, s'abriter, puis une cible par but. */
		TArray<TPair<FString, double>> SpatialRiskBiasMapFor(FNpc& Npc);
		/** `forecastEatTarget` : le foyer si le sac a des vivres, sinon le marche (prevu). */
		bool ForecastEatTarget(FNpc& Npc, FPoint& Out);
		/** `forecastDrinkTarget` = `drinkTarget` : croyance (ecart n°33), puits / berge, le camp. */
		bool ForecastDrinkTarget(FNpc& Npc, FPoint& Out);
		/** `forecastRestTarget` : lit connu (ecart n°33), le camp, le marche. */
		bool ForecastRestTarget(FNpc& Npc, FPoint& Out);
		/** `shelterRainAccess(sim, npc)`, sans les ecritures de l'acte d'abri. */
		bool ShelterRainAccess(FNpc& Npc, FPoint& Out);
		/** `bestKnownBed` reduit au lit du foyer seme par `seedHomeBedBelief` (ecart n°33). */
		bool KnownBedOf(const FNpc& Npc, FPoint& Out) const;
		/** `stableBuildingAccess(sim, npc, building)` : la cible en cours si elle reste un seuil libre. */
		bool StableBuildingAccess(FNpc& Npc, FBuilding& Building, FPoint& Out);
		/** `spatialRiskTargetForGoal(sim, npc, goal)`. */
		bool SpatialRiskTargetForGoal(FNpc& Npc, const FString& Goal, FPoint& Out);
		/** `recallOrSearch(sim, npc, resource)` : le gisement dont il se souvient, sinon `exploreTarget` (tire). */
		bool RecallOrSearch(FNpc& Npc, const FString& Resource, FPoint& Out);
		/** `recallResource(sim, npc, resource)` (memory.js) : le meilleur gisement connu. */
		bool RecallResource(const FNpc& Npc, const FString& Resource, FPoint& Out) const;
		/** `workCommutePos(actor, goal)`, pour les buts que le risque spatial lit. */
		bool WorkCommutePos(FNpc& Npc, const FString& Goal, FPoint& Out);
		/** `maintenancePos(actor)`. */
		bool MaintenancePos(FNpc& Npc, FPoint& Out);
		/** `farmPos(actor)`. */
		bool FarmPos(FNpc& Npc, FPoint& Out);
		/** `householdAidTarget(sim, npc)`, sans plan d'aide (ecart n°7). */
		bool HouseholdAidTarget(FNpc& Npc, FPoint& Out);

		uint32 ClaimedFieldPosts(const FNpc& Npc, int32 TileX, int32 TileY) const;
		/** `claimedFieldPosts(sim, ignoredNpc, tile)`, `Ignored` nul = personne n'est exclu. */
		uint32 ClaimedFieldPostsExcept(const FNpc* Ignored, int32 TileX, int32 TileY) const;
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
		TMap<int32, double> SoilWaterByTile;
		bool bSoilWaterEnabled = false;
		int64 RegrownFood = 0;
		/** `sim.spatial` : reconstruite une fois par tick, au debut de la boucle des habitants. */
		AnastasisSpatialGrid::FGrid Grid;
		/** `sim.life.recentVillageEmits` : instants des paroles de rue. */
		TArray<double> RecentVillageEmits;
		FAnastasisRng VillageRng = FAnastasisRng(0x6a09e667u);
		AnastasisNav::FNavGrid Nav;
		bool bTerrainTravelCostEnabled = false;
		FString MaterialCourierId;
		TFunction<double(double, double)> RainCanopyCover;
		/** `sim.traffic` : Float32Array de la reference, une case par tuile, plafonnee a 180 (`recordPassage`). Float : `decayFootTraffic` (x0,88 - 0,55). */
		TArray<float> Traffic;
		TMap<int32, AnastasisTraffic::FRoadTile> Roads;
		TMap<int32, double> RoadEfforts;
		bool bRoadEvolutionEnabled = false;
		/** ecart n°50 (valmire-grows-001). */
		bool bGrowthEnabled = false;
		int32 GrowthSitesOpened = 0;
		FString LastBuildDecision;
		/** ecart n°46 : biographies par identifiant de batiment ; un batiment demoli garde la sienne. */
		TMap<FString, FBuildingBiography> Biographies;
		bool bBiographyEnabled = false;
		int64 PassageCount = 0;
		/** ecart n°40 : le chantier d'ouverture a attribuer quand il s'acheve, et l'issue. `Bind` les vide. */
		FString OpeningSiteId;
		FOpeningHomeOutcome OpeningHome;
		int32 NavVersion = 0;
		/** `sim.navService` (nav-wiring-001). */
		AnastasisNavService::FNavService NavService;
		/** Vues des habitants prises par le service pendant un appel, recopiees a la sortie (`FlushNavAgents`). */
		TMap<FString, AnastasisNavService::FNavAgent> NavAgents;
		/** Ce que `findPath(sim, ...)` interroge, construit a la demande (references sur `Nav` et `World`). */
		mutable TSharedPtr<AnastasisPath::FWorldNavSource> NavSourceShared;
		FPoint Settlement;
		TOptional<double> MarketDx;
		TOptional<double> MarketDy;
		/** La colonie (planner-wiring-001) ; absente d'un village cree par le C++. */
		bool bHasColony = false;
		AnastasisPlanner::FColonyState Colony;
		AnastasisPlanner::FOrderedMap MarketStock;
		TOptional<double> SettlementClearRadius;
		TOptional<double> ColonyTreasury;
		/** `sim._npcCollectiveUrgency` : le cache d'urgence du planificateur, par seconde de jeu. */
		FString UrgencyBucket;
		TOptional<AnastasisPlanner::FUrgencySnapshot> UrgencyCache;
		int32 NextBuildingId = 0;
		TArray<FDeath> DeathLog;
		/** ecart n°48 : toutes les demandes d'aide et leurs reponses, dans l'ordre. */
		TArray<FHelpAnswer> HelpLog;
		/** ecart n°44 : les foyers poses par l'hote, et le compteur de leurs identifiants. */
		TArray<FFamily> Families;
		int32 NextFamilyId = 0;
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

	/**
	 * ABANDON_001 -- `stampHouseVacant` / `stampHouseOccupied` / `vacantAgeDays` / `vacantAgeBand`
	 * (collectivePriorities.js), portes tels quels. Une maison libre date de `VacantSinceDay` ; a defaut
	 * de `createdDay` (jamais occupee) ; a defaut du jour courant. Bandes : 0 frais, 1 vide (>= 6 j),
	 * 2 use (>= 18 j), 3 long abandon (>= 45 j).
	 */
	ANASTASISSIM_API void StampHouseVacant(FBuilding& Building, int32 Day);
	ANASTASISSIM_API void StampHouseOccupied(FBuilding& Building);
	ANASTASISSIM_API int32 VacantAgeDays(const FBuilding& Building, int32 Day);
	ANASTASISSIM_API int32 VacantAgeBand(const FBuilding& Building, int32 Day);

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
