// SAVE_STATE_001 (ecart n°45) -- un seul parcours de l'etat, trois usages.
//
// L'etat de la simulation est decrit UNE fois, par les fonctions `VisitState(FStateArchive&, T&)`
// (Private/Village/AnastasisVillageStateDigest.cpp, FAnastasisSimulation::VisitState). Le meme parcours :
//
//   - HACHE  : il nourrit un AnastasisDigest::FStateWriter, bit pour bit comme l'ancien hacheur
//              (FVillage::StateDigest, FAnastasisSimulation::StateDigest -- l'oracle de STATE_ORACLE_001) ;
//   - ECRIT  : il produit la sauvegarde, octets exacts de chaque champ (un double reste un double) ;
//   - LIT    : il relit cette sauvegarde dans les memes champs.
//
// Un champ oublie dans la sauvegarde serait donc aussi absent de l'oracle, et
// tools/migration/check-state-fields.mjs l'attrape deja : il n'y a pas de seconde liste a tenir.
//
// Format : chaque cle est ecrite et RELUE avec son nom, chaque nombre avec sa taille, chaque objet et
// chaque tableau avec un marqueur. Une sauvegarde qui ne suit pas le parcours actuel (champ ajoute,
// retire, deplace, type change) est refusee a la premiere difference, avec le chemin du champ fautif,
// au lieu d'etre lue de travers. Le numero de format (FAnastasisSimulation::SaveFormatVersion) monte a
// chaque changement du parcours.

#pragma once

#include "CoreMinimal.h"

#include <type_traits>

namespace AnastasisDigest { class FStateWriter; }
namespace AnastasisWorld { struct FTile; }

namespace AnastasisArchive
{
	enum class EMode : uint8
	{
		Hash,
		Save,
		Load,
	};

	class ANASTASISSIM_API FStateArchive
	{
	public:
		/** Hache dans `Writer` (ne modifie jamais l'etat visite). */
		static FStateArchive ForHash(AnastasisDigest::FStateWriter& Writer);
		/** Ecrit a la fin de `Bytes`. */
		static FStateArchive ForSave(TArray<uint8>& Bytes);
		/** Relit `Bytes` a partir de `Offset`. */
		static FStateArchive ForLoad(const TArray<uint8>& Bytes, int32 Offset = 0);

		EMode GetMode() const { return Mode; }
		bool IsHashing() const { return Mode == EMode::Hash; }
		bool IsSaving() const { return Mode == EMode::Save; }
		bool IsLoading() const { return Mode == EMode::Load; }

		/** Faux des la premiere erreur de lecture ; toute lecture suivante est sans effet. */
		bool Ok() const { return Error.IsEmpty(); }
		const FString& GetError() const { return Error; }
		/** Position de lecture (octets consommes). */
		int32 Tell() const { return Offset; }
		/** Lecture : tout a-t-il ete consomme ? */
		bool AtEnd() const { return LoadBytes && Offset == LoadBytes->Num(); }

		FStateArchive& BeginObject();
		FStateArchive& EndObject();
		FStateArchive& Key(const TCHAR* Name);

		/**
		 * Tableau de `Count` elements. En lecture, `Count` recoit le nombre sauve : c'est a l'appelant
		 * de dimensionner son conteneur avant de visiter les elements.
		 */
		FStateArchive& BeginArray(int32& Count);
		FStateArchive& EndArray();

		FStateArchive& Null();
		FStateArchive& Bool(bool& Value);
		FStateArchive& String(FString& Value);

		/** Entier, flottant ou enum : hache comme un nombre (double), sauve a sa taille exacte. */
		template <typename T>
		FStateArchive& Number(T& Value)
		{
			static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>, "Number : type arithmetique ou enum");
			if (Mode == EMode::Hash)
			{
				if constexpr (std::is_enum_v<T>)
				{
					HashNumber(static_cast<double>(static_cast<std::underlying_type_t<T>>(Value)));
				}
				else
				{
					HashNumber(static_cast<double>(Value));
				}
			}
			else
			{
				Raw(Marker::Number, &Value, static_cast<int32>(sizeof(T)));
			}
			return *this;
		}

		/**
		 * Grille de POD (taille du monde) : le hacheur n'en garde qu'une empreinte FNV-1a en hexadecimal
		 * (comme avant), la sauvegarde garde chaque octet.
		 */
		template <typename T>
		FStateArchive& Blob(TArray<T>& Items)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Blob : type trivialement copiable");
			return BlobBytes(Items.Num(), static_cast<int32>(sizeof(T)),
				[&Items](int32 Num) -> uint8* { Items.SetNumUninitialized(Num); return reinterpret_cast<uint8*>(Items.GetData()); },
				reinterpret_cast<const uint8*>(Items.GetData()));
		}

		/** Lecture : refuse si `Got` n'est pas `Want` (taille fixe d'un tableau C, par exemple). */
		void Expect(int32 Got, int32 Want, const TCHAR* What);

		/** Lecture : erreur explicite (valeur incoherente). Sans effet hors lecture. */
		void Fail(const FString& Message);

	private:
		struct Marker
		{
			static constexpr uint8 ObjectBegin = 0xB0;
			static constexpr uint8 ObjectEnd = 0xB1;
			static constexpr uint8 ArrayBegin = 0xA0;
			static constexpr uint8 ArrayEnd = 0xA1;
			static constexpr uint8 Key = 0xC0;
			static constexpr uint8 Null = 0xD0;
			static constexpr uint8 Bool = 0xD1;
			static constexpr uint8 String = 0xD2;
			static constexpr uint8 Number = 0xD3;
			static constexpr uint8 Blob = 0xD4;
		};

		explicit FStateArchive(EMode InMode) : Mode(InMode) {}

		void HashNumber(double Value);
		/** Sauve ou relit `Size` octets sous un marqueur et leur taille. */
		void Raw(uint8 Tag, void* Data, int32 Size);
		FStateArchive& BlobBytes(int32 Num, int32 ElementSize, TFunctionRef<uint8*(int32)> Resize, const uint8* Data);

		void WriteByte(uint8 Value);
		void WriteInt(int32 Value);
		void WriteBytes(const void* Data, int32 Size);
		bool ReadByte(uint8& Value);
		bool ReadInt(int32& Value);
		bool ReadBytes(void* Data, int32 Size);
		bool ExpectMarker(uint8 Tag);
		FString Where() const;

		EMode Mode;
		AnastasisDigest::FStateWriter* Writer = nullptr;
		TArray<uint8>* SaveBytes = nullptr;
		const TArray<uint8>* LoadBytes = nullptr;
		int32 Offset = 0;
		FString Error;
		/** Dernieres cles visitees, pour situer une erreur de lecture. */
		TArray<FString> Path;
	};

	/** Tableau d'elements visites par `VisitState(Ar, T&)`. En lecture, le tableau prend la taille sauvee. */
	template <typename T, typename FVisit>
	void VisitArray(FStateArchive& Ar, TArray<T>& Items, FVisit&& Visit)
	{
		int32 Count = Items.Num();
		Ar.BeginArray(Count);
		if (Ar.IsLoading())
		{
			if (!Ar.Ok()) return;
			Items.Reset();
			Items.SetNum(Count);
		}
		for (T& Item : Items)
		{
			if (!Ar.Ok()) return;
			Visit(Ar, Item);
		}
		Ar.EndArray();
	}

	/** TOptional<T> : la valeur ou Null. */
	template <typename T, typename FVisit>
	void VisitOptional(FStateArchive& Ar, TOptional<T>& Value, FVisit&& Visit)
	{
		// Un drapeau precede la valeur en sauvegarde ; le hacheur ne voit que la valeur ou Null (comme avant).
		bool bSet = Value.IsSet();
		if (!Ar.IsHashing())
		{
			Ar.Bool(bSet);
			if (Ar.IsLoading())
			{
				if (!Ar.Ok()) return;
				if (bSet) { if (!Value.IsSet()) Value.Emplace(); }
				else Value.Reset();
			}
		}
		if (bSet) Visit(Ar, Value.GetValue()); else if (Ar.IsHashing()) Ar.Null();
	}

	/** Une tuile du monde : le parcours du village (une seule liste des champs de FTile). */
	ANASTASISSIM_API void VisitTile(FStateArchive& Ar, AnastasisWorld::FTile& Tile);
}
