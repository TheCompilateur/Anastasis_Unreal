#include "Core/AnastasisStateDigest.h"

namespace AnastasisDigest
{
	void FFnv1a64::Bytes(const uint8* Data, int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Byte(Data[Index]);
		}
	}

	FString ToHex(uint64 Digest)
	{
		return FString::Printf(TEXT("%016llx"), Digest);
	}

	FStateWriter::FStateWriter()
	{
		// Pile de tampons: le sommet recoit ce qu'on ecrit. Une valeur d'objet
		// s'ecrit dans son propre tampon parce qu'elle sera reordonnee avec les
		// autres paires avant d'etre hachee.
		Buffers.Emplace();
	}

	TArray<uint8>& FStateWriter::Sink()
	{
		return Buffers.Last();
	}

	void FStateWriter::Tag(ETag Value)
	{
		Sink().Add(static_cast<uint8>(Value));
	}

	void FStateWriter::Uint32(uint32 Value)
	{
		TArray<uint8>& Out = Sink();
		Out.Add(static_cast<uint8>(Value & 0xff));
		Out.Add(static_cast<uint8>((Value >> 8) & 0xff));
		Out.Add(static_cast<uint8>((Value >> 16) & 0xff));
		Out.Add(static_cast<uint8>((Value >> 24) & 0xff));
	}

	void FStateWriter::RawString(const FString& Value)
	{
		const FTCHARToUTF8 Utf8(*Value);
		Tag(ETag::String);
		Uint32(static_cast<uint32>(Utf8.Length()));
		Sink().Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	FStateWriter& FStateWriter::Null()
	{
		Tag(ETag::Null);
		return *this;
	}

	FStateWriter& FStateWriter::Bool(bool bValue)
	{
		Tag(bValue ? ETag::True : ETag::False);
		return *this;
	}

	FStateWriter& FStateWriter::Number(double Value)
	{
		Tag(ETag::Number);

		// NaN normalise: V8 peut porter plusieurs charges utiles pour un meme
		// NaN, le C++ aussi. Sans normalisation, deux "pas un nombre" egaux
		// pour la simulation donneraient deux empreintes differentes, et le
		// harnais accuserait une divergence qui n'existe pas.
		uint64 Bits;
		if (FMath::IsNaN(Value))
		{
			Bits = 0x7ff8000000000000ull;
		}
		else
		{
			FMemory::Memcpy(&Bits, &Value, sizeof(double));
		}

		TArray<uint8>& Out = Sink();
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Out.Add(static_cast<uint8>((Bits >> (Index * 8)) & 0xff));
		}
		return *this;
	}

	FStateWriter& FStateWriter::String(const FString& Value)
	{
		RawString(Value);
		return *this;
	}

	FStateWriter& FStateWriter::BeginArray(int32 Count)
	{
		Tag(ETag::Array);
		Uint32(static_cast<uint32>(Count));
		ArrayDepth.Add(Count);
		return *this;
	}

	FStateWriter& FStateWriter::EndArray()
	{
		checkf(ArrayDepth.Num() > 0, TEXT("EndArray sans BeginArray"));
		ArrayDepth.Pop();
		return *this;
	}

	FStateWriter& FStateWriter::BeginObject()
	{
		Objects.Emplace();
		return *this;
	}

	void FStateWriter::CloseOpenKey()
	{
		FObjectFrame& Frame = Objects.Last();
		if (!Frame.bHasOpenKey)
		{
			return;
		}
		checkf(Buffers.Num() > 1, TEXT("pile de tampons incoherente"));
		FPending Pair;
		Pair.KeyName = MoveTemp(Frame.OpenKey);
		Pair.Encoded = MoveTemp(Buffers.Last());
		Buffers.Pop();
		Frame.Pairs.Add(MoveTemp(Pair));
		Frame.bHasOpenKey = false;
	}

	FStateWriter& FStateWriter::Key(const FString& Name)
	{
		checkf(Objects.Num() > 0, TEXT("Key hors d'un objet"));
		CloseOpenKey();
		FObjectFrame& Frame = Objects.Last();
		Frame.OpenKey = Name;
		Frame.bHasOpenKey = true;
		Buffers.Emplace();
		return *this;
	}

	FStateWriter& FStateWriter::EndObject()
	{
		checkf(Objects.Num() > 0, TEXT("EndObject sans BeginObject"));
		CloseOpenKey();
		FObjectFrame Frame = MoveTemp(Objects.Last());
		Objects.Pop();

		// Tri par unites de code UTF-16 croissantes, comme le `.sort()` par
		// defaut de JavaScript. Pour des cles ASCII c'est aussi l'ordre des
		// octets; la distinction ne coute rien et evite d'avoir a s'en
		// souvenir le jour ou une cle cessera d'etre ASCII.
		Frame.Pairs.Sort([](const FPending& A, const FPending& B)
		{
			return A.KeyName.Compare(B.KeyName, ESearchCase::CaseSensitive) < 0;
		});

		Tag(ETag::Object);
		Uint32(static_cast<uint32>(Frame.Pairs.Num()));
		for (const FPending& Pair : Frame.Pairs)
		{
			RawString(Pair.KeyName);
			Sink().Append(Pair.Encoded.GetData(), Pair.Encoded.Num());
		}
		return *this;
	}

	uint64 FStateWriter::Digest() const
	{
		checkf(Objects.Num() == 0, TEXT("objet non ferme"));
		checkf(ArrayDepth.Num() == 0, TEXT("tableau non ferme"));
		FFnv1a64 Hasher;
		Hasher.Bytes(Buffers[0].GetData(), Buffers[0].Num());
		return Hasher.Hash;
	}

	FString FStateWriter::Hex() const
	{
		return ToHex(Digest());
	}
}
