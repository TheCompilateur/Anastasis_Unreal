// Trace d'empreintes — la moitié Unreal du harnais différentiel.
//
// Le harnais compare deux traces tick par tick et rend le premier tick
// divergent. Celle du JS existe et tourne (`tools/migration/emit-state-digests.mjs`);
// celle d'Unreal n'avait pas d'emetteur, parce que le C++ n'a pas encore d'etat
// complet a projeter.
//
// Cette classe est cet emetteur. Elle ne connait aucune simulation: on lui
// decrit un etat, section par section, et elle ecrit le JSONL que
// `compare-digests.mjs` relit. Brancher une vraie simulation dessus — celle de
// `Sim/AnastasisSimulation` quand elle sera integree — n'est plus qu'un
// cablage, et il n'y aura pas de second format a inventer.
//
// --- L'EMPREINTE GLOBALE ----------------------------------------------------
//
// Elle n'est PAS le hachage de l'etat entier: c'est celui de la suite
// (nom de section, empreinte de section), sections triees par nom. Deux etats
// qui ont les memes sections ont donc le meme global, et la relation reste
// verifiable a la main — ce qui compte le jour ou un rapport accuse une section
// et qu'on veut s'assurer que le global dit la meme chose.
//
// --- LE PIEGE DU NOMBRE -----------------------------------------------------
//
// `dt` et `dayLength` sont relus par le comparateur, qui REFUSE de comparer
// deux traces prises dans des conditions differentes. Ces nombres passent donc
// par JSON: ils doivent se reparser exactement au meme double des deux cotes.
// D'ou `%.17g`, qui garantit l'aller-retour — et non un `%f` qui tronquerait
// `1/30` en silence et ferait refuser la comparaison sans dire pourquoi.

#pragma once

#include "CoreMinimal.h"
#include "Core/AnastasisStateDigest.h"

namespace AnastasisTrace
{
	/** Conditions de la trace. Le comparateur relit `Spec`, `Seed`, `Dt`, `DayLength`. */
	struct FHeader
	{
		int32 Spec = 1;
		/** "unreal" ou "js" — informatif, jamais compare. */
		FString Source = TEXT("unreal");
		FString Ref = TEXT("(unreal)");
		FString RefHead = TEXT("");
		uint32 Seed = 0;
		double Dt = 1.0 / 30.0;
		int32 Ticks = 0;
		int32 Every = 1;
		double DayLength = 90.0;
	};

	/**
	 * Ecrivain de trace.
	 *
	 *   FTraceWriter Trace(Header);
	 *   Trace.BeginSample(Tick, Day, Time);
	 *   Trace.Section(TEXT("clock"), [&](AnastasisDigest::FStateWriter& W){ ... });
	 *   Trace.EndSample();
	 *   Trace.SaveToFile(Chemin);
	 *
	 * L'ordre des appels a `Section` est libre: les sections sont triees par nom
	 * avant d'etre ecrites, comme le fait le cote JS. Un portage qui decrirait
	 * ses champs dans un autre ordre rendrait donc la meme trace.
	 */
	class ANASTASISSIM_API FTraceWriter
	{
	public:
		explicit FTraceWriter(const FHeader& InHeader);

		void BeginSample(int32 Tick, int32 Day, double Time);

		/** Decrit une section; son empreinte est calculee immediatement. */
		void Section(const FString& Name, TFunctionRef<void(AnastasisDigest::FStateWriter&)> Describe);

		void EndSample();

		/** Nombre d'echantillons clos. */
		int32 NumSamples() const { return SampleCount; }

		/** La trace entiere, une ligne JSON par echantillon, en-tete compris. */
		FString ToJsonl() const;

		/** Ecrit `ToJsonl()` en UTF-8 sans BOM — ce que `JSON.parse` attend. */
		bool SaveToFile(const FString& Path) const;

	private:
		struct FSection
		{
			FString Name;
			FString Hex;
		};

		FHeader Header;
		TArray<FString> Lines;
		TArray<FSection> Pending;
		int32 PendingTick = 0;
		int32 PendingDay = 0;
		double PendingTime = 0.0;
		bool bInSample = false;
		int32 SampleCount = 0;
	};

	/** Decimal qui se reparse au bit pres — `%.17g`, plus le cas de l'entier. */
	ANASTASISSIM_API FString JsonNumber(double Value);

	/** Chaine JSON echappee, guillemets compris. */
	ANASTASISSIM_API FString JsonString(const FString& Value);
}
