#include "Core/AnastasisStateTrace.h"

#include "Misc/FileHelper.h"

namespace AnastasisTrace
{
	FString JsonNumber(double Value)
	{
		// Un entier s'ecrit en entier: `90` et non `90.000000000000000`. Ce
		// n'est pas cosmetique — c'est ce que rend `JSON.stringify`, et une
		// trace qui se relit a l'oeil a cote d'une trace JS vaut mieux qu'une
		// qui oblige a parser pour comparer.
		if (FMath::IsFinite(Value) && Value == FMath::TruncToDouble(Value)
			&& FMath::Abs(Value) < 1.0e15)
		{
			return FString::Printf(TEXT("%lld"), static_cast<int64>(Value));
		}

		// Sinon, la precision qui garantit l'aller-retour. Le comparateur relit
		// des doubles, pas des chaines: ce qui compte est que `JSON.parse` de
		// cette ecriture rende exactement ce double.
		return FString::Printf(TEXT("%.17g"), Value);
	}

	FString JsonString(const FString& Value)
	{
		FString Out = TEXT("\"");
		for (const TCHAR C : Value)
		{
			switch (C)
			{
			case TEXT('"'): Out += TEXT("\\\""); break;
			case TEXT('\\'): Out += TEXT("\\\\"); break;
			case TEXT('\n'): Out += TEXT("\\n"); break;
			case TEXT('\r'): Out += TEXT("\\r"); break;
			case TEXT('\t'): Out += TEXT("\\t"); break;
			default:
				if (C < 0x20)
				{
					Out += FString::Printf(TEXT("\\u%04x"), static_cast<int32>(C));
				}
				else
				{
					Out.AppendChar(C);
				}
				break;
			}
		}
		Out += TEXT("\"");
		return Out;
	}

	FTraceWriter::FTraceWriter(const FHeader& InHeader)
		: Header(InHeader)
	{
	}

	void FTraceWriter::BeginSample(int32 Tick, int32 Day, double Time)
	{
		checkf(!bInSample, TEXT("BeginSample sans EndSample"));
		Pending.Reset();
		PendingTick = Tick;
		PendingDay = Day;
		PendingTime = Time;
		bInSample = true;
	}

	void FTraceWriter::Section(
		const FString& Name,
		TFunctionRef<void(AnastasisDigest::FStateWriter&)> Describe)
	{
		checkf(bInSample, TEXT("Section hors d'un echantillon"));
		AnastasisDigest::FStateWriter Writer;
		Describe(Writer);
		Pending.Add(FSection{ Name, Writer.Hex() });
	}

	void FTraceWriter::EndSample()
	{
		checkf(bInSample, TEXT("EndSample sans BeginSample"));

		// Tri par nom, comme le `.sort()` du cote JS: l'ordre dans lequel
		// l'appelant a decrit ses sections ne doit pas entrer dans l'empreinte.
		Pending.Sort([](const FSection& A, const FSection& B)
		{
			return A.Name.Compare(B.Name, ESearchCase::CaseSensitive) < 0;
		});

		// Global = empreinte de la suite (nom, empreinte de section).
		AnastasisDigest::FStateWriter GlobalWriter;
		for (const FSection& S : Pending)
		{
			GlobalWriter.String(S.Name);
			GlobalWriter.String(S.Hex);
		}

		FString Sections;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			if (Index > 0)
			{
				Sections += TEXT(",");
			}
			Sections += JsonString(Pending[Index].Name) + TEXT(":") + JsonString(Pending[Index].Hex);
		}

		Lines.Add(FString::Printf(
			TEXT("{\"t\":%d,\"day\":%d,\"time\":%s,\"g\":%s,\"s\":{%s}}"),
			PendingTick,
			PendingDay,
			*JsonNumber(PendingTime),
			*JsonString(GlobalWriter.Hex()),
			*Sections));

		Pending.Reset();
		bInSample = false;
		SampleCount += 1;
	}

	FString FTraceWriter::ToJsonl() const
	{
		checkf(!bInSample, TEXT("trace close au milieu d'un echantillon"));

		const FString HeaderLine = FString::Printf(
			TEXT("{\"kind\":\"header\",\"spec\":%d,\"source\":%s,\"ref\":%s,\"refHead\":%s,")
			TEXT("\"seed\":%u,\"dt\":%s,\"ticks\":%d,\"every\":%d,\"dayLength\":%s,\"emittedAt\":%s}"),
			Header.Spec,
			*JsonString(Header.Source),
			*JsonString(Header.Ref),
			*JsonString(Header.RefHead),
			Header.Seed,
			*JsonNumber(Header.Dt),
			Header.Ticks,
			Header.Every,
			*JsonNumber(Header.DayLength),
			*JsonString(FDateTime::UtcNow().ToIso8601()));

		FString Out = HeaderLine;
		for (const FString& Line : Lines)
		{
			Out += TEXT("\n");
			Out += Line;
		}
		Out += TEXT("\n");
		return Out;
	}

	bool FTraceWriter::SaveToFile(const FString& Path) const
	{
		// UTF-8 sans BOM: `JSON.parse` de Node ne mange pas un BOM sur la
		// premiere ligne, et le comparateur lit ligne par ligne.
		return FFileHelper::SaveStringToFile(
			ToJsonl(),
			*Path,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}
