// EMETTEUR DE TRACE UNREAL — la moitie C++ du harnais differentiel.
//
// Mission sim-digest-emitter-001 (P3_PLAN.md, jalon A). Le harnais compare deux
// traces d'empreintes tick par tick (tools/migration/compare-digests.mjs). La
// trace JS vient de emit-state-digests.mjs ; celle-ci vient du C++ :
//
//   1. le scenario (sauvegarde JS) est lu (Harness/AnastasisJsSave.h) ;
//   2. l'etat lu est REPRIS par l'hote : monde fourni, horloge de la sauvegarde
//      (`FAnastasisSimulation::ResetFromWorld`), batiments, habitants et
//      registre des repas (`FVillage::RestoreForHarness`) ;
//   3. a chaque echantillon, l'etat VIVANT du C++ est projete sur les sections
//      du perimetre du scenario, hache comme `digestState`, et ecrit en JSONL au
//      format de l'emetteur JS.
//
// L'en-tete porte la graine, le pas, la longueur du jour, et le scenario (nom,
// empreinte, masques, perimetre) TELS QUE LE FICHIER LES DONNE : le comparateur
// refuse deux traces qui ne les partagent pas. Les masques decrivent la
// neutralisation cote JS ; cote C++, ce sont des systemes qui n'existent pas.
//
// Ce que la projection C++ ne sait pas dire, elle le RECOPIE de la sauvegarde
// (champs non lus d'un habitant, d'un batiment) : ces champs restent figes, et
// la trace le montrera des que le JS les fait bouger. `rng` en particulier : le
// C++ n'a pas de flux `sim.rng` (les rumeurs tirent sur un flux du village,
// ecart n°16) ; la section rend l'etat lu, fige.

#pragma once

#include "CoreMinimal.h"
#include "Core/AnastasisJson.h"
#include "Harness/AnastasisJsSave.h"

class FAnastasisSimulation;

namespace AnastasisHarnessTrace
{
	/** Ce qu'il faut savoir d'un scenario pour le rejouer et en tracer l'en-tete. */
	struct FScenarioInfo
	{
		FString Name;
		FString Empreinte;
		FString ReferenceCommit;
		double Dt = 0.0;
		FString DayDeferred;
		TArray<FString> Sections;
		AnastasisJson::FValue Masques;
	};

	/** Lit un fichier de scenario (tools/migration/scenarios/*.json) : en-tete et etat. */
	ANASTASISSIM_API bool LoadScenario(const FString& Path, FScenarioInfo& OutInfo, AnastasisJsSave::FState& OutState, FString& OutError);

	/** Reprend l'etat lu dans l'hote (monde, horloge, village, registre des repas). */
	ANASTASISSIM_API bool Restore(const AnastasisJsSave::FState& Read, FAnastasisSimulation& Sim, FString& OutError);

	/**
	 * L'etat vivant du C++, vu comme le lecteur l'aurait lu : base des projections.
	 * `InOut` doit avoir ete initialise a l'etat lu (monde, champs non lus) ; seul
	 * ce que la simulation C++ fait evoluer y est remplace.
	 */
	ANASTASISSIM_API void Snapshot(const FAnastasisSimulation& Sim, AnastasisJsSave::FState& InOut);

	/** Empreintes des sections demandees, et l'empreinte globale de `digestState` sur ces sections. */
	ANASTASISSIM_API bool DigestSections(const AnastasisJsSave::FState& State, const TArray<FString>& Sections,
		TMap<FString, uint64>& OutDigests, uint64& OutGlobal, FString& OutError);

	struct FRunResult
	{
		int32 Samples = 0;
		/** Empreintes du tick 0 (apres reprise, avant le premier pas). */
		TMap<FString, uint64> TickZero;
		/** Empreintes du dernier echantillon. */
		TMap<FString, uint64> Last;
		uint64 LastGlobal = 0;
	};

	/**
	 * Charge le scenario, le reprend, fait `Ticks` pas de `dt` du scenario, et
	 * ecrit la trace JSONL dans `OutPath` (un echantillon tous les `Every` ticks).
	 *
	 * Forage : si `DrillTick` >= 0, les sections projetees a ce tick sont aussi
	 * ecrites en JSON, a cote de la trace (`<trace>.tick<N>.json`), pour une
	 * comparaison champ par champ avec la reference (tools/migration/diff-states.mjs).
	 */
	ANASTASISSIM_API bool Run(const FString& ScenarioPath, const FString& OutPath, int32 Ticks, int32 Every,
		FRunResult& OutResult, FString& OutError, int32 DrillTick = -1);

	/** Chemin du forage d'une trace : `<trace sans extension>.tick<N>.json`. */
	ANASTASISSIM_API FString DrillPath(const FString& TracePath, int32 Tick);
}
