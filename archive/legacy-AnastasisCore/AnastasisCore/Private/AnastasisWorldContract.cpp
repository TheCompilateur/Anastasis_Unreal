#include "AnastasisWorldContract.h"

#include "AnastasisNumeric.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

const TCHAR* const AnastasisWorldContractVersion = TEXT("anastasis-world-v1");

namespace
{
	using namespace Anastasis;

	/**
	 * `textOrNull` / `stringOrNull` : une valeur absente, nulle, ou d'un autre
	 * type que chaine vaut `null`, donc `FString` vide. On ne convertit PAS un
	 * nombre en texte au passage : cote JS `stringOrNull(42)` rend `null`, et
	 * fabriquer "42" ici inventerait un identifiant qui n'existe pas.
	 */
	FString ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		if (!Object.IsValid())
		{
			return FString();
		}

		FString Value;
		return Object->TryGetStringField(Field, Value) ? Value : FString();
	}

	/** `finiteNumber(object[field], fallback)`. Champ absent = repli, pas d'erreur. */
	double ReadNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double Fallback = 0.0)
	{
		if (!Object.IsValid())
		{
			return Fallback;
		}

		double Value = 0.0;
		return Object->TryGetNumberField(Field, Value) ? FiniteNumber(Value, Fallback) : Fallback;
	}

	/** `Boolean(object[field])` sur un champ booleen du contrat. */
	bool ReadBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		if (!Object.IsValid())
		{
			return false;
		}

		bool Value = false;
		return Object->TryGetBoolField(Field, Value) ? Value : false;
	}

	TSharedPtr<FJsonObject> ReadObject(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		if (!Object.IsValid())
		{
			return nullptr;
		}

		const TSharedPtr<FJsonObject>* Nested = nullptr;
		return Object->TryGetObjectField(Field, Nested) && Nested ? *Nested : nullptr;
	}

	/** `copyPoint` : les deux composantes doivent etre finies, sinon `null`. */
	FAnastasisPoint2D ReadPoint(const TSharedPtr<FJsonObject>& Object)
	{
		FAnastasisPoint2D Point;
		if (!Object.IsValid())
		{
			return Point;
		}

		double X = 0.0;
		double Y = 0.0;
		if (!Object->TryGetNumberField(TEXT("x"), X) || !Object->TryGetNumberField(TEXT("y"), Y))
		{
			return Point;
		}
		if (!FMath::IsFinite(X) || !FMath::IsFinite(Y))
		{
			return Point;
		}

		Point.bIsSet = true;
		Point.X = X;
		Point.Y = Y;
		return Point;
	}

	/** Ecrit une chaine, ou un `null` JSON quand elle est vide (convention du contrat). */
	void WriteStringOrNull(const TSharedRef<FJsonObject>& Object, const TCHAR* Field, const FString& Value)
	{
		if (Value.IsEmpty())
		{
			Object->SetField(Field, MakeShared<FJsonValueNull>());
			return;
		}
		Object->SetStringField(Field, Value);
	}

	void WritePointOrNull(const TSharedRef<FJsonObject>& Object, const TCHAR* Field, const FAnastasisPoint2D& Point)
	{
		if (!Point.bIsSet)
		{
			Object->SetField(Field, MakeShared<FJsonValueNull>());
			return;
		}

		const TSharedRef<FJsonObject> Nested = MakeShared<FJsonObject>();
		Nested->SetNumberField(TEXT("x"), Point.X);
		Nested->SetNumberField(TEXT("y"), Point.Y);
		Object->SetObjectField(Field, Nested);
	}

	FAnastasisActor ReadActor(const TSharedPtr<FJsonObject>& Json)
	{
		FAnastasisActor Actor;
		Actor.Id = ReadString(Json, TEXT("id"));
		Actor.Name = ReadString(Json, TEXT("name"));
		Actor.X = ReadNumber(Json, TEXT("x"));
		Actor.Y = ReadNumber(Json, TEXT("y"));
		Actor.JobId = ReadString(Json, TEXT("jobId"));
		Actor.Gender = ReadString(Json, TEXT("gender"));
		Actor.LifeStage = ReadString(Json, TEXT("lifeStage"));
		Actor.Age = ReadNumber(Json, TEXT("age"));
		Actor.Biome = ReadString(Json, TEXT("biome"));
		Actor.SocialClass = ReadString(Json, TEXT("socialClass"));
		Actor.HomeId = ReadString(Json, TEXT("homeId"));
		Actor.InsideBuildingId = ReadString(Json, TEXT("insideBuildingId"));

		const TSharedPtr<FJsonObject> Activity = ReadObject(Json, TEXT("activity"));
		Actor.Activity.JobId = ReadString(Activity, TEXT("jobId"));
		Actor.Activity.State = ReadString(Activity, TEXT("state"));
		Actor.Activity.Goal = ReadString(Activity, TEXT("goal"));
		Actor.Activity.WorkplaceId = ReadString(Activity, TEXT("workplaceId"));
		Actor.Activity.DestinationBuildingId = ReadString(Activity, TEXT("destinationBuildingId"));
		Actor.Activity.Destination = ReadPoint(ReadObject(Activity, TEXT("destination")));

		const TSharedPtr<FJsonObject> Needs = ReadObject(Json, TEXT("needs"));
		Actor.Needs.Hunger = ReadNumber(Needs, TEXT("hunger"));
		Actor.Needs.Energy = ReadNumber(Needs, TEXT("energy"));
		Actor.Needs.Thirst = ReadNumber(Needs, TEXT("thirst"));
		Actor.Needs.Social = ReadNumber(Needs, TEXT("social"));
		Actor.Needs.Leisure = ReadNumber(Needs, TEXT("leisure"));
		Actor.Needs.Hygiene = ReadNumber(Needs, TEXT("hygiene"));
		Actor.Needs.Health = ReadNumber(Needs, TEXT("health"));
		Actor.Needs.Morale = ReadNumber(Needs, TEXT("morale"));

		const TSharedPtr<FJsonObject> Navigation = ReadObject(Json, TEXT("navigation"));
		Actor.Navigation.PathLength = NonNegativeInt(ReadNumber(Navigation, TEXT("pathLength")));
		Actor.Navigation.PathIndex = NonNegativeInt(ReadNumber(Navigation, TEXT("pathIndex")));
		Actor.Navigation.StuckTicks = NonNegativeInt(ReadNumber(Navigation, TEXT("stuckTicks")));
		Actor.Navigation.StuckStage = NonNegativeInt(ReadNumber(Navigation, TEXT("stuckStage")));
		Actor.Navigation.bAwaitingPath = ReadBool(Navigation, TEXT("awaitingPath"));
		return Actor;
	}

	FAnastasisBuilding ReadBuilding(const TSharedPtr<FJsonObject>& Json)
	{
		FAnastasisBuilding Building;
		Building.Id = ReadString(Json, TEXT("id"));
		Building.Type = ReadString(Json, TEXT("type"));
		Building.X = ReadNumber(Json, TEXT("x"));
		Building.Y = ReadNumber(Json, TEXT("y"));
		// Repli a 1, pas a 0 : `finiteNumber(building?.progress, 1)`. Un batiment
		// sans champ de progression est ACHEVE, et le mettre a 0 le ferait
		// reapparaitre en chantier a chaque instantane.
		Building.Progress = Clamp01(ReadNumber(Json, TEXT("progress"), 1.0));
		return Building;
	}

	TSharedRef<FJsonObject> WriteActor(const FAnastasisActor& Actor)
	{
		const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		WriteStringOrNull(Json, TEXT("id"), Actor.Id);
		WriteStringOrNull(Json, TEXT("name"), Actor.Name);
		Json->SetNumberField(TEXT("x"), Actor.X);
		Json->SetNumberField(TEXT("y"), Actor.Y);
		WriteStringOrNull(Json, TEXT("jobId"), Actor.JobId);
		WriteStringOrNull(Json, TEXT("gender"), Actor.Gender);
		WriteStringOrNull(Json, TEXT("lifeStage"), Actor.LifeStage);
		Json->SetNumberField(TEXT("age"), Actor.Age);
		WriteStringOrNull(Json, TEXT("biome"), Actor.Biome);
		WriteStringOrNull(Json, TEXT("socialClass"), Actor.SocialClass);
		WriteStringOrNull(Json, TEXT("homeId"), Actor.HomeId);
		WriteStringOrNull(Json, TEXT("insideBuildingId"), Actor.InsideBuildingId);

		const TSharedRef<FJsonObject> Activity = MakeShared<FJsonObject>();
		WriteStringOrNull(Activity, TEXT("jobId"), Actor.Activity.JobId);
		WriteStringOrNull(Activity, TEXT("state"), Actor.Activity.State);
		WriteStringOrNull(Activity, TEXT("goal"), Actor.Activity.Goal);
		WriteStringOrNull(Activity, TEXT("workplaceId"), Actor.Activity.WorkplaceId);
		WriteStringOrNull(Activity, TEXT("destinationBuildingId"), Actor.Activity.DestinationBuildingId);
		WritePointOrNull(Activity, TEXT("destination"), Actor.Activity.Destination);
		Json->SetObjectField(TEXT("activity"), Activity);

		const TSharedRef<FJsonObject> Needs = MakeShared<FJsonObject>();
		Needs->SetNumberField(TEXT("hunger"), Actor.Needs.Hunger);
		Needs->SetNumberField(TEXT("energy"), Actor.Needs.Energy);
		Needs->SetNumberField(TEXT("thirst"), Actor.Needs.Thirst);
		Needs->SetNumberField(TEXT("social"), Actor.Needs.Social);
		Needs->SetNumberField(TEXT("leisure"), Actor.Needs.Leisure);
		Needs->SetNumberField(TEXT("hygiene"), Actor.Needs.Hygiene);
		Needs->SetNumberField(TEXT("health"), Actor.Needs.Health);
		Needs->SetNumberField(TEXT("morale"), Actor.Needs.Morale);
		Json->SetObjectField(TEXT("needs"), Needs);

		const TSharedRef<FJsonObject> Navigation = MakeShared<FJsonObject>();
		Navigation->SetNumberField(TEXT("pathLength"), Actor.Navigation.PathLength);
		Navigation->SetNumberField(TEXT("pathIndex"), Actor.Navigation.PathIndex);
		Navigation->SetNumberField(TEXT("stuckTicks"), Actor.Navigation.StuckTicks);
		Navigation->SetNumberField(TEXT("stuckStage"), Actor.Navigation.StuckStage);
		Navigation->SetBoolField(TEXT("awaitingPath"), Actor.Navigation.bAwaitingPath);
		Json->SetObjectField(TEXT("navigation"), Navigation);
		return Json;
	}

	TSharedRef<FJsonObject> WriteBuilding(const FAnastasisBuilding& Building)
	{
		const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		WriteStringOrNull(Json, TEXT("id"), Building.Id);
		WriteStringOrNull(Json, TEXT("type"), Building.Type);
		Json->SetNumberField(TEXT("x"), Building.X);
		Json->SetNumberField(TEXT("y"), Building.Y);
		Json->SetNumberField(TEXT("progress"), Building.Progress);
		return Json;
	}
}

namespace AnastasisWorld
{
	FAnastasisValidation Validate(const FAnastasisWorldSnapshot& Snapshot)
	{
		FAnastasisValidation Result;

		if (Snapshot.ContractVersion != AnastasisWorldContractVersion)
		{
			Result.Errors.Add(TEXT("unsupported contractVersion"));
		}

		// AJOUT PROPRE AU PORT, pas une traduction : cote JS `seed` est deja
		// passe par `>>> 0` avant d'entrer dans l'instantane, donc la seule
		// chose que le contrat source pouvait encore verifier etait sa
		// finitude. Ici la graine peut arriver de JSON avec n'importe quelle
		// magnitude, et une graine hors plage n'identifie plus le run qu'elle
		// est censee identifier. Message distinct pour qu'un lecteur ne le
		// prenne pas pour l'erreur JS d'origine.
		if (Snapshot.Seed < 0 || Snapshot.Seed > static_cast<int64>(MAX_uint32))
		{
			Result.Errors.Add(TEXT("seed is outside the unsigned 32-bit range"));
		}

		if (!FMath::IsFinite(Snapshot.Time))
		{
			Result.Errors.Add(TEXT("time is not finite"));
		}
		if (Snapshot.Day < 1)
		{
			Result.Errors.Add(TEXT("day is not a positive integer"));
		}
		if (!FMath::IsFinite(Snapshot.DayFraction) || Snapshot.DayFraction < 0.0 || Snapshot.DayFraction > 1.0)
		{
			Result.Errors.Add(TEXT("dayFraction is not within [0, 1]"));
		}
		if (Snapshot.Dimensions.Width < 0 || Snapshot.Dimensions.Height < 0)
		{
			Result.Errors.Add(TEXT("dimensions are invalid"));
		}

		Result.bValid = Result.Errors.Num() == 0;
		return Result;
	}

	bool Assert(const FAnastasisWorldSnapshot& Snapshot, FString& OutError)
	{
		const FAnastasisValidation Result = Validate(Snapshot);
		if (Result.bValid)
		{
			OutError.Reset();
			return true;
		}

		OutError = FString::Printf(TEXT("Invalid ANASTASIS world contract: %s"),
			*FString::Join(Result.Errors, TEXT("; ")));
		return false;
	}

	bool FromJson(const TSharedPtr<FJsonObject>& Json, FAnastasisWorldSnapshot& OutSnapshot, FString& OutError)
	{
		if (!Json.IsValid())
		{
			OutError = TEXT("snapshot is not an object");
			return false;
		}

		// Ces trois controles sont ceux que `Validate` ne peut plus faire une
		// fois la structure C++ construite : c'est le seul endroit ou un
		// `actors` non-tableau existe encore.
		const TArray<TSharedPtr<FJsonValue>>* ActorValues = nullptr;
		if (!Json->TryGetArrayField(TEXT("actors"), ActorValues))
		{
			OutError = TEXT("actors is not an array");
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* BuildingValues = nullptr;
		if (!Json->TryGetArrayField(TEXT("buildings"), BuildingValues))
		{
			OutError = TEXT("buildings is not an array");
			return false;
		}

		const TSharedPtr<FJsonObject> ResourceObject = ReadObject(Json, TEXT("resources"));
		if (!ResourceObject.IsValid())
		{
			OutError = TEXT("resources are invalid");
			return false;
		}

		FAnastasisWorldSnapshot Snapshot;
		Snapshot.ContractVersion = ReadString(Json, TEXT("contractVersion"));
		Snapshot.Seed = static_cast<int64>(ReadNumber(Json, TEXT("seed")));
		Snapshot.Time = FMath::Max(0.0, ReadNumber(Json, TEXT("time")));
		Snapshot.Day = FMath::Max(1, NonNegativeInt(ReadNumber(Json, TEXT("day"), 1.0)));
		Snapshot.DayFraction = Clamp01(ReadNumber(Json, TEXT("dayFraction")));

		const TSharedPtr<FJsonObject> Dimensions = ReadObject(Json, TEXT("dimensions"));
		Snapshot.Dimensions.Width = NonNegativeInt(ReadNumber(Dimensions, TEXT("width")));
		Snapshot.Dimensions.Height = NonNegativeInt(ReadNumber(Dimensions, TEXT("height")));

		if (const TSharedPtr<FJsonObject> Settlement = ReadObject(Json, TEXT("settlement")))
		{
			Snapshot.Settlement.bIsSet = true;
			Snapshot.Settlement.Name = ReadString(Settlement, TEXT("name"));
			Snapshot.Settlement.X = ReadNumber(Settlement, TEXT("x"));
			Snapshot.Settlement.Y = ReadNumber(Settlement, TEXT("y"));
		}

		Snapshot.Actors.Reserve(ActorValues->Num());
		for (const TSharedPtr<FJsonValue>& Value : *ActorValues)
		{
			const TSharedPtr<FJsonObject>* AsObject = nullptr;
			if (Value.IsValid() && Value->TryGetObject(AsObject) && AsObject)
			{
				Snapshot.Actors.Add(ReadActor(*AsObject));
			}
		}

		Snapshot.Buildings.Reserve(BuildingValues->Num());
		for (const TSharedPtr<FJsonValue>& Value : *BuildingValues)
		{
			const TSharedPtr<FJsonObject>* AsObject = nullptr;
			if (Value.IsValid() && Value->TryGetObject(AsObject) && AsObject)
			{
				Snapshot.Buildings.Add(ReadBuilding(*AsObject));
			}
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : ResourceObject->Values)
		{
			double Amount = 0.0;
			if (Entry.Value.IsValid() && Entry.Value->TryGetNumber(Amount))
			{
				Snapshot.Resources.Add(Entry.Key, FMath::Max(0.0, FiniteNumber(Amount)));
			}
			else
			{
				// `copyStock` fait `Math.max(0, finiteNumber(amount))` : une
				// quantite illisible devient 0 et la ressource RESTE listee.
				// La retirer ferait disparaitre une ressource connue du monde.
				Snapshot.Resources.Add(Entry.Key, 0.0);
			}
		}

		OutSnapshot = MoveTemp(Snapshot);
		OutError.Reset();
		return true;
	}

	TSharedRef<FJsonObject> ToJson(const FAnastasisWorldSnapshot& Snapshot)
	{
		const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("contractVersion"), Snapshot.ContractVersion);
		Json->SetNumberField(TEXT("seed"), static_cast<double>(Snapshot.Seed));
		Json->SetNumberField(TEXT("time"), Snapshot.Time);
		Json->SetNumberField(TEXT("day"), Snapshot.Day);
		Json->SetNumberField(TEXT("dayFraction"), Snapshot.DayFraction);

		const TSharedRef<FJsonObject> Dimensions = MakeShared<FJsonObject>();
		Dimensions->SetNumberField(TEXT("width"), Snapshot.Dimensions.Width);
		Dimensions->SetNumberField(TEXT("height"), Snapshot.Dimensions.Height);
		Json->SetObjectField(TEXT("dimensions"), Dimensions);

		if (Snapshot.Settlement.bIsSet)
		{
			const TSharedRef<FJsonObject> Settlement = MakeShared<FJsonObject>();
			WriteStringOrNull(Settlement, TEXT("name"), Snapshot.Settlement.Name);
			Settlement->SetNumberField(TEXT("x"), Snapshot.Settlement.X);
			Settlement->SetNumberField(TEXT("y"), Snapshot.Settlement.Y);
			Json->SetObjectField(TEXT("settlement"), Settlement);
		}
		else
		{
			Json->SetField(TEXT("settlement"), MakeShared<FJsonValueNull>());
		}

		TArray<TSharedPtr<FJsonValue>> ActorValues;
		ActorValues.Reserve(Snapshot.Actors.Num());
		for (const FAnastasisActor& Actor : Snapshot.Actors)
		{
			ActorValues.Add(MakeShared<FJsonValueObject>(WriteActor(Actor)));
		}
		Json->SetArrayField(TEXT("actors"), ActorValues);

		TArray<TSharedPtr<FJsonValue>> BuildingValues;
		BuildingValues.Reserve(Snapshot.Buildings.Num());
		for (const FAnastasisBuilding& Building : Snapshot.Buildings)
		{
			BuildingValues.Add(MakeShared<FJsonValueObject>(WriteBuilding(Building)));
		}
		Json->SetArrayField(TEXT("buildings"), BuildingValues);

		const TSharedRef<FJsonObject> Resources = MakeShared<FJsonObject>();
		for (const TPair<FString, double>& Entry : Snapshot.Resources)
		{
			Resources->SetNumberField(Entry.Key, Entry.Value);
		}
		Json->SetObjectField(TEXT("resources"), Resources);

		return Json;
	}
}
