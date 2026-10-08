#include "Sim/AnastasisDialogueLines.h"

#include "Anastasis_UnrealV2.h"
#include "Dom/JsonObject.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace AnastasisDialogue
{
	bool FLibrary::AddJson(const FString& Json, FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("json illisible");
			return false;
		}
		const TSharedPtr<FJsonObject>* Data = nullptr;
		if (!Root->TryGetObjectField(TEXT("data"), Data) || !Data)
		{
			OutError = TEXT("champ data absent");
			return false;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*Data)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
			if (!Entry.Value.IsValid() || !Entry.Value->TryGetArray(Items) || !Items) continue;
			TArray<FString>& Lines = Pools.FindOrAdd(Entry.Key);
			for (const TSharedPtr<FJsonValue>& Item : *Items)
			{
				FString Line;
				if (Item.IsValid() && Item->TryGetString(Line) && !Line.IsEmpty()) Lines.Add(Line);
			}
		}
		return true;
	}

	int32 FLibrary::LoadDefaults(FString& OutError)
	{
		int32 Loaded = 0;
		for (const TCHAR* File : { TEXT("repliques-reference.json"), TEXT("repliques-valmire.json") })
		{
			const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anastasis/Dialogue"), File);
			FString Json;
			FString Error;
			if (!FFileHelper::LoadFileToString(Json, *Path))
			{
				OutError += FString::Printf(TEXT("%s introuvable ; "), File);
				continue;
			}
			if (!AddJson(Json, Error))
			{
				OutError += FString::Printf(TEXT("%s : %s ; "), File, *Error);
				continue;
			}
			++Loaded;
		}
		return Loaded;
	}

	int32 FLibrary::LineCount() const
	{
		int32 Count = 0;
		for (const TPair<FString, TArray<FString>>& Entry : Pools) Count += Entry.Value.Num();
		return Count;
	}

	FString FLibrary::Pick(const FString& Name, uint32 Key, const TMap<FString, FString>& Holes) const
	{
		const TArray<FString>* Lines = Pools.Find(Name);
		if (!Lines || Lines->IsEmpty()) return FString();
		FString Line = (*Lines)[Key % static_cast<uint32>(Lines->Num())];
		for (const TPair<FString, FString>& Hole : Holes)
		{
			Line.ReplaceInline(*FString::Printf(TEXT("{%s}"), *Hole.Key), *Hole.Value, ESearchCase::CaseSensitive);
		}
		return Line;
	}

	FString FLibrary::EpisodeLine(const AnastasisEpisodes::FEpisode& Event) const
	{
		const FString Name = FString::Printf(TEXT("episodes.EPISODE_LINES.%s.%s"), *Event.Kind,
			AnastasisEpisodes::IsLegend(Event) ? TEXT("legend") : TEXT("firsthand"));
		const TArray<FString>* Lines = Pools.Find(Name);
		if (!Lines || Lines->IsEmpty()) return TEXT("quelque chose est arrive");
		FString Line = (*Lines)[AnastasisEpisodes::VariantIndex(Event, Lines->Num())];
		const FString Detail = Event.Detail == 0.0 ? FString(TEXT("mon"))
			: (FMath::IsNearlyEqual(Event.Detail, FMath::RoundToDouble(Event.Detail)) ? FString::Printf(TEXT("%d"), FMath::RoundToInt32(Event.Detail)) : FString::SanitizeFloat(Event.Detail));
		Line.ReplaceInline(TEXT("{who}"), Event.AboutName.IsEmpty() ? TEXT("quelqu'un") : *Event.AboutName, ESearchCase::CaseSensitive);
		Line.ReplaceInline(TEXT("{note}"), Event.Note.IsEmpty() ? TEXT("un reve") : *Event.Note, ESearchCase::CaseSensitive);
		Line.ReplaceInline(TEXT("{detail}"), *Detail, ESearchCase::CaseSensitive);
		return Line;
	}

	FString FLibrary::TellerVersion(const AnastasisEpisodes::FEpisode& Event, const FString& OriginName) const
	{
		const FString Story = EpisodeLine(Event);
		if (Event.bFirsthand || AnastasisEpisodes::IsLegend(Event) || OriginName.IsEmpty()) return Story;
		const uint32 Key = FCrc::StrCrc32(*Event.Id);
		const FString Framed = Pick(TEXT("memoire.raconte_par"), Key, { { TEXT("source"), OriginName }, { TEXT("histoire"), Story } });
		return Framed.IsEmpty() ? Story : Framed;
	}

	const FLibrary& FLibrary::Get()
	{
		static const FLibrary Library = []()
		{
			FLibrary Loaded;
			FString Error;
			const int32 Files = Loaded.LoadDefaults(Error);
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_DIALOGUE files=%d pools=%d lines=%d%s%s"),
				Files, Loaded.PoolCount(), Loaded.LineCount(), Error.IsEmpty() ? TEXT("") : TEXT(" error="), *Error);
			return Loaded;
		}();
		return Library;
	}

	uint32 FLibrary::KeyOf(uint32 Seed, const FString& Speaker, const FString& Situation, int32 Rank)
	{
		uint32 Key = FCrc::StrCrc32(*Speaker, Seed);
		Key = FCrc::StrCrc32(*Situation, Key);
		return FCrc::MemCrc32(&Rank, sizeof(Rank), Key);
	}
}
