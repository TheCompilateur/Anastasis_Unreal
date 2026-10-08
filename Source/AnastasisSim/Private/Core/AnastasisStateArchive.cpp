#include "Core/AnastasisStateArchive.h"

#include "Core/AnastasisStateDigest.h"

namespace AnastasisArchive
{
	FStateArchive FStateArchive::ForHash(AnastasisDigest::FStateWriter& InWriter)
	{
		FStateArchive Ar(EMode::Hash);
		Ar.Writer = &InWriter;
		return Ar;
	}

	FStateArchive FStateArchive::ForSave(TArray<uint8>& Bytes)
	{
		FStateArchive Ar(EMode::Save);
		Ar.SaveBytes = &Bytes;
		return Ar;
	}

	FStateArchive FStateArchive::ForLoad(const TArray<uint8>& Bytes, int32 InOffset)
	{
		FStateArchive Ar(EMode::Load);
		Ar.LoadBytes = &Bytes;
		Ar.Offset = InOffset;
		return Ar;
	}

	// --- Octets -------------------------------------------------------------------------------------------

	void FStateArchive::WriteByte(uint8 Value)
	{
		SaveBytes->Add(Value);
	}

	void FStateArchive::WriteInt(int32 Value)
	{
		// Petit-boutiste explicite : le fichier ne depend pas de la machine.
		for (int32 I = 0; I < 4; ++I) SaveBytes->Add(static_cast<uint8>((static_cast<uint32>(Value) >> (8 * I)) & 0xFF));
	}

	void FStateArchive::WriteBytes(const void* Data, int32 Size)
	{
		if (Size > 0) SaveBytes->Append(static_cast<const uint8*>(Data), Size);
	}

	bool FStateArchive::ReadByte(uint8& Value)
	{
		if (!Ok()) return false;
		if (Offset + 1 > LoadBytes->Num())
		{
			Fail(TEXT("fin de fichier inattendue"));
			return false;
		}
		Value = (*LoadBytes)[Offset++];
		return true;
	}

	bool FStateArchive::ReadInt(int32& Value)
	{
		uint32 Bits = 0;
		for (int32 I = 0; I < 4; ++I)
		{
			uint8 B = 0;
			if (!ReadByte(B)) return false;
			Bits |= static_cast<uint32>(B) << (8 * I);
		}
		Value = static_cast<int32>(Bits);
		return true;
	}

	bool FStateArchive::ReadBytes(void* Data, int32 Size)
	{
		if (!Ok()) return false;
		if (Size < 0 || Offset + Size > LoadBytes->Num())
		{
			Fail(FString::Printf(TEXT("fin de fichier inattendue (%d octets demandes)"), Size));
			return false;
		}
		if (Size > 0) FMemory::Memcpy(Data, LoadBytes->GetData() + Offset, Size);
		Offset += Size;
		return true;
	}

	bool FStateArchive::ExpectMarker(uint8 Tag)
	{
		const int32 At = Offset;
		uint8 Got = 0;
		if (!ReadByte(Got)) return false;
		if (Got != Tag)
		{
			Fail(FString::Printf(TEXT("marqueur 0x%02X attendu, 0x%02X lu a l'octet %d"), Tag, Got, At));
			return false;
		}
		return true;
	}

	FString FStateArchive::Where() const
	{
		FString Out;
		for (const FString& P : Path)
		{
			if (P.IsEmpty()) continue;
			if (!Out.IsEmpty()) Out += TEXT(".");
			Out += P;
		}
		return Out.IsEmpty() ? FString(TEXT("(racine)")) : Out;
	}

	void FStateArchive::Fail(const FString& Message)
	{
		if (Mode != EMode::Load || !Error.IsEmpty()) return;
		Error = FString::Printf(TEXT("%s : %s"), *Where(), *Message);
	}

	void FStateArchive::Expect(int32 Got, int32 Want, const TCHAR* What)
	{
		if (Mode == EMode::Load && Ok() && Got != Want)
		{
			Fail(FString::Printf(TEXT("%s : %d attendu, %d lu"), What, Want, Got));
		}
	}

	// --- Structure ----------------------------------------------------------------------------------------

	FStateArchive& FStateArchive::BeginObject()
	{
		switch (Mode)
		{
		case EMode::Hash: Writer->BeginObject(); break;
		case EMode::Save: WriteByte(Marker::ObjectBegin); break;
		case EMode::Load: ExpectMarker(Marker::ObjectBegin); break;
		}
		Path.Add(FString());
		return *this;
	}

	FStateArchive& FStateArchive::EndObject()
	{
		switch (Mode)
		{
		case EMode::Hash: Writer->EndObject(); break;
		case EMode::Save: WriteByte(Marker::ObjectEnd); break;
		case EMode::Load: ExpectMarker(Marker::ObjectEnd); break;
		}
		if (Path.Num() > 0) Path.Pop(EAllowShrinking::No);
		return *this;
	}

	FStateArchive& FStateArchive::Key(const TCHAR* Name)
	{
		if (Path.Num() > 0) Path.Last() = Name;
		switch (Mode)
		{
		case EMode::Hash:
			Writer->Key(Name);
			break;
		case EMode::Save:
		{
			const FTCHARToUTF8 Utf8(Name);
			WriteByte(Marker::Key);
			WriteInt(Utf8.Length());
			WriteBytes(Utf8.Get(), Utf8.Length());
			break;
		}
		case EMode::Load:
		{
			if (!ExpectMarker(Marker::Key)) break;
			int32 Len = 0;
			if (!ReadInt(Len)) break;
			if (Len < 0 || Len > 4096)
			{
				Fail(FString::Printf(TEXT("cle de longueur %d"), Len));
				break;
			}
			TArray<uint8> Raw;
			Raw.SetNumUninitialized(Len);
			if (!ReadBytes(Raw.GetData(), Len)) break;
			const FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Raw.GetData()), Len);
			const FString Got(Conv.Length(), Conv.Get());
			if (!Got.Equals(Name, ESearchCase::CaseSensitive))
			{
				Fail(FString::Printf(TEXT("cle « %s » attendue, « %s » lue (format different)"), Name, *Got));
			}
			break;
		}
		}
		return *this;
	}

	FStateArchive& FStateArchive::BeginArray(int32& Count)
	{
		switch (Mode)
		{
		case EMode::Hash:
			Writer->BeginArray(Count);
			break;
		case EMode::Save:
			WriteByte(Marker::ArrayBegin);
			WriteInt(Count);
			break;
		case EMode::Load:
		{
			int32 Got = 0;
			if (ExpectMarker(Marker::ArrayBegin) && ReadInt(Got))
			{
				// Garde-fou contre un fichier corrompu : jamais plus d'elements que d'octets restants.
				if (Got < 0 || Got > LoadBytes->Num() - Offset)
				{
					Fail(FString::Printf(TEXT("tableau de %d elements"), Got));
					Got = 0;
				}
				Count = Got;
			}
			else
			{
				Count = 0;
			}
			break;
		}
		}
		Path.Add(FString());
		return *this;
	}

	FStateArchive& FStateArchive::EndArray()
	{
		switch (Mode)
		{
		case EMode::Hash: Writer->EndArray(); break;
		case EMode::Save: WriteByte(Marker::ArrayEnd); break;
		case EMode::Load: ExpectMarker(Marker::ArrayEnd); break;
		}
		if (Path.Num() > 0) Path.Pop(EAllowShrinking::No);
		return *this;
	}

	// --- Valeurs ------------------------------------------------------------------------------------------

	FStateArchive& FStateArchive::Null()
	{
		switch (Mode)
		{
		case EMode::Hash: Writer->Null(); break;
		case EMode::Save: WriteByte(Marker::Null); break;
		case EMode::Load: ExpectMarker(Marker::Null); break;
		}
		return *this;
	}

	FStateArchive& FStateArchive::Bool(bool& Value)
	{
		switch (Mode)
		{
		case EMode::Hash:
			Writer->Bool(Value);
			break;
		case EMode::Save:
			WriteByte(Marker::Bool);
			WriteByte(Value ? 1 : 0);
			break;
		case EMode::Load:
		{
			uint8 B = 0;
			if (ExpectMarker(Marker::Bool) && ReadByte(B))
			{
				if (B > 1) Fail(FString::Printf(TEXT("booleen %d"), B));
				else Value = B == 1;
			}
			break;
		}
		}
		return *this;
	}

	FStateArchive& FStateArchive::String(FString& Value)
	{
		switch (Mode)
		{
		case EMode::Hash:
			Writer->String(Value);
			break;
		case EMode::Save:
		{
			const FTCHARToUTF8 Utf8(*Value);
			WriteByte(Marker::String);
			WriteInt(Utf8.Length());
			WriteBytes(Utf8.Get(), Utf8.Length());
			break;
		}
		case EMode::Load:
		{
			int32 Len = 0;
			if (!ExpectMarker(Marker::String) || !ReadInt(Len)) break;
			if (Len < 0 || Len > LoadBytes->Num() - Offset)
			{
				Fail(FString::Printf(TEXT("chaine de %d octets"), Len));
				break;
			}
			if (Len == 0)
			{
				Value.Reset();
				break;
			}
			const FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(LoadBytes->GetData() + Offset), Len);
			Value = FString(Conv.Length(), Conv.Get());
			Offset += Len;
			break;
		}
		}
		return *this;
	}

	void FStateArchive::HashNumber(double Value)
	{
		Writer->Number(Value);
	}

	void FStateArchive::Raw(uint8 Tag, void* Data, int32 Size)
	{
		if (Mode == EMode::Save)
		{
			WriteByte(Tag);
			WriteByte(static_cast<uint8>(Size));
			WriteBytes(Data, Size);
			return;
		}
		uint8 GotSize = 0;
		if (!ExpectMarker(Tag) || !ReadByte(GotSize)) return;
		if (GotSize != Size)
		{
			Fail(FString::Printf(TEXT("nombre de %d octets attendu, %d lu (type change)"), Size, GotSize));
			return;
		}
		ReadBytes(Data, Size);
	}

	FStateArchive& FStateArchive::BlobBytes(int32 Num, int32 ElementSize, TFunctionRef<uint8*(int32)> Resize, const uint8* Data)
	{
		switch (Mode)
		{
		case EMode::Hash:
		{
			AnastasisDigest::FFnv1a64 Hash;
			Hash.Bytes(Data, Num * ElementSize);
			Writer->String(AnastasisDigest::ToHex(Hash.Hash));
			break;
		}
		case EMode::Save:
			WriteByte(Marker::Blob);
			WriteInt(ElementSize);
			WriteInt(Num);
			WriteBytes(Data, Num * ElementSize);
			break;
		case EMode::Load:
		{
			int32 GotSize = 0;
			int32 GotNum = 0;
			if (!ExpectMarker(Marker::Blob) || !ReadInt(GotSize) || !ReadInt(GotNum)) break;
			if (GotSize != ElementSize)
			{
				Fail(FString::Printf(TEXT("grille d'elements de %d octets attendue, %d lue"), ElementSize, GotSize));
				break;
			}
			if (GotNum < 0 || static_cast<int64>(GotNum) * ElementSize > LoadBytes->Num() - Offset)
			{
				Fail(FString::Printf(TEXT("grille de %d elements"), GotNum));
				break;
			}
			uint8* Target = Resize(GotNum);
			ReadBytes(Target, GotNum * ElementSize);
			break;
		}
		}
		return *this;
	}
}
