#include "WorldView/AnastasisWorldProbeSubsystem.h"

#include "Anastasis_UnrealV2.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "World/AnastasisWorld.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisWorldView.h"

namespace
{
	struct FSliceProbeScore
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Size = 0;
		int32 Score = -1;
		int32 Water = 0;
		int32 Forest = 0;
		int32 Field = 0;
		int32 Grass = 0;
		int32 Scrub = 0;
		int32 Stone = 0;
		int32 ShoreContacts = 0;
		int32 ForestEdgeContacts = 0;
		double MinAlt = 0.0;
		double MaxAlt = 0.0;
		double MeanWetness = 0.0;
		double MeanShore = 0.0;
		bool bValid = false;
	};

	FString WriteJson(const TSharedRef<FJsonObject>& Root)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}

	TSharedRef<FJsonObject> ProbeScopeObject(const TCHAR* ToolName)
	{
		TSharedRef<FJsonObject> Scope = MakeShared<FJsonObject>();
		Scope->SetStringField(TEXT("tool"), ToolName);
		Scope->SetStringField(TEXT("mode"), TEXT("EXPERIMENTAL_PROBE"));
		Scope->SetStringField(TEXT("modifies_world"), TEXT("false unless capture_fixed_view_probe moves transient probe camera"));
		Scope->SetStringField(TEXT("status_domain"), TEXT("PASS|FAIL|UNKNOWN"));
		Scope->SetStringField(TEXT("mec"), TEXT("deterministic comparable artifact"));
		Scope->SetStringField(TEXT("scn"), TEXT("UNKNOWN unless paired capture artifact is produced"));
		Scope->SetStringField(TEXT("ply"), TEXT("UNKNOWN"));
		return Scope;
	}

	void AddProbeCheck(TArray<TSharedPtr<FJsonValue>>& Checks, const TCHAR* Code, const TCHAR* Status, const FString& Detail)
	{
		TSharedRef<FJsonObject> Check = MakeShared<FJsonObject>();
		Check->SetStringField(TEXT("code"), Code);
		Check->SetStringField(TEXT("status"), Status);
		Check->SetStringField(TEXT("detail"), Detail);
		Checks.Add(MakeShared<FJsonValueObject>(Check));
	}

	const TCHAR* ProbeStatus(int32 FailCount, int32 UnknownCount)
	{
		if (FailCount > 0)
		{
			return TEXT("FAIL");
		}
		if (UnknownCount > 0)
		{
			return TEXT("UNKNOWN");
		}
		return TEXT("PASS");
	}

	void SetCountsAndStatus(TSharedRef<FJsonObject> Root, int32 PassCount, int32 FailCount, int32 UnknownCount)
	{
		TSharedRef<FJsonObject> Counts = MakeShared<FJsonObject>();
		Counts->SetNumberField(TEXT("pass"), PassCount);
		Counts->SetNumberField(TEXT("fail"), FailCount);
		Counts->SetNumberField(TEXT("unknown"), UnknownCount);
		Root->SetObjectField(TEXT("counts"), Counts);
		Root->SetStringField(TEXT("status"), ProbeStatus(FailCount, UnknownCount));
	}

	TSharedRef<FJsonObject> LockedParamsObject(
		uint32 Seed,
		int32 SourceW,
		int32 SourceH,
		int32 OriginX,
		int32 OriginY,
		int32 Size,
		double Hour,
		const FString& Camera,
		const FString& Profile)
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetNumberField(TEXT("seed"), Seed);
		Params->SetNumberField(TEXT("source_width"), SourceW);
		Params->SetNumberField(TEXT("source_height"), SourceH);
		Params->SetNumberField(TEXT("origin_x"), OriginX);
		Params->SetNumberField(TEXT("origin_y"), OriginY);
		Params->SetNumberField(TEXT("size"), Size);
		Params->SetNumberField(TEXT("hour"), Hour);
		Params->SetStringField(TEXT("camera"), Camera);
		Params->SetStringField(TEXT("profile"), Profile);
		Params->SetStringField(TEXT("comparison_key"), FString::Printf(
			TEXT("seed=%u;source=%dx%d;origin=%d,%d;size=%d;hour=%.3f;camera=%s;profile=%s"),
			Seed,
			SourceW,
			SourceH,
			OriginX,
			OriginY,
			Size,
			Hour,
			*Camera,
			*Profile));
		return Params;
	}

	TSharedRef<FJsonObject> CountsObject(const AnastasisWorldView::FWorldVisualSnapshot& Snapshot)
	{
		TSharedRef<FJsonObject> Counts = MakeShared<FJsonObject>();
		Counts->SetNumberField(TEXT("grass"), Snapshot.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Grass)]);
		Counts->SetNumberField(TEXT("water"), Snapshot.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Water)]);
		Counts->SetNumberField(TEXT("stone"), Snapshot.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Stone)]);
		Counts->SetNumberField(TEXT("ruin"), Snapshot.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Ruin)]);
		Counts->SetNumberField(TEXT("forest"), Snapshot.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Forest)]);
		Counts->SetNumberField(TEXT("scrub"), Snapshot.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Scrub)]);
		Counts->SetNumberField(TEXT("field"), Snapshot.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Field)]);
		return Counts;
	}

	uint32 SnapshotFingerprint(const AnastasisWorldView::FWorldVisualSnapshot& Snapshot)
	{
		FString Accumulator;
		Accumulator.Reserve(Snapshot.Tiles.Num() * 48);
		Accumulator += FString::Printf(
			TEXT("seed=%u;source=%dx%d;origin=%d,%d;size=%dx%d|"),
			Snapshot.Seed,
			Snapshot.SourceW,
			Snapshot.SourceH,
			Snapshot.OriginX,
			Snapshot.OriginY,
			Snapshot.W,
			Snapshot.H);

		for (const AnastasisWorldView::FVisualTile& Tile : Snapshot.Tiles)
		{
			Accumulator += FString::Printf(
				TEXT("%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%d|"),
				Tile.X,
				Tile.Y,
				static_cast<int32>(Tile.Type),
				Tile.Alt,
				Tile.Wetness,
				Tile.Shore,
				Tile.Fertility,
				Tile.Amount);
		}
		return FCrc::StrCrc32(*Accumulator);
	}

	bool IsInsideSnapshotRect(const AnastasisWorldView::FWorldVisualSnapshot& Snapshot, int32 X, int32 Y, int32 Size)
	{
		return Size > 0
			&& X >= Snapshot.OriginX
			&& Y >= Snapshot.OriginY
			&& X + Size <= Snapshot.OriginX + Snapshot.W
			&& Y + Size <= Snapshot.OriginY + Snapshot.H;
	}

	bool IsSemanticallyRich(const FSliceProbeScore& Candidate)
	{
		return Candidate.bValid
			&& Candidate.Water > 0
			&& Candidate.Forest > 0
			&& Candidate.Field > 0
			&& Candidate.Grass + Candidate.Scrub > 0
			&& Candidate.ShoreContacts > 0;
	}

	FSliceProbeScore ScoreSlice(const AnastasisWorldView::FWorldVisualSnapshot& Snapshot, int32 X0, int32 Y0, int32 Size)
	{
		FSliceProbeScore Result;
		Result.X = X0;
		Result.Y = Y0;
		Result.Size = Size;
		Result.MinAlt = TNumericLimits<double>::Max();
		Result.MaxAlt = TNumericLimits<double>::Lowest();
		if (!IsInsideSnapshotRect(Snapshot, X0, Y0, Size))
		{
			return Result;
		}

		Result.bValid = true;
		for (int32 Y = Y0; Y < Y0 + Size; ++Y)
		{
			for (int32 X = X0; X < X0 + Size; ++X)
			{
				const AnastasisWorldView::FVisualTile* Tile = AnastasisWorldView::FindTile(Snapshot, X, Y);
				if (!Tile)
				{
					Result.bValid = false;
					continue;
				}

				switch (Tile->Type)
				{
				case AnastasisWorld::ETileType::Water: ++Result.Water; break;
				case AnastasisWorld::ETileType::Forest: ++Result.Forest; break;
				case AnastasisWorld::ETileType::Field: ++Result.Field; break;
				case AnastasisWorld::ETileType::Grass: ++Result.Grass; break;
				case AnastasisWorld::ETileType::Scrub: ++Result.Scrub; break;
				case AnastasisWorld::ETileType::Stone: ++Result.Stone; break;
				default: break;
				}

				Result.MinAlt = FMath::Min(Result.MinAlt, Tile->Alt);
				Result.MaxAlt = FMath::Max(Result.MaxAlt, Tile->Alt);
				Result.MeanWetness += Tile->Wetness;
				Result.MeanShore += Tile->Shore;

				const int32 NeighbourX[4] = { X - 1, X + 1, X, X };
				const int32 NeighbourY[4] = { Y, Y, Y - 1, Y + 1 };
				for (int32 Neighbour = 0; Neighbour < 4; ++Neighbour)
				{
					const AnastasisWorldView::FVisualTile* Adjacent = AnastasisWorldView::FindTile(Snapshot, NeighbourX[Neighbour], NeighbourY[Neighbour]);
					if (!Adjacent)
					{
						continue;
					}
					if (Tile->Type == AnastasisWorld::ETileType::Water && Adjacent->Type != AnastasisWorld::ETileType::Water)
					{
						++Result.ShoreContacts;
					}
					if (Tile->Type == AnastasisWorld::ETileType::Forest && Adjacent->Type != AnastasisWorld::ETileType::Forest)
					{
						++Result.ForestEdgeContacts;
					}
				}
			}
		}

		const double Area = static_cast<double>(Size * Size);
		Result.MeanWetness /= Area;
		Result.MeanShore /= Area;

		const int32 RequiredPresence =
			(Result.Water > 0 ? 20 : 0) +
			(Result.Forest > 0 ? 15 : 0) +
			(Result.Field > 0 ? 10 : 0) +
			(Result.Grass + Result.Scrub > 0 ? 10 : 0);
		const int32 WaterEdge = FMath::Min(Result.ShoreContacts, 20);
		const int32 ForestEdge = FMath::Min(Result.ForestEdgeContacts / 2, 10);
		const int32 Relief = FMath::Clamp(FMath::RoundToInt((Result.MaxAlt - Result.MinAlt) * 20.0), 0, 10);
		const int32 Moisture = FMath::Clamp(FMath::RoundToInt(Result.MeanWetness * 20.0), 0, 10);
		Result.Score = RequiredPresence + WaterEdge + ForestEdge + Relief + Moisture;
		return Result;
	}

	bool CandidateOrder(const FSliceProbeScore& A, const FSliceProbeScore& B)
	{
		if (A.Score != B.Score)
		{
			return A.Score > B.Score;
		}
		if (A.X != B.X)
		{
			return A.X < B.X;
		}
		return A.Y < B.Y;
	}

	void InsertTopCandidate(TArray<FSliceProbeScore>& Top, const FSliceProbeScore& Candidate, int32 Limit)
	{
		Top.Add(Candidate);
		Top.Sort(CandidateOrder);
		if (Top.Num() > Limit)
		{
			Top.SetNum(Limit);
		}
	}

	TSharedRef<FJsonObject> CandidateObject(const FSliceProbeScore& Candidate)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("origin_x"), Candidate.X);
		Obj->SetNumberField(TEXT("origin_y"), Candidate.Y);
		Obj->SetNumberField(TEXT("size"), Candidate.Size);
		Obj->SetBoolField(TEXT("valid"), Candidate.bValid);
		Obj->SetBoolField(TEXT("semantically_rich"), IsSemanticallyRich(Candidate));
		Obj->SetNumberField(TEXT("score"), Candidate.Score);
		Obj->SetNumberField(TEXT("water"), Candidate.Water);
		Obj->SetNumberField(TEXT("forest"), Candidate.Forest);
		Obj->SetNumberField(TEXT("field"), Candidate.Field);
		Obj->SetNumberField(TEXT("grass"), Candidate.Grass);
		Obj->SetNumberField(TEXT("scrub"), Candidate.Scrub);
		Obj->SetNumberField(TEXT("stone"), Candidate.Stone);
		Obj->SetNumberField(TEXT("shore_contacts"), Candidate.ShoreContacts);
		Obj->SetNumberField(TEXT("forest_edge_contacts"), Candidate.ForestEdgeContacts);
		Obj->SetNumberField(TEXT("min_alt"), Candidate.MinAlt);
		Obj->SetNumberField(TEXT("max_alt"), Candidate.MaxAlt);
		Obj->SetNumberField(TEXT("mean_wetness"), Candidate.MeanWetness);
		Obj->SetNumberField(TEXT("mean_shore"), Candidate.MeanShore);
		return Obj;
	}

	TArray<TSharedPtr<FJsonValue>> CandidateArray(const TArray<FSliceProbeScore>& Candidates)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FSliceProbeScore& Candidate : Candidates)
		{
			Values.Add(MakeShared<FJsonValueObject>(CandidateObject(Candidate)));
		}
		return Values;
	}

	TArray<FSliceProbeScore> ParseAndScoreCandidates(
		const AnastasisWorldView::FWorldVisualSnapshot& Snapshot,
		const FString& CandidateSpec,
		int32 Size,
		int32& OutInvalidSpecs)
	{
		TArray<FSliceProbeScore> Candidates;
		TArray<FString> Segments;
		CandidateSpec.ParseIntoArray(Segments, TEXT(";"), true);
		for (const FString& Segment : Segments)
		{
			TArray<FString> Coords;
			Segment.ParseIntoArray(Coords, TEXT(","), true);
			if (Coords.Num() != 2)
			{
				++OutInvalidSpecs;
				continue;
			}
			Candidates.Add(ScoreSlice(Snapshot, FCString::Atoi(*Coords[0]), FCString::Atoi(*Coords[1]), Size));
		}
		Candidates.Sort(CandidateOrder);
		return Candidates;
	}
}

FString UAnastasisWorldProbeSubsystem::WriteProbeObject(const FString& Slug, const TSharedRef<FJsonObject>& Root) const
{
	Root->SetStringField(TEXT("written_at_utc"), FDateTime::UtcNow().ToIso8601());

	const FString Json = WriteJson(Root);
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Anastasis/Diagnostics/probes");
	IFileManager::Get().MakeDirectory(*Dir, true);

	const FString SafeSlug = FPaths::MakeValidFileName(Slug.IsEmpty() ? TEXT("probe") : Slug);
	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	const FString Path = FPaths::ConvertRelativePathToFull(Dir / (Timestamp + TEXT("-") + SafeSlug + TEXT(".json")));
	const FString LatestPath = FPaths::ConvertRelativePathToFull(Dir / (TEXT("latest-") + SafeSlug + TEXT(".json")));

	if (!FFileHelper::SaveStringToFile(Json, *Path))
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_PROBE write failed path=%s"), *Path);
		return FString();
	}
	FFileHelper::SaveStringToFile(Json, *LatestPath);

	FString Schema;
	FString Status;
	Root->TryGetStringField(TEXT("schema"), Schema);
	Root->TryGetStringField(TEXT("status"), Status);
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PROBE path=%s schema=%s status=%s"), *Path, *Schema, *Status);
	return Path;
}

FString UAnastasisWorldProbeSubsystem::RunWorldgenProbe(uint32 Seed, int32 SourceW, int32 SourceH, double Hour, const FString& Profile)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Checks;
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.probe_worldgen.v1"));
	Root->SetStringField(TEXT("operation"), TEXT("run_worldgen_probe"));
	Root->SetObjectField(TEXT("evidence_scope"), ProbeScopeObject(TEXT("run_worldgen_probe")));
	Root->SetObjectField(TEXT("locked_parameters"), LockedParamsObject(Seed, SourceW, SourceH, 0, 0, SourceW, Hour, TEXT("none"), Profile));

	int32 PassCount = 0;
	int32 FailCount = 0;
	int32 UnknownCount = 0;
	auto Record = [&Checks, &PassCount, &FailCount, &UnknownCount](const TCHAR* Code, const TCHAR* Status, const FString& Detail)
	{
		AddProbeCheck(Checks, Code, Status, Detail);
		if (FCString::Strcmp(Status, TEXT("PASS")) == 0) { ++PassCount; }
		else if (FCString::Strcmp(Status, TEXT("FAIL")) == 0) { ++FailCount; }
		else { ++UnknownCount; }
	};

	if (SourceW <= 0 || SourceH <= 0)
	{
		Record(TEXT("valid_dimensions"), TEXT("FAIL"), FString::Printf(TEXT("source=%dx%d"), SourceW, SourceH));
		Root->SetArrayField(TEXT("checks"), Checks);
		SetCountsAndStatus(Root, PassCount, FailCount, UnknownCount);
		return WriteProbeObject(TEXT("run_worldgen_probe"), Root);
	}
	Record(TEXT("valid_dimensions"), TEXT("PASS"), FString::Printf(TEXT("source=%dx%d"), SourceW, SourceH));

	const AnastasisWorld::FWorld FirstWorld = AnastasisWorld::GenerateWorld(Seed, SourceW, SourceH);
	const AnastasisWorld::FWorld SecondWorld = AnastasisWorld::GenerateWorld(Seed, SourceW, SourceH);
	const AnastasisWorldView::FWorldVisualSnapshot FirstSnapshot = AnastasisWorldView::CaptureSnapshot(Seed, FirstWorld);
	const AnastasisWorldView::FWorldVisualSnapshot SecondSnapshot = AnastasisWorldView::CaptureSnapshot(Seed, SecondWorld);
	const uint32 FirstFingerprint = SnapshotFingerprint(FirstSnapshot);
	const uint32 SecondFingerprint = SnapshotFingerprint(SecondSnapshot);

	Record(TEXT("tile_count"), FirstWorld.Tiles.Num() == SourceW * SourceH ? TEXT("PASS") : TEXT("FAIL"),
		FString::Printf(TEXT("tiles=%d expected=%d"), FirstWorld.Tiles.Num(), SourceW * SourceH));
	Record(TEXT("deterministic_replay"), FirstFingerprint == SecondFingerprint ? TEXT("PASS") : TEXT("FAIL"),
		FString::Printf(TEXT("first=%08x second=%08x"), FirstFingerprint, SecondFingerprint));
	Record(TEXT("canonical_source_policy"), SourceW == 96 && SourceH == 96 ? TEXT("PASS") : TEXT("UNKNOWN"),
		SourceW == 96 && SourceH == 96 ? TEXT("Using canonical GenerateWorld(seed,96,96).") : TEXT("Non-canonical source size; comparable only within same locked size."));

	TSharedRef<FJsonObject> Metrics = MakeShared<FJsonObject>();
	Metrics->SetNumberField(TEXT("tile_count"), FirstSnapshot.Tiles.Num());
	Metrics->SetObjectField(TEXT("terrain_counts"), CountsObject(FirstSnapshot));
	Metrics->SetNumberField(TEXT("min_alt"), FirstSnapshot.MinAlt);
	Metrics->SetNumberField(TEXT("max_alt"), FirstSnapshot.MaxAlt);
	Metrics->SetStringField(TEXT("fingerprint_crc32"), FString::Printf(TEXT("%08x"), FirstFingerprint));
	Root->SetObjectField(TEXT("metrics"), Metrics);

	Root->SetArrayField(TEXT("checks"), Checks);
	SetCountsAndStatus(Root, PassCount, FailCount, UnknownCount);
	return WriteProbeObject(TEXT("run_worldgen_probe"), Root);
}

FString UAnastasisWorldProbeSubsystem::FindSemanticSlice(uint32 Seed, int32 Size, const FString& Profile)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Checks;
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.probe_find_semantic_slice.v1"));
	Root->SetStringField(TEXT("operation"), TEXT("find_semantic_slice"));
	Root->SetObjectField(TEXT("evidence_scope"), ProbeScopeObject(TEXT("find_semantic_slice")));
	Root->SetObjectField(TEXT("locked_parameters"), LockedParamsObject(Seed, 96, 96, 0, 0, Size, 12.0, TEXT("none"), Profile));

	int32 PassCount = 0;
	int32 FailCount = 0;
	int32 UnknownCount = 0;
	auto Record = [&Checks, &PassCount, &FailCount, &UnknownCount](const TCHAR* Code, const TCHAR* Status, const FString& Detail)
	{
		AddProbeCheck(Checks, Code, Status, Detail);
		if (FCString::Strcmp(Status, TEXT("PASS")) == 0) { ++PassCount; }
		else if (FCString::Strcmp(Status, TEXT("FAIL")) == 0) { ++FailCount; }
		else { ++UnknownCount; }
	};

	if (Size <= 0 || Size > 96)
	{
		Record(TEXT("valid_size"), TEXT("FAIL"), FString::Printf(TEXT("size=%d"), Size));
		Root->SetArrayField(TEXT("checks"), Checks);
		SetCountsAndStatus(Root, PassCount, FailCount, UnknownCount);
		return WriteProbeObject(TEXT("find_semantic_slice"), Root);
	}
	Record(TEXT("valid_size"), TEXT("PASS"), FString::Printf(TEXT("size=%d"), Size));

	const AnastasisWorldView::FWorldVisualSnapshot Snapshot = AnastasisWorldView::CaptureCanonicalWorld(Seed);
	TArray<FSliceProbeScore> Top;
	for (int32 Y = 0; Y <= Snapshot.SourceH - Size; ++Y)
	{
		for (int32 X = 0; X <= Snapshot.SourceW - Size; ++X)
		{
			InsertTopCandidate(Top, ScoreSlice(Snapshot, X, Y, Size), 5);
		}
	}

	const bool bHasCandidate = Top.Num() > 0;
	const bool bBestRich = bHasCandidate && IsSemanticallyRich(Top[0]);
	Record(TEXT("candidate_found"), bHasCandidate ? TEXT("PASS") : TEXT("FAIL"), FString::Printf(TEXT("candidates=%d"), Top.Num()));
	Record(TEXT("best_semantic_richness"), bBestRich ? TEXT("PASS") : TEXT("FAIL"), bHasCandidate ? FString::Printf(TEXT("score=%d"), Top[0].Score) : TEXT("no candidate"));

	if (bHasCandidate)
	{
		Root->SetObjectField(TEXT("best_candidate"), CandidateObject(Top[0]));
	}
	Root->SetArrayField(TEXT("top_candidates"), CandidateArray(Top));
	Root->SetArrayField(TEXT("checks"), Checks);
	SetCountsAndStatus(Root, PassCount, FailCount, UnknownCount);
	return WriteProbeObject(TEXT("find_semantic_slice"), Root);
}

FString UAnastasisWorldProbeSubsystem::CompareSliceCandidates(uint32 Seed, int32 Size, const FString& CandidateSpec, const FString& Profile)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Checks;
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.probe_compare_slice_candidates.v1"));
	Root->SetStringField(TEXT("operation"), TEXT("compare_slice_candidates"));
	Root->SetObjectField(TEXT("evidence_scope"), ProbeScopeObject(TEXT("compare_slice_candidates")));
	Root->SetObjectField(TEXT("locked_parameters"), LockedParamsObject(Seed, 96, 96, 0, 0, Size, 12.0, TEXT("none"), Profile));
	Root->SetStringField(TEXT("candidate_spec"), CandidateSpec);

	int32 PassCount = 0;
	int32 FailCount = 0;
	int32 UnknownCount = 0;
	auto Record = [&Checks, &PassCount, &FailCount, &UnknownCount](const TCHAR* Code, const TCHAR* Status, const FString& Detail)
	{
		AddProbeCheck(Checks, Code, Status, Detail);
		if (FCString::Strcmp(Status, TEXT("PASS")) == 0) { ++PassCount; }
		else if (FCString::Strcmp(Status, TEXT("FAIL")) == 0) { ++FailCount; }
		else { ++UnknownCount; }
	};

	if (Size <= 0 || Size > 96)
	{
		Record(TEXT("valid_size"), TEXT("FAIL"), FString::Printf(TEXT("size=%d"), Size));
		Root->SetArrayField(TEXT("checks"), Checks);
		SetCountsAndStatus(Root, PassCount, FailCount, UnknownCount);
		return WriteProbeObject(TEXT("compare_slice_candidates"), Root);
	}
	Record(TEXT("valid_size"), TEXT("PASS"), FString::Printf(TEXT("size=%d"), Size));

	const AnastasisWorldView::FWorldVisualSnapshot Snapshot = AnastasisWorldView::CaptureCanonicalWorld(Seed);
	int32 InvalidSpecs = 0;
	TArray<FSliceProbeScore> Candidates = ParseAndScoreCandidates(Snapshot, CandidateSpec, Size, InvalidSpecs);
	int32 ValidCandidates = 0;
	for (const FSliceProbeScore& Candidate : Candidates)
	{
		if (Candidate.bValid)
		{
			++ValidCandidates;
		}
	}

	Record(TEXT("candidate_spec_parse"), InvalidSpecs == 0 ? TEXT("PASS") : TEXT("FAIL"), FString::Printf(TEXT("invalid_specs=%d"), InvalidSpecs));
	Record(TEXT("candidate_count"), Candidates.Num() > 0 ? TEXT("PASS") : TEXT("FAIL"), FString::Printf(TEXT("candidates=%d"), Candidates.Num()));
	Record(TEXT("valid_candidate_count"), ValidCandidates > 0 ? TEXT("PASS") : TEXT("FAIL"), FString::Printf(TEXT("valid=%d"), ValidCandidates));
	if (Candidates.Num() > 0)
	{
		Root->SetObjectField(TEXT("winner"), CandidateObject(Candidates[0]));
	}
	Root->SetArrayField(TEXT("ranked_candidates"), CandidateArray(Candidates));
	Root->SetArrayField(TEXT("checks"), Checks);
	SetCountsAndStatus(Root, PassCount, FailCount, UnknownCount);
	return WriteProbeObject(TEXT("compare_slice_candidates"), Root);
}

FString UAnastasisWorldProbeSubsystem::CaptureFixedViewProbe(
	uint32 Seed,
	int32 OriginX,
	int32 OriginY,
	int32 Size,
	double Hour,
	const FString& CameraName,
	const FString& Profile)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Checks;
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.probe_capture_fixed_view.v1"));
	Root->SetStringField(TEXT("operation"), TEXT("capture_fixed_view_probe"));
	Root->SetObjectField(TEXT("evidence_scope"), ProbeScopeObject(TEXT("capture_fixed_view_probe")));
	Root->SetObjectField(TEXT("locked_parameters"), LockedParamsObject(Seed, 96, 96, OriginX, OriginY, Size, Hour, CameraName, Profile));

	int32 PassCount = 0;
	int32 FailCount = 0;
	int32 UnknownCount = 0;
	auto Record = [&Checks, &PassCount, &FailCount, &UnknownCount](const TCHAR* Code, const TCHAR* Status, const FString& Detail)
	{
		AddProbeCheck(Checks, Code, Status, Detail);
		if (FCString::Strcmp(Status, TEXT("PASS")) == 0) { ++PassCount; }
		else if (FCString::Strcmp(Status, TEXT("FAIL")) == 0) { ++FailCount; }
		else { ++UnknownCount; }
	};

	if (Size <= 0)
	{
		Record(TEXT("valid_size"), TEXT("FAIL"), FString::Printf(TEXT("size=%d"), Size));
		Root->SetArrayField(TEXT("checks"), Checks);
		SetCountsAndStatus(Root, PassCount, FailCount, UnknownCount);
		return WriteProbeObject(TEXT("capture_fixed_view_probe"), Root);
	}
	Record(TEXT("valid_size"), TEXT("PASS"), FString::Printf(TEXT("size=%d"), Size));

	AAnastasisWorldEmbodiment* Embodiment = FindEmbodiment();
	if (!Embodiment)
	{
		Record(TEXT("embodied_scene_lock"), TEXT("UNKNOWN"), TEXT("No AAnastasisWorldEmbodiment is present; cannot prove scene seed/origin/size."));
	}
	else
	{
		const AnastasisWorldView::FWorldVisualSnapshot& Snapshot = Embodiment->GetSnapshot();
		const bool bMatches = Snapshot.Seed == Seed
			&& Snapshot.OriginX == OriginX
			&& Snapshot.OriginY == OriginY
			&& Snapshot.W == Size
			&& Snapshot.H == Size;
		Record(TEXT("embodied_scene_lock"), bMatches ? TEXT("PASS") : TEXT("UNKNOWN"),
			FString::Printf(
				TEXT("expected seed=%u origin=(%d,%d) size=%d; actual seed=%u origin=(%d,%d) size=%dx%d"),
				Seed,
				OriginX,
				OriginY,
				Size,
				Snapshot.Seed,
				Snapshot.OriginX,
				Snapshot.OriginY,
				Snapshot.W,
				Snapshot.H));
	}

	Record(TEXT("hour_lock"), TEXT("UNKNOWN"), FString::Printf(TEXT("hour=%.3f recorded, but no canonical Unreal time-of-day owner is wired to this probe yet."), Hour));
	Record(TEXT("profile_lock"), Profile.IsEmpty() ? TEXT("UNKNOWN") : TEXT("PASS"), Profile.IsEmpty() ? TEXT("empty profile") : FString::Printf(TEXT("profile=%s"), *Profile));

	if (!FApp::CanEverRender())
	{
		Record(TEXT("render_context"), TEXT("UNKNOWN"), TEXT("Current process cannot render, e.g. -nullrhi/headless."));
		Root->SetStringField(TEXT("capture_request"), TEXT("NOT_REQUESTED_HEADLESS"));
		Root->SetArrayField(TEXT("checks"), Checks);
		SetCountsAndStatus(Root, PassCount, FailCount, UnknownCount);
		return WriteProbeObject(TEXT("capture_fixed_view_probe"), Root);
	}
	Record(TEXT("render_context"), TEXT("PASS"), TEXT("Engine can render in this process."));

	FString Reason;
	if (!GotoBookmark(FName(*CameraName), Reason))
	{
		Record(TEXT("camera_lock"), TEXT("UNKNOWN"), Reason);
		Root->SetStringField(TEXT("capture_request"), TEXT("NOT_REQUESTED_CAMERA_UNREACHABLE"));
		Root->SetArrayField(TEXT("checks"), Checks);
		SetCountsAndStatus(Root, PassCount, FailCount, UnknownCount);
		return WriteProbeObject(TEXT("capture_fixed_view_probe"), Root);
	}
	Record(TEXT("camera_lock"), TEXT("PASS"), FString::Printf(TEXT("bookmark=%s"), *CameraName));

	const FString Mission = FString::Printf(TEXT("fixed-view-seed-%u-origin-%d-%d-size-%d-hour-%.3f-profile-%s"), Seed, OriginX, OriginY, Size, Hour, *Profile);
	RequestCapture(FName(*CameraName), Mission);
	Root->SetStringField(TEXT("capture_request"), TEXT("REQUESTED_ASYNC"));
	Record(TEXT("paired_capture_artifact"), TEXT("UNKNOWN"), TEXT("Async capture requested; wait for CAPTURE::PASS and paired screenshot JSON before claiming SCN."));
	Root->SetStringField(TEXT("claim_boundary"), TEXT("Probe remains UNKNOWN until CAPTURE::PASS emits a paired screenshot JSON artifact."));
	Root->SetArrayField(TEXT("checks"), Checks);
	SetCountsAndStatus(Root, PassCount, FailCount, UnknownCount);
	return WriteProbeObject(TEXT("capture_fixed_view_probe"), Root);
}
