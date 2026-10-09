// Monde exterieur, V1 (geopolitical-world-001). EXTENSION — ecart n°38. Voir Public/Geo/AnastasisGeo.h.

#include "Geo/AnastasisGeo.h"

#include "Core/AnastasisJson.h"

#include <limits>

namespace AnastasisGeo
{
	namespace
	{
		const TCHAR* const PressureNames[PressureCount] = {
			TEXT("TradeDisruption"),
			TEXT("Insecurity"),
			TEXT("Migration"),
			TEXT("Military"),
			TEXT("Extraction"),
		};

		const TCHAR* const PressureMeanings[PressureCount] = {
			TEXT("echanges coupes : marchands absents, biens introuvables"),
			TEXT("danger sur les chemins et aux champs : raids, enlevements"),
			TEXT("gens en mouvement vers le village : fuyards, familles"),
			TEXT("troupes presentes ou en marche : requisitions, passages"),
			TEXT("prelevement exige : impot, tribut, corvee"),
		};

		/** Combinaison de deux intensites independantes : « ou bruite », reste dans [0, 1]. */
		double Combine(double Current, double Incoming)
		{
			const double C = FMath::Clamp(Current, 0.0, 1.0);
			const double M = FMath::Clamp(Incoming, 0.0, 1.0);
			return 1.0 - (1.0 - C) * (1.0 - M);
		}

		bool InUnit(double Value)
		{
			return FMath::IsFinite(Value) && Value >= 0.0 && Value <= 1.0;
		}

		FString Fmt(double Value)
		{
			return FString::Printf(TEXT("%.3f"), Value);
		}

		// --- Lecture JSON du scenario ---

		using AnastasisJson::FValue;
		using AnastasisJson::EKind;

		struct FReader
		{
			TArray<FString>& Errors;

			const FValue* Field(const FValue& Obj, const TCHAR* Key, const FString& Where, bool bRequired) const
			{
				const FValue* V = Obj.IsObject() ? Obj.Find(Key) : nullptr;
				if (!V && bRequired)
				{
					Errors.Add(FString::Printf(TEXT("%s : champ requis '%s' absent"), *Where, Key));
				}
				return V;
			}

			FString Str(const FValue& Obj, const TCHAR* Key, const FString& Where, bool bRequired, const FString& Default = FString()) const
			{
				const FValue* V = Field(Obj, Key, Where, bRequired);
				if (!V)
				{
					return Default;
				}
				if (!V->IsString())
				{
					Errors.Add(FString::Printf(TEXT("%s : '%s' doit etre une chaine"), *Where, Key));
					return Default;
				}
				return V->String;
			}

			double Num(const FValue& Obj, const TCHAR* Key, const FString& Where, bool bRequired, double Default) const
			{
				const FValue* V = Field(Obj, Key, Where, bRequired);
				if (!V)
				{
					return Default;
				}
				if (!V->IsNumber() || !FMath::IsFinite(V->Number))
				{
					Errors.Add(FString::Printf(TEXT("%s : '%s' doit etre un nombre fini"), *Where, Key));
					return Default;
				}
				return V->Number;
			}

			bool Bool(const FValue& Obj, const TCHAR* Key, const FString& Where, bool Default) const
			{
				const FValue* V = Field(Obj, Key, Where, false);
				if (!V)
				{
					return Default;
				}
				if (!V->IsBool())
				{
					Errors.Add(FString::Printf(TEXT("%s : '%s' doit etre un booleen"), *Where, Key));
					return Default;
				}
				return V->bBool;
			}

			TArray<FString> Strings(const FValue& Obj, const TCHAR* Key, const FString& Where) const
			{
				TArray<FString> Out;
				const FValue* V = Field(Obj, Key, Where, false);
				if (!V)
				{
					return Out;
				}
				if (!V->IsArray())
				{
					Errors.Add(FString::Printf(TEXT("%s : '%s' doit etre un tableau de chaines"), *Where, Key));
					return Out;
				}
				for (const FValue& Item : V->Items)
				{
					if (Item.IsString())
					{
						Out.Add(Item.String);
					}
					else
					{
						Errors.Add(FString::Printf(TEXT("%s : '%s' contient autre chose qu'une chaine"), *Where, Key));
					}
				}
				return Out;
			}

			/** Objet { "<canal>": nombre } : ecrit dans Out, refuse un canal inconnu. */
			void PressureMap(const FValue& Obj, const TCHAR* Key, const FString& Where, double (&Out)[PressureCount]) const
			{
				const FValue* V = Field(Obj, Key, Where, false);
				if (!V)
				{
					return;
				}
				if (!V->IsObject())
				{
					Errors.Add(FString::Printf(TEXT("%s : '%s' doit etre un objet { canal: nombre }"), *Where, Key));
					return;
				}
				for (int32 I = 0; I < V->Keys.Num(); ++I)
				{
					EPressure P;
					if (!ParsePressure(V->Keys[I], P))
					{
						Errors.Add(FString::Printf(TEXT("%s : canal inconnu '%s' dans '%s'"), *Where, *V->Keys[I], Key));
						continue;
					}
					if (!V->Items[I].IsNumber())
					{
						Errors.Add(FString::Printf(TEXT("%s : '%s.%s' doit etre un nombre"), *Where, Key, *V->Keys[I]));
						continue;
					}
					Out[static_cast<int32>(P)] = V->Items[I].Number;
				}
			}

			FProvenance Provenance(const FValue& Obj, const FString& Where) const
			{
				FProvenance P;
				const FValue* V = Field(Obj, TEXT("provenance"), Where, true);
				if (!V)
				{
					return P;
				}
				const FString W = Where + TEXT(".provenance");
				P.SourceId = Str(*V, TEXT("source"), W, true);
				P.SourceRef = Str(*V, TEXT("ref"), W, false);
				P.Confidence = Num(*V, TEXT("confidence"), W, true, 0.0);
				P.Note = Str(*V, TEXT("note"), W, false);
				const FString StatusText = Str(*V, TEXT("status"), W, true);
				if (!StatusText.IsEmpty() && !ParseStatus(StatusText, P.Status))
				{
					Errors.Add(FString::Printf(TEXT("%s : statut historique inconnu '%s' (VERIFIED, PLAUSIBLE, ABSTRACTION, UNKNOWN)"), *W, *StatusText));
				}
				return P;
			}
		};

		void ReadShock(const FReader& R, const FValue& S, const FString& Where, FShockDef& Out)
		{
			Out.Id = R.Str(S, TEXT("id"), Where, true);
			Out.Label = R.Str(S, TEXT("label"), Where, false, Out.Id);
			Out.Telling = R.Str(S, TEXT("recit"), Where, false);
			Out.SourceNode = R.Str(S, TEXT("source"), Where, true);
			Out.ActorId = R.Str(S, TEXT("actor"), Where, false);
			Out.ParentCauseId = R.Str(S, TEXT("parent"), Where, false);
			Out.StartDay = static_cast<int32>(R.Num(S, TEXT("startDay"), Where, true, 1.0));
			Out.DurationDays = static_cast<int32>(R.Num(S, TEXT("durationDays"), Where, false, 1.0));
			Out.InformationMagnitude = R.Num(S, TEXT("information"), Where, false, 0.0);
			Out.CauseTags = R.Strings(S, TEXT("tags"), Where);
			// NaN = canal absent : toute valeur ecrite, meme hors bornes, devient une emission que la
			// validation jugera (rien n'est ecarte en silence).
			double Emit[PressureCount];
			for (double& E : Emit)
			{
				E = std::numeric_limits<double>::quiet_NaN();
			}
			R.PressureMap(S, TEXT("emissions"), Where, Emit);
			for (int32 P = 0; P < PressureCount; ++P)
			{
				if (!FMath::IsNaN(Emit[P]))
				{
					FEmission E;
					E.Pressure = static_cast<EPressure>(P);
					E.Magnitude = Emit[P];
					Out.Emissions.Add(E);
				}
			}
			Out.Provenance = R.Provenance(S, Where);
		}

		// --- Ecriture JSON de l'etat ---

		FValue Str(const FString& S) { return FValue::MakeString(S); }
		FValue Num(double D) { return FValue::MakeNumber(D); }

		FValue StrArray(const TArray<FString>& Items)
		{
			FValue A = FValue::MakeArray();
			for (const FString& S : Items)
			{
				A.Items.Add(Str(S));
			}
			return A;
		}

		FValue ProvenanceToJson(const FProvenance& P)
		{
			FValue O = FValue::MakeObject();
			O.Set(TEXT("source"), Str(P.SourceId));
			O.Set(TEXT("ref"), Str(P.SourceRef));
			O.Set(TEXT("status"), Str(StatusName(P.Status)));
			O.Set(TEXT("confidence"), Num(P.Confidence));
			O.Set(TEXT("note"), Str(P.Note));
			return O;
		}

		FValue ShockToJson(const FShockDef& S)
		{
			FValue O = FValue::MakeObject();
			O.Set(TEXT("id"), Str(S.Id));
			O.Set(TEXT("label"), Str(S.Label));
			if (!S.Telling.IsEmpty()) O.Set(TEXT("recit"), Str(S.Telling));
			O.Set(TEXT("source"), Str(S.SourceNode));
			O.Set(TEXT("actor"), Str(S.ActorId));
			O.Set(TEXT("parent"), Str(S.ParentCauseId));
			O.Set(TEXT("startDay"), Num(S.StartDay));
			O.Set(TEXT("durationDays"), Num(S.DurationDays));
			FValue E = FValue::MakeObject();
			for (const FEmission& Em : S.Emissions)
			{
				E.Set(PressureName(Em.Pressure), Num(Em.Magnitude));
			}
			O.Set(TEXT("emissions"), E);
			O.Set(TEXT("information"), Num(S.InformationMagnitude));
			O.Set(TEXT("tags"), StrArray(S.CauseTags));
			O.Set(TEXT("provenance"), ProvenanceToJson(S.Provenance));
			return O;
		}

		FValue PressurePacketToJson(const FPressurePacket& P)
		{
			FValue O = FValue::MakeObject();
			O.Set(TEXT("id"), Str(P.Id));
			O.Set(TEXT("seq"), Num(static_cast<double>(P.Seq)));
			O.Set(TEXT("root"), Str(P.RootCauseId));
			O.Set(TEXT("parent"), Str(P.ParentPacketId));
			O.Set(TEXT("pressure"), Str(PressureName(P.Pressure)));
			O.Set(TEXT("magnitude"), Num(P.Magnitude));
			O.Set(TEXT("from"), Str(P.FromNode));
			O.Set(TEXT("to"), Str(P.ToNode));
			O.Set(TEXT("route"), Str(P.RouteId));
			O.Set(TEXT("departure"), Num(P.DepartureDay));
			O.Set(TEXT("arrival"), Num(P.ArrivalDay));
			O.Set(TEXT("hop"), Num(P.Hop));
			return O;
		}

		FValue InformationPacketToJson(const FInformationPacket& P)
		{
			FValue O = FValue::MakeObject();
			O.Set(TEXT("id"), Str(P.Id));
			O.Set(TEXT("seq"), Num(static_cast<double>(P.Seq)));
			O.Set(TEXT("root"), Str(P.RootCauseId));
			O.Set(TEXT("parent"), Str(P.ParentPacketId));
			O.Set(TEXT("origin"), Str(P.OriginNode));
			O.Set(TEXT("from"), Str(P.FromNode));
			O.Set(TEXT("to"), Str(P.ToNode));
			O.Set(TEXT("route"), Str(P.RouteId));
			O.Set(TEXT("creation"), Num(P.CreationDay));
			O.Set(TEXT("departure"), Num(P.DepartureDay));
			O.Set(TEXT("arrival"), Num(P.ArrivalDay));
			O.Set(TEXT("hop"), Num(P.Hop));
			O.Set(TEXT("tags"), StrArray(P.SubjectTags));
			O.Set(TEXT("reported"), Num(P.ReportedMagnitude));
			O.Set(TEXT("reliability"), Num(P.Reliability));
			O.Set(TEXT("sourceType"), Str(P.SourceType));
			return O;
		}

		FValue ReportToJson(const FKnownReport& K)
		{
			FValue O = FValue::MakeObject();
			O.Set(TEXT("id"), Str(K.Id));
			O.Set(TEXT("packet"), Str(K.PacketId));
			O.Set(TEXT("cause"), Str(K.CauseId));
			O.Set(TEXT("learnedDay"), Num(K.LearnedDay));
			O.Set(TEXT("eventDay"), Num(K.EventDay));
			O.Set(TEXT("origin"), Str(K.OriginNode));
			O.Set(TEXT("tags"), StrArray(K.SubjectTags));
			O.Set(TEXT("reported"), Num(K.ReportedMagnitude));
			O.Set(TEXT("reliability"), Num(K.Reliability));
			O.Set(TEXT("sourceType"), Str(K.SourceType));
			return O;
		}

		// --- Relecture de l'etat ---

		struct FStateReader
		{
			TArray<FString>& Errors;
			FReader R{ Errors };

			FPressurePacket Pressure(const FValue& O) const
			{
				FPressurePacket P;
				const FString W = TEXT("etat.paquet");
				P.Id = R.Str(O, TEXT("id"), W, true);
				P.Seq = static_cast<int64>(R.Num(O, TEXT("seq"), W, true, 0.0));
				P.RootCauseId = R.Str(O, TEXT("root"), W, true);
				P.ParentPacketId = R.Str(O, TEXT("parent"), W, false);
				if (!ParsePressure(R.Str(O, TEXT("pressure"), W, true), P.Pressure))
				{
					Errors.Add(TEXT("etat.paquet : canal inconnu"));
				}
				P.Magnitude = R.Num(O, TEXT("magnitude"), W, true, 0.0);
				P.FromNode = R.Str(O, TEXT("from"), W, false);
				P.ToNode = R.Str(O, TEXT("to"), W, true);
				P.RouteId = R.Str(O, TEXT("route"), W, false);
				P.DepartureDay = static_cast<int32>(R.Num(O, TEXT("departure"), W, true, 0.0));
				P.ArrivalDay = static_cast<int32>(R.Num(O, TEXT("arrival"), W, true, 0.0));
				P.Hop = static_cast<int32>(R.Num(O, TEXT("hop"), W, true, 0.0));
				return P;
			}

			FInformationPacket Information(const FValue& O) const
			{
				FInformationPacket P;
				const FString W = TEXT("etat.nouvelle");
				P.Id = R.Str(O, TEXT("id"), W, true);
				P.Seq = static_cast<int64>(R.Num(O, TEXT("seq"), W, true, 0.0));
				P.RootCauseId = R.Str(O, TEXT("root"), W, true);
				P.ParentPacketId = R.Str(O, TEXT("parent"), W, false);
				P.OriginNode = R.Str(O, TEXT("origin"), W, true);
				P.FromNode = R.Str(O, TEXT("from"), W, false);
				P.ToNode = R.Str(O, TEXT("to"), W, true);
				P.RouteId = R.Str(O, TEXT("route"), W, false);
				P.CreationDay = static_cast<int32>(R.Num(O, TEXT("creation"), W, true, 0.0));
				P.DepartureDay = static_cast<int32>(R.Num(O, TEXT("departure"), W, true, 0.0));
				P.ArrivalDay = static_cast<int32>(R.Num(O, TEXT("arrival"), W, true, 0.0));
				P.Hop = static_cast<int32>(R.Num(O, TEXT("hop"), W, true, 0.0));
				P.SubjectTags = R.Strings(O, TEXT("tags"), W);
				P.ReportedMagnitude = R.Num(O, TEXT("reported"), W, true, 0.0);
				P.Reliability = R.Num(O, TEXT("reliability"), W, true, 0.0);
				P.SourceType = R.Str(O, TEXT("sourceType"), W, false);
				return P;
			}

			FKnownReport Report(const FValue& O) const
			{
				FKnownReport K;
				const FString W = TEXT("etat.savoir");
				K.Id = R.Str(O, TEXT("id"), W, true);
				K.PacketId = R.Str(O, TEXT("packet"), W, false);
				K.CauseId = R.Str(O, TEXT("cause"), W, false);
				K.LearnedDay = static_cast<int32>(R.Num(O, TEXT("learnedDay"), W, true, 0.0));
				K.EventDay = static_cast<int32>(R.Num(O, TEXT("eventDay"), W, true, 0.0));
				K.OriginNode = R.Str(O, TEXT("origin"), W, false);
				K.SubjectTags = R.Strings(O, TEXT("tags"), W);
				K.ReportedMagnitude = R.Num(O, TEXT("reported"), W, true, 0.0);
				K.Reliability = R.Num(O, TEXT("reliability"), W, true, 0.0);
				K.SourceType = R.Str(O, TEXT("sourceType"), W, false);
				return K;
			}
		};

		const FValue* ArrayField(const FValue& Root, const TCHAR* Key, TArray<FString>& Errors)
		{
			const FValue* V = Root.Find(Key);
			if (!V || !V->IsArray())
			{
				Errors.Add(FString::Printf(TEXT("etat : tableau '%s' absent"), Key));
				return nullptr;
			}
			return V;
		}
	}

	const TCHAR* PressureName(EPressure Pressure)
	{
		const int32 I = static_cast<int32>(Pressure);
		return I >= 0 && I < PressureCount ? PressureNames[I] : TEXT("?");
	}

	const TCHAR* PressureMeaning(EPressure Pressure)
	{
		const int32 I = static_cast<int32>(Pressure);
		return I >= 0 && I < PressureCount ? PressureMeanings[I] : TEXT("?");
	}

	bool ParsePressure(const FString& Name, EPressure& Out)
	{
		for (int32 I = 0; I < PressureCount; ++I)
		{
			if (Name.Equals(PressureNames[I], ESearchCase::IgnoreCase))
			{
				Out = static_cast<EPressure>(I);
				return true;
			}
		}
		return false;
	}

	const TCHAR* StatusName(EHistoricalStatus Status)
	{
		switch (Status)
		{
		case EHistoricalStatus::Verified: return TEXT("VERIFIED");
		case EHistoricalStatus::Plausible: return TEXT("PLAUSIBLE");
		case EHistoricalStatus::Abstraction: return TEXT("ABSTRACTION");
		default: return TEXT("UNKNOWN");
		}
	}

	bool ParseStatus(const FString& Name, EHistoricalStatus& Out)
	{
		static const EHistoricalStatus All[] = { EHistoricalStatus::Verified, EHistoricalStatus::Plausible,
			EHistoricalStatus::Abstraction, EHistoricalStatus::Unknown };
		for (EHistoricalStatus S : All)
		{
			if (Name.Equals(StatusName(S), ESearchCase::IgnoreCase))
			{
				Out = S;
				return true;
			}
		}
		return false;
	}

	const FNode* FScenario::FindNode(const FString& NodeId) const
	{
		return Nodes.FindByPredicate([&](const FNode& N) { return N.Id == NodeId; });
	}

	const FRoute* FScenario::FindRoute(const FString& RouteId) const
	{
		return Routes.FindByPredicate([&](const FRoute& R) { return R.Id == RouteId; });
	}

	const FActor* FScenario::FindActor(const FString& ActorId) const
	{
		return Actors.FindByPredicate([&](const FActor& A) { return A.Id == ActorId; });
	}

	bool ParseScenario(const FString& Json, FScenario& Out, TArray<FString>& OutErrors)
	{
		Out = FScenario();
		FValue Root;
		FString ParseError;
		if (!AnastasisJson::Parse(Json, Root, ParseError))
		{
			OutErrors.Add(FString::Printf(TEXT("scenario : JSON illisible (%s)"), *ParseError));
			return false;
		}
		if (!Root.IsObject())
		{
			OutErrors.Add(TEXT("scenario : la racine doit etre un objet"));
			return false;
		}
		const int32 ErrorsBefore = OutErrors.Num();
		const FReader R{ OutErrors };

		Out.Id = R.Str(Root, TEXT("id"), TEXT("scenario"), true);
		Out.Label = R.Str(Root, TEXT("label"), TEXT("scenario"), false, Out.Id);
		Out.VillageNodeId = R.Str(Root, TEXT("villageNode"), TEXT("scenario"), true);
		Out.CalendarNote = R.Str(Root, TEXT("calendarNote"), TEXT("scenario"), false);
		Out.Provenance = R.Provenance(Root, TEXT("scenario"));

		if (const FValue* Rules = R.Field(Root, TEXT("rules"), TEXT("scenario"), true))
		{
			Out.MaxHops = static_cast<int32>(R.Num(*Rules, TEXT("maxHops"), TEXT("rules"), true, Out.MaxHops));
			Out.AdministrationDamping = R.Num(*Rules, TEXT("administrationDamping"), TEXT("rules"), true, Out.AdministrationDamping);
			if (const FValue* Channels = R.Field(*Rules, TEXT("channels"), TEXT("rules"), true))
			{
				for (int32 P = 0; P < PressureCount; ++P)
				{
					const FString W = FString::Printf(TEXT("rules.channels.%s"), PressureNames[P]);
					const FValue* C = R.Field(*Channels, PressureNames[P], TEXT("rules.channels"), true);
					if (!C)
					{
						continue;
					}
					FChannelRules& Rule = Out.Channels[P];
					Rule.RetentionPerDay = R.Num(*C, TEXT("retentionPerDay"), W, true, Rule.RetentionPerDay);
					Rule.SpeedFactor = R.Num(*C, TEXT("speedFactor"), W, true, Rule.SpeedFactor);
					Rule.HopAttenuation = R.Num(*C, TEXT("hopAttenuation"), W, true, Rule.HopAttenuation);
					Rule.MinMagnitude = R.Num(*C, TEXT("minMagnitude"), W, true, Rule.MinMagnitude);
				}
				for (int32 I = 0; I < Channels->Keys.Num(); ++I)
				{
					EPressure Ignored;
					if (!ParsePressure(Channels->Keys[I], Ignored))
					{
						OutErrors.Add(FString::Printf(TEXT("rules.channels : canal inconnu '%s'"), *Channels->Keys[I]));
					}
				}
			}
			if (const FValue* Info = R.Field(*Rules, TEXT("information"), TEXT("rules"), true))
			{
				const FString W = TEXT("rules.information");
				Out.Information.SpeedFactor = R.Num(*Info, TEXT("speedFactor"), W, true, Out.Information.SpeedFactor);
				Out.Information.ReliabilityPerHop = R.Num(*Info, TEXT("reliabilityPerHop"), W, true, Out.Information.ReliabilityPerHop);
				Out.Information.ExaggerationPerHop = R.Num(*Info, TEXT("exaggerationPerHop"), W, true, Out.Information.ExaggerationPerHop);
				Out.Information.MinReliability = R.Num(*Info, TEXT("minReliability"), W, true, Out.Information.MinReliability);
			}
			if (const FValue* Mig = R.Field(*Rules, TEXT("migration"), TEXT("rules"), true))
			{
				const FString W = TEXT("rules.migration");
				Out.Migration.PersonsPerUnit = R.Num(*Mig, TEXT("personsPerUnit"), W, true, Out.Migration.PersonsPerUnit);
				Out.Migration.MaxPersonsPerBatch = static_cast<int32>(R.Num(*Mig, TEXT("maxPersonsPerBatch"), W, true, Out.Migration.MaxPersonsPerBatch));
				Out.Migration.MinMagnitude = R.Num(*Mig, TEXT("minMagnitude"), W, true, Out.Migration.MinMagnitude);
			}
		}

		if (const FValue* Nodes = R.Field(Root, TEXT("nodes"), TEXT("scenario"), true))
		{
			for (int32 I = 0; I < Nodes->Items.Num(); ++I)
			{
				const FValue& N = Nodes->Items[I];
				const FString W = FString::Printf(TEXT("nodes[%d]"), I);
				FNode Node;
				Node.Id = R.Str(N, TEXT("id"), W, true);
				Node.Label = R.Str(N, TEXT("label"), W, false, Node.Id);
				Node.Type = R.Str(N, TEXT("type"), W, true);
				Node.Tags = R.Strings(N, TEXT("tags"), W);
				Node.PopulationWeight = R.Num(N, TEXT("populationWeight"), W, false, 0.0);
				Node.EconomicWeight = R.Num(N, TEXT("economicWeight"), W, false, 0.0);
				Node.StrategicWeight = R.Num(N, TEXT("strategicWeight"), W, false, 0.0);
				R.PressureMap(N, TEXT("baseline"), W, Node.Baseline);
				Node.Provenance = R.Provenance(N, W);
				Out.Nodes.Add(MoveTemp(Node));
			}
		}

		if (const FValue* Routes = R.Field(Root, TEXT("routes"), TEXT("scenario"), true))
		{
			for (int32 I = 0; I < Routes->Items.Num(); ++I)
			{
				const FValue& V = Routes->Items[I];
				const FString W = FString::Printf(TEXT("routes[%d]"), I);
				FRoute Route;
				Route.Id = R.Str(V, TEXT("id"), W, true);
				Route.From = R.Str(V, TEXT("from"), W, true);
				Route.To = R.Str(V, TEXT("to"), W, true);
				Route.TravelDays = R.Num(V, TEXT("travelDays"), W, true, Route.TravelDays);
				R.PressureMap(V, TEXT("transmission"), W, Route.Transmission);
				Route.InformationTransmission = R.Num(V, TEXT("information"), W, false, Route.InformationTransmission);
				Route.Capacity = R.Num(V, TEXT("capacity"), W, false, Route.Capacity);
				Route.BaseRisk = R.Num(V, TEXT("baseRisk"), W, false, Route.BaseRisk);
				Route.bEnabled = R.Bool(V, TEXT("enabled"), W, true);
				Route.bBidirectional = R.Bool(V, TEXT("bidirectional"), W, true);
				Route.Provenance = R.Provenance(V, W);
				Out.Routes.Add(MoveTemp(Route));
			}
		}

		if (const FValue* Actors = R.Field(Root, TEXT("actors"), TEXT("scenario"), false))
		{
			for (int32 I = 0; I < Actors->Items.Num(); ++I)
			{
				const FValue& V = Actors->Items[I];
				const FString W = FString::Printf(TEXT("actors[%d]"), I);
				FActor Actor;
				Actor.Id = R.Str(V, TEXT("id"), W, true);
				Actor.Label = R.Str(V, TEXT("label"), W, false, Actor.Id);
				Actor.Tags = R.Strings(V, TEXT("tags"), W);
				if (const FValue* Infl = R.Field(V, TEXT("influence"), W, false))
				{
					for (int32 J = 0; J < Infl->Items.Num(); ++J)
					{
						const FValue& E = Infl->Items[J];
						const FString WI = FString::Printf(TEXT("%s.influence[%d]"), *W, J);
						FInfluence Inf;
						const FString NodeId = R.Str(E, TEXT("node"), WI, true);
						Inf.Political = R.Num(E, TEXT("political"), WI, false, 0.0);
						Inf.Military = R.Num(E, TEXT("military"), WI, false, 0.0);
						Inf.Trade = R.Num(E, TEXT("trade"), WI, false, 0.0);
						Inf.Administrative = R.Num(E, TEXT("administrative"), WI, false, 0.0);
						Actor.InfluenceByNode.Add(TPair<FString, FInfluence>(NodeId, Inf));
					}
				}
				Actor.Provenance = R.Provenance(V, W);
				Out.Actors.Add(MoveTemp(Actor));
			}
		}

		if (const FValue* Shocks = R.Field(Root, TEXT("shocks"), TEXT("scenario"), false))
		{
			for (int32 I = 0; I < Shocks->Items.Num(); ++I)
			{
				FShockDef Shock;
				ReadShock(R, Shocks->Items[I], FString::Printf(TEXT("shocks[%d]"), I), Shock);
				Out.InitialShocks.Add(MoveTemp(Shock));
			}
		}

		if (OutErrors.Num() > ErrorsBefore)
		{
			return false;
		}
		return ValidateScenario(Out, OutErrors);
	}

	bool ValidateShock(const FScenario& Scenario, const FShockDef& Shock, TArray<FString>& OutErrors)
	{
		const int32 Before = OutErrors.Num();
		const FString W = FString::Printf(TEXT("choc '%s'"), *Shock.Id);
		if (!Scenario.FindNode(Shock.SourceNode))
		{
			OutErrors.Add(FString::Printf(TEXT("%s : noeud source inconnu '%s'"), *W, *Shock.SourceNode));
		}
		if (!Shock.ActorId.IsEmpty() && !Scenario.FindActor(Shock.ActorId))
		{
			OutErrors.Add(FString::Printf(TEXT("%s : acteur inconnu '%s'"), *W, *Shock.ActorId));
		}
		if (Shock.StartDay < 1)
		{
			OutErrors.Add(FString::Printf(TEXT("%s : jour de debut %d impossible (>= 1)"), *W, Shock.StartDay));
		}
		if (Shock.DurationDays < 1)
		{
			OutErrors.Add(FString::Printf(TEXT("%s : duree %d impossible (>= 1)"), *W, Shock.DurationDays));
		}
		if (!InUnit(Shock.InformationMagnitude))
		{
			OutErrors.Add(FString::Printf(TEXT("%s : information hors [0, 1]"), *W));
		}
		if (Shock.Emissions.Num() == 0 && Shock.InformationMagnitude <= 0.0)
		{
			OutErrors.Add(FString::Printf(TEXT("%s : n'emet rien (ni pression ni information)"), *W));
		}
		for (const FEmission& E : Shock.Emissions)
		{
			if (!InUnit(E.Magnitude))
			{
				OutErrors.Add(FString::Printf(TEXT("%s : emission %s hors [0, 1]"), *W, PressureName(E.Pressure)));
			}
		}
		if (!InUnit(Shock.Provenance.Confidence))
		{
			OutErrors.Add(FString::Printf(TEXT("%s : confiance hors [0, 1]"), *W));
		}
		return OutErrors.Num() == Before;
	}

	bool ValidateScenario(const FScenario& Scenario, TArray<FString>& OutErrors)
	{
		const int32 Before = OutErrors.Num();
		auto CheckUnit = [&](double V, const FString& What)
		{
			if (!InUnit(V))
			{
				OutErrors.Add(FString::Printf(TEXT("%s hors [0, 1] (%g)"), *What, V));
			}
		};

		if (Scenario.Id.IsEmpty())
		{
			OutErrors.Add(TEXT("scenario sans identifiant"));
		}
		if (Scenario.MaxHops < 1)
		{
			OutErrors.Add(TEXT("rules.maxHops doit etre >= 1"));
		}
		CheckUnit(Scenario.AdministrationDamping, TEXT("rules.administrationDamping"));
		for (int32 P = 0; P < PressureCount; ++P)
		{
			const FChannelRules& R = Scenario.Channels[P];
			const FString W = FString::Printf(TEXT("rules.channels.%s"), PressureName(static_cast<EPressure>(P)));
			CheckUnit(R.RetentionPerDay, W + TEXT(".retentionPerDay"));
			CheckUnit(R.HopAttenuation, W + TEXT(".hopAttenuation"));
			if (!(R.SpeedFactor > 0.0) || !FMath::IsFinite(R.SpeedFactor))
			{
				OutErrors.Add(W + TEXT(".speedFactor doit etre > 0"));
			}
			if (!(R.MinMagnitude > 0.0) || R.MinMagnitude > 1.0)
			{
				OutErrors.Add(W + TEXT(".minMagnitude doit etre dans ]0, 1]"));
			}
		}
		{
			const FInformationRules& I = Scenario.Information;
			if (!(I.SpeedFactor > 0.0) || !FMath::IsFinite(I.SpeedFactor))
			{
				OutErrors.Add(TEXT("rules.information.speedFactor doit etre > 0"));
			}
			CheckUnit(I.ReliabilityPerHop, TEXT("rules.information.reliabilityPerHop"));
			if (!(I.ExaggerationPerHop >= 0.0) || !FMath::IsFinite(I.ExaggerationPerHop))
			{
				OutErrors.Add(TEXT("rules.information.exaggerationPerHop doit etre >= 0"));
			}
			if (!(I.MinReliability > 0.0) || I.MinReliability > 1.0)
			{
				OutErrors.Add(TEXT("rules.information.minReliability doit etre dans ]0, 1]"));
			}
		}
		if (!(Scenario.Migration.PersonsPerUnit >= 0.0) || Scenario.Migration.MaxPersonsPerBatch < 0)
		{
			OutErrors.Add(TEXT("rules.migration : personsPerUnit et maxPersonsPerBatch doivent etre >= 0"));
		}
		CheckUnit(Scenario.Migration.MinMagnitude, TEXT("rules.migration.minMagnitude"));

		TSet<FString> NodeIds;
		for (const FNode& N : Scenario.Nodes)
		{
			if (N.Id.IsEmpty())
			{
				OutErrors.Add(TEXT("noeud sans identifiant"));
				continue;
			}
			if (NodeIds.Contains(N.Id))
			{
				OutErrors.Add(FString::Printf(TEXT("noeud en double '%s'"), *N.Id));
			}
			NodeIds.Add(N.Id);
			for (int32 P = 0; P < PressureCount; ++P)
			{
				CheckUnit(N.Baseline[P], FString::Printf(TEXT("noeud '%s' base %s"), *N.Id, PressureName(static_cast<EPressure>(P))));
			}
			CheckUnit(N.Provenance.Confidence, FString::Printf(TEXT("noeud '%s' confiance"), *N.Id));
		}
		if (Scenario.VillageNodeId.IsEmpty() || !NodeIds.Contains(Scenario.VillageNodeId))
		{
			OutErrors.Add(FString::Printf(TEXT("ancre du village absente ou inconnue ('%s')"), *Scenario.VillageNodeId));
		}

		TSet<FString> RouteIds;
		for (const FRoute& R : Scenario.Routes)
		{
			const FString W = FString::Printf(TEXT("route '%s'"), *R.Id);
			if (R.Id.IsEmpty())
			{
				OutErrors.Add(TEXT("route sans identifiant"));
			}
			else if (RouteIds.Contains(R.Id))
			{
				OutErrors.Add(FString::Printf(TEXT("route en double '%s'"), *R.Id));
			}
			RouteIds.Add(R.Id);
			if (!NodeIds.Contains(R.From))
			{
				OutErrors.Add(FString::Printf(TEXT("%s : depart inconnu '%s'"), *W, *R.From));
			}
			if (!NodeIds.Contains(R.To))
			{
				OutErrors.Add(FString::Printf(TEXT("%s : arrivee inconnue '%s'"), *W, *R.To));
			}
			if (R.From == R.To)
			{
				OutErrors.Add(FString::Printf(TEXT("%s : boucle sur un seul noeud"), *W));
			}
			if (!(R.TravelDays > 0.0) || !FMath::IsFinite(R.TravelDays))
			{
				OutErrors.Add(FString::Printf(TEXT("%s : temps de trajet doit etre > 0 (%g)"), *W, R.TravelDays));
			}
			for (int32 P = 0; P < PressureCount; ++P)
			{
				CheckUnit(R.Transmission[P], FString::Printf(TEXT("%s transmission %s"), *W, PressureName(static_cast<EPressure>(P))));
			}
			CheckUnit(R.InformationTransmission, W + TEXT(" transmission information"));
			CheckUnit(R.Capacity, W + TEXT(" capacite"));
			CheckUnit(R.BaseRisk, W + TEXT(" risque"));
			CheckUnit(R.Provenance.Confidence, W + TEXT(" confiance"));
		}

		TSet<FString> ActorIds;
		for (const FActor& A : Scenario.Actors)
		{
			if (A.Id.IsEmpty())
			{
				OutErrors.Add(TEXT("acteur sans identifiant"));
				continue;
			}
			if (ActorIds.Contains(A.Id))
			{
				OutErrors.Add(FString::Printf(TEXT("acteur en double '%s'"), *A.Id));
			}
			ActorIds.Add(A.Id);
			for (const TPair<FString, FInfluence>& E : A.InfluenceByNode)
			{
				const FString W = FString::Printf(TEXT("acteur '%s' influence sur '%s'"), *A.Id, *E.Key);
				if (!NodeIds.Contains(E.Key))
				{
					OutErrors.Add(W + TEXT(" : noeud inconnu"));
				}
				CheckUnit(E.Value.Political, W + TEXT(" politique"));
				CheckUnit(E.Value.Military, W + TEXT(" militaire"));
				CheckUnit(E.Value.Trade, W + TEXT(" commerce"));
				CheckUnit(E.Value.Administrative, W + TEXT(" administration"));
			}
			CheckUnit(A.Provenance.Confidence, FString::Printf(TEXT("acteur '%s' confiance"), *A.Id));
		}

		TSet<FString> ShockIds;
		for (const FShockDef& S : Scenario.InitialShocks)
		{
			if (S.Id.IsEmpty())
			{
				OutErrors.Add(TEXT("choc initial sans identifiant"));
			}
			else if (ShockIds.Contains(S.Id))
			{
				OutErrors.Add(FString::Printf(TEXT("identifiant causal en double '%s'"), *S.Id));
			}
			ShockIds.Add(S.Id);
			ValidateShock(Scenario, S, OutErrors);
		}
		// Une cause mere doit etre declaree AVANT son choc derive (ordre d'injection).
		TSet<FString> Declared;
		for (const FShockDef& S : Scenario.InitialShocks)
		{
			if (!S.ParentCauseId.IsEmpty() && !Declared.Contains(S.ParentCauseId))
			{
				OutErrors.Add(FString::Printf(TEXT("choc '%s' : cause mere '%s' inconnue ou declaree apres lui"), *S.Id, *S.ParentCauseId));
			}
			Declared.Add(S.Id);
		}
		return OutErrors.Num() == Before;
	}

	// --- FGeoWorld -------------------------------------------------------------------------------

	void FGeoWorld::ResetRuntime()
	{
		NodePressure.Reset();
		RouteEnabled.Reset();
		Shocks.Reset();
		ShockEmitted.Reset();
		PressureQueue.Reset();
		InformationQueue.Reset();
		PressureArchive.Reset();
		PressureArchiveIndex.Reset();
		Arrivals.Reset();
		Visited.Reset();
		Knowledge.Reset();
		Batches.Reset();
		Exposure = FVillageExposure();
		PendingEvents.Reset();
		NextSeq = 1;
		NextShockId = 1;
		NextPacketId = 1;
		NextInformationId = 1;
		NextReportId = 1;
		NextBatchId = 1;
	}

	void FGeoWorld::Unload()
	{
		bLoaded = false;
		Scenario = FScenario();
		Day = 0;
		ResetRuntime();
	}

	bool FGeoWorld::Load(const FScenario& InScenario, int32 InDay, TArray<FString>& OutErrors)
	{
		Unload();
		if (!ValidateScenario(InScenario, OutErrors))
		{
			return false;
		}
		Scenario = InScenario;
		Day = InDay;
		NodePressure.SetNum(Scenario.Nodes.Num());
		for (int32 N = 0; N < Scenario.Nodes.Num(); ++N)
		{
			NodePressure[N].SetNum(PressureCount);
			for (int32 P = 0; P < PressureCount; ++P)
			{
				NodePressure[N][P] = Scenario.Nodes[N].Baseline[P];
			}
		}
		RouteEnabled.SetNum(Scenario.Routes.Num());
		for (int32 R = 0; R < Scenario.Routes.Num(); ++R)
		{
			RouteEnabled[R] = Scenario.Routes[R].bEnabled;
		}
		bLoaded = true;
		Event(FString::Printf(TEXT("load scenario=%s day=%d nodes=%d routes=%d actors=%d shocks=%d village=%s"),
			*Scenario.Id, Day, Scenario.Nodes.Num(), Scenario.Routes.Num(), Scenario.Actors.Num(),
			Scenario.InitialShocks.Num(), *Scenario.VillageNodeId));
		for (const FShockDef& S : Scenario.InitialShocks)
		{
			TArray<FString> ShockErrors;
			if (InjectShock(S, ShockErrors).IsEmpty())
			{
				OutErrors.Append(ShockErrors);
				Unload();
				return false;
			}
		}
		RefreshExposure();
		return true;
	}

	FString FGeoWorld::InjectShock(const FShockDef& InShock, TArray<FString>& OutErrors)
	{
		if (!bLoaded)
		{
			OutErrors.Add(TEXT("aucun scenario charge"));
			return FString();
		}
		FShockDef Shock = InShock;
		if (Shock.Id.IsEmpty())
		{
			do
			{
				Shock.Id = FString::Printf(TEXT("shock-%d"), NextShockId++);
			}
			while (FindShock(Shock.Id));
		}
		else if (FindShock(Shock.Id))
		{
			OutErrors.Add(FString::Printf(TEXT("identifiant causal en double '%s'"), *Shock.Id));
			return FString();
		}
		if (!ValidateShock(Scenario, Shock, OutErrors))
		{
			return FString();
		}
		if (!Shock.ParentCauseId.IsEmpty() && !FindShock(Shock.ParentCauseId))
		{
			OutErrors.Add(FString::Printf(TEXT("cause mere inconnue '%s'"), *Shock.ParentCauseId));
			return FString();
		}
		Shocks.Add(Shock);
		ShockEmitted.Add(false);
		Event(FString::Printf(TEXT("inject shock=%s source=%s start=%d duration=%d info=%s status=%s src=%s"),
			*Shock.Id, *Shock.SourceNode, Shock.StartDay, Shock.DurationDays, *Fmt(Shock.InformationMagnitude),
			StatusName(Shock.Provenance.Status), *Shock.Provenance.SourceId));
		// Un choc deja commence emet au jour courant ; les autres attendent leur jour.
		if (Shock.StartDay <= Day)
		{
			EmitShock(Shocks.Num() - 1, Day);
			RefreshExposure();
		}
		return Shock.Id;
	}

	bool FGeoWorld::SetRouteEnabled(const FString& RouteId, bool bEnabled)
	{
		for (int32 R = 0; R < Scenario.Routes.Num(); ++R)
		{
			if (Scenario.Routes[R].Id == RouteId)
			{
				RouteEnabled[R] = bEnabled;
				Event(FString::Printf(TEXT("route %s %s day=%d"), *RouteId, bEnabled ? TEXT("open") : TEXT("closed"), Day));
				return true;
			}
		}
		return false;
	}

	void FGeoWorld::AdvanceToDay(int32 ToDay)
	{
		if (!bLoaded)
		{
			return;
		}
		while (Day < ToDay)
		{
			StepDay(Day + 1);
		}
	}

	void FGeoWorld::StepDay(int32 D)
	{
		Day = D;
		// 1. Le temps use ce qui pesait hier.
		ApplyDecay();
		// 2. Les causes du jour emettent.
		for (int32 S = 0; S < Shocks.Num(); ++S)
		{
			if (!ShockEmitted[S] && Shocks[S].StartDay <= D)
			{
				EmitShock(S, D);
			}
		}
		// 3. Ce qui etait en route arrive.
		ProcessArrivals(D);
		RefreshExposure();
	}

	bool FGeoWorld::IsShockActive(const FShockDef& Shock) const
	{
		return Day >= Shock.StartDay && Day < Shock.StartDay + Shock.DurationDays;
	}

	void FGeoWorld::ApplyDecay()
	{
		for (int32 N = 0; N < Scenario.Nodes.Num(); ++N)
		{
			for (int32 P = 0; P < PressureCount; ++P)
			{
				const double Base = Scenario.Nodes[N].Baseline[P];
				double& Value = NodePressure[N][P];
				Value = Base + (Value - Base) * Scenario.Channels[P].RetentionPerDay;
			}
		}
		// Un choc qui dure tient sa source : la pression n'y redescend pas sous ce qu'il emet.
		for (int32 S = 0; S < Shocks.Num(); ++S)
		{
			const FShockDef& Shock = Shocks[S];
			if (!ShockEmitted[S] || !IsShockActive(Shock))
			{
				continue;
			}
			const int32 N = NodeIndex(Shock.SourceNode);
			for (const FEmission& E : Shock.Emissions)
			{
				double& Value = NodePressure[N][static_cast<int32>(E.Pressure)];
				Value = FMath::Max(Value, E.Magnitude);
			}
		}
	}

	void FGeoWorld::EmitShock(int32 ShockIndex, int32 D)
	{
		ShockEmitted[ShockIndex] = true;
		const FShockDef& Shock = Shocks[ShockIndex];
		Event(FString::Printf(TEXT("shock %s begins day=%d at %s (%s)"), *Shock.Id, D, *Shock.SourceNode, *Shock.Label));
		// Chaque emission est un paquet qui « arrive » a la source le jour meme : la source est
		// traitee comme n'importe quel noeud (pression, trace, propagation).
		for (const FEmission& E : Shock.Emissions)
		{
			if (E.Magnitude <= 0.0)
			{
				continue;
			}
			FPressurePacket P;
			P.Id = FString::Printf(TEXT("pk-%d"), NextPacketId++);
			P.Seq = NextSeq++;
			P.RootCauseId = Shock.Id;
			P.Pressure = E.Pressure;
			P.Magnitude = E.Magnitude;
			P.ToNode = Shock.SourceNode;
			P.DepartureDay = D;
			P.ArrivalDay = D;
			P.Hop = 0;
			PressureArchiveIndex.Add(P.Id, PressureArchive.Num());
			PressureArchive.Add(P);
			PressureQueue.Add(P);
		}
		if (Shock.InformationMagnitude > 0.0)
		{
			FInformationPacket I;
			I.Id = FString::Printf(TEXT("info-%d"), NextInformationId++);
			I.Seq = NextSeq++;
			I.RootCauseId = Shock.Id;
			I.OriginNode = Shock.SourceNode;
			I.ToNode = Shock.SourceNode;
			I.CreationDay = D;
			I.DepartureDay = D;
			I.ArrivalDay = D;
			I.SubjectTags = Shock.CauseTags;
			I.ReportedMagnitude = Shock.InformationMagnitude;
			I.Reliability = 1.0;
			I.SourceType = TEXT("origin");
			InformationQueue.Add(I);
		}
		ProcessArrivals(D);
	}

	void FGeoWorld::ProcessArrivals(int32 D)
	{
		// Les arrivees d'un jour peuvent en creer d'autres le meme jour (relais a temps nul) : on
		// boucle tant qu'il reste des paquets dus, toujours dans l'ordre (jour, seq).
		for (;;)
		{
			int32 BestPressure = INDEX_NONE;
			for (int32 I = 0; I < PressureQueue.Num(); ++I)
			{
				const FPressurePacket& P = PressureQueue[I];
				if (P.ArrivalDay <= D && (BestPressure == INDEX_NONE
					|| P.ArrivalDay < PressureQueue[BestPressure].ArrivalDay
					|| (P.ArrivalDay == PressureQueue[BestPressure].ArrivalDay && P.Seq < PressureQueue[BestPressure].Seq)))
				{
					BestPressure = I;
				}
			}
			int32 BestInfo = INDEX_NONE;
			for (int32 I = 0; I < InformationQueue.Num(); ++I)
			{
				const FInformationPacket& P = InformationQueue[I];
				if (P.ArrivalDay <= D && (BestInfo == INDEX_NONE
					|| P.ArrivalDay < InformationQueue[BestInfo].ArrivalDay
					|| (P.ArrivalDay == InformationQueue[BestInfo].ArrivalDay && P.Seq < InformationQueue[BestInfo].Seq)))
				{
					BestInfo = I;
				}
			}
			if (BestPressure == INDEX_NONE && BestInfo == INDEX_NONE)
			{
				return;
			}
			const bool bTakePressure = BestInfo == INDEX_NONE
				|| (BestPressure != INDEX_NONE && PressureQueue[BestPressure].Seq < InformationQueue[BestInfo].Seq);
			if (bTakePressure)
			{
				const FPressurePacket Packet = PressureQueue[BestPressure];
				PressureQueue.RemoveAt(BestPressure);
				ArrivePressure(Packet, D);
			}
			else
			{
				const FInformationPacket Packet = InformationQueue[BestInfo];
				InformationQueue.RemoveAt(BestInfo);
				ArriveInformation(Packet, D);
			}
		}
	}

	double FGeoWorld::MaxAdministrativeReach(const FString& NodeId) const
	{
		double Best = 0.0;
		for (const FActor& A : Scenario.Actors)
		{
			for (const TPair<FString, FInfluence>& E : A.InfluenceByNode)
			{
				if (E.Key == NodeId)
				{
					Best = FMath::Max(Best, E.Value.Administrative);
				}
			}
		}
		return Best;
	}

	void FGeoWorld::ArrivePressure(const FPressurePacket& Packet, int32 D)
	{
		// Toute arrivee pese sur le noeud (deux routes, deux courants). Seule la PREMIERE d'une meme
		// cause et d'un meme canal s'y relaie : la propagation reste finie, sans cycle, deterministe.
		const FString Key = FString::Printf(TEXT("%s|%s|%s"), *Packet.RootCauseId, PressureName(Packet.Pressure), *Packet.ToNode);
		const bool bFirst = !Visited.Contains(Key);
		Visited.Add(Key);

		const int32 N = NodeIndex(Packet.ToNode);
		const int32 P = static_cast<int32>(Packet.Pressure);
		double Magnitude = Packet.Magnitude;
		// Une administration presente amortit l'insecurite qui arrive (milice, juge, garnison).
		if (Packet.Pressure == EPressure::Insecurity && Packet.Hop > 0)
		{
			Magnitude *= 1.0 - Scenario.AdministrationDamping * MaxAdministrativeReach(Packet.ToNode);
		}
		FArrival A;
		A.PacketId = Packet.Id;
		A.NodeId = Packet.ToNode;
		A.Pressure = Packet.Pressure;
		A.Magnitude = Magnitude;
		A.Before = NodePressure[N][P];
		NodePressure[N][P] = Combine(A.Before, Magnitude);
		A.After = NodePressure[N][P];
		A.Day = D;
		Arrivals.Add(A);
		Event(FString::Printf(TEXT("arrive %s %s at %s day=%d m=%s %s->%s hop=%d cause=%s%s"), *Packet.Id, PressureName(Packet.Pressure),
			*Packet.ToNode, D, *Fmt(Magnitude), *Fmt(A.Before), *Fmt(A.After), Packet.Hop, *Packet.RootCauseId,
			bFirst ? TEXT("") : TEXT(" (second courant, sans relais)")));

		if (Packet.ToNode == Scenario.VillageNodeId)
		{
			// Le village VIT la pression : c'est aussi un savoir, de premiere main.
			FKnownReport K;
			K.PacketId = Packet.Id;
			K.CauseId = Packet.RootCauseId;
			K.LearnedDay = D;
			K.EventDay = D;
			K.OriginNode = Packet.FromNode;
			K.SubjectTags.Add(PressureName(Packet.Pressure));
			K.ReportedMagnitude = Magnitude;
			K.Reliability = 1.0;
			K.SourceType = TEXT("experience");
			AddKnowledge(K);

			if (Packet.Pressure == EPressure::Migration && Magnitude >= Scenario.Migration.MinMagnitude)
			{
				const int32 Persons = FMath::Min(Scenario.Migration.MaxPersonsPerBatch,
					static_cast<int32>(FMath::FloorToDouble(Magnitude * Scenario.Migration.PersonsPerUnit + 0.5)));
				if (Persons > 0)
				{
					FMigrationBatch B;
					B.Id = FString::Printf(TEXT("mig-%d"), NextBatchId++);
					B.CauseId = Packet.RootCauseId;
					B.PacketId = Packet.Id;
					B.OriginNode = Packet.FromNode;
					B.Day = D;
					B.Persons = Persons;
					B.Status = TEXT("pending");
					Batches.Add(B);
					Event(FString::Printf(TEXT("migration %s persons=%d day=%d from=%s cause=%s packet=%s"),
						*B.Id, Persons, D, *B.OriginNode, *B.CauseId, *B.PacketId));
				}
			}
		}
		if (bFirst)
		{
			PropagatePressure(Packet, D);
		}
	}

	void FGeoWorld::OutgoingRoutes(const FString& NodeId, TArray<TPair<int32, FString>>& Out) const
	{
		Out.Reset();
		for (int32 R = 0; R < Scenario.Routes.Num(); ++R)
		{
			const FRoute& Route = Scenario.Routes[R];
			if (!RouteEnabled[R])
			{
				continue;
			}
			if (Route.From == NodeId)
			{
				Out.Add(TPair<int32, FString>(R, Route.To));
			}
			else if (Route.bBidirectional && Route.To == NodeId)
			{
				Out.Add(TPair<int32, FString>(R, Route.From));
			}
		}
	}

	int32 FGeoWorld::TravelDays(const FRoute& Route, double SpeedFactor) const
	{
		// Au moins un jour : rien ne traverse une route dans la journee ou il part.
		return FMath::Max(1, static_cast<int32>(FMath::CeilToDouble(Route.TravelDays * SpeedFactor - 1e-9)));
	}

	void FGeoWorld::PropagatePressure(const FPressurePacket& Packet, int32 D)
	{
		if (Packet.Hop >= Scenario.MaxHops)
		{
			return;
		}
		const FChannelRules& Rules = Scenario.Channels[static_cast<int32>(Packet.Pressure)];
		TArray<TPair<int32, FString>> Out;
		OutgoingRoutes(Packet.ToNode, Out);
		for (const TPair<int32, FString>& Edge : Out)
		{
			const FRoute& Route = Scenario.Routes[Edge.Key];
			const FString& Next = Edge.Value;
			if (Next == Packet.FromNode)
			{
				continue;
			}
			const FString NextKey = FString::Printf(TEXT("%s|%s|%s"), *Packet.RootCauseId, PressureName(Packet.Pressure), *Next);
			if (Visited.Contains(NextKey))
			{
				continue;
			}
			double M = Packet.Magnitude * Route.Transmission[static_cast<int32>(Packet.Pressure)] * Rules.HopAttenuation;
			if (Packet.Pressure == EPressure::Insecurity)
			{
				M = FMath::Min(1.0, M * (1.0 + Route.BaseRisk));
			}
			if (Packet.Pressure == EPressure::Migration || Packet.Pressure == EPressure::Military)
			{
				M = FMath::Min(M, Route.Capacity);
			}
			if (M < Rules.MinMagnitude)
			{
				continue;
			}
			FPressurePacket Child;
			Child.Id = FString::Printf(TEXT("pk-%d"), NextPacketId++);
			Child.Seq = NextSeq++;
			Child.RootCauseId = Packet.RootCauseId;
			Child.ParentPacketId = Packet.Id;
			Child.Pressure = Packet.Pressure;
			Child.Magnitude = M;
			Child.FromNode = Packet.ToNode;
			Child.ToNode = Next;
			Child.RouteId = Route.Id;
			Child.DepartureDay = D;
			Child.ArrivalDay = D + TravelDays(Route, Rules.SpeedFactor);
			Child.Hop = Packet.Hop + 1;
			PressureArchiveIndex.Add(Child.Id, PressureArchive.Num());
			PressureArchive.Add(Child);
			PressureQueue.Add(Child);
		}
	}

	void FGeoWorld::ArriveInformation(const FInformationPacket& Packet, int32 D)
	{
		// Meme regle que la pression : la premiere nouvelle se relaie ; une seconde, venue par une autre
		// route, n'est qu'une corroboration au village.
		const FString Key = FString::Printf(TEXT("%s|info|%s"), *Packet.RootCauseId, *Packet.ToNode);
		const bool bFirst = !Visited.Contains(Key);
		Visited.Add(Key);
		if (Packet.ToNode == Scenario.VillageNodeId)
		{
			FKnownReport K;
			K.PacketId = Packet.Id;
			K.CauseId = Packet.RootCauseId;
			K.LearnedDay = D;
			K.EventDay = Packet.CreationDay;
			K.OriginNode = Packet.OriginNode;
			K.SubjectTags = Packet.SubjectTags;
			K.ReportedMagnitude = Packet.ReportedMagnitude;
			K.Reliability = Packet.Reliability;
			K.SourceType = bFirst ? TEXT("rumor") : TEXT("corroboration");
			AddKnowledge(K);
			Event(FString::Printf(TEXT("knowledge %s reaches village day=%d about=%s reported=%s reliability=%s hop=%d cause=%s"),
				*Packet.Id, D, *FString::Join(Packet.SubjectTags, TEXT(",")), *Fmt(Packet.ReportedMagnitude),
				*Fmt(Packet.Reliability), Packet.Hop, *Packet.RootCauseId));
		}
		if (bFirst)
		{
			PropagateInformation(Packet, D);
		}
	}

	void FGeoWorld::PropagateInformation(const FInformationPacket& Packet, int32 D)
	{
		if (Packet.Hop >= Scenario.MaxHops)
		{
			return;
		}
		const FInformationRules& Rules = Scenario.Information;
		TArray<TPair<int32, FString>> Out;
		OutgoingRoutes(Packet.ToNode, Out);
		for (const TPair<int32, FString>& Edge : Out)
		{
			const FRoute& Route = Scenario.Routes[Edge.Key];
			const FString& Next = Edge.Value;
			if (Next == Packet.FromNode || Visited.Contains(FString::Printf(TEXT("%s|info|%s"), *Packet.RootCauseId, *Next)))
			{
				continue;
			}
			const double Reliability = Packet.Reliability * Rules.ReliabilityPerHop * Route.InformationTransmission;
			if (Reliability < Rules.MinReliability)
			{
				continue;
			}
			FInformationPacket Child = Packet;
			Child.Id = FString::Printf(TEXT("info-%d"), NextInformationId++);
			Child.Seq = NextSeq++;
			Child.ParentPacketId = Packet.Id;
			Child.FromNode = Packet.ToNode;
			Child.ToNode = Next;
			Child.RouteId = Route.Id;
			Child.DepartureDay = D;
			Child.ArrivalDay = D + TravelDays(Route, Rules.SpeedFactor);
			Child.Hop = Packet.Hop + 1;
			Child.Reliability = Reliability;
			// La nouvelle grossit en route ; ce qu'elle dit ne remonte jamais a la verite.
			Child.ReportedMagnitude = FMath::Min(1.0, Packet.ReportedMagnitude * (1.0 + Rules.ExaggerationPerHop));
			Child.SourceType = TEXT("route");
			InformationQueue.Add(Child);
		}
	}

	void FGeoWorld::AddKnowledge(FKnownReport Report)
	{
		Report.Id = FString::Printf(TEXT("know-%d"), NextReportId++);
		Knowledge.Add(MoveTemp(Report));
	}

	void FGeoWorld::RefreshExposure()
	{
		Exposure = FVillageExposure();
		Exposure.Day = Day;
		const int32 N = NodeIndex(Scenario.VillageNodeId);
		if (N == INDEX_NONE)
		{
			return;
		}
		for (int32 P = 0; P < PressureCount; ++P)
		{
			Exposure.Pressure[P] = NodePressure[N][P];
		}
		constexpr int32 RecentCount = 8;
		for (int32 I = Knowledge.Num() - 1; I >= 0 && Exposure.RecentInformation.Num() < RecentCount; --I)
		{
			Exposure.RecentInformation.Add(Knowledge[I]);
		}
	}

	TArray<int32> FGeoWorld::PendingMigration() const
	{
		TArray<int32> Out;
		for (int32 I = 0; I < Batches.Num(); ++I)
		{
			if (Batches[I].Status == TEXT("pending"))
			{
				Out.Add(I);
			}
		}
		return Out;
	}

	void FGeoWorld::RecordAdmission(int32 BatchIndex, const TArray<FString>& AdmittedIds)
	{
		if (!Batches.IsValidIndex(BatchIndex))
		{
			return;
		}
		FMigrationBatch& B = Batches[BatchIndex];
		B.AdmittedIds = AdmittedIds;
		B.Status = AdmittedIds.Num() > 0 ? TEXT("admitted") : TEXT("blocked");
		Event(FString::Printf(TEXT("admission %s %s persons=%d admitted=%d ids=%s day=%d cause=%s"), *B.Id, *B.Status, B.Persons,
			AdmittedIds.Num(), *FString::Join(AdmittedIds, TEXT(",")), Day, *B.CauseId));
	}

	double FGeoWorld::GetTruePressure(const FString& NodeId, EPressure Pressure) const
	{
		const int32 N = NodeIndex(NodeId);
		return N == INDEX_NONE ? 0.0 : NodePressure[N][static_cast<int32>(Pressure)];
	}

	FInfluence FGeoWorld::GetInfluence(const FString& ActorId, const FString& NodeId) const
	{
		if (const FActor* A = Scenario.FindActor(ActorId))
		{
			for (const TPair<FString, FInfluence>& E : A->InfluenceByNode)
			{
				if (E.Key == NodeId)
				{
					return E.Value;
				}
			}
		}
		return FInfluence();
	}

	int32 FGeoWorld::NodeIndex(const FString& NodeId) const
	{
		for (int32 N = 0; N < Scenario.Nodes.Num(); ++N)
		{
			if (Scenario.Nodes[N].Id == NodeId)
			{
				return N;
			}
		}
		return INDEX_NONE;
	}

	const FShockDef* FGeoWorld::FindShock(const FString& ShockId) const
	{
		return Shocks.FindByPredicate([&](const FShockDef& S) { return S.Id == ShockId; });
	}

	void FGeoWorld::Event(const FString& Line)
	{
		PendingEvents.Add(Line);
	}

	TArray<FString> FGeoWorld::DrainEvents()
	{
		TArray<FString> Out = MoveTemp(PendingEvents);
		PendingEvents.Reset();
		return Out;
	}

	TArray<FCauseTrace> FGeoWorld::TraceCause(const FString& NodeId, EPressure Pressure, int32 MaxArrivals) const
	{
		TArray<FCauseTrace> Out;
		for (int32 I = Arrivals.Num() - 1; I >= 0 && Out.Num() < MaxArrivals; --I)
		{
			const FArrival& A = Arrivals[I];
			if (A.NodeId != NodeId || A.Pressure != Pressure)
			{
				continue;
			}
			FCauseTrace T;
			T.Arrival = A;
			FString Cursor = A.PacketId;
			// La lignee est finie et sans cycle (chaque paquet a un parent cree avant lui).
			for (int32 Guard = 0; Guard <= Scenario.MaxHops + 1 && !Cursor.IsEmpty(); ++Guard)
			{
				const int32* Index = PressureArchiveIndex.Find(Cursor);
				if (!Index)
				{
					break;
				}
				const FPressurePacket& P = PressureArchive[*Index];
				FTraceStep S;
				S.PacketId = P.Id;
				S.FromNode = P.FromNode;
				S.ToNode = P.ToNode;
				S.RouteId = P.RouteId;
				S.DepartureDay = P.DepartureDay;
				S.ArrivalDay = P.ArrivalDay;
				S.Magnitude = P.Magnitude;
				T.Steps.Add(S);
				T.RootCauseId = P.RootCauseId;
				Cursor = P.ParentPacketId;
			}
			if (const FShockDef* Root = FindShock(T.RootCauseId))
			{
				T.RootLabel = Root->Label;
				T.RootActorId = Root->ActorId;
				T.RootProvenance = Root->Provenance;
			}
			Out.Add(MoveTemp(T));
		}
		return Out;
	}

	FString FGeoWorld::SaveState() const
	{
		FValue Root = FValue::MakeObject();
		Root.Set(TEXT("version"), Num(1));
		Root.Set(TEXT("scenario"), Str(Scenario.Id));
		Root.Set(TEXT("day"), Num(Day));
		FValue Nodes = FValue::MakeObject();
		for (int32 N = 0; N < Scenario.Nodes.Num(); ++N)
		{
			FValue Values = FValue::MakeArray();
			for (int32 P = 0; P < PressureCount; ++P)
			{
				Values.Items.Add(Num(NodePressure[N][P]));
			}
			Nodes.Set(Scenario.Nodes[N].Id, Values);
		}
		Root.Set(TEXT("nodePressure"), Nodes);
		FValue Routes = FValue::MakeObject();
		for (int32 R = 0; R < Scenario.Routes.Num(); ++R)
		{
			Routes.Set(Scenario.Routes[R].Id, FValue::MakeBool(RouteEnabled[R]));
		}
		Root.Set(TEXT("routeEnabled"), Routes);
		FValue ShockArray = FValue::MakeArray();
		for (int32 S = 0; S < Shocks.Num(); ++S)
		{
			FValue O = ShockToJson(Shocks[S]);
			O.Set(TEXT("emitted"), FValue::MakeBool(ShockEmitted[S]));
			ShockArray.Items.Add(O);
		}
		Root.Set(TEXT("shocks"), ShockArray);
		FValue Queue = FValue::MakeArray();
		for (const FPressurePacket& P : PressureQueue)
		{
			Queue.Items.Add(PressurePacketToJson(P));
		}
		Root.Set(TEXT("pressureQueue"), Queue);
		FValue InfoQueue = FValue::MakeArray();
		for (const FInformationPacket& P : InformationQueue)
		{
			InfoQueue.Items.Add(InformationPacketToJson(P));
		}
		Root.Set(TEXT("informationQueue"), InfoQueue);
		FValue Archive = FValue::MakeArray();
		for (const FPressurePacket& P : PressureArchive)
		{
			Archive.Items.Add(PressurePacketToJson(P));
		}
		Root.Set(TEXT("pressureArchive"), Archive);
		FValue ArrivalArray = FValue::MakeArray();
		for (const FArrival& A : Arrivals)
		{
			FValue O = FValue::MakeObject();
			O.Set(TEXT("packet"), Str(A.PacketId));
			O.Set(TEXT("node"), Str(A.NodeId));
			O.Set(TEXT("pressure"), Str(PressureName(A.Pressure)));
			O.Set(TEXT("magnitude"), Num(A.Magnitude));
			O.Set(TEXT("before"), Num(A.Before));
			O.Set(TEXT("after"), Num(A.After));
			O.Set(TEXT("day"), Num(A.Day));
			ArrivalArray.Items.Add(O);
		}
		Root.Set(TEXT("arrivals"), ArrivalArray);
		// L'ensemble des cles atteintes, trie : l'ordre d'un TSet n'est pas un etat.
		TArray<FString> VisitedSorted = Visited.Array();
		VisitedSorted.Sort([](const FString& A, const FString& B) { return A.Compare(B, ESearchCase::CaseSensitive) < 0; });
		Root.Set(TEXT("visited"), StrArray(VisitedSorted));
		FValue KnowledgeArray = FValue::MakeArray();
		for (const FKnownReport& K : Knowledge)
		{
			KnowledgeArray.Items.Add(ReportToJson(K));
		}
		Root.Set(TEXT("knowledge"), KnowledgeArray);
		FValue BatchArray = FValue::MakeArray();
		for (const FMigrationBatch& B : Batches)
		{
			FValue O = FValue::MakeObject();
			O.Set(TEXT("id"), Str(B.Id));
			O.Set(TEXT("cause"), Str(B.CauseId));
			O.Set(TEXT("packet"), Str(B.PacketId));
			O.Set(TEXT("origin"), Str(B.OriginNode));
			O.Set(TEXT("day"), Num(B.Day));
			O.Set(TEXT("persons"), Num(B.Persons));
			O.Set(TEXT("status"), Str(B.Status));
			O.Set(TEXT("admitted"), StrArray(B.AdmittedIds));
			BatchArray.Items.Add(O);
		}
		Root.Set(TEXT("batches"), BatchArray);
		FValue Counters = FValue::MakeObject();
		Counters.Set(TEXT("seq"), Num(static_cast<double>(NextSeq)));
		Counters.Set(TEXT("shock"), Num(NextShockId));
		Counters.Set(TEXT("packet"), Num(NextPacketId));
		Counters.Set(TEXT("information"), Num(NextInformationId));
		Counters.Set(TEXT("report"), Num(NextReportId));
		Counters.Set(TEXT("batch"), Num(NextBatchId));
		Root.Set(TEXT("counters"), Counters);
		return AnastasisJson::Stringify(Root);
	}

	bool FGeoWorld::LoadState(const FScenario& InScenario, const FString& Json, TArray<FString>& OutErrors)
	{
		FValue Root;
		FString ParseError;
		if (!AnastasisJson::Parse(Json, Root, ParseError) || !Root.IsObject())
		{
			OutErrors.Add(FString::Printf(TEXT("etat : JSON illisible (%s)"), *ParseError));
			return false;
		}
		const int32 Before = OutErrors.Num();
		const FStateReader SR{ OutErrors };
		const FReader& R = SR.R;
		const FString ScenarioId = R.Str(Root, TEXT("scenario"), TEXT("etat"), true);
		if (ScenarioId != InScenario.Id)
		{
			OutErrors.Add(FString::Printf(TEXT("etat produit par le scenario '%s', pas '%s'"), *ScenarioId, *InScenario.Id));
			return false;
		}
		FGeoWorld Fresh;
		TArray<FString> LoadErrors;
		if (!Fresh.Load(InScenario, 0, LoadErrors))
		{
			OutErrors.Append(LoadErrors);
			return false;
		}
		// L'etat sauvegarde remplace tout ce que Load vient de poser (chocs initiaux compris).
		Fresh.ResetRuntime();
		Fresh.NodePressure.SetNum(InScenario.Nodes.Num());
		Fresh.RouteEnabled.SetNum(InScenario.Routes.Num());
		Fresh.Day = static_cast<int32>(R.Num(Root, TEXT("day"), TEXT("etat"), true, 0.0));

		const FValue* Nodes = R.Field(Root, TEXT("nodePressure"), TEXT("etat"), true);
		for (int32 N = 0; N < InScenario.Nodes.Num(); ++N)
		{
			Fresh.NodePressure[N].SetNum(PressureCount);
			const FValue* V = Nodes ? Nodes->Find(InScenario.Nodes[N].Id) : nullptr;
			if (!V || !V->IsArray() || V->Items.Num() != PressureCount)
			{
				OutErrors.Add(FString::Printf(TEXT("etat : pressions du noeud '%s' absentes ou mal formees"), *InScenario.Nodes[N].Id));
				continue;
			}
			for (int32 P = 0; P < PressureCount; ++P)
			{
				Fresh.NodePressure[N][P] = V->Items[P].Number;
			}
		}
		const FValue* Routes = R.Field(Root, TEXT("routeEnabled"), TEXT("etat"), true);
		for (int32 I = 0; I < InScenario.Routes.Num(); ++I)
		{
			const FValue* V = Routes ? Routes->Find(InScenario.Routes[I].Id) : nullptr;
			Fresh.RouteEnabled[I] = V && V->IsBool() ? V->bBool : InScenario.Routes[I].bEnabled;
		}
		if (const FValue* A = ArrayField(Root, TEXT("shocks"), OutErrors))
		{
			for (int32 I = 0; I < A->Items.Num(); ++I)
			{
				FShockDef S;
				ReadShock(R, A->Items[I], FString::Printf(TEXT("etat.shocks[%d]"), I), S);
				Fresh.Shocks.Add(S);
				Fresh.ShockEmitted.Add(R.Bool(A->Items[I], TEXT("emitted"), TEXT("etat.shocks"), false));
			}
		}
		if (const FValue* A = ArrayField(Root, TEXT("pressureQueue"), OutErrors))
		{
			for (const FValue& V : A->Items)
			{
				Fresh.PressureQueue.Add(SR.Pressure(V));
			}
		}
		if (const FValue* A = ArrayField(Root, TEXT("informationQueue"), OutErrors))
		{
			for (const FValue& V : A->Items)
			{
				Fresh.InformationQueue.Add(SR.Information(V));
			}
		}
		if (const FValue* A = ArrayField(Root, TEXT("pressureArchive"), OutErrors))
		{
			for (const FValue& V : A->Items)
			{
				const FPressurePacket P = SR.Pressure(V);
				Fresh.PressureArchiveIndex.Add(P.Id, Fresh.PressureArchive.Num());
				Fresh.PressureArchive.Add(P);
			}
		}
		if (const FValue* A = ArrayField(Root, TEXT("arrivals"), OutErrors))
		{
			for (const FValue& V : A->Items)
			{
				FArrival Arr;
				const FString W = TEXT("etat.arrivee");
				Arr.PacketId = R.Str(V, TEXT("packet"), W, true);
				Arr.NodeId = R.Str(V, TEXT("node"), W, true);
				ParsePressure(R.Str(V, TEXT("pressure"), W, true), Arr.Pressure);
				Arr.Magnitude = R.Num(V, TEXT("magnitude"), W, true, 0.0);
				Arr.Before = R.Num(V, TEXT("before"), W, true, 0.0);
				Arr.After = R.Num(V, TEXT("after"), W, true, 0.0);
				Arr.Day = static_cast<int32>(R.Num(V, TEXT("day"), W, true, 0.0));
				Fresh.Arrivals.Add(Arr);
			}
		}
		for (const FString& Key : R.Strings(Root, TEXT("visited"), TEXT("etat")))
		{
			Fresh.Visited.Add(Key);
		}
		if (const FValue* A = ArrayField(Root, TEXT("knowledge"), OutErrors))
		{
			for (const FValue& V : A->Items)
			{
				Fresh.Knowledge.Add(SR.Report(V));
			}
		}
		if (const FValue* A = ArrayField(Root, TEXT("batches"), OutErrors))
		{
			for (const FValue& V : A->Items)
			{
				FMigrationBatch B;
				const FString W = TEXT("etat.groupe");
				B.Id = R.Str(V, TEXT("id"), W, true);
				B.CauseId = R.Str(V, TEXT("cause"), W, true);
				B.PacketId = R.Str(V, TEXT("packet"), W, false);
				B.OriginNode = R.Str(V, TEXT("origin"), W, false);
				B.Day = static_cast<int32>(R.Num(V, TEXT("day"), W, true, 0.0));
				B.Persons = static_cast<int32>(R.Num(V, TEXT("persons"), W, true, 0.0));
				B.Status = R.Str(V, TEXT("status"), W, true);
				B.AdmittedIds = R.Strings(V, TEXT("admitted"), W);
				Fresh.Batches.Add(B);
			}
		}
		if (const FValue* C = R.Field(Root, TEXT("counters"), TEXT("etat"), true))
		{
			Fresh.NextSeq = static_cast<int64>(R.Num(*C, TEXT("seq"), TEXT("etat.counters"), true, 1.0));
			Fresh.NextShockId = static_cast<int32>(R.Num(*C, TEXT("shock"), TEXT("etat.counters"), true, 1.0));
			Fresh.NextPacketId = static_cast<int32>(R.Num(*C, TEXT("packet"), TEXT("etat.counters"), true, 1.0));
			Fresh.NextInformationId = static_cast<int32>(R.Num(*C, TEXT("information"), TEXT("etat.counters"), true, 1.0));
			Fresh.NextReportId = static_cast<int32>(R.Num(*C, TEXT("report"), TEXT("etat.counters"), true, 1.0));
			Fresh.NextBatchId = static_cast<int32>(R.Num(*C, TEXT("batch"), TEXT("etat.counters"), true, 1.0));
		}
		// Un paquet date d'avant le jour sauve aurait du etre traite : l'etat est corrompu.
		for (const FPressurePacket& P : Fresh.PressureQueue)
		{
			if (P.ArrivalDay <= Fresh.Day || Fresh.NodeIndex(P.ToNode) == INDEX_NONE)
			{
				OutErrors.Add(FString::Printf(TEXT("etat : paquet %s impossible (arrivee %d, jour %d, noeud '%s')"),
					*P.Id, P.ArrivalDay, Fresh.Day, *P.ToNode));
			}
		}
		for (const FInformationPacket& P : Fresh.InformationQueue)
		{
			if (P.ArrivalDay <= Fresh.Day || Fresh.NodeIndex(P.ToNode) == INDEX_NONE)
			{
				OutErrors.Add(FString::Printf(TEXT("etat : nouvelle %s impossible (arrivee %d, jour %d)"), *P.Id, P.ArrivalDay, Fresh.Day));
			}
		}
		if (OutErrors.Num() > Before)
		{
			return false;
		}
		Fresh.RefreshExposure();
		Fresh.PendingEvents.Reset();
		*this = MoveTemp(Fresh);
		Event(FString::Printf(TEXT("restore scenario=%s day=%d inTransit=%d+%d"), *Scenario.Id, Day, PressureQueue.Num(), InformationQueue.Num()));
		return true;
	}

	uint64 FGeoWorld::Digest() const
	{
		const FString State = SaveState();
		uint64 Hash = 14695981039346656037ull;
		for (const TCHAR C : State)
		{
			Hash ^= static_cast<uint64>(C);
			Hash *= 1099511628211ull;
		}
		return Hash;
	}

	FString FGeoWorld::DescribeStatus() const
	{
		if (!bLoaded)
		{
			return TEXT("ANASTASIS_GEO status: aucun scenario charge (Anastasis.Geo.Load)");
		}
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("ANASTASIS_GEO status scenario=%s day=%d village=%s nodes=%d routes=%d actors=%d shocks=%d inTransit=%d info=%d knowledge=%d batches=%d"),
			*Scenario.Id, Day, *Scenario.VillageNodeId, Scenario.Nodes.Num(), Scenario.Routes.Num(), Scenario.Actors.Num(), Shocks.Num(),
			PressureQueue.Num(), InformationQueue.Num(), Knowledge.Num(), Batches.Num()));
		for (int32 N = 0; N < Scenario.Nodes.Num(); ++N)
		{
			FString Values;
			for (int32 P = 0; P < PressureCount; ++P)
			{
				Values += FString::Printf(TEXT(" %s=%s"), PressureName(static_cast<EPressure>(P)), *Fmt(NodePressure[N][P]));
			}
			Lines.Add(FString::Printf(TEXT("  node %s [%s]%s"), *Scenario.Nodes[N].Id, *Scenario.Nodes[N].Type, *Values));
		}
		for (int32 R = 0; R < Scenario.Routes.Num(); ++R)
		{
			const FRoute& Route = Scenario.Routes[R];
			Lines.Add(FString::Printf(TEXT("  route %s %s%s%s %.1fd %s"), *Route.Id, *Route.From, Route.bBidirectional ? TEXT("<->") : TEXT("->"),
				*Route.To, Route.TravelDays, RouteEnabled[R] ? TEXT("open") : TEXT("CLOSED")));
		}
		for (const FActor& A : Scenario.Actors)
		{
			Lines.Add(FString::Printf(TEXT("  actor %s (%s) nodes=%d %s"), *A.Id, *A.Label, A.InfluenceByNode.Num(), StatusName(A.Provenance.Status)));
		}
		for (int32 S = 0; S < Shocks.Num(); ++S)
		{
			const FShockDef& Shock = Shocks[S];
			Lines.Add(FString::Printf(TEXT("  shock %s at %s day %d+%d %s%s"), *Shock.Id, *Shock.SourceNode, Shock.StartDay, Shock.DurationDays,
				ShockEmitted[S] ? TEXT("emitted") : TEXT("pending"), IsShockActive(Shock) ? TEXT(" active") : TEXT("")));
		}
		for (const FPressurePacket& P : PressureQueue)
		{
			Lines.Add(FString::Printf(TEXT("  transit %s %s %s->%s m=%s arrives day %d (cause %s)"), *P.Id, PressureName(P.Pressure), *P.FromNode,
				*P.ToNode, *Fmt(P.Magnitude), P.ArrivalDay, *P.RootCauseId));
		}
		for (const FInformationPacket& P : InformationQueue)
		{
			Lines.Add(FString::Printf(TEXT("  rumor %s %s->%s arrives day %d reliability=%s (cause %s)"), *P.Id, *P.FromNode, *P.ToNode,
				P.ArrivalDay, *Fmt(P.Reliability), *P.RootCauseId));
		}
		Lines.Add(DescribeExposure());
		return FString::Join(Lines, TEXT("\n"));
	}

	FString FGeoWorld::DescribeNode(const FString& NodeId) const
	{
		const int32 N = NodeIndex(NodeId);
		if (N == INDEX_NONE)
		{
			return FString::Printf(TEXT("ANASTASIS_GEO node %s: inconnu"), *NodeId);
		}
		const FNode& Node = Scenario.Nodes[N];
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("ANASTASIS_GEO node %s (%s) type=%s day=%d source=%s %s %s"), *Node.Id, *Node.Label, *Node.Type, Day,
			*Node.Provenance.SourceId, *Node.Provenance.SourceRef, StatusName(Node.Provenance.Status)));
		for (int32 P = 0; P < PressureCount; ++P)
		{
			Lines.Add(FString::Printf(TEXT("  %s=%s (base %s)"), PressureName(static_cast<EPressure>(P)), *Fmt(NodePressure[N][P]), *Fmt(Node.Baseline[P])));
		}
		for (const FActor& A : Scenario.Actors)
		{
			for (const TPair<FString, FInfluence>& E : A.InfluenceByNode)
			{
				if (E.Key == NodeId)
				{
					Lines.Add(FString::Printf(TEXT("  influence %s political=%s military=%s trade=%s administrative=%s"), *A.Id,
						*Fmt(E.Value.Political), *Fmt(E.Value.Military), *Fmt(E.Value.Trade), *Fmt(E.Value.Administrative)));
				}
			}
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FString FGeoWorld::DescribeExposure() const
	{
		FString Values;
		for (int32 P = 0; P < PressureCount; ++P)
		{
			Values += FString::Printf(TEXT(" %s=%s"), PressureName(static_cast<EPressure>(P)), *Fmt(Exposure.Pressure[P]));
		}
		FString Text = FString::Printf(TEXT("ANASTASIS_GEO exposure day=%d%s knowledge=%d"), Exposure.Day, *Values, Knowledge.Num());
		for (const FKnownReport& K : Exposure.RecentInformation)
		{
			Text += FString::Printf(TEXT("\n  knows %s %s learned=%d event=%d about=%s reported=%s reliability=%s"), *K.Id, *K.SourceType,
				K.LearnedDay, K.EventDay, *FString::Join(K.SubjectTags, TEXT(",")), *Fmt(K.ReportedMagnitude), *Fmt(K.Reliability));
		}
		return Text;
	}

	FString FGeoWorld::DescribeTrace(const FString& NodeId, EPressure Pressure) const
	{
		const TArray<FCauseTrace> Traces = TraceCause(NodeId, Pressure);
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("ANASTASIS_GEO trace %s %s=%s day=%d arrivals=%d"), *NodeId, PressureName(Pressure),
			*Fmt(GetTruePressure(NodeId, Pressure)), Day, Traces.Num()));
		for (const FCauseTrace& T : Traces)
		{
			Lines.Add(FString::Printf(TEXT("  arrival day %d m=%s (%s->%s) via %s"), T.Arrival.Day, *Fmt(T.Arrival.Magnitude), *Fmt(T.Arrival.Before),
				*Fmt(T.Arrival.After), *T.Arrival.PacketId));
			for (const FTraceStep& S : T.Steps)
			{
				Lines.Add(S.RouteId.IsEmpty()
					? FString::Printf(TEXT("    <- %s emitted at %s day %d m=%s"), *S.PacketId, *S.ToNode, S.DepartureDay, *Fmt(S.Magnitude))
					: FString::Printf(TEXT("    <- %s %s->%s route %s day %d->%d m=%s"), *S.PacketId, *S.FromNode, *S.ToNode, *S.RouteId,
						S.DepartureDay, S.ArrivalDay, *Fmt(S.Magnitude)));
			}
			Lines.Add(FString::Printf(TEXT("    root %s \"%s\" actor=%s source=%s %s %s confidence=%s"), *T.RootCauseId, *T.RootLabel,
				T.RootActorId.IsEmpty() ? TEXT("-") : *T.RootActorId, *T.RootProvenance.SourceId, *T.RootProvenance.SourceRef,
				StatusName(T.RootProvenance.Status), *Fmt(T.RootProvenance.Confidence)));
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FString FGeoWorld::StatusJson() const
	{
		FValue Root = FValue::MakeObject();
		Root.Set(TEXT("loaded"), FValue::MakeBool(bLoaded));
		Root.Set(TEXT("scenario"), Str(Scenario.Id));
		Root.Set(TEXT("day"), Num(Day));
		Root.Set(TEXT("village"), Str(Scenario.VillageNodeId));
		FValue Truth = FValue::MakeObject();
		for (int32 N = 0; N < Scenario.Nodes.Num() && bLoaded; ++N)
		{
			FValue O = FValue::MakeObject();
			for (int32 P = 0; P < PressureCount; ++P)
			{
				O.Set(PressureName(static_cast<EPressure>(P)), Num(NodePressure[N][P]));
			}
			Truth.Set(Scenario.Nodes[N].Id, O);
		}
		Root.Set(TEXT("truth"), Truth);
		FValue Exp = FValue::MakeObject();
		for (int32 P = 0; P < PressureCount; ++P)
		{
			Exp.Set(PressureName(static_cast<EPressure>(P)), Num(Exposure.Pressure[P]));
		}
		Root.Set(TEXT("exposure"), Exp);
		FValue KnowledgeArray = FValue::MakeArray();
		for (const FKnownReport& K : Knowledge)
		{
			KnowledgeArray.Items.Add(ReportToJson(K));
		}
		Root.Set(TEXT("knowledge"), KnowledgeArray);
		FValue BatchArray = FValue::MakeArray();
		for (const FMigrationBatch& B : Batches)
		{
			FValue O = FValue::MakeObject();
			O.Set(TEXT("id"), Str(B.Id));
			O.Set(TEXT("cause"), Str(B.CauseId));
			O.Set(TEXT("packet"), Str(B.PacketId));
			O.Set(TEXT("day"), Num(B.Day));
			O.Set(TEXT("persons"), Num(B.Persons));
			O.Set(TEXT("status"), Str(B.Status));
			O.Set(TEXT("admitted"), StrArray(B.AdmittedIds));
			BatchArray.Items.Add(O);
		}
		Root.Set(TEXT("batches"), BatchArray);
		FValue VillageArrivals = FValue::MakeArray();
		for (const FArrival& A : Arrivals)
		{
			if (A.NodeId != Scenario.VillageNodeId)
			{
				continue;
			}
			const int32* Index = PressureArchiveIndex.Find(A.PacketId);
			FValue O = FValue::MakeObject();
			O.Set(TEXT("day"), Num(A.Day));
			O.Set(TEXT("pressure"), Str(PressureName(A.Pressure)));
			O.Set(TEXT("magnitude"), Num(A.Magnitude));
			O.Set(TEXT("after"), Num(A.After));
			O.Set(TEXT("packet"), Str(A.PacketId));
			O.Set(TEXT("cause"), Str(Index ? PressureArchive[*Index].RootCauseId : FString()));
			O.Set(TEXT("from"), Str(Index ? PressureArchive[*Index].FromNode : FString()));
			VillageArrivals.Items.Add(O);
		}
		Root.Set(TEXT("villageArrivals"), VillageArrivals);
		Root.Set(TEXT("inTransit"), Num(PressureQueue.Num()));
		Root.Set(TEXT("informationInTransit"), Num(InformationQueue.Num()));
		Root.Set(TEXT("arrivals"), Num(Arrivals.Num()));
		return AnastasisJson::Stringify(Root);
	}
}
