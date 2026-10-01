#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WorldView/AnastasisRiverbank.h"
#include "WorldView/AnastasisDrainage.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisWorldView.h"

namespace AnastasisRiverbankTestDetail
{
/**
 * Un chenal droit le long de X : eau a Z = 0, berges en talus de 15 % qui croisent l'eau a
 * |Y| = 1000. Moitie ouest calme (0.2 m/s), moitie est vive (3 m/s).
 */
double ChannelGround(double X, double Y) { return 0.15 * (FMath::Abs(Y) - 1000.0); }

void ChannelInputs(AnastasisRiverbank::FInputs& In, AnastasisRiverbank::FSpeedField& Speed, int32 Seed)
{
	In.SampleHeight = [](double X, double Y, double& Z) { Z = ChannelGround(X, Y); return true; };
	In.SampleWaterHeight = [](double X, double Y, double& Z) { Z = 0.0; return true; };
	In.Bounds = FBox2D(FVector2D(-10000.0, -3000.0), FVector2D(10000.0, 3000.0));
	In.Seed = Seed;
	Speed.W = 41;
	Speed.H = 13;
	Speed.X0 = -10000.0;
	Speed.Y0 = -3000.0;
	Speed.Step = 500.0;
	Speed.Speed.SetNumUninitialized(Speed.W * Speed.H);
	for (int32 Y = 0; Y < Speed.H; ++Y)
	{
		for (int32 X = 0; X < Speed.W; ++X) Speed.Speed[Y * Speed.W + X] = X < 20 ? 0.2f : 3.0f;
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisRiverbankCalmAndFast, "Anastasis.Terrain.Riverbank.CalmAndFast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisRiverbankCalmAndFast::RunTest(const FString&)
{
	namespace RB = AnastasisRiverbank;
	RB::FInputs In;
	RB::FSpeedField Speed;
	AnastasisRiverbankTestDetail::ChannelInputs(In, Speed, 12345);
	const RB::FSettings S;
	RB::FPlan Plan;
	FString Error;
	if (!TestTrue(TEXT("plan built"), RB::Build(In, Speed, S, Plan, Error))) return false;
	AddInfo(FString::Printf(TEXT("reeds=%d clumps=%d cobbles=%d boulders=%d calm_shore=%d fast_shore=%d"),
		Plan.Counts[0], Plan.ReedClumps, Plan.Counts[1], Plan.Counts[2], Plan.CalmShore, Plan.FastShore));
	TestTrue(TEXT("reed clumps on the calm half"), Plan.ReedClumps > 0 && Plan.Counts[static_cast<int32>(RB::EFamily::Reed)] > Plan.ReedClumps);
	TestTrue(TEXT("cobbles on the fast half"), Plan.Counts[static_cast<int32>(RB::EFamily::Cobble)] > 0);
	TestTrue(TEXT("both kinds of shore were examined"), Plan.CalmShore > 0 && Plan.FastShore > 0);
	int32 WrongSide = 0, BadFoot = 0;
	for (const RB::FPlacement& P : Plan.Instances)
	{
		const double Free = AnastasisRiverbankTestDetail::ChannelGround(P.Location.X, P.Location.Y);
		// Le fondu de vitesse tient dans une maille du champ (500 uu) autour de X = 0.
		switch (P.Family)
		{
		case RB::EFamily::Reed:
			WrongSide += P.Location.X > 600.0 ? 1 : 0;
			BadFoot += (Free > S.ReedMaxFreeboard + 15.0 || Free < S.ReedMinFreeboard - 30.0) ? 1 : 0;
			break;
		case RB::EFamily::Cobble:
			WrongSide += P.Location.X < -600.0 ? 1 : 0;
			BadFoot += (Free <= S.CobbleMinFreeboard || Free >= S.CobbleMaxFreeboard) ? 1 : 0;
			break;
		case RB::EFamily::Boulder:
			WrongSide += P.Location.X < -600.0 ? 1 : 0;
			BadFoot += Free > S.BoulderMaxFreeboard ? 1 : 0;
			break;
		default:
			break;
		}
		BadFoot += FMath::Abs(P.Location.Z - AnastasisRiverbankTestDetail::ChannelGround(P.Location.X, P.Location.Y)) > 1.0 ? 1 : 0;
	}
	TestEqual(TEXT("reeds and tufts in calm water, stones in fast water"), WrongSide, 0);
	TestEqual(TEXT("every foot on the ground, in its band"), BadFoot, 0);

	// Deterministe ; une autre graine change le plan.
	RB::FPlan Again, Other;
	RB::Build(In, Speed, S, Again, Error);
	bool bSame = Again.Instances.Num() == Plan.Instances.Num();
	for (int32 I = 0; bSame && I < Plan.Instances.Num(); ++I)
	{
		bSame = Again.Instances[I].Location == Plan.Instances[I].Location && Again.Instances[I].Size == Plan.Instances[I].Size;
	}
	TestTrue(TEXT("same plan, bit for bit"), bSame);
	RB::FInputs In2 = In;
	In2.Seed = 777;
	RB::Build(In2, Speed, S, Other, Error);
	TestTrue(TEXT("another seed, another plan"), Other.Instances.Num() != Plan.Instances.Num()
		|| (Other.Instances.Num() > 0 && Other.Instances[0].Location != Plan.Instances[0].Location));

	// Eau partout calme (aucun champ) : pas une pierre.
	RB::FPlan Calm;
	RB::Build(In, RB::FSpeedField(), S, Calm, Error);
	TestEqual(TEXT("no stones without current"), Calm.Counts[static_cast<int32>(RB::EFamily::Cobble)] + Calm.Counts[static_cast<int32>(RB::EFamily::Boulder)], 0);

	// Entrees invalides : refus explicite.
	RB::FInputs Bad;
	RB::FPlan Refused;
	TestFalse(TEXT("missing samplers rejected"), RB::Build(Bad, Speed, S, Refused, Error));
	RB::FSettings Wrong;
	Wrong.FastVelocity = Wrong.CalmVelocity;
	TestFalse(TEXT("inverted velocity thresholds rejected"), RB::Build(In, Speed, Wrong, Refused, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisRiverbankPaint, "Anastasis.Terrain.Riverbank.PaintBanks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisRiverbankPaint::RunTest(const FString&)
{
	namespace RB = AnastasisRiverbank;
	RB::FInputs In;
	RB::FSpeedField Speed;
	AnastasisRiverbankTestDetail::ChannelInputs(In, Speed, 1);
	// Une colonne de sommets traversant la berge, a l'ouest (calme) et a l'est (vif).
	AnastasisTerrainSurface::FGeometry Geo;
	const FLinearColor Grass(0.118f, 0.171f, 0.078f, 0.0f);
	for (const double X : {-6000.0, 6000.0})
	{
		for (int32 K = 0; K <= 40; ++K)
		{
			const double Y = 500.0 + 50.0 * K;
			const double Z = AnastasisRiverbankTestDetail::ChannelGround(X, Y);
			Geo.Vertices.Add(FVector(X, Y, Z));
			Geo.WaterVertices.Add(FVector(X, Y, 0.0));
			Geo.Colors.Add(FLinearColor(Grass.R, Grass.G, Grass.B, Z < 0.0 ? 1.0f : 0.0f));
			Geo.UV0.Add(FVector2D::ZeroVector);
			Geo.UV1.Add(FVector2D::ZeroVector);
		}
	}
	// L'eau des sommets secs pres de la rive est celle du chenal (0) ; loin, la sentinelle sol - 1 m.
	for (int32 I = 0; I < Geo.Vertices.Num(); ++I)
	{
		if (Geo.Vertices[I].Z >= 0.0) Geo.WaterVertices[I].Z = Geo.Vertices[I].Z > 150.0 ? Geo.Vertices[I].Z - 100.0 : 0.0;
	}
	const TArray<FLinearColor> Before = Geo.Colors;
	const RB::FPaintResult R = RB::PaintBanks(Speed, RB::FSettings(), Geo);
	AddInfo(FString::Printf(TEXT("mud=%d gravel=%d"), R.MudVertices, R.GravelVertices));
	TestTrue(TEXT("mud on the calm bank"), R.MudVertices > 0);
	TestTrue(TEXT("gravel on the fast bank"), R.GravelVertices > 0);
	int32 Touched = 0, Submerged = 0, Far = 0, Alpha = 0, CalmLighter = 0, FastDarkerNoRock = 0, Wet = 0;
	for (int32 I = 0; I < Geo.Vertices.Num(); ++I)
	{
		const double Free = Geo.Vertices[I].Z - Geo.WaterVertices[I].Z;
		const bool bChanged = !Geo.Colors[I].Equals(Before[I], 1.e-6f);
		Touched += bChanged ? 1 : 0;
		Alpha += Geo.Colors[I].A != Before[I].A ? 1 : 0;
		if (Free < 0.0) Submerged += bChanged ? 1 : 0;
		if (Free >= 110.0) Far += bChanged ? 1 : 0;
		if (Free >= 0.0 && Free < 20.0)
		{
			const double Luma = Geo.Colors[I].R + Geo.Colors[I].G + Geo.Colors[I].B, Base = Grass.R + Grass.G + Grass.B;
			if (Geo.Vertices[I].X < 0.0)
			{
				CalmLighter += Luma >= Base ? 1 : 0;
				Wet += Geo.UV1[I].Y >= 0.6 ? 1 : 0;
			}
			else
			{
				FastDarkerNoRock += (Luma <= Base || Geo.UV0[I].X < 0.3) ? 1 : 0;
			}
		}
	}
	TestTrue(TEXT("banks painted"), Touched > 0);
	TestEqual(TEXT("submerged bed untouched"), Submerged, 0);
	TestEqual(TEXT("dry land far from water untouched"), Far, 0);
	TestEqual(TEXT("underwater flag (alpha) kept"), Alpha, 0);
	TestEqual(TEXT("calm waterline darker than grass (mud)"), CalmLighter, 0);
	TestTrue(TEXT("calm waterline shines (wetness above DampStart)"), Wet > 0);
	TestEqual(TEXT("fast waterline lighter and stony (gravel)"), FastDarkerNoRock, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisRiverbankCanonical, "Anastasis.Terrain.Riverbank.CanonicalWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisRiverbankCanonical::RunTest(const FString&)
{
	namespace RB = AnastasisRiverbank;
	AnastasisWorldView::FWorldVisualSnapshot S = AnastasisWorldView::CaptureCanonicalWorld(AnastasisWorldView::ReferenceSeed);
	S.SpatialScale = 5;
	S.bHumanGeography = true;
	AnastasisTerrainSurface::FGeometry G;
	AnastasisTerrainForge::FMesh M;
	if (!TestTrue(TEXT("forge"), AnastasisTerrainSurface::Build(S, G) && AnastasisTerrainForge::Apply(S, G, M))) return false;
	AnastasisDrainage::FParams Params;
	Params.bWaterLook = true;
	AnastasisDrainage::FNetwork Net;
	if (!TestTrue(TEXT("drains"), AnastasisDrainage::Apply(S, M, Net, Params))) return false;
	// Les lacs aux rives continues ne cassent pas le reseau.
	const AnastasisDrainage::FCheck C = AnastasisDrainage::Check(Net, M);
	TestEqual(TEXT("no isolated water body"), C.IsolatedWaterBodies, 0);
	TestEqual(TEXT("every interior lake is fed or drained"), C.LakesWithoutRole, 0);
	TestEqual(TEXT("every mouth reaches a river, a lake or the world edge"), C.DanglingMouths, 0);

	RB::FSpeedField Speed;
	RB::BuildSpeedField(Net, Speed);
	TestTrue(TEXT("speed field on the drainage grid"), Speed.IsValid() && Speed.W == Net.GridW && Speed.H == Net.GridH);
	double MaxSpeed = 0.0;
	for (const float V : Speed.Speed) MaxSpeed = FMath::Max(MaxSpeed, static_cast<double>(V));
	TestTrue(TEXT("rivers carry their velocity into the field"), MaxSpeed > 0.5);

	AnastasisTerrainForge::SetActive(M);
	RB::FInputs In;
	In.SampleHeight = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActive(X, Y, Z); };
	In.SampleWaterHeight = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActiveWater(X, Y, Z); };
	const double T = AnastasisWorldView::TileWorldSize * S.SpatialScale;
	In.Bounds = FBox2D(FVector2D(S.OriginX * T, S.OriginY * T), FVector2D((S.OriginX + S.W) * T, (S.OriginY + S.H) * T));
	In.Seed = S.Seed;
	RB::FPlan Plan;
	FString Error;
	const bool bBuilt = RB::Build(In, Speed, RB::FSettings(), Plan, Error);
	int32 Floating = 0;
	for (const RB::FPlacement& P : Plan.Instances)
	{
		double Z = 0.0;
		Floating += (!AnastasisTerrainForge::SampleActive(P.Location.X, P.Location.Y, Z) || FMath::Abs(Z - P.Location.Z) > 1.0) ? 1 : 0;
	}
	AnastasisTerrainForge::ClearActive();
	if (!TestTrue(TEXT("plan built"), bBuilt)) return false;
	AddInfo(FString::Printf(TEXT("reeds=%d clumps=%d cobbles=%d boulders=%d calm_shore=%d fast_shore=%d max_speed=%.2f plan_ms=%.0f"),
		Plan.Counts[0], Plan.ReedClumps, Plan.Counts[1], Plan.Counts[2], Plan.CalmShore, Plan.FastShore, MaxSpeed, Plan.MilliSeconds));
	TestTrue(TEXT("reed beds exist"), Plan.ReedClumps >= 20);
	TestTrue(TEXT("cobbles where the water runs"), Plan.Counts[static_cast<int32>(RB::EFamily::Cobble)] > 0);
	TestTrue(TEXT("calm and fast shores both exist"), Plan.CalmShore > 0 && Plan.FastShore > 0);
	TestFalse(TEXT("not truncated"), Plan.bTruncated);
	TestEqual(TEXT("every instance stands on the rendered ground"), Floating, 0);
	return true;
}
#endif
