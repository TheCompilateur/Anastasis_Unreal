// JSON minimal — lire une sauvegarde JS sans perdre un bit.
//
// Le harnais differentiel lit l'etat JS (P2_MODELE_DONNEES.md, decision 2): un
// scenario est la sortie de `serialize`, ecrite en JSON. Ce lecteur existe pour
// une seule exigence, celle du harnais: relire chaque nombre a l'identique.
//
//   - Un nombre JSON produit par `JSON.stringify` est la plus courte ecriture
//     decimale qui redonne le double. Il est relu par `FCString::Atod`
//     (`wcstod` de l'UCRT, correctement arrondi) — jamais par une
//     accumulation de chiffres maison, qui perdrait le dernier bit. Le lecteur
//     verifie a chaque lecture que le separateur decimal est bien le point: une
//     culture qui lirait "0,5" fausserait tout en silence.
//   - Un objet garde l'ordre de ses cles tel que le texte les donne. L'empreinte
//     les trie de toute facon (state-digest.mjs) ; l'ordre est garde pour qu'une
//     projection reecrive un objet tel qu'il a ete lu.
//
// Pas de dependance au module `Json` d'Unreal: `AnastasisSim` ne depend que de
// Core et CoreUObject (PORTAGE.md), et la question du dernier bit ne se delegue
// pas a un analyseur dont on ne controle pas la lecture des nombres.

#pragma once

#include "CoreMinimal.h"

namespace AnastasisDigest { class FStateWriter; }

namespace AnastasisJson
{
	enum class EKind : uint8
	{
		Null,
		Bool,
		Number,
		String,
		Array,
		Object,
	};

	/**
	 * Une valeur JSON.
	 *
	 * Tableau: `Items`. Objet: `Keys[i]` -> `Items[i]`, dans l'ordre du texte.
	 * Deux tableaux paralleles plutot qu'un tableau de paires: un `TArray` de
	 * son propre type est le seul motif recursif que le moteur emploie lui-meme.
	 */
	struct ANASTASISSIM_API FValue
	{
		EKind Kind = EKind::Null;
		bool bBool = false;
		double Number = 0.0;
		FString String;
		TArray<FString> Keys;
		TArray<FValue> Items;

		static FValue MakeBool(bool bValue);
		static FValue MakeNumber(double Value);
		static FValue MakeString(const FString& Value);
		static FValue MakeArray();
		static FValue MakeObject();

		bool IsNull() const { return Kind == EKind::Null; }
		bool IsBool() const { return Kind == EKind::Bool; }
		bool IsNumber() const { return Kind == EKind::Number; }
		bool IsString() const { return Kind == EKind::String; }
		bool IsArray() const { return Kind == EKind::Array; }
		bool IsObject() const { return Kind == EKind::Object; }

		/** Champ d'un objet, ou nullptr (absent, ou pas un objet). */
		const FValue* Find(const FString& Key) const;
		FValue* Find(const FString& Key);

		/** Objet: remplace le champ s'il existe (a sa place), l'ajoute a la fin sinon. */
		void Set(const FString& Key, const FValue& Value);
	};

	/**
	 * Lit `Text`. Rend false et un message (avec la position) sur toute entree
	 * qui n'est pas du JSON strict: pas de commentaire, pas de virgule finale.
	 */
	ANASTASISSIM_API bool Parse(const FString& Text, FValue& Out, FString& OutError);

	/**
	 * Decrit `Value` a l'ecrivain d'etat, comme `Digest.value(v)` de
	 * state-digest.mjs: null, booleen, nombre (motif binaire), chaine,
	 * tableau (longueur puis elements), objet (cles triees par l'ecrivain).
	 */
	ANASTASISSIM_API void Write(AnastasisDigest::FStateWriter& Writer, const FValue& Value);

	/** `digestValue(v)`: l'empreinte d'une valeur isolee. */
	ANASTASISSIM_API uint64 DigestOf(const FValue& Value);
}
