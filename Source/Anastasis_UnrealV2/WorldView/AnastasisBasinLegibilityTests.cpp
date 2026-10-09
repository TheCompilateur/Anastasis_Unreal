#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WorldView/AnastasisCanonicalGeography.h"
#include "WorldView/AnastasisDrainage.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisWorldView.h"

namespace AnastasisBasinLegibilityTestDetail
{
struct FProfile
{
	TArray<double> RiseM;
	int32 Outside = 0;
	int32 OtherWater = 0;
	double FlattestM = TNumericLimits<double>::Max();
	FVector FlattestPoint = FVector::ZeroVector;

	void Add(const AnastasisTerrainForge::FMesh& Mesh, const AnastasisDrainage::FRiverPoint& Point,
		const FVector2D& Normal, double RadiusM)
	{
		const double Offset = RadiusM * 100.0;
		double Left = 0.0, Right = 0.0;
		if (!AnastasisTerrainForge::SampleHeight(Mesh, Point.Location.X + Normal.X * Offset,
			Point.Location.Y + Normal.Y * Offset, Left)
			|| !AnastasisTerrainForge::SampleHeight(Mesh, Point.Location.X - Normal.X * Offset,
			Point.Location.Y - Normal.Y * Offset, Right))
		{
			++Outside;
			return;
		}
		// A lake or another channel crossing the transect is not a dry valley shoulder.
		if (Left < Point.Location.Z - 10.0 || Right < Point.Location.Z - 10.0)
		{
			++OtherWater;
			return;
		}
		const double Rise = (FMath::Min(Left, Right) - Point.Location.Z) / 100.0;
		RiseM.Add(Rise);
		if (Rise < FlattestM)
		{
			FlattestM = Rise;
			FlattestPoint = Point.Location;
		}
	}

	static double Quantile(const TArray<double>& Sorted, double Fraction)
	{
		return Sorted[FMath::Clamp(FMath::FloorToInt(Fraction * (Sorted.Num() - 1)), 0, Sorted.Num() - 1)];
	}

	FString Describe(double RadiusM) const
	{
		TArray<double> Sorted = RiseM;
		Sorted.Sort();
		if (Sorted.IsEmpty()) return FString::Printf(TEXT("radius_m=%.0f samples=0 outside=%d other_water=%d"),
			RadiusM, Outside, OtherWater);
		return FString::Printf(TEXT("radius_m=%.0f samples=%d outside=%d other_water=%d rise_m_p10=%.2f p50=%.2f p90=%.2f flattest=(%.0f,%.0f)"),
			RadiusM, Sorted.Num(), Outside, OtherWater,
			Quantile(Sorted, 0.10), Quantile(Sorted, 0.50), Quantile(Sorted, 0.90),
			FlattestPoint.X / 100.0, FlattestPoint.Y / 100.0);
	}
};
}

/**
 * BASIN_LEGIBILITY_001 is a diagnostic, not a historical or visual PASS. At each
 * sampled river point it reads the rendered ground on both sides of the flow and
 * records the LOWER shoulder above the water surface. Three radii distinguish
 * a carved bank from a valley that remains legible beyond the bank material.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisBasinLegibility,
	"Anastasis.Terrain.BasinLegibility.Profile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisBasinLegibility::RunTest(const FString&)
{
	using namespace AnastasisWorldView;
	const auto Canonical = AnastasisCanonicalGeography::Compute(ReferenceSeed);
	if (!TestTrue(TEXT("canonical geography exists"), Canonical.bValid)) return false;

	FWorldVisualSnapshot Snapshot = CaptureCanonicalWorld(ReferenceSeed);
	Snapshot.SpatialScale = 5.0;
	Snapshot.bHumanGeography = true;
	AnastasisTerrainSurface::FGeometry Geometry;
	AnastasisTerrainForge::FMesh Mesh;
	const auto Forge = AnastasisTerrainForge::FSettings::Canonical();
	if (!TestTrue(TEXT("canonical surface exists"), AnastasisTerrainSurface::Build(Snapshot, Geometry))) return false;
	if (!TestTrue(TEXT("canonical relief exists"), AnastasisTerrainForge::Apply(Snapshot, Geometry, Mesh, Forge))) return false;
	AnastasisDrainage::FParams Params;
	Params.bWaterLook = true;
	Params.ForgeExaggeration = Forge.Exaggerate;
	if (Mesh.bBasinFound || Mesh.bHumanGeography) Params.Protected.Add(FVector2D(Mesh.BasinX, Mesh.BasinY));
	if (Mesh.bLandmarkFound) Params.Protected.Add(FVector2D(Mesh.LandmarkX, Mesh.LandmarkY));
	AnastasisDrainage::FNetwork Network;
	if (!TestTrue(TEXT("canonical relief drains"), AnastasisDrainage::Apply(Snapshot, Mesh, Network, Params))) return false;
	TestEqual(TEXT("same river count as canonical geography"), Network.Rivers.Num(), Canonical.Rivers);
	TestEqual(TEXT("same lake count as canonical geography"), Network.Lakes.Num(), Canonical.Lakes);

	constexpr double RadiiM[] = {30.0, 100.0, 200.0};
	AnastasisBasinLegibilityTestDetail::FProfile Profiles[UE_ARRAY_COUNT(RadiiM)];
	int32 RiverCount = 0, PointCount = 0;
	for (const AnastasisDrainage::FRiver& River : Network.Rivers)
	{
		if (River.Order < 2 || River.LengthM < 250.0 || River.Points.Num() < 3) continue;
		++RiverCount;
		for (int32 I = 1; I + 1 < River.Points.Num(); I += 4)
		{
			const FVector& Before = River.Points[I - 1].Location;
			const FVector& After = River.Points[I + 1].Location;
			const FVector2D Tangent(After.X - Before.X, After.Y - Before.Y);
			if (Tangent.SizeSquared() < 1.0) continue;
			const FVector2D Normal(-Tangent.Y, Tangent.X);
			const FVector2D UnitNormal = Normal.GetSafeNormal();
			++PointCount;
			for (int32 R = 0; R < UE_ARRAY_COUNT(RadiiM); ++R)
			{
				Profiles[R].Add(Mesh, River.Points[I], UnitNormal, RadiiM[R]);
			}
		}
	}
	AddInfo(FString::Printf(TEXT("BASIN_PROFILE seed=%u rivers_order2plus=%d sampled_points=%d"),
		ReferenceSeed, RiverCount, PointCount));
	for (int32 R = 0; R < UE_ARRAY_COUNT(RadiiM); ++R) AddInfo(Profiles[R].Describe(RadiiM[R]));
	TestTrue(TEXT("major rivers were sampled"), RiverCount >= 1 && PointCount >= 30);
	for (int32 R = 0; R < UE_ARRAY_COUNT(RadiiM); ++R)
	{
		TestTrue(FString::Printf(TEXT("paired dry shoulders at %.0f m"), RadiiM[R]), Profiles[R].RiseM.Num() >= 10);
	}
	return true;
}
#endif
