// Empreinte canonique d'un etat de simulation — cote C++.
//
// Reimplementation de `tools/migration/state-digest.mjs`, qui EST la
// specification. Le test `Anastasis.Sim.Empreinte.Parite` prouve que les deux
// rendent les memes bits sur une batterie de valeurs generee depuis le JS.
//
// Pourquoi cette preuve est le coeur du harnais: le jour ou une trace Unreal et
// une trace JS divergeront au tick 812, il faudra pouvoir dire "la simulation a
// devie" sans avoir a se demander d'abord si c'est le hacheur qui compte
// autrement. Une empreinte non prouvee ne prouve rien.

#pragma once

#include "CoreMinimal.h"

namespace AnastasisDigest
{
	/** Etiquettes du flux — identiques a state-digest.mjs. */
	enum class ETag : uint8
	{
		Null = 0x01,
		False = 0x02,
		True = 0x03,
		Number = 0x04,
		String = 0x05,
		Array = 0x06,
		Object = 0x07,
		Undefined = 0x08,
	};

	/** FNV-1a 64 bits. Choisi parce qu'il se reecrit a l'identique partout. */
	struct ANASTASISSIM_API FFnv1a64
	{
		static constexpr uint64 OffsetBasis = 0xcbf29ce484222325ull;
		static constexpr uint64 Prime = 0x100000001b3ull;

		uint64 Hash = OffsetBasis;

		FORCEINLINE void Byte(uint8 Value)
		{
			Hash ^= static_cast<uint64>(Value);
			Hash *= Prime;
		}

		void Bytes(const uint8* Data, int32 Count);
	};

	/**
	 * Ecrivain d'etat: on lui decrit l'etat, il rend l'empreinte.
	 *
	 * Les paires d'un objet sont MISES EN ATTENTE puis triees par cle (unites
	 * de code UTF-16 croissantes) avant d'etre hachees. L'ordre d'insertion
	 * d'un objet JS depend de l'ordre d'ecriture du code, pas de l'etat: trier
	 * rend l'empreinte independante de la forme du code des deux cotes. Pour
	 * des cles ASCII — toutes celles de `serialize(sim)` le sont — cet ordre
	 * est aussi l'ordre des octets.
	 *
	 * `BeginArray` exige le nombre d'elements: le JS ecrit la longueur avant
	 * les elements, et une longueur rapiecee apres coup serait une occasion de
	 * plus de diverger en silence.
	 */
	class ANASTASISSIM_API FStateWriter
	{
	public:
		FStateWriter();

		FStateWriter& Null();
		FStateWriter& Bool(bool bValue);
		FStateWriter& Number(double Value);
		FStateWriter& String(const FString& Value);

		FStateWriter& BeginArray(int32 Count);
		FStateWriter& EndArray();

		FStateWriter& BeginObject();
		FStateWriter& Key(const FString& Name);
		FStateWriter& EndObject();

		/** Empreinte des octets ecrits jusqu'ici. */
		uint64 Digest() const;

		/** Meme empreinte, en 16 caracteres hexadecimaux minuscules. */
		FString Hex() const;

		/** Octets canoniques produits — utile pour diagnostiquer un ecart. */
		const TArray<uint8>& Bytes() const { return Buffers[0]; }

	private:
		struct FPending
		{
			FString KeyName;
			TArray<uint8> Encoded;
		};

		struct FObjectFrame
		{
			TArray<FPending> Pairs;
			FString OpenKey;
			bool bHasOpenKey = false;
		};

		/** Pile de tampons: [0] est la racine, le sommet est la cible courante. */
		TArray<TArray<uint8>> Buffers;
		TArray<FObjectFrame> Objects;
		TArray<int32> ArrayDepth;

		TArray<uint8>& Sink();
		void Tag(ETag Value);
		void Uint32(uint32 Value);
		void RawString(const FString& Value);
		void CloseOpenKey();
	};

	/** Empreinte d'une chaine hexadecimale, pour comparer a une trace JSONL. */
	ANASTASISSIM_API FString ToHex(uint64 Digest);
}
