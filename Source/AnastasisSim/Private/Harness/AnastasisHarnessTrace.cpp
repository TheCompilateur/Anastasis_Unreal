#include "Harness/AnastasisHarnessTrace.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Core/AnastasisStateDigest.h"
#include "Sim/AnastasisSimulation.h"

namespace AnastasisHarnessTrace
{
	namespace
	{
		using AnastasisJson::FValue;

		/** `DIGEST_SPEC_VERSION` de state-digest.mjs : le format que FStateWriter sait produire. */
		constexpr int32 DigestSpecVersion = 1;

		bool StringField(const FValue& Obj, const TCHAR* Key, FString& Out)
		{
			const FValue* V = Obj.Find(Key);
			if (!V || !V->IsString()) return false;
			Out = V->String;
			return true;
		}

		FValue Hex(uint64 Digest) { return FValue::MakeString(AnastasisDigest::ToHex(Digest)); }
	}

	bool LoadScenario(const FString& Path, FScenarioInfo& OutInfo, AnastasisJsSave::FState& OutState, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutError = FString::Printf(TEXT("scenario illisible : %s"), *Path);
			return false;
		}
		FValue Root;
		if (!AnastasisJson::Parse(Text, Root, OutError))
		{
			return false;
		}
		FString Kind;
		if (!StringField(Root, TEXT("kind"), Kind) || Kind != TEXT("anastasis-scenario"))
		{
			OutError = TEXT("pas un scenario (kind != anastasis-scenario)");
			return false;
		}
		if (!StringField(Root, TEXT("name"), OutInfo.Name) || !StringField(Root, TEXT("empreinte"), OutInfo.Empreinte)
			|| !StringField(Root, TEXT("dayDeferred"), OutInfo.DayDeferred))
		{
			OutError = TEXT("scenario : name, empreinte ou dayDeferred absent");
			return false;
		}
		// L'hote C++ draine 2 travaux de minuit par tick, comme le jeu ; il n'a pas
		// de vidage apres chaque tick (mode `flush` de la trace sans scenario).
		if (OutInfo.DayDeferred != TEXT("tick"))
		{
			OutError = FString::Printf(TEXT("dayDeferred=%s : l'hote C++ ne sait rejouer que `tick`"), *OutInfo.DayDeferred);
			return false;
		}
		if (const FValue* Reference = Root.Find(TEXT("reference")))
		{
			StringField(*Reference, TEXT("commit"), OutInfo.ReferenceCommit);
		}
		const FValue* Dt = Root.Find(TEXT("dt"));
		if (!Dt || !Dt->IsNumber() || !(Dt->Number > 0.0))
		{
			OutError = TEXT("scenario : dt absent ou non positif");
			return false;
		}
		OutInfo.Dt = Dt->Number;
		const FValue* Sections = Root.Find(TEXT("sections"));
		if (!Sections || !Sections->IsArray())
		{
			OutError = TEXT("scenario : sections absentes");
			return false;
		}
		for (const FValue& S : Sections->Items)
		{
			if (!S.IsString() || !AnastasisJsSave::PortedSections().Contains(S.String))
			{
				OutError = FString::Printf(TEXT("section `%s` du perimetre : le lecteur C++ ne la projette pas"), S.IsString() ? *S.String : TEXT("?"));
				return false;
			}
			OutInfo.Sections.Add(S.String);
		}
		const FValue* Masques = Root.Find(TEXT("masques"));
		if (!Masques || !Masques->IsArray())
		{
			OutError = TEXT("scenario : masques absents");
			return false;
		}
		OutInfo.Masques = *Masques;
		if (const FValue* Vue = Root.Find(TEXT("vue")); Vue && !Vue->IsNull())
		{
			const FValue* X = Vue->Find(TEXT("x"));
			const FValue* Y = Vue->Find(TEXT("y"));
			if (!X || !Y || !X->IsNumber() || !Y->IsNumber())
			{
				OutError = TEXT("scenario : vue {x, y} attendue");
				return false;
			}
			OutInfo.bHasVue = true;
			OutInfo.VueX = X->Number;
			OutInfo.VueY = Y->Number;
		}
		const FValue* Save = Root.Find(TEXT("save"));
		if (!Save)
		{
			OutError = TEXT("scenario : save absente");
			return false;
		}
		return AnastasisJsSave::Read(*Save, OutState, OutError);
	}

	bool Restore(const AnastasisJsSave::FState& Read, FAnastasisSimulation& Sim, FString& OutError)
	{
		AnastasisWorld::FWorld World = Read.World;
		Sim.ResetFromWorld(Read.Seed, MoveTemp(World), Read.Time, Read.Day);
		AnastasisVillage::FVillage& Village = Sim.GetVillage();

		// `sim.settlement` : l'origine vers laquelle les portes s'ouvrent.
		if (const FValue* Settlement = Read.Source.Find(TEXT("settlement")))
		{
			const FValue* X = Settlement->Find(TEXT("x"));
			const FValue* Y = Settlement->Find(TEXT("y"));
			if (X && Y && X->IsNumber() && Y->IsNumber())
			{
				Village.SetSettlement(X->Number, Y->Number);
			}
			// Le site reserve au marche (build-decision-001) : la cible d'un batisseur sans chantier.
			const FValue* Dx = Settlement->Find(TEXT("marketDx"));
			const FValue* Dy = Settlement->Find(TEXT("marketDy"));
			Village.SetMarketOffset(
				(Dx && Dx->IsNumber()) ? TOptional<double>(Dx->Number) : TOptional<double>(),
				(Dy && Dy->IsNumber()) ? TOptional<double>(Dy->Number) : TOptional<double>());
		}
		// `_nextBuildingId`, `_nextId` : les identifiants des entites a venir.
		int32 NextBuildingId = Read.Buildings.Num();
		int32 NextNpcId = Read.Actors.Num();
		if (const FValue* V = Read.Source.Find(TEXT("nextBuildingId")); V && V->IsNumber()) NextBuildingId = static_cast<int32>(V->Number);
		if (const FValue* V = Read.Source.Find(TEXT("nextId")); V && V->IsNumber()) NextNpcId = static_cast<int32>(V->Number);

		if (!Village.RestoreForHarness(Read.Buildings.GetItems(), Read.Actors.GetItems(),
			Read.Meals.Reservations, Read.Meals.Seq, NextBuildingId, NextNpcId, OutError))
		{
			return false;
		}
		// `sim.rng` reprend la ou la sauvegarde l'a laisse (`save.rng`) : apres `ResetFromWorld`, qui
		// l'a seme sur la graine (`makeRng(seed)`), comme `deserialize` (reader-rng-001).
		Village.SetSimRngState(Read.RngState);
		return true;
	}

	void Snapshot(const FAnastasisSimulation& Sim, AnastasisJsSave::FState& InOut)
	{
		const AnastasisVillage::FVillage& Village = Sim.GetVillage();
		InOut.Time = Sim.GetTime();
		InOut.Day = Sim.GetDay();

		// Le monde : l'etat vivant des tuiles que la simulation a touchees.
		auto Refresh = [&InOut, &Village](int32 Index)
		{
			if (!InOut.World.Tiles.IsValidIndex(Index)) return;
			const AnastasisWorld::FTile& Old = InOut.World.Tiles[Index];
			InOut.World.Tiles[Index] = Village.LiveTileAt(Old.X, Old.Y);
		};
		for (const TPair<int32, AnastasisWorld::FTile>& Pair : Village.GetLiveTiles())
		{
			Refresh(Pair.Key);
		}
		for (const AnastasisVillage::FFoodSource& Source : Village.GetFoodSources())
		{
			Refresh(Source.TileIndex);
		}

		InOut.Buildings = TAnastasisEntityTable<AnastasisVillage::FBuilding>();
		for (const AnastasisVillage::FBuilding& B : Village.GetBuildings())
		{
			InOut.Buildings.Add(B);
		}
		InOut.Actors = TAnastasisEntityTable<AnastasisVillage::FNpc>();
		for (const AnastasisVillage::FNpc& N : Village.GetActors())
		{
			InOut.Actors.Add(N);
		}
		InOut.Meals.Seq = Village.GetMealSeq();
		InOut.Meals.Reservations = Village.GetMealReservations();
		// La section `rng` : l'etat VIVANT du flux partage, plus celui de la lecture (reader-rng-001).
		InOut.RngState = Village.GetSimRngState();
	}

	bool DigestSections(const AnastasisJsSave::FState& State, const TArray<FString>& Sections,
		TMap<FString, uint64>& OutDigests, uint64& OutGlobal, FString& OutError)
	{
		OutDigests.Reset();
		TArray<FString> Sorted = Sections;
		// `Object.keys(state).sort()` : unites de code UTF-16 croissantes.
		Sorted.Sort([](const FString& A, const FString& B) { return A.Compare(B, ESearchCase::CaseSensitive) < 0; });
		AnastasisDigest::FStateWriter Global;
		for (const FString& Section : Sorted)
		{
			FValue Value;
			if (!AnastasisJsSave::Project(State, Section, Value, OutError))
			{
				return false;
			}
			const uint64 H = AnastasisJson::DigestOf(Value);
			OutDigests.Add(Section, H);
			// `global.string(k).string(h)`.
			Global.String(Section).String(AnastasisDigest::ToHex(H));
		}
		OutGlobal = Global.Digest();
		return true;
	}

	FString DrillPath(const FString& TracePath, int32 Tick)
	{
		return FPaths::Combine(FPaths::GetPath(TracePath), FString::Printf(TEXT("%s.tick%d.json"), *FPaths::GetBaseFilename(TracePath), Tick));
	}

	bool Run(const FString& ScenarioPath, const FString& OutPath, int32 Ticks, int32 Every, FRunResult& OutResult, FString& OutError, int32 DrillTick)
	{
		if (Ticks < 0 || Every < 1)
		{
			OutError = TEXT("ticks >= 0 et every >= 1 attendus");
			return false;
		}
		FScenarioInfo Info;
		AnastasisJsSave::FState Read;
		if (!LoadScenario(ScenarioPath, Info, Read, OutError))
		{
			return false;
		}
		FAnastasisSimulation Sim;
		if (!Restore(Read, Sim, OutError))
		{
			return false;
		}
		// `pinSimulationView(sim.simulationBudget, vue.x, vue.y)`, comme l'emetteur JS apres `deserialize`.
		if (Info.bHasVue)
		{
			Sim.GetVillage().SetSimulationView(Info.VueX, Info.VueY);
		}

		TArray<FString> Lines;
		{
			FValue Scenario = FValue::MakeObject();
			Scenario.Set(TEXT("name"), FValue::MakeString(Info.Name));
			Scenario.Set(TEXT("empreinte"), FValue::MakeString(Info.Empreinte));
			Scenario.Set(TEXT("masques"), Info.Masques);
			FValue Sections = FValue::MakeArray();
			for (const FString& S : Info.Sections) Sections.Items.Add(FValue::MakeString(S));
			Scenario.Set(TEXT("sections"), Sections);
			if (Info.bHasVue)
			{
				FValue Vue = FValue::MakeObject();
				Vue.Set(TEXT("x"), FValue::MakeNumber(Info.VueX));
				Vue.Set(TEXT("y"), FValue::MakeNumber(Info.VueY));
				Scenario.Set(TEXT("vue"), Vue);
			}
			else
			{
				Scenario.Set(TEXT("vue"), FValue());
			}
			Scenario.Set(TEXT("avertissementsChargement"), FValue::MakeNumber(0.0));

			FValue Header = FValue::MakeObject();
			Header.Set(TEXT("kind"), FValue::MakeString(TEXT("header")));
			Header.Set(TEXT("spec"), FValue::MakeNumber(DigestSpecVersion));
			Header.Set(TEXT("source"), FValue::MakeString(TEXT("unreal")));
			Header.Set(TEXT("ref"), FValue::MakeString(TEXT("Source/AnastasisSim")));
			Header.Set(TEXT("refHead"), FValue::MakeString(Info.ReferenceCommit.Left(7)));
			Header.Set(TEXT("refCommit"), FValue::MakeString(Info.ReferenceCommit));
			Header.Set(TEXT("seed"), FValue::MakeNumber(static_cast<double>(Read.Seed)));
			Header.Set(TEXT("dt"), FValue::MakeNumber(Info.Dt));
			Header.Set(TEXT("ticks"), FValue::MakeNumber(Ticks));
			Header.Set(TEXT("every"), FValue::MakeNumber(Every));
			Header.Set(TEXT("dayLength"), FValue::MakeNumber(FAnastasisSimulation::DayLength));
			Header.Set(TEXT("dayDeferred"), FValue::MakeString(Info.DayDeferred));
			Header.Set(TEXT("tick"), FValue::MakeString(TEXT("cpp")));
			Header.Set(TEXT("scenario"), Scenario);
			Header.Set(TEXT("perturb"), FValue());
			Lines.Add(AnastasisJson::Stringify(Header));
		}

		AnastasisJsSave::FState Snap = Read;
		auto Sample = [&](int32 Tick, TMap<FString, uint64>* Keep) -> bool
		{
			Snapshot(Sim, Snap);
			TMap<FString, uint64> Digests;
			uint64 Global = 0;
			if (!DigestSections(Snap, Info.Sections, Digests, Global, OutError))
			{
				return false;
			}
			FValue S = FValue::MakeObject();
			for (const FString& Section : Info.Sections) S.Set(Section, Hex(Digests[Section]));
			FValue Line = FValue::MakeObject();
			Line.Set(TEXT("t"), FValue::MakeNumber(Tick));
			Line.Set(TEXT("day"), FValue::MakeNumber(Sim.GetDay()));
			Line.Set(TEXT("time"), FValue::MakeNumber(Sim.GetTime()));
			Line.Set(TEXT("g"), Hex(Global));
			Line.Set(TEXT("s"), S);
			Lines.Add(AnastasisJson::Stringify(Line));
			++OutResult.Samples;
			if (Keep) *Keep = Digests;
			if (Tick == DrillTick)
			{
				FValue Dump = FValue::MakeObject();
				for (const FString& Section : Info.Sections)
				{
					FValue Value;
					if (!AnastasisJsSave::Project(Snap, Section, Value, OutError)) return false;
					Dump.Set(Section, Value);
				}
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), true);
				FFileHelper::SaveStringToFile(AnastasisJson::Stringify(Dump), *DrillPath(OutPath, Tick), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			}
			OutResult.Last = Digests;
			OutResult.LastGlobal = Global;
			return true;
		};

		// Tick 0 : l'etat repris, avant le premier pas.
		if (!Sample(0, &OutResult.TickZero)) return false;
		for (int32 Tick = 1; Tick <= Ticks; ++Tick)
		{
			Sim.Tick(Info.Dt);
			if ((Tick % Every == 0 || Tick == DrillTick) && !Sample(Tick, nullptr)) return false;
		}

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), true);
		if (!FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")) + TEXT("\n"), *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("trace non ecrite : %s"), *OutPath);
			return false;
		}
		return true;
	}
}
