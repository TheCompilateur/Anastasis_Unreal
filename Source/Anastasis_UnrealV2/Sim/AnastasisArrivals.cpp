// arrivants-001 -- qui sont les arrivants, et ce qu'on dit au conseil du soir (hote, ecart n°53).

#include "Sim/AnastasisArrivals.h"

#include "Anastasis_UnrealV2.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sim/AnastasisDialogueLines.h"
#include "Sim/AnastasisSimulation.h"

namespace AnastasisArrivals
{
	using FGroup = AnastasisVillage::FVillage::FArrivalGroup;
	using FMember = AnastasisVillage::FVillage::FArrivalMember;

	bool ParsePool(const FString& Json, TArray<FGroup>& Out, FString& OutError)
	{
		Out.Reset();
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("json illisible");
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Groups = nullptr;
		if (!Root->TryGetArrayField(TEXT("groupes"), Groups) || !Groups)
		{
			OutError = TEXT("champ groupes absent");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& GroupValue : *Groups)
		{
			const TSharedPtr<FJsonObject>* GroupObject = nullptr;
			if (!GroupValue.IsValid() || !GroupValue->TryGetObject(GroupObject) || !GroupObject) continue;
			FGroup& Group = Out.AddDefaulted_GetRef();
			(*GroupObject)->TryGetStringField(TEXT("nom"), Group.FamilyName);
			const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
			if (!(*GroupObject)->TryGetArrayField(TEXT("membres"), Members) || !Members) continue;
			for (const TSharedPtr<FJsonValue>& MemberValue : *Members)
			{
				const TSharedPtr<FJsonObject>* MemberObject = nullptr;
				if (!MemberValue.IsValid() || !MemberValue->TryGetObject(MemberObject) || !MemberObject) continue;
				FMember& Member = Group.Members.AddDefaulted_GetRef();
				(*MemberObject)->TryGetStringField(TEXT("prenom"), Member.Name);
				FString Sex;
				(*MemberObject)->TryGetStringField(TEXT("sexe"), Sex);
				Member.Gender = Sex == TEXT("F") ? TEXT("female") : TEXT("male");
				(*MemberObject)->TryGetNumberField(TEXT("age"), Member.Age);
				(*MemberObject)->TryGetStringField(TEXT("lien"), Member.KinRole);
				// Comme les fondateurs : les adultes d'un cote, les moins de seize ans de l'autre.
				Member.bAdult = Member.Age >= 16.0;
			}
		}
		return true;
	}

	const TArray<FGroup>& DefaultPool()
	{
		static const TArray<FGroup> Pool = []()
		{
			TArray<FGroup> Groups;
			const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anastasis/Scenario/valmire-arrivants.json"));
			FString Json;
			FString Error;
			if (!FFileHelper::LoadFileToString(Json, *Path) || !ParsePool(Json, Groups, Error))
			{
				UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_ARRIVALS unavailable path=%s error=%s"), *Path,
					Error.IsEmpty() ? TEXT("file") : *Error);
				return TArray<FGroup>();
			}
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_ARRIVALS loaded groups=%d"), Groups.Num());
			return Groups;
		}();
		return Pool;
	}

	FString StatusJson(const FAnastasisSimulation& Sim)
	{
		const AnastasisVillage::FVillage& Village = Sim.GetVillage();
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetBoolField(TEXT("geo_loaded"), Sim.GetGeo().IsLoaded());
		Root->SetNumberField(TEXT("day"), Sim.GetDay());
		Root->SetNumberField(TEXT("people"), Village.GetActors().Num());
		TArray<TSharedPtr<FJsonValue>> Groups;
		for (const AnastasisVillage::FVillage::FFamily& Family : Village.GetFamilies())
		{
			if (Family.ArrivedDay <= 0) continue;
			const TSharedRef<FJsonObject> G = MakeShared<FJsonObject>();
			G->SetStringField(TEXT("family"), Family.Id);
			G->SetStringField(TEXT("name"), Family.Name);
			G->SetNumberField(TEXT("arrived_day"), Family.ArrivedDay);
			G->SetStringField(TEXT("cause"), Family.Cause);
			G->SetStringField(TEXT("origin"), Family.Origin);
			G->SetBoolField(TEXT("guest"), Family.bGuest);
			G->SetBoolField(TEXT("left"), Family.bLeft);
			G->SetNumberField(TEXT("members"), Family.Adults.Num() + Family.Dependents.Num());
			bool bHouse = false;
			for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
			{
				bHouse |= Building.OwnerFamilyId == Family.Id && Building.Progress >= 1.0;
			}
			G->SetBoolField(TEXT("house_done"), bHouse);
			Groups.Add(MakeShared<FJsonValueObject>(G));
		}
		Root->SetArrayField(TEXT("groups"), Groups);
		TArray<TSharedPtr<FJsonValue>> Councils;
		for (const AnastasisVillage::FVillage::FCouncil& Council : Village.GetCouncilLog())
		{
			const TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
			C->SetNumberField(TEXT("day"), Council.Day);
			C->SetStringField(TEXT("family"), Council.FamilyId);
			C->SetStringField(TEXT("cause"), Council.Cause);
			C->SetBoolField(TEXT("accepted"), Council.bAccepted);
			TArray<TSharedPtr<FJsonValue>> Votes;
			for (const AnastasisVillage::FVillage::FWelcomeVote& Vote : Council.Votes)
			{
				const TSharedRef<FJsonObject> V = MakeShared<FJsonObject>();
				V->SetStringField(TEXT("voter"), Vote.VoterId);
				V->SetBoolField(TEXT("yes"), Vote.bYes);
				V->SetStringField(TEXT("reason"), Vote.Reason);
				V->SetNumberField(TEXT("score"), Vote.Score);
				V->SetStringField(TEXT("terms"), Vote.Terms);
				Votes.Add(MakeShared<FJsonValueObject>(V));
			}
			C->SetArrayField(TEXT("votes"), Votes);
			Councils.Add(MakeShared<FJsonValueObject>(C));
		}
		Root->SetArrayField(TEXT("councils"), Councils);
		// voix-conseil-001 (ecart n°54) : ce que le joueur a fait et ce qu'on lui a repondu.
		const FString& Player = Village.GetPlayerPersonId();
		const TSharedRef<FJsonObject> Me = MakeShared<FJsonObject>();
		Me->SetStringField(TEXT("id"), Player);
		int32 Pending = 0;
		int32 OldestPending = 0;
		for (int32 I = 0; I < Village.GetPlayerAsks().Num(); ++I)
		{
			if (Village.GetPlayerAsks()[I].bAnswered) continue;
			++Pending;
			if (OldestPending == 0) OldestPending = I + 1;
		}
		Me->SetNumberField(TEXT("asks_pending"), Pending);
		Me->SetNumberField(TEXT("asks_total"), Village.GetPlayerAsks().Num());
		Me->SetNumberField(TEXT("oldest_pending"), OldestPending);
		Me->SetBoolField(TEXT("unproven"), !Player.IsEmpty() && Village.FindNpc(Player) && Village.IsUnproven(*Village.FindNpc(Player)));
		TArray<TSharedPtr<FJsonValue>> Answers;
		for (const AnastasisVillage::FVillage::FHelpAnswer& Answer : Village.GetHelpLog())
		{
			if (Player.IsEmpty() || (Answer.FromId != Player && Answer.ToId != Player)) continue;
			const TSharedRef<FJsonObject> A = MakeShared<FJsonObject>();
			A->SetNumberField(TEXT("day"), Answer.Day);
			A->SetStringField(TEXT("from"), Answer.FromId);
			A->SetStringField(TEXT("to"), Answer.ToId);
			A->SetBoolField(TEXT("accepted"), Answer.bAccepted);
			A->SetStringField(TEXT("reason"), Answer.Reason);
			Answers.Add(MakeShared<FJsonValueObject>(A));
		}
		Me->SetArrayField(TEXT("help"), Answers);
		Root->SetObjectField(TEXT("player"), Me);
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}

	FString VoteLine(const AnastasisDialogue::FLibrary& Lines, uint32 Seed, const AnastasisVillage::FVillage::FWelcomeVote& Vote,
		const FString& Cause, int32 Rank)
	{
		const FString Pool = FString::Printf(TEXT("accueil.%s.%s"), Vote.bYes ? TEXT("oui") : TEXT("non"), *Vote.Reason);
		return Lines.Pick(Pool, AnastasisDialogue::FLibrary::KeyOf(Seed, Vote.VoterId, Pool, Rank), { { TEXT("cause"), Cause } });
	}
}
