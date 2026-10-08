#pragma once

// familles-feu-001 -- la base de repliques des habitants, en francais.
//
// Deux fichiers, lus ensemble (Content/Anastasis/Dialogue/) :
//   repliques-reference.json  recopie de la reference JS par tools/migration/gen-talk-lines.mjs (sans accents)
//   repliques-valmire.json    ecrit a la main pour ANASTASIS : le moine au feu, l'aide, la vie de tous les jours
// Une « situation » (pool) est un nom : `feu.moine.question_ville`, `talkCatalog.BOND_LINES.friend`. Une
// replique s'y choisit par une cle (qui parle, a qui, quand) : jamais par le flux aleatoire de la
// simulation, comme `hashTalk` de la reference. Ce module ne lit ni n'ecrit la simulation.

#include "CoreMinimal.h"

namespace AnastasisDialogue
{
	class ANASTASIS_UNREALV2_API FLibrary
	{
	public:
		/** Ajoute les pools d'un fichier au format `{ "data": { "<pool>": ["...", ...] } }`. Rend faux si illisible. */
		bool AddJson(const FString& Json, FString& OutError);

		/** Charge les deux fichiers livres. Rend le nombre de fichiers lus. */
		int32 LoadDefaults(FString& OutError);

		const TArray<FString>* Pool(const FString& Name) const { return Pools.Find(Name); }
		bool HasPool(const FString& Name) const { return Pools.Contains(Name); }
		int32 PoolCount() const { return Pools.Num(); }
		int32 LineCount() const;

		/**
		 * Une replique du pool, choisie par `Key` (meme cle, meme replique), trous remplaces : `{nom}` -> Holes["nom"].
		 * Vide si le pool n'existe pas.
		 */
		FString Pick(const FString& Name, uint32 Key, const TMap<FString, FString>& Holes = TMap<FString, FString>()) const;

		/** La base livree, chargee une fois. Jamais nulle ; vide si les fichiers manquent (un avertissement au log). */
		static const FLibrary& Get();

		/** Cle de choix stable : graine de la partie, qui parle, la situation, et un rang. */
		static uint32 KeyOf(uint32 Seed, const FString& Speaker, const FString& Situation, int32 Rank = 0);

	private:
		TMap<FString, TArray<FString>> Pools;
	};
}
