// Table d'entites — la semantique d'un tableau JavaScript, tenue exprès.
//
// Dans le simulateur de reference, `sim.actors` est un tableau ordonne: on
// ajoute a la fin (`actors.push`), on retire en decalant (`actors.splice(i, 1)`
// dans mortality.js). Cet ordre n'est pas un detail d'implementation — il est
// OBSERVABLE:
//
//   - la boucle de simulation itere `sim.actors` dans l'ordre, et deux
//     habitants qui convoitent la meme ressource sont departages par leur rang;
//   - `serialize(sim)` emet le tableau dans l'ordre, donc l'empreinte du
//     harnais differentiel en depend directement.
//
// D'ou cette table, et surtout d'ou l'ABSENCE de `RemoveAtSwap`. C'est le
// reflexe C++ correct — retirer en O(1) en echangeant avec le dernier — et
// c'est precisement ce qui ferait diverger le portage: mêmes habitants, meme
// etat, ordre different, et le harnais accuserait une divergence au premier
// tick ou deux PNJ se disputent quelque chose. Le cout d'un decalage est le
// prix de la parite; il se paiera ailleurs si un profil le reclame, pas ici.
//
// L'index par identifiant reproduit `_actorsById` de simulation.js: construit
// paresseusement, invalide a chaque mutation. Un index perime ne rend pas une
// erreur, il rend le MAUVAIS habitant — d'ou l'invalidation systematique plutot
// qu'une mise a jour incrementale qu'on finirait par oublier quelque part.

#pragma once

#include "CoreMinimal.h"

/**
 * Tableau ordonne + index par identifiant.
 *
 * `T` doit exposer un membre `Id` (FString), comme toutes les entites de la
 * reference: "npc-0", "building-3", "animal-12".
 */
template <typename T>
class TAnastasisEntityTable
{
public:
	/** L'ordre de ce tableau fait partie de l'etat. Ne pas le trier. */
	const TArray<T>& GetItems() const { return Items; }
	TArray<T>& GetItemsMutable() { InvalidateIndex(); return Items; }

	int32 Num() const { return Items.Num(); }
	bool IsEmpty() const { return Items.Num() == 0; }

	const T& operator[](int32 Index) const { return Items[Index]; }
	T& operator[](int32 Index) { InvalidateIndex(); return Items[Index]; }

	/** `actors.push(npc)` — toujours a la fin. */
	T& Add(const T& Item)
	{
		InvalidateIndex();
		return Items[Items.Add(Item)];
	}

	T& Add(T&& Item)
	{
		InvalidateIndex();
		return Items[Items.Add(MoveTemp(Item))];
	}

	/**
	 * `actors.splice(index, 1)` — decalage, l'ordre relatif survit.
	 *
	 * Volontairement pas de variante par echange: voir l'en-tete du fichier.
	 */
	void RemoveAt(int32 Index)
	{
		InvalidateIndex();
		Items.RemoveAt(Index, EAllowShrinking::No);
	}

	/** Retire par identifiant. Rend false si personne ne porte cet identifiant. */
	bool RemoveById(const FString& Id)
	{
		const int32 Index = IndexOfId(Id);
		if (Index == INDEX_NONE)
		{
			return false;
		}
		RemoveAt(Index);
		return true;
	}

	/** Rang courant d'un identifiant, ou INDEX_NONE. */
	int32 IndexOfId(const FString& Id) const
	{
		EnsureIndex();
		const int32* Found = IndexById.Find(Id);
		return Found ? *Found : INDEX_NONE;
	}

	const T* FindById(const FString& Id) const
	{
		const int32 Index = IndexOfId(Id);
		return Index == INDEX_NONE ? nullptr : &Items[Index];
	}

	T* FindById(const FString& Id)
	{
		const int32 Index = IndexOfId(Id);
		if (Index == INDEX_NONE)
		{
			return nullptr;
		}
		InvalidateIndex();
		return &Items[Index];
	}

	/**
	 * A appeler apres toute mutation faite hors de cette classe — un
	 * chargement, une reconstruction en bloc.
	 */
	void InvalidateIndex() const { bIndexDirty = true; }

private:
	void EnsureIndex() const
	{
		if (!bIndexDirty)
		{
			return;
		}
		IndexById.Reset();
		IndexById.Reserve(Items.Num());
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			IndexById.Add(Items[Index].Id, Index);
		}
		bIndexDirty = false;
	}

	TArray<T> Items;
	mutable TMap<FString, int32> IndexById;
	mutable bool bIndexDirty = true;
};
