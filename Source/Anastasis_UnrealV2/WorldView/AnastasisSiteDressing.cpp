#include "WorldView/AnastasisSiteDressing.h"

#include "WorldView/AnastasisHumanGeography.h"

namespace AnastasisSiteDressing
{
FRead Read(double TileX, double TileY)
{
	const AnastasisHumanGeography::FSample Sample = AnastasisHumanGeography::Evaluate(TileX, TileY, 0.0);
	FRead Out;
	Out.Valley = Sample.ValleyWeight;
	Out.River = Sample.RiverWeight;
	Out.Passage = Sample.PassageWeight;
	Out.bPassage = Sample.PassageWeight > 0.62;
	Out.bRiparian = Sample.RiverWeight > 0.22;
	Out.bMeadow = Sample.ValleyWeight > 0.58 && Sample.RiverWeight < 0.18 && Sample.PassageWeight < 0.35;
	return Out;
}

bool ShouldOmitForest(const FRead& Site, uint32 VisualSeed)
{
	if (Site.bPassage)
	{
		return true;
	}
	if (Site.bMeadow)
	{
		return (VisualSeed % 13u) != 0u;
	}
	return false;
}

bool IsFutureSettlement(double TileX, double TileY)
{
	const double Primary = FMath::Square(TileX - 53.0) + FMath::Square(TileY - 53.0);
	const double Secondary = FMath::Square(TileX - 62.0) + FMath::Square(TileY - 47.0);
	return Primary < 4.5 * 4.5 || Secondary < 4.0 * 4.0;
}

namespace
{
void Add(TArray<FProp>& Out, const TCHAR* Mesh, double X, double Y, float Yaw, float Scale,
	float StepX, float StepY, ESeat Seat, bool bStone, bool bBlock, bool bSink, bool bTree)
{
	FProp Prop;
	Prop.Mesh = Mesh;
	Prop.X = X;
	Prop.Y = Y;
	Prop.Yaw = Yaw;
	Prop.Scale = Scale;
	Prop.StepX = StepX;
	Prop.StepY = StepY;
	Prop.Seat = Seat;
	Prop.bStone = bStone;
	Prop.bBlock = bBlock;
	Prop.bSink = bSink;
	Prop.bTree = bTree;
	Out.Add(Prop);
}

const TCHAR* Conifer = TEXT("/Game/Anastasis/Vegetation/SM_Tree_Conifer_Emergent_01.SM_Tree_Conifer_Emergent_01");
const TCHAR* Broadleaf = TEXT("/Game/Anastasis/Vegetation/SM_Tree_Broadleaf_Emergent_01.SM_Tree_Broadleaf_Emergent_01");
const TCHAR* Log = TEXT("/Game/Anastasis/Ecotone/SM_Ecotone_FallenLog_01.SM_Ecotone_FallenLog_01");
const TCHAR* Drift = TEXT("/Game/Anastasis/Ecotone/SM_Ecotone_Driftwood_01.SM_Ecotone_Driftwood_01");
const TCHAR* Branches = TEXT("/Game/Anastasis/Ecotone/SM_Ecotone_BranchPile_01.SM_Ecotone_BranchPile_01");
const TCHAR* Roots = TEXT("/Game/Anastasis/Ecotone/SM_Ecotone_ExposedRoots_01.SM_Ecotone_ExposedRoots_01");
const TCHAR* Reed = TEXT("/Game/Anastasis/Ecotone/SM_Ecotone_Reed_01.SM_Ecotone_Reed_01");
const TCHAR* Shore = TEXT("/Game/Anastasis/Ecotone/SM_Ecotone_ShoreTuft_01.SM_Ecotone_ShoreTuft_01");
const TCHAR* Grass = TEXT("/Game/Anastasis/Ecotone/SM_Ecotone_GrassTuft_01.SM_Ecotone_GrassTuft_01");
const TCHAR* Bush = TEXT("/Game/Anastasis/Ecotone/SM_Ecotone_Bush_Low_01.SM_Ecotone_Bush_Low_01");
const TCHAR* Sapling = TEXT("/Game/Anastasis/Ecotone/SM_Ecotone_Sapling_01.SM_Ecotone_Sapling_01");
const TCHAR* Stump = TEXT("/Game/Anastasis/Ecotone/SM_Ecotone_Stump_01.SM_Ecotone_Stump_01");
const TCHAR* Summit = TEXT("/Game/Anastasis/Lithos/SM_Lithos_Summit_01.SM_Lithos_Summit_01");
const TCHAR* Wall = TEXT("/Game/Anastasis/Lithos/SM_Lithos_VerticalWall_01.SM_Lithos_VerticalWall_01");
const TCHAR* Inclined = TEXT("/Game/Anastasis/Lithos/SM_Lithos_InclinedWall_01.SM_Lithos_InclinedWall_01");
const TCHAR* Talus = TEXT("/Game/Anastasis/Lithos/SM_Lithos_TalusCluster_01.SM_Lithos_TalusCluster_01");
const TCHAR* Outcrop = TEXT("/Game/Anastasis/Lithos/SM_Lithos_Outcrop_01.SM_Lithos_Outcrop_01");
const TCHAR* Cliff = TEXT("/Game/Anastasis/Rock/SM_Rock_CliffFragment_01.SM_Rock_CliffFragment_01");
const TCHAR* CliffB = TEXT("/Game/Anastasis/Rock/SM_Rock_CliffFragment_02.SM_Rock_CliffFragment_02");
const TCHAR* Vertical = TEXT("/Game/Anastasis/Rock/SM_Rock_Vertical_01.SM_Rock_Vertical_01");
const TCHAR* Boulder = TEXT("/Game/Anastasis/Rock/SM_Rock_Boulder_02.SM_Rock_Boulder_02");
const TCHAR* Low = TEXT("/Game/Anastasis/Rock/SM_Rock_Low_01.SM_Rock_Low_01");
const TCHAR* LowB = TEXT("/Game/Anastasis/Rock/SM_Rock_Low_02.SM_Rock_Low_02");
const TCHAR* Hearth = TEXT("/Game/Anastasis/Architecture/SM_Ruin_Foyer_02.SM_Ruin_Foyer_02");
const TCHAR* Borrowed = TEXT("/Game/Anastasis/Architecture/SM_Ruin_Reemploi_01.SM_Ruin_Reemploi_01");
const TCHAR* BorrowedB = TEXT("/Game/Anastasis/Architecture/SM_Ruin_Reemploi_03.SM_Ruin_Reemploi_03");
}

void AppendCompositions(TArray<FProp>& Out)
{
	// The pass. Two conifers and rock shoulders; the tread itself stays empty.
	Add(Out, Wall, 39.7, 35.0, 35.f, 4.0f, -0.30f, 0.25f, ESeat::Dry, true, true, true, false);
	Add(Out, CliffB, 39.2, 35.6, 80.f, 2.6f, -0.30f, 0.25f, ESeat::Dry, true, true, true, false);
	Add(Out, Talus, 40.2, 34.4, 10.f, 3.0f, -0.25f, 0.20f, ESeat::Dry, true, true, true, false);
	Add(Out, Conifer, 38.8, 35.8, 20.f, 7.2f, -0.30f, 0.25f, ESeat::Dry, false, true, false, true);
	Add(Out, Inclined, 44.4, 31.0, 200.f, 3.6f, 0.30f, -0.25f, ESeat::Dry, true, true, true, false);
	Add(Out, Low, 44.9, 30.4, 40.f, 1.5f, 0.30f, -0.25f, ESeat::Dry, true, true, true, false);
	Add(Out, Conifer, 45.2, 31.8, 140.f, 6.6f, 0.30f, -0.20f, ESeat::Dry, false, true, false, true);

	// The high point the relief actually keeps, south of the wet shoulder.
	Add(Out, Summit, 67.2, 13.6, 210.f, 5.2f, 0.10f, 0.25f, ESeat::Dry, true, true, true, false);
	Add(Out, Vertical, 66.4, 14.2, 250.f, 3.1f, 0.10f, 0.25f, ESeat::Dry, true, true, true, false);
	Add(Out, Cliff, 68.0, 13.8, 180.f, 2.3f, 0.10f, 0.25f, ESeat::Dry, true, true, true, false);
	Add(Out, Talus, 67.8, 14.8, 30.f, 2.8f, 0.10f, 0.22f, ESeat::Dry, true, true, true, false);
	Add(Out, Boulder, 66.0, 13.0, 15.f, 2.0f, 0.10f, 0.22f, ESeat::Dry, true, true, true, false);
	Add(Out, Conifer, 65.6, 15.4, 40.f, 6.8f, 0.10f, 0.25f, ESeat::Dry, false, true, false, true);
	Add(Out, Conifer, 68.6, 15.2, 300.f, 7.4f, 0.10f, 0.22f, ESeat::Dry, false, true, false, true);

	// The hooked bend of the north river: a jam on the bank, not debris in the field.
	Add(Out, Log, 49.1, 59.5, 95.f, 3.4f, 0.f, 0.12f, ESeat::Dry, false, true, false, false);
	Add(Out, Log, 49.5, 59.7, 70.f, 3.0f, 0.f, 0.12f, ESeat::Dry, false, true, false, false);
	Add(Out, Drift, 48.9, 59.4, 40.f, 2.6f, 0.f, 0.12f, ESeat::Dry, false, true, false, false);
	Add(Out, Branches, 49.6, 59.8, 15.f, 2.3f, 0.f, 0.12f, ESeat::Dry, false, true, false, false);
	Add(Out, Roots, 49.3, 59.9, 120.f, 2.1f, 0.f, 0.12f, ESeat::Dry, false, true, false, false);
	Add(Out, Stump, 50.0, 59.6, 0.f, 1.8f, 0.f, 0.12f, ESeat::Dry, false, true, false, false);
	Add(Out, Reed, 49.2, 58.8, 10.f, 1.35f, 0.f, -0.10f, ESeat::Bank, false, false, false, false);
	Add(Out, Reed, 48.7, 58.7, 40.f, 1.2f, 0.f, -0.10f, ESeat::Bank, false, false, false, false);
	Add(Out, Reed, 49.7, 58.9, 80.f, 1.5f, 0.f, -0.10f, ESeat::Bank, false, false, false, false);
	Add(Out, Reed, 50.1, 58.6, 0.f, 1.25f, 0.f, -0.10f, ESeat::Bank, false, false, false, false);
	Add(Out, Reed, 48.4, 58.9, 160.f, 1.15f, 0.f, -0.10f, ESeat::Bank, false, false, false, false);

	// The brook leaving the high valley. The throat is the obstacle, the floor is not.
	Add(Out, Log, 23.4, 13.6, 140.f, 3.2f, 0.30f, 0.30f, ESeat::Dry, false, true, false, false);
	Add(Out, Log, 22.6, 12.8, 125.f, 2.8f, 0.30f, 0.30f, ESeat::Dry, false, true, false, false);
	Add(Out, Drift, 23.8, 14.2, 40.f, 2.4f, 0.30f, 0.30f, ESeat::Dry, false, true, false, false);
	Add(Out, LowB, 22.2, 13.8, 20.f, 1.4f, 0.25f, 0.25f, ESeat::Dry, true, true, true, false);
	Add(Out, Reed, 22.8, 12.4, 30.f, 1.3f, -0.25f, -0.25f, ESeat::Bank, false, false, false, false);
	Add(Out, Reed, 23.6, 12.6, 90.f, 1.2f, -0.25f, -0.25f, ESeat::Bank, false, false, false, false);
	Add(Out, Reed, 22.4, 13.0, 150.f, 1.4f, -0.25f, -0.25f, ESeat::Bank, false, false, false, false);

	// The lake's south shore, and one rock across the water. The outlet stays clear.
	Add(Out, Reed, 69.0, 66.6, 20.f, 1.4f, 0.f, 0.30f, ESeat::Bank, false, false, false, false);
	Add(Out, Reed, 68.3, 66.4, 70.f, 1.2f, 0.f, 0.30f, ESeat::Bank, false, false, false, false);
	Add(Out, Reed, 69.7, 66.5, 0.f, 1.5f, 0.f, 0.30f, ESeat::Bank, false, false, false, false);
	Add(Out, Shore, 68.6, 66.9, 15.f, 1.6f, 0.f, 0.25f, ESeat::Bank, false, false, false, false);
	Add(Out, Shore, 69.5, 67.1, 80.f, 1.4f, 0.f, 0.25f, ESeat::Bank, false, false, false, false);
	Add(Out, Drift, 68.0, 67.2, 30.f, 2.5f, 0.f, 0.30f, ESeat::Bank, false, true, false, false);
	Add(Out, Outcrop, 65.8, 73.2, 140.f, 4.2f, -0.40f, 0.20f, ESeat::Dry, true, true, true, false);

	// One hearth and two borrowed stones. Not a house.
	Add(Out, Hearth, 56.4, 63.6, 25.f, 1.4f, 0.f, 0.12f, ESeat::Dry, true, true, true, false);
	Add(Out, Borrowed, 56.7, 63.5, 70.f, 1.15f, 0.f, 0.12f, ESeat::Dry, true, true, true, false);
	Add(Out, BorrowedB, 56.2, 63.8, 200.f, 1.05f, 0.f, 0.12f, ESeat::Dry, true, true, true, false);
	Add(Out, Stump, 56.9, 63.9, 10.f, 1.7f, 0.f, 0.12f, ESeat::Dry, false, true, false, false);

	// The high valley's one tree, and the edge of ground someone could later work.
	Add(Out, Broadleaf, 30.6, 26.4, 35.f, 7.6f, 0.2f, 0.2f, ESeat::Dry, false, true, false, true);
	Add(Out, Sapling, 36.2, 22.4, 80.f, 1.8f, 0.2f, -0.2f, ESeat::Dry, false, false, false, false);
	Add(Out, Stump, 35.4, 21.6, 0.f, 1.9f, 0.2f, -0.2f, ESeat::Dry, false, true, false, false);

	// Where the pass delivers you into the open land. The floors stay empty.
	Add(Out, Grass, 56.6, 49.8, 10.f, 1.5f, 0.2f, -0.15f, ESeat::Dry, false, false, false, false);
	Add(Out, Grass, 57.5, 48.7, 40.f, 1.3f, 0.2f, -0.15f, ESeat::Dry, false, false, false, false);
	Add(Out, Grass, 58.2, 49.5, 90.f, 1.6f, 0.2f, -0.15f, ESeat::Dry, false, false, false, false);
	Add(Out, Grass, 57.0, 50.3, 140.f, 1.4f, 0.2f, -0.15f, ESeat::Dry, false, false, false, false);
	Add(Out, Grass, 56.3, 48.8, 200.f, 1.5f, 0.2f, -0.15f, ESeat::Dry, false, false, false, false);
	Add(Out, Bush, 57.8, 50.1, 30.f, 2.4f, 0.2f, -0.15f, ESeat::Dry, false, false, false, false);
	Add(Out, Bush, 56.9, 48.5, 110.f, 2.2f, 0.2f, -0.15f, ESeat::Dry, false, false, false, false);
}
}
