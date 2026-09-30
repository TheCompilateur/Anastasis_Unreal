#include "WorldView/AnastasisPlaces.h"
#include "WorldView/AnastasisHumanGeography.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
using namespace AnastasisPlaces;

/**
 * Sol de test : l'altitude de simulation interpolee, passee par la geographie humaine et
 * l'echelle spatiale exactement comme AnastasisHumanGeography::Apply le fait sur le maillage
 * forge. Pas de forge (erosion, ravines) : ce n'est pas ce que ces tests jugent.
 */
struct FTestWorld
{
	AnastasisWorldView::FWorldVisualSnapshot S;
	double T = 0;

	explicit FTestWorld(bool bHumanGeography, double Scale)
	{
		S = AnastasisWorldView::CaptureCanonicalWorld(AnastasisWorldView::ReferenceSeed);
		S.SpatialScale = Scale;
		S.bHumanGeography = bHumanGeography;
		T = AnastasisWorldView::TileWorldSize * Scale;
	}
	double Alt(double U, double V) const
	{
		const double X = FMath::Clamp(U - 0.5, 0.0, S.W - 1.001), Y = FMath::Clamp(V - 0.5, 0.0, S.H - 1.001);
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const double FX = X - X0, FY = Y - Y0;
		auto A = [&](int32 I, int32 J) { return S.Tiles[J * S.W + I].Alt; };
		return FMath::Lerp(FMath::Lerp(A(X0, Y0), A(X0 + 1, Y0), FX), FMath::Lerp(A(X0, Y0 + 1), A(X0 + 1, Y0 + 1), FX), FY);
	}
	bool Ground(double X, double Y, double& Z) const
	{
		if (X < 0 || Y < 0 || X > S.W * T || Y > S.H * T) return false;
		const double Sea = AnastasisTerrainSurface::WaterPlaneZ;
		double Metres = Alt(X / T, Y / T) * 10.0;
		if (S.bHumanGeography) Metres = AnastasisHumanGeography::Evaluate(X / T, Y / T, Metres).Height;
		Z = Sea + (Metres * 100.0 - Sea) * S.SpatialScale;
		return true;
	}
	bool Water(double X, double Y, double& Z) const
	{
		const double Sea = AnastasisTerrainSurface::WaterPlaneZ;
		const double Metres = S.bHumanGeography ? AnastasisHumanGeography::Evaluate(X / T, Y / T, 0.0).WaterHeight : Sea / 100.0;
		Z = Sea + (Metres * 100.0 - Sea) * S.SpatialScale;
		return true;
	}
	FInputs Inputs() const
	{
		FInputs In;
		In.Source = &S;
		In.Ground = [this](double X, double Y, double& Z) { return Ground(X, Y, Z); };
		In.Water = [this](double X, double Y, double& Z) { return Water(X, Y, Z); };
		// Point haut : la tuile la plus haute hors bord, comme le fait la forge.
		double Best = -1;
		for (const auto& Tl : S.Tiles)
		{
			if (Tl.X < 3 || Tl.Y < 3 || Tl.X > S.W - 4 || Tl.Y > S.H - 4 || Tl.Alt <= Best) continue;
			Best = Tl.Alt;
			In.Landmark = FVector((Tl.X + 0.5) * T, (Tl.Y + 0.5) * T, 0);
		}
		In.bLandmark = Best > 0;
		In.bBasin = true;
		In.Basin = FVector(53.0 * T, 53.0 * T, 0);
		return In;
	}
};

bool SamePiece(const FPiece& A, const FPiece& B)
{
	return A.Family == B.Family && A.Variant == B.Variant && A.Place == B.Place && A.XY == B.XY && A.Yaw == B.Yaw
		&& A.Scale == B.Scale && A.Sink == B.Sink && A.Tilt == B.Tilt && A.TiltToward == B.TiltToward && A.bFollowSlope == B.bFollowSlope;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPlacesCanonical, "Anastasis.Places.CanonicalDeterminismAndReserve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPlacesCanonical::RunTest(const FString&)
{
	const FTestWorld W(true, 5.0);
	const auto Before = W.S.Tiles;
	const FInputs In = W.Inputs();
	FPlan A, B;
	FString E;
	TestTrue(TEXT("plan A"), Compose(In, A, E));
	TestTrue(TEXT("plan B"), Compose(In, B, E));
	TestEqual(TEXT("same places"), A.Places.Num(), B.Places.Num());
	TestEqual(TEXT("same pieces"), A.Pieces.Num(), B.Pieces.Num());
	for (int32 I = 0; I < FMath::Min(A.Pieces.Num(), B.Pieces.Num()); ++I)
	{
		if (!SamePiece(A.Pieces[I], B.Pieces[I])) { AddError(FString::Printf(TEXT("piece %d differs"), I)); break; }
	}
	// Chaque lieu nomme de la graine canonique est trouve : c'est la promesse de la mission.
	for (const EKind K : {EKind::Spring, EKind::Pass, EKind::Lookout, EKind::Crags, EKind::Hamlet, EKind::Marsh, EKind::OldTree, EKind::OldWood})
	{
		if (!A.Places.FindByPredicate([K](const FPlace& P) { return P.Kind == K && P.NumPieces > 0; }))
		{
			AddError(FString::Printf(TEXT("place kind %d missing; missing=[%s]"), static_cast<int32>(K), *FString::Join(A.Missing, TEXT(","))));
		}
	}
	// Des lieux, pas une distribution : un plafond garde le dressing lisible et bon marche.
	TestTrue(TEXT("bounded piece count"), A.Pieces.Num() > 100 && A.Pieces.Num() < 2500);
	int32 Bad = 0;
	for (const FPiece& P : A.Pieces)
	{
		double Z, Wz;
		const bool bGround = W.Ground(P.XY.X, P.XY.Y, Z);
		W.Water(P.XY.X, P.XY.Y, Wz);
		const bool bDry = bGround && Z > Wz;
		const bool bPlace = A.Places.IsValidIndex(P.Place) && A.Places[P.Place].FirstPiece <= static_cast<int32>(&P - A.Pieces.GetData());
		// Le col borde son passage jusqu'a 0.8 ; le fond aplani, lui, reste libre partout.
		const double Limit = bPlace && A.Places[P.Place].Kind == EKind::Pass ? 0.8 : ValleyReserve;
		const bool bValley = ValleyWeightAt(W.S, P.XY.X, P.XY.Y) > Limit;
		const bool bBasin = FVector2D::Distance(P.XY, FVector2D(In.Basin)) < BasinReserveTiles * W.T;
		if (!bDry || bValley || bBasin || !bPlace)
		{
			if (++Bad <= 5)
			{
				AddError(FString::Printf(TEXT("piece %s at (%.0f,%.0f): dry=%d valley=%d basin=%d place=%d"),
					FamilyName(P.Family), P.XY.X, P.XY.Y, bDry, bValley, bBasin, bPlace));
			}
		}
	}
	TestEqual(TEXT("pieces outside the reserve, on dry ground"), Bad, 0);
	// Le hameau remplace le moignon par tuile de SES ruines, et de rien d'autre.
	int32 Superseded = 0;
	for (int32 I = 0; I < W.S.Tiles.Num(); ++I)
	{
		if (!SupersedesTile(A, I)) continue;
		++Superseded;
		const EKind K = A.Places[A.TileOwner[I]].Kind;
		TestTrue(TEXT("only composed ruins supersede"), K == EKind::Hamlet || K == EKind::Vestige);
	}
	TestTrue(TEXT("hamlet supersedes its ruin tiles"), Superseded > 0);
	for (int32 I = 0; I < W.S.Tiles.Num(); ++I)
	{
		if (W.S.Tiles[I].Alt != Before[I].Alt || W.S.Tiles[I].Type != Before[I].Type) { AddError(TEXT("snapshot mutated")); break; }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPlacesWithoutGeography, "Anastasis.Places.WithoutHumanGeography",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPlacesWithoutGeography::RunTest(const FString&)
{
	// Sans geographie humaine, les lieux qui la lisent (source, col, marais, chene) se
	// declarent absents au lieu d'etre inventes ; les autres lisent les tuiles seules.
	const FTestWorld W(false, 1.0);
	FPlan P;
	FString E;
	TestTrue(TEXT("plan"), Compose(W.Inputs(), P, E));
	for (const TCHAR* Id : {TEXT("source"), TEXT("col"), TEXT("marais"), TEXT("vieux_chene")})
	{
		TestTrue(FString::Printf(TEXT("%s reported missing"), Id), P.Missing.ContainsByPredicate([Id](const FString& M) { return M.StartsWith(Id); }));
		TestFalse(FString::Printf(TEXT("%s not invented"), Id), P.Places.ContainsByPredicate([Id](const FPlace& Pl) { return Pl.Id == Id; }));
	}
	for (const TCHAR* Id : {TEXT("guet"), TEXT("hameau"), TEXT("hautes_pierres"), TEXT("vieille_foret")})
	{
		TestTrue(FString::Printf(TEXT("%s from tiles alone"), Id), P.Places.ContainsByPredicate([Id](const FPlace& Pl) { return Pl.Id == Id; }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPlacesInvalid, "Anastasis.Places.RejectsInvalidInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPlacesInvalid::RunTest(const FString&)
{
	FPlan P;
	FString E;
	FInputs In;
	TestFalse(TEXT("no source"), Compose(In, P, E));
	const FTestWorld W(true, 5.0);
	In.Source = &W.S;
	TestFalse(TEXT("no ground sampler"), Compose(In, P, E));
	AnastasisWorldView::FWorldVisualSnapshot Broken = W.S;
	Broken.Tiles.Pop();
	In.Source = &Broken;
	In.Ground = [](double, double, double& Z) { Z = 0; return true; };
	TestFalse(TEXT("malformed snapshot"), Compose(In, P, E));
	TestEqual(TEXT("no partial plan"), P.Pieces.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPlacesMeshes, "Anastasis.Places.MeshesResolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPlacesMeshes::RunTest(const FString&)
{
	// Chaque piece que le plan peut demander existe, se charge et a un volume : un mesh absent
	// laisserait un lieu a moitie pose sans que personne ne le voie.
	for (int32 F = 0; F < static_cast<int32>(EFamily::Count); ++F)
	{
		for (int32 V = 0; V < VariantCount(static_cast<EFamily>(F)); ++V)
		{
			const FString Path = MeshPath(static_cast<EFamily>(F), V);
			const UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
			if (!Mesh) { AddError(FString::Printf(TEXT("missing mesh %s"), *Path)); continue; }
			TestTrue(FString::Printf(TEXT("volume %s"), *Path), Mesh->GetBoundingBox().GetVolume() > 1.0);
		}
	}
	return true;
}
#endif
