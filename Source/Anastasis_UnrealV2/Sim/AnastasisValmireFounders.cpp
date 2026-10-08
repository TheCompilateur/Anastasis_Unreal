#include "Sim/AnastasisValmireFounders.h"

#include "Anastasis_UnrealV2.h"
#include "Dom/JsonObject.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sim/AnastasisDialogueLines.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisPathfinding.h"

namespace AnastasisFounders
{
	namespace
	{
		bool ReadMember(const TSharedPtr<FJsonObject>& Object, FMember& Out)
		{
			if (!Object.IsValid()) return false;
			Object->TryGetStringField(TEXT("cle"), Out.Key);
			Object->TryGetStringField(TEXT("prenom"), Out.Given);
			Object->TryGetStringField(TEXT("surnom"), Out.Byname);
			Object->TryGetStringField(TEXT("lien"), Out.Link);
			FString Sex;
			Object->TryGetStringField(TEXT("sexe"), Sex);
			Out.bFemale = Sex == TEXT("F");
			Object->TryGetNumberField(TEXT("age"), Out.Age);
			return !Out.Key.IsEmpty() && !Out.Given.IsEmpty();
		}

		void ReadAnswers(const TSharedPtr<FJsonObject>& Family, const TCHAR* Field, const TCHAR* SubjectField, TArray<FAnswer>& Out)
		{
			const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
			if (!Family->TryGetArrayField(Field, Items) || !Items) return;
			for (const TSharedPtr<FJsonValue>& Item : *Items)
			{
				const TSharedPtr<FJsonObject>* Object = nullptr;
				if (!Item.IsValid() || !Item->TryGetObject(Object) || !Object) continue;
				FAnswer Answer;
				(*Object)->TryGetStringField(SubjectField, Answer.Subject);
				(*Object)->TryGetStringField(TEXT("texte"), Answer.Text);
				if (!Answer.Text.IsEmpty()) Out.Add(Answer);
			}
		}

		const FFounder* FounderOf(const TArray<FFounder>& Founders, const FString& Key)
		{
			return Founders.FindByPredicate([&Key](const FFounder& F) { return F.Key == Key; });
		}

		FString Capitalize(const FString& Text)
		{
			FString Out = Text;
			if (!Out.IsEmpty()) Out[0] = FChar::ToUpper(Out[0]);
			return Out;
		}
	}

	bool FScenario::Parse(const FString& Json, FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("json illisible");
			return false;
		}
		const TSharedPtr<FJsonObject>* MonkObject = nullptr;
		if (!Root->TryGetObjectField(TEXT("moine"), MonkObject) || !MonkObject || !ReadMember(*MonkObject, Monk))
		{
			OutError = TEXT("moine absent");
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* FamilyItems = nullptr;
		if (!Root->TryGetArrayField(TEXT("familles"), FamilyItems) || !FamilyItems || FamilyItems->IsEmpty())
		{
			OutError = TEXT("familles absentes");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Item : *FamilyItems)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Item.IsValid() || !Item->TryGetObject(Object) || !Object) continue;
			FFamilyDef& Family = Families.AddDefaulted_GetRef();
			(*Object)->TryGetStringField(TEXT("cle"), Family.Key);
			(*Object)->TryGetStringField(TEXT("nom"), Family.Name);
			(*Object)->TryGetStringField(TEXT("matrice"), Family.Matrix);
			(*Object)->TryGetStringField(TEXT("repondant"), Family.Respondent);
			(*Object)->TryGetStringField(TEXT("presentation"), Family.Presentation);
			const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
			if ((*Object)->TryGetArrayField(TEXT("membres"), Members) && Members)
			{
				for (const TSharedPtr<FJsonValue>& M : *Members)
				{
					const TSharedPtr<FJsonObject>* MemberObject = nullptr;
					FMember Member;
					if (M.IsValid() && M->TryGetObject(MemberObject) && MemberObject && ReadMember(*MemberObject, Member)) Family.Members.Add(Member);
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Where = nullptr;
			if ((*Object)->TryGetArrayField(TEXT("ville"), Where) && Where)
			{
				for (const TSharedPtr<FJsonValue>& W : *Where)
				{
					FString Text;
					if (W.IsValid() && W->TryGetString(Text) && !Text.IsEmpty()) Family.Where.Add(Text);
				}
			}
			ReadAnswers(*Object, TEXT("emporte"), TEXT("objet"), Family.Carried);
			ReadAnswers(*Object, TEXT("absent"), TEXT("qui"), Family.Missing);
			if (Family.Members.IsEmpty() || Family.Where.IsEmpty() || Family.Carried.IsEmpty() || Family.Missing.IsEmpty())
			{
				OutError = FString::Printf(TEXT("famille %s incomplete"), *Family.Key);
				return false;
			}
		}
		const TSharedPtr<FJsonObject>* Order = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
		if (Root->TryGetObjectField(TEXT("ordre_de_pose"), Order) && Order && (*Order)->TryGetArrayField(TEXT("cles"), Keys) && Keys)
		{
			for (const TSharedPtr<FJsonValue>& K : *Keys)
			{
				FString Key;
				if (K.IsValid() && K->TryGetString(Key)) PoseOrder.Add(Key);
			}
		}
		// Toute personne absente de l'ordre de pose vient apres, dans l'ordre du fichier : personne n'est oublie.
		for (const FFamilyDef& Family : Families)
		{
			for (const FMember& Member : Family.Members)
			{
				if (!PoseOrder.Contains(Member.Key)) PoseOrder.Add(Member.Key);
			}
		}
		if (!PoseOrder.Contains(Monk.Key)) PoseOrder.Add(Monk.Key);
		for (const FString& Key : PoseOrder)
		{
			int32 FamilyIndex = INDEX_NONE;
			if (!FindMember(Key, FamilyIndex))
			{
				OutError = FString::Printf(TEXT("ordre de pose : %s inconnu"), *Key);
				return false;
			}
		}
		return true;
	}

	int32 FScenario::PeopleCount() const
	{
		int32 Count = 1;
		for (const FFamilyDef& Family : Families) Count += Family.Members.Num();
		return Count;
	}

	const FMember* FScenario::FindMember(const FString& Key, int32& OutFamily) const
	{
		OutFamily = INDEX_NONE;
		if (Key == Monk.Key) return &Monk;
		for (int32 F = 0; F < Families.Num(); ++F)
		{
			for (const FMember& Member : Families[F].Members)
			{
				if (Member.Key == Key)
				{
					OutFamily = F;
					return &Member;
				}
			}
		}
		return nullptr;
	}

	const FScenario* FScenario::Get()
	{
		static const TOptional<FScenario> Loaded = []() -> TOptional<FScenario>
		{
			const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anastasis/Scenario/valmire-fondateurs.json"));
			FString Json;
			FString Error;
			FScenario Scenario;
			if (!FFileHelper::LoadFileToString(Json, *Path) || !Scenario.Parse(Json, Error))
			{
				UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_FOUNDERS unavailable path=%s error=%s"), *Path,
					Error.IsEmpty() ? TEXT("file") : *Error);
				return TOptional<FScenario>();
			}
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_FOUNDERS loaded families=%d people=%d"),
				Scenario.Families.Num(), Scenario.PeopleCount());
			return Scenario;
		}();
		return Loaded.IsSet() ? &Loaded.GetValue() : nullptr;
	}

	FString FullName(const FMember& Member)
	{
		return Member.Byname.IsEmpty() ? Member.Given : FString::Printf(TEXT("%s %s"), *Member.Given, *Member.Byname);
	}

	int32 Variant(uint32 Seed, const FString& FamilyKey, const TCHAR* Question, int32 Count)
	{
		if (Count <= 0) return 0;
		const uint32 Key = FCrc::StrCrc32(Question, FCrc::StrCrc32(*FamilyKey, Seed));
		return static_cast<int32>(Key % static_cast<uint32>(Count));
	}

	TArray<FFounder> Seed(FAnastasisSimulation& Sim, const FString& WellId, const FScenario& Scenario)
	{
		using namespace AnastasisVillage;
		TArray<FFounder> Founders;
		FVillage& Village = Sim.GetVillage();
		const FBuilding* Well = Village.FindBuilding(WellId);
		if (!Well) return Founders;
		const TArray<FPoint> Doors = Well->AccessPoints;
		const int32 WellX = FMath::FloorToInt32(Well->X);
		const int32 WellY = FMath::FloorToInt32(Well->Y);
		const AnastasisNav::FNavGrid& Nav = Village.GetNavGrid();
		const AnastasisPath::FWorldNavSource Source(Nav, Sim.GetWorld());
		TSet<int32> Taken;

		auto ReachesWell = [&](int32 X, int32 Y)
		{
			for (const FPoint& Door : Doors)
			{
				TArray<FPoint> Path;
				if (AnastasisPath::FindPath(Source, { X + 0.5, Y + 0.5 }, Door, {}, Path)) return true;
			}
			return false;
		};
		// Premier sol libre, non pris, d'ou un chemin mene au puits, en anneaux autour de (CX, CY).
		auto FindTile = [&](int32 CX, int32 CY, int32 MaxRadius, int32& OutX, int32& OutY)
		{
			for (int32 R = 0; R <= MaxRadius; ++R)
			{
				for (int32 DY = -R; DY <= R; ++DY)
				{
					for (int32 DX = -R; DX <= R; ++DX)
					{
						if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
						const int32 X = CX + DX;
						const int32 Y = CY + DY;
						if (X < 2 || Y < 2 || X > Nav.W - 3 || Y > Nav.H - 3) continue;
						if (Taken.Contains(Y * Nav.W + X) || Village.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
						if (!ReachesWell(X, Y)) continue;
						OutX = X;
						OutY = Y;
						return true;
					}
				}
			}
			return false;
		};

		// Chaque famille a son cote du puits ; le moine se tient pres du feu.
		const int32 FamilyCount = FMath::Max(1, Scenario.Families.Num());
		TArray<FIntPoint> Anchors;
		for (int32 F = 0; F < Scenario.Families.Num(); ++F)
		{
			const double Angle = UE_DOUBLE_PI / 4.0 + 2.0 * UE_DOUBLE_PI * F / FamilyCount;
			Anchors.Add(FIntPoint(WellX + FMath::RoundToInt32(6.0 * FMath::Cos(Angle)), WellY + FMath::RoundToInt32(6.0 * FMath::Sin(Angle))));
		}
		TArray<FString> FamilyIds;
		for (const FFamilyDef& Family : Scenario.Families) FamilyIds.Add(Village.AddFamily(Family.Name));

		for (int32 K = 0; K < Scenario.PoseOrder.Num(); ++K)
		{
			const FString& Key = Scenario.PoseOrder[K];
			int32 FamilyIndex = INDEX_NONE;
			const FMember* Member = Scenario.FindMember(Key, FamilyIndex);
			if (!Member) continue;
			const FIntPoint Anchor = FamilyIndex == INDEX_NONE ? FIntPoint(WellX + 2, WellY) : Anchors[FamilyIndex];
			int32 X = 0;
			int32 Y = 0;
			if (!FindTile(Anchor.X, Anchor.Y, 6, X, Y) && !FindTile(WellX, WellY, 14, X, Y))
			{
				UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_FOUNDERS no reachable tile for %s near the well"), *Key);
				continue;
			}
			Taken.Add(Y * Nav.W + X);
			// Memes besoins que les habitants anonymes du lancement : soifs echelonnees, chacun part a son tour.
			AnastasisNeeds::FNeeds Needs;
			Needs.Hunger = 10.0;
			Needs.Energy = 80.0;
			Needs.Social = 70.0;
			Needs.Leisure = 70.0;
			Needs.Hygiene = 60.0;
			Needs.Thirst = FMath::Max(5.0, 80.0 - 12.0 * K);
			Needs.Health = 90.0;
			Needs.Morale = 55.0;
			const FString NpcId = Village.SpawnNpc(X + 0.5, Y + 0.5, Needs, 4.0);
			if (NpcId.IsEmpty()) continue;
			// ecart n°44 : identite et foyer, poses par l'hote.
			const FString FamilyName = FamilyIndex == INDEX_NONE ? FString() : Scenario.Families[FamilyIndex].Name;
			Village.SetIdentity(NpcId, Member->Given, FamilyName, Member->bFemale ? TEXT("female") : TEXT("male"), Member->Age);
			FFounder& Founder = Founders.AddDefaulted_GetRef();
			Founder.NpcId = NpcId;
			Founder.Key = Key;
			Founder.Family = FamilyIndex;
			if (FamilyIndex != INDEX_NONE)
			{
				Founder.FamilyId = FamilyIds[FamilyIndex];
				// `createFamily` : les adultes d'un cote, les dependants (moins de seize ans) de l'autre.
				Village.JoinFamily(NpcId, Founder.FamilyId, Member->Age >= 16.0, Member->Link);
			}
		}
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_FOUNDERS seeded %d of %d around %s"),
			Founders.Num(), Scenario.PoseOrder.Num(), *WellId);
		return Founders;
	}

	FString FamilyIntro(const FFamilyDef& Family)
	{
		const FMember* Chef = Family.Members.FindByPredicate([](const FMember& M) { return M.Link == TEXT("chef"); });
		const bool bCouple = Family.Members.ContainsByPredicate([](const FMember& M) { return M.Link == TEXT("epouse"); });
		// « Konstantinos le Scribe, avec Eudokia (sa femme) et Michael (leur fils, 12 ans). » : le chef, puis les siens.
		TArray<FString> Others;
		for (const FMember& Member : Family.Members)
		{
			if (&Member == Chef) continue;
			FString Role;
			if (Member.Link == TEXT("epouse")) Role = TEXT("sa femme");
			else if (Member.Link == TEXT("fils")) Role = bCouple ? TEXT("leur fils") : TEXT("son fils");
			else if (Member.Link == TEXT("fille")) Role = bCouple ? TEXT("leur fille") : TEXT("sa fille");
			else if (Member.Link == TEXT("frere")) Role = TEXT("son frère");
			else if (Member.Link == TEXT("pupille")) Role = TEXT("son pupille");
			else if (Member.Link == TEXT("engage")) Role = TEXT("muletier engagé");
			if (Member.Age > 0.0 && Member.Age < 16.0) Role += FString::Printf(TEXT("%s%d ans"), Role.IsEmpty() ? TEXT("") : TEXT(", "), FMath::RoundToInt32(Member.Age));
			Others.Add(Role.IsEmpty() ? Member.Given : FString::Printf(TEXT("%s (%s)"), *Member.Given, *Role));
		}
		FString List = Chef ? FullName(*Chef) : FString();
		for (int32 I = 0; I < Others.Num(); ++I)
		{
			List += I == 0 ? TEXT(", avec ") : (I == Others.Num() - 1 ? TEXT(" et ") : TEXT(", "));
			List += Others[I];
		}
		return FString::Printf(TEXT("%s : %s. %s"), *Capitalize(Family.Name), *List, *Family.Presentation);
	}

	TArray<FLine> FireScene(const FScenario& Scenario, const TArray<FFounder>& Founders, uint32 Seed,
		const AnastasisDialogue::FLibrary& Lines)
	{
		using AnastasisDialogue::FLibrary;
		TArray<FLine> Scene;
		const FFounder* Monk = FounderOf(Founders, Scenario.Monk.Key);
		if (!Monk) return Scene;
		auto Say = [&Scene](const FString& SpeakerId, const FString& Text)
		{
			if (!Text.IsEmpty()) Scene.Add(FLine{ SpeakerId, Text });
		};
		auto MonkSays = [&](const TCHAR* Pool, int32 Rank, const TMap<FString, FString>& Holes = TMap<FString, FString>())
		{
			Say(Monk->NpcId, Lines.Pick(Pool, FLibrary::KeyOf(Seed, Monk->Key, Pool, Rank), Holes));
		};

		MonkSays(TEXT("feu.moine.ouverture"), 0);
		for (int32 F = 0; F < Scenario.Families.Num(); ++F)
		{
			const FFamilyDef& Family = Scenario.Families[F];
			const FFounder* Respondent = FounderOf(Founders, Family.Respondent);
			if (!Respondent) continue;
			const FAnswer& Carried = Family.Carried[Variant(Seed, Family.Key, TEXT("emporte"), Family.Carried.Num())];
			const FAnswer& Missing = Family.Missing[Variant(Seed, Family.Key, TEXT("absent"), Family.Missing.Num())];
			MonkSays(TEXT("feu.moine.question_ville"), F);
			Say(Respondent->NpcId, Family.Where[Variant(Seed, Family.Key, TEXT("ville"), Family.Where.Num())]);
			MonkSays(TEXT("feu.moine.question_emporte"), F);
			Say(Respondent->NpcId, Carried.Text);
			MonkSays(TEXT("feu.moine.question_absent"), F);
			Say(Respondent->NpcId, Missing.Text);
			// Quelqu'un de la famille suivante repond a ce qu'il vient d'entendre.
			const FFamilyDef& Next = Scenario.Families[(F + 1) % Scenario.Families.Num()];
			if (const FFounder* Listener = FounderOf(Founders, Next.Respondent))
			{
				if (Listener != Respondent)
				{
					Say(Listener->NpcId, Lines.Pick(TEXT("feu.ecoute"), FLibrary::KeyOf(Seed, Listener->Key, TEXT("feu.ecoute"), F)));
				}
			}
			int32 Unused = INDEX_NONE;
			const FMember* RespondentMember = Scenario.FindMember(Respondent->Key, Unused);
			MonkSays(TEXT("feu.moine.apres_reponse"), F, {
				{ TEXT("absent"), Missing.Subject },
				{ TEXT("nom"), RespondentMember ? RespondentMember->Given : FString() } });
		}
		MonkSays(TEXT("feu.moine.benediction"), 0);
		return Scene;
	}
}
