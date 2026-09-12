#pragma once

#include "CoreMinimal.h"
#include "Core/AnastasisJsNumeric.h"

/**
 * Generateur pseudo-aleatoire deterministe (mulberry32) — port de src/sim/rng.js.
 *
 * Regle d'or de la simulation: meme seed => meme monde => meme deroulement.
 * C'est ce qui permet de rejouer un bug exactement et d'equilibrer sans hasard.
 *
 * L'etat interne est lisible et restaurable: sans ca, une partie rechargee
 * repartirait du debut de la suite aleatoire et divergerait de l'originale.
 *
 * NE PAS remplacer par FRandomStream. FRandomStream est un LCG different: il
 * produirait une suite valide mais AUTRE, et toute sauvegarde JS existante, tout
 * scenario de reference et tout test de non-regression deviendraient faux.
 */
struct ANASTASISSIM_API FAnastasisRng
{
	/** Etat 32 bits, identique au `s` de makeRng. */
	uint32 State = 0u;

	FAnastasisRng() = default;
	explicit FAnastasisRng(uint32 Seed)
		: State(Seed)
	{
	}

	/** Prochain double dans [0, 1). Equivalent de l'appel `rng()`. */
	double Next()
	{
		State = State + 0x6d2b79f5u;
		uint32 T = State;
		T = AnastasisJs::Imul(T ^ (T >> 15), T | 1u);
		T ^= T + AnastasisJs::Imul(T ^ (T >> 7), T | 61u);
		return static_cast<double>(T ^ (T >> 14)) / AnastasisJs::UInt32Modulo;
	}

	/** Etat serialisable (rng.state() cote JS). */
	uint32 GetState() const { return State; }

	/** Restauration au chargement (rng.setState() cote JS). */
	void SetState(uint32 Value) { State = Value; }

	/** rngRange: reel dans [Min, Max). */
	double Range(double Min, double Max)
	{
		return Min + Next() * (Max - Min);
	}

	/** rngInt: entier dans [Min, Max] — borne HAUTE INCLUSE, comme le JS. */
	int32 IntRange(int32 Min, int32 Max)
	{
		const double Value = Range(static_cast<double>(Min), static_cast<double>(Max) + 1.0);
		return static_cast<int32>(FMath::FloorToDouble(Value));
	}

	/**
	 * rngPick: element tire uniformement. Rend nullptr sur liste vide — le JS
	 * rendait `undefined`, silencieux; ici l'appelant est force de voir le cas.
	 */
	template <typename ElementType>
	const ElementType* Pick(const TArray<ElementType>& List)
	{
		if (List.Num() == 0)
		{
			return nullptr;
		}
		const int32 Index = static_cast<int32>(FMath::FloorToDouble(Next() * static_cast<double>(List.Num())));
		return &List[FMath::Clamp(Index, 0, List.Num() - 1)];
	}
};

/**
 * RNG de secours pour les chemins sans acces a sim.rng (reparation d'un npc
 * legacy, appel hors sim). Seed fixe et distinct du seed de partie: deterministe
 * en soi, jamais un rand() brut.
 *
 * Etat GLOBAL et mutable, exactement comme le `fallbackRng` du JS: deux chemins
 * qui y puisent s'influencent. C'est tolerable pour de la reparation, jamais
 * pour de la logique de jeu — celle-la prend le rng de la sim.
 */
ANASTASISSIM_API FAnastasisRng& GetAnastasisFallbackRng();

/** Seed du RNG de secours (0x9e3779b1), expose pour les tests de parite. */
inline constexpr uint32 AnastasisFallbackRngSeed = 0x9e3779b1u;
