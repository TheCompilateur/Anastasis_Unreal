// LECTEUR DE SAUVEGARDE JS — l'etat de depart commun du harnais.
//
// Mission sim-state-reader-001 (P3_PLAN.md, jalon A, vague 4). Un scenario du
// harnais est une sauvegarde JS (`serialize` de src/sim/save.js, fichier de
// tools/migration/scenarios/). Ce lecteur la charge en etat C++ et sait la
// REPROJETER sur les sections de `serialize`: c'est la preuve qu'un etat lu n'a
// rien perdu — meme empreinte qu'au tick 0 cote JS, section par section.
//
// Format JS lu, jamais ecrit (P2_MODELE_DONNEES.md, decision 2): c'est un
// instrument du harnais, pas un format de jeu.
//
// CE QUI EST LU EN ETAT C++ — les sections du perimetre du premier scenario
// (`endurance`): graine, etat `rng`, carte, horloge, `tileDiff`, batiments,
// habitants, `mealReservations`.
//
//   - Le monde est REGENERE (`GenerateWorld(graine, w, h)`), puis `tileDiff`
//     est applique comme `applyTileDiff` le fait. Les champs de tuile que FTile
//     n'a pas (routes, pont, couronne, clairiere, place de colonisation) sont
//     tenus a cote (`FTileExtra`).
//   - Batiments et habitants: les champs que FBuilding et FNpc portent sont lus
//     en champs C++ (liste dans le .cpp). Un habitant JS en a 105 au premier
//     niveau, dont des dizaines d'objets; FNpc n'en porte qu'une partie.
//
// CE QUI N'EST PAS LU: le reste de chaque entite, et les sections hors
// perimetre. Ils sont gardes tels quels (`Source`) pour que la projection les
// rende — c'est de la RECOPIE, pas de la lecture, et le dire est le contrat:
//
//   - `tileDiff` est RECALCULE depuis le monde C++, contre une generation
//     vierge: rien n'y est recopie.
//   - Pour un batiment, un habitant ou une reservation, la projection repart
//     de l'objet d'origine (meme identifiant) et y REECRIT chaque champ lu,
//     depuis la valeur C++. Un champ lu faux se voit; un champ non lu reste
//     celui du depart — fige: tant que la simulation C++ ne le fait pas
//     evoluer, l'empreinte le montrera des que le JS, lui, le change.
//
// Un champ que le lecteur ne sait pas representer fait ECHOUER la lecture
// (nombre non entier la ou le C++ tient un entier, `inside` non nul…): un
// lecteur qui arrondit en silence rendrait le harnais menteur.

#pragma once

#include "CoreMinimal.h"
#include "Core/AnastasisJson.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisEntityTable.h"
#include "World/AnastasisWorld.h"

namespace AnastasisJsSave
{
	/** Les champs de tuile que `tileDiff` decrit et que `FTile` n'a pas. */
	struct FTileExtra
	{
		bool bBridge = false;
		/** Vide = null. */
		FString RoadClass;
		FString RoadSurface;
		bool bHasRoadBuiltDay = false;
		int32 RoadBuiltDay = 0;
		bool bHasRoadBuildEffort = false;
		double RoadBuildEffort = 0.0;
		bool bHasRoadUpgradeEffort = false;
		double RoadUpgradeEffort = 0.0;
		bool bHasRoadPaveEffort = false;
		double RoadPaveEffort = 0.0;
		/** `tile.clearing` ; 0 dans la generation (pristineWorld.js). */
		double Clearing = 0.0;
		bool bColonizationPad = false;
		/**
		 * `tile.crown`. Le JS pose la couronne du village au chargement
		 * (`stampVillageCrown`) avant le diff; non porte ici: hors diff, 0.
		 */
		double Crown = 0.0;
	};

	/** `sim.mealReservations` serialise: `{ seq, reservations }`, ou null. */
	struct FMealLedger
	{
		bool bNull = false;
		int32 Seq = 0;
		TArray<AnastasisVillage::FMealReservation> Reservations;
	};

	/** L'etat lu. */
	struct FState
	{
		uint32 Seed = 0;
		/** `sim.rng.state()` — l'etat mulberry32. */
		uint32 RngState = 0;
		int32 W = 0;
		int32 H = 0;
		double Time = 0.0;
		int32 Day = 1;

		/** Generation de la graine, puis `tileDiff` applique. */
		AnastasisWorld::FWorld World;
		/** Indexe comme `World.Tiles`. */
		TArray<FTileExtra> TileExtras;

		TAnastasisEntityTable<AnastasisVillage::FBuilding> Buildings;
		TAnastasisEntityTable<AnastasisVillage::FNpc> Actors;
		FMealLedger Meals;

		/** La sauvegarde d'origine: base des projections pour ce qui n'est pas lu. */
		AnastasisJson::FValue Source;
	};

	/** Sections que `Project` sait rendre depuis l'etat C++. */
	ANASTASISSIM_API const TArray<FString>& PortedSections();

	/** Lit `Save` (l'objet de `serialize`). Rend false et un message precis sur toute entree non representable. */
	ANASTASISSIM_API bool Read(const AnastasisJson::FValue& Save, FState& Out, FString& OutError);

	/** Projette une section de `PortedSections()` depuis l'etat C++. */
	ANASTASISSIM_API bool Project(const FState& State, const FString& Section, AnastasisJson::FValue& Out, FString& OutError);
}
