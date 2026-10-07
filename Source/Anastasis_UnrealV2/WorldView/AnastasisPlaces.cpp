#include "WorldView/AnastasisPlaces.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RandomStream.h"
#include "Misc/Paths.h"
#include "WorldView/AnastasisHumanGeography.h"
#include "WorldView/AnastasisTerrainSurface.h"

namespace
{
using namespace AnastasisPlaces;
using AnastasisWorld::ETileType;
using AnastasisWorldView::FVisualTile;
using AnastasisWorldView::FWorldVisualSnapshot;

enum class EMat : uint8 { Own, Stone, Ruin };

struct FSpec
{
	const TCHAR* Stem;
	int32 Variants;
	EMat Mat;
	bool bCollide;
};

// Un rang par EFamily, dans l'ordre. Rock et ruines n'ont pas de couleur de sommet (leur
// materiau d'import est WorldGrid) : ils prennent la teinte. Lithos, Ecotone et arbres
// portent la leur.
constexpr FSpec Specs[] = {
	{TEXT("Rock/SM_Rock_Vertical"), 3, EMat::Stone, true},
	{TEXT("Rock/SM_Rock_Boulder"), 3, EMat::Stone, true},
	{TEXT("Rock/SM_Rock_Massive"), 3, EMat::Stone, true},
	{TEXT("Rock/SM_Rock_Split"), 3, EMat::Stone, true},
	{TEXT("Rock/SM_Rock_CliffFragment"), 3, EMat::Stone, true},
	{TEXT("Rock/SM_Rock_Low"), 3, EMat::Stone, true},
	{TEXT("Rock/SM_Rock_Cluster"), 1, EMat::Stone, true},
	{TEXT("Lithos/SM_Lithos_Summit"), 1, EMat::Own, true},
	{TEXT("Lithos/SM_Lithos_Outcrop"), 1, EMat::Own, true},
	{TEXT("Lithos/SM_Lithos_Stratum"), 1, EMat::Own, true},
	{TEXT("Lithos/SM_Lithos_VerticalWall"), 1, EMat::Own, true},
	{TEXT("Lithos/SM_Lithos_InclinedWall"), 1, EMat::Own, true},
	{TEXT("Lithos/SM_Lithos_Cornice"), 1, EMat::Own, true},
	{TEXT("Lithos/SM_Lithos_TalusCluster"), 1, EMat::Own, true},
	{TEXT("Lithos/SM_Lithos_DetachedBlock"), 1, EMat::Own, true},
	{TEXT("Lithos/SM_Lithos_Fractured"), 1, EMat::Own, true},
	{TEXT("Architecture/SM_Ruin_Soubassement"), 3, EMat::Ruin, true},
	{TEXT("Architecture/SM_Ruin_Angle"), 3, EMat::Ruin, true},
	{TEXT("Architecture/SM_Ruin_Mur"), 3, EMat::Ruin, true},
	{TEXT("Architecture/SM_Ruin_Foyer"), 3, EMat::Ruin, true},
	{TEXT("Architecture/SM_Ruin_Enclos"), 3, EMat::Ruin, true},
	{TEXT("Architecture/SM_Ruin_Reemploi"), 3, EMat::Ruin, false},
	{TEXT("Vegetation/SM_Tree_Broadleaf_Emergent"), 0, EMat::Own, true},
	{TEXT("Vegetation/SM_Tree_Conifer_Emergent"), 0, EMat::Own, true},
	{TEXT("Vegetation/SM_Tree_Broadleaf_Canopy"), 0, EMat::Own, true},
	{TEXT("Ecotone/SM_Ecotone_FallenLog"), 1, EMat::Own, true},
	{TEXT("Ecotone/SM_Ecotone_Stump"), 1, EMat::Own, true},
	{TEXT("Ecotone/SM_Ecotone_BranchPile"), 1, EMat::Own, false},
	{TEXT("Ecotone/SM_Ecotone_ExposedRoots"), 1, EMat::Own, false},
	{TEXT("Ecotone/SM_Ecotone_Bush_Low"), 1, EMat::Own, false},
	{TEXT("Ecotone/SM_Ecotone_Sapling"), 1, EMat::Own, false},
	{TEXT("Ecotone/SM_Ecotone_Reed"), 1, EMat::Own, false},
	{TEXT("Ecotone/SM_Ecotone_ShoreTuft"), 1, EMat::Own, false},
	{TEXT("Ecotone/SM_Ecotone_Driftwood"), 1, EMat::Own, false},
};
static_assert(UE_ARRAY_COUNT(Specs) == static_cast<int32>(EFamily::Count), "one spec per EFamily");

const FSpec& Spec(EFamily F) { return Specs[static_cast<int32>(F)]; }

// Pierre froide des affleurements, calcaire plus chaud et plus clair des ruines : de loin,
// ce qui a ete bati se distingue de ce qui a pousse du sol.
const FLinearColor StoneTint(0.105f, 0.101f, 0.096f, 1.0f);
const FLinearColor RuinTint(0.255f, 0.228f, 0.188f, 1.0f);

/** Marge au-dessus de l'eau pour qu'une piece compte comme posee au sec (uu). */
constexpr double DryMargin = 30.0;

double Heading(const FVector2D& D) { return FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)); }
FVector2D Dir(double Deg) { const double R = FMath::DegreesToRadians(Deg); return FVector2D(FMath::Cos(R), FMath::Sin(R)); }

struct FComponent
{
	TArray<int32> Tiles;
	FVector2D Centroid = FVector2D::ZeroVector;
	bool bTouchesBorder = false;
};

class FComposer
{
public:
	FComposer(const AnastasisPlaces::FInputs& InInputs, AnastasisPlaces::FPlan& InOut)
		: In(InInputs), S(*InInputs.Source), Out(InOut)
		, T(AnastasisWorldView::TileWorldSize * InInputs.Source->SpatialScale)
		, bHG(InInputs.Source->bHumanGeography && InInputs.Source->Seed == AnastasisWorldView::ReferenceSeed)
	{
		Owner.Init(INDEX_NONE, S.Tiles.Num());
	}

	void Run()
	{
		// Ordre = priorite : un lieu pose avant reserve ses tuiles, les suivants les evitent.
		// Les lieux qui nomment la geographie (guet, source, col, marais) passent d'abord ;
		// les etendues (rochers, ruines, foret) remplissent autour.
		Lookout();
		Spring();
		Pass();
		Marsh();
		Hamlet();
		Crags();
		OldTree();
		OldWood();
		Out.TileOwner = Owner;
	}

private:
	const AnastasisPlaces::FInputs& In;
	const FWorldVisualSnapshot& S;
	AnastasisPlaces::FPlan& Out;
	const double T;
	const bool bHG;
	/** Lieu proprietaire de chaque tuile : un lieu ne pose rien sur les tuiles d'un autre. */
	TArray<int32> Owner;
	int32 Current = INDEX_NONE;
	/** Poids de vallee au-dela duquel rien ne se pose. Le col le releve le temps de border son passage. */
	double ValleyLimit = ValleyReserve;
	/** Tete de la riviere et direction de l'aval, trouvees par Spring(), suivies par Marsh(). */
	bool bSpring = false;
	FVector2D SpringHead = FVector2D::ZeroVector;
	FVector2D SpringDown = FVector2D(1, 0);

	// ---- lecture du monde --------------------------------------------------------------

	const FVisualTile* TileAt(int32 X, int32 Y) const
	{
		const int32 LX = X - S.OriginX, LY = Y - S.OriginY;
		if (LX < 0 || LY < 0 || LX >= S.W || LY >= S.H) return nullptr;
		return &S.Tiles[LY * S.W + LX];
	}
	FVector2D Center(const FVisualTile& Tile) const { return FVector2D((Tile.X + 0.5) * T, (Tile.Y + 0.5) * T); }
	bool Ground(const FVector2D& P, double& Z) const { return In.Ground(P.X, P.Y, Z); }
	double WaterZ(const FVector2D& P) const
	{
		double W = AnastasisTerrainSurface::WaterPlaneZ;
		if (In.Water) In.Water(P.X, P.Y, W);
		return W;
	}
	bool Dry(const FVector2D& P, double Margin = DryMargin) const
	{
		double Z;
		return Ground(P, Z) && Z > WaterZ(P) + Margin;
	}
	/** Hauteur au-dessus de l'eau locale, ou -1 si pas de sol. */
	double Freeboard(const FVector2D& P) const
	{
		double Z;
		return Ground(P, Z) ? Z - WaterZ(P) : -1.0;
	}
	/** Pente (degres) et direction de descente, mesurees sur le sol rendu. */
	double Slope(const FVector2D& P, FVector2D* Downhill = nullptr) const
	{
		const double H = 0.3 * T;
		double X0, X1, Y0, Y1;
		if (!Ground(P - FVector2D(H, 0), X0) || !Ground(P + FVector2D(H, 0), X1)
			|| !Ground(P - FVector2D(0, H), Y0) || !Ground(P + FVector2D(0, H), Y1))
		{
			if (Downhill) *Downhill = FVector2D(1, 0);
			return 90.0;
		}
		const FVector2D G((X1 - X0) / (2 * H), (Y1 - Y0) / (2 * H));
		if (Downhill) *Downhill = G.IsNearlyZero() ? FVector2D(1, 0) : -G.GetSafeNormal();
		return FMath::RadiansToDegrees(FMath::Atan(G.Size()));
	}
	AnastasisHumanGeography::FSample Geo(const FVector2D& P) const
	{
		return AnastasisHumanGeography::Evaluate(P.X / T, P.Y / T, 0.0);
	}
	bool Reserved(const FVector2D& P) const
	{
		if (ValleyWeightAt(S, P.X, P.Y) > ValleyLimit) return true;
		return In.bBasin && FVector2D::Distance(P, FVector2D(In.Basin)) < BasinReserveTiles * T;
	}
	int32 IndexAt(const FVector2D& P) const
	{
		const int32 X = FMath::FloorToInt(P.X / T) - S.OriginX, Y = FMath::FloorToInt(P.Y / T) - S.OriginY;
		return (X < 0 || Y < 0 || X >= S.W || Y >= S.H) ? INDEX_NONE : Y * S.W + X;
	}
	bool Claimed(const FVector2D& P) const
	{
		const int32 I = IndexAt(P);
		return I != INDEX_NONE && Owner[I] != INDEX_NONE && Owner[I] != Current;
	}
	void Claim(const TArray<int32>& Tiles)
	{
		for (const int32 I : Tiles)
		{
			if (Owner[I] == INDEX_NONE) Owner[I] = Current;
		}
	}
	/** Tuiles dont le centre tombe dans le disque. */
	TArray<int32> Disc(const FVector2D& C, double Radius) const
	{
		TArray<int32> Tiles;
		for (int32 I = 0; I < S.Tiles.Num(); ++I)
		{
			if (FVector2D::DistSquared(Center(S.Tiles[I]), C) <= Radius * Radius) Tiles.Add(I);
		}
		return Tiles;
	}
	/** Les tuiles et leur premier anneau : la marge d'un lieu etendu. */
	TArray<int32> Grow(const TArray<int32>& Tiles) const
	{
		TSet<int32> Set;
		for (const int32 I : Tiles)
		{
			const int32 X = I % S.W, Y = I / S.W;
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					if (X + DX >= 0 && Y + DY >= 0 && X + DX < S.W && Y + DY < S.H) Set.Add((Y + DY) * S.W + X + DX);
				}
			}
		}
		TArray<int32> Result = Set.Array();
		Result.Sort();
		return Result;
	}
	bool InWorld(const FVector2D& P) const
	{
		return P.X >= S.OriginX * T && P.Y >= S.OriginY * T && P.X <= (S.OriginX + S.W) * T && P.Y <= (S.OriginY + S.H) * T;
	}

	TArray<FComponent> Components(ETileType Type, bool bSkipReserved) const
	{
		TArray<int32> Label;
		Label.Init(INDEX_NONE, S.Tiles.Num());
		TArray<FComponent> Result;
		for (int32 Seed = 0; Seed < S.Tiles.Num(); ++Seed)
		{
			const FVisualTile& First = S.Tiles[Seed];
			if (Label[Seed] != INDEX_NONE || First.Type != Type || (bSkipReserved && Reserved(Center(First)))) continue;
			FComponent C;
			TArray<int32> Queue{Seed};
			Label[Seed] = Result.Num();
			for (int32 Q = 0; Q < Queue.Num(); ++Q)
			{
				const int32 I = Queue[Q];
				C.Tiles.Add(I);
				const int32 X = I % S.W, Y = I / S.W;
				C.bTouchesBorder |= X == 0 || Y == 0 || X == S.W - 1 || Y == S.H - 1;
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					for (int32 DX = -1; DX <= 1; ++DX)
					{
						const int32 NX = X + DX, NY = Y + DY;
						if ((DX == 0 && DY == 0) || NX < 0 || NY < 0 || NX >= S.W || NY >= S.H) continue;
						const int32 N = NY * S.W + NX;
						if (Label[N] != INDEX_NONE || S.Tiles[N].Type != Type) continue;
						if (bSkipReserved && Reserved(Center(S.Tiles[N]))) continue;
						Label[N] = Result.Num();
						Queue.Add(N);
					}
				}
			}
			for (const int32 I : C.Tiles) C.Centroid += Center(S.Tiles[I]);
			C.Centroid /= C.Tiles.Num();
			Result.Add(MoveTemp(C));
		}
		// Taille decroissante, puis ordre de balayage : stable d'un run a l'autre.
		Result.StableSort([](const FComponent& A, const FComponent& B) { return A.Tiles.Num() > B.Tiles.Num(); });
		return Result;
	}

	// ---- ecriture du plan --------------------------------------------------------------

	FRandomStream Stream(uint32 Salt) const { return FRandomStream(static_cast<int32>(HashCombineFast(S.Seed, Salt))); }

	/** Ouvre un lieu. Radius > 0 reserve le disque ; un lieu etendu reserve ensuite ses tuiles. */
	void Begin(EKind Kind, const FString& Id, const TCHAR* Name, const FVector2D& C, double Radius)
	{
		AnastasisPlaces::FPlace P;
		P.Kind = Kind;
		P.Id = Id;
		P.Name = Name;
		P.Center = C;
		P.Radius = Radius;
		P.FirstPiece = Out.Pieces.Num();
		Current = Out.Places.Add(P);
		if (Radius > 0) Claim(Disc(C, Radius));
	}
	void End()
	{
		AnastasisPlaces::FPlace& P = Out.Places[Current];
		P.NumPieces = Out.Pieces.Num() - P.FirstPiece;
		if (P.NumPieces == 0)
		{
			// Un lieu vide n'existe pas : il rend ses tuiles.
			Out.Missing.Add(FString::Printf(TEXT("%s:empty@(%.1f,%.1f)"), *P.Id, P.Center.X / T, P.Center.Y / T));
			for (int32& O : Owner)
			{
				if (O == Current) O = INDEX_NONE;
			}
			Out.Places.Pop();
		}
		Current = INDEX_NONE;
	}
	void Miss(const TCHAR* Id, const TCHAR* Why) { Out.Missing.Add(FString::Printf(TEXT("%s:%s"), Id, Why)); }

	/** Ajoute une piece si son pied est au sec, dans le monde, hors reserve et hors d'un autre lieu. */
	bool Put(FRandomStream& R, EFamily F, const FVector2D& P, double Yaw, double Scale, double Sink = 0.1,
		double Tilt = 0.0, double Toward = 0.0, bool bFollow = false, int32 Variant = -1)
	{
		if (!InWorld(P) || !Dry(P) || Reserved(P) || Claimed(P)) return false;
		AnastasisPlaces::FPiece Piece;
		Piece.Family = F;
		const int32 Count = FMath::Max(1, Spec(F).Variants);
		Piece.Variant = static_cast<uint8>(Variant >= 0 ? Variant % Count : R.RandRange(0, Count - 1));
		Piece.Place = Current;
		Piece.XY = P;
		Piece.Yaw = FRotator::NormalizeAxis(Yaw);
		Piece.Scale = Scale;
		Piece.Sink = Sink;
		Piece.Tilt = Tilt;
		Piece.TiltToward = Toward;
		Piece.bFollowSlope = bFollow;
		Out.Pieces.Add(Piece);
		return true;
	}
	FVector2D Jitter(FRandomStream& R, const FVector2D& P, double Radius) const
	{
		const double A = R.FRandRange(0.0, 360.0), D = Radius * FMath::Sqrt(R.FRand());
		return P + Dir(A) * D;
	}

	// ---- LE GUET : le point haut de la forge devient un repere ---------------------------

	void Lookout()
	{
		if (!In.bLandmark) { Miss(TEXT("guet"), TEXT("no_landmark")); return; }
		const FVector2D L(In.Landmark);
		FRandomStream R = Stream(0x6775u);
		const FVector2D Toward = In.bBasin ? (FVector2D(In.Basin) - L).GetSafeNormal()
			: (FVector2D(S.OriginX + S.W * 0.5, S.OriginY + S.H * 0.5) * T - L).GetSafeNormal();
		const FVector2D Side(-Toward.Y, Toward.X);
		Begin(EKind::Lookout, TEXT("guet"), TEXT("Le Guet"), L, 3.2 * T);
		// La couronne : un tor d'une vingtaine de metres, visible de toute la vallee, flanque de
		// deux bancs de roche. A 1,9 km de cote, un repere plus petit disparait.
		Put(R, EFamily::RockVertical, L, R.FRandRange(0, 360), 6.5, 0.12, 4.0, Heading(Toward));
		Put(R, EFamily::RockMassive, L + Side * 0.3 * T - Toward * 0.2 * T, R.FRandRange(0, 360), 6.0, 0.18, 9.0, Heading(Side));
		Put(R, EFamily::RockSplit, L - Side * 0.32 * T - Toward * 0.1 * T, R.FRandRange(0, 360), 5.5, 0.18, 6.0, Heading(-Side));
		Put(R, EFamily::RockCliff, L - Toward * 0.4 * T, R.FRandRange(0, 360), 4.5, 0.2, 12.0, Heading(-Toward));
		// La tour de guet effondree, au pied du tor, cote vallee : le guetteur s'abritait du vent.
		const FVector2D Tower = L + Toward * 0.6 * T;
		const double TowerYaw = Heading(Toward);
		Put(R, EFamily::RuinSoubassement, Tower, TowerYaw, 1.6, 0.08, 0, 0, true, 1);
		Put(R, EFamily::RuinAngle, Tower + Side * 0.06 * T, TowerYaw + 180, 1.6, 0.05, 0, 0, true, 0);
		Put(R, EFamily::RuinMur, Tower - Toward * 0.28 * T, TowerYaw + 90, 1.5, 0.1, 0, 0, true);
		for (int32 I = 0; I < 7; ++I)
		{
			Put(R, EFamily::RuinReemploi, Jitter(R, Tower, 0.9 * T), R.FRandRange(0, 360), R.FRandRange(1.0, 1.5), 0.3, 0, 0, true);
		}
		// Eboulis sous le tor, cote pente.
		for (int32 I = 0; I < 5; ++I)
		{
			FVector2D Down;
			Slope(L, &Down);
			const FVector2D P = L + Dir(Heading(Down) + R.FRandRange(-60, 60)) * R.FRandRange(1.2, 2.6) * T;
			Put(R, EFamily::LithosTalus, P, R.FRandRange(0, 360), R.FRandRange(5.0, 7.0), 0.25);
		}
		End();
	}

	// ---- LA SOURCE : un cercle de pierres levees ou nait la riviere ----------------------

	void Spring()
	{
		if (!bHG) { Miss(TEXT("source"), TEXT("no_human_geography")); return; }
		// Tete de la riviere = le point d'eau courante le plus haut.
		double Best = -1.e9;
		FVector2D Head = FVector2D::ZeroVector;
		int32 N = 0;
		TArray<TPair<FVector2D, double>> River;
		for (double V = S.OriginY + 0.25; V < S.OriginY + S.H; V += 0.5)
		{
			for (double U = S.OriginX + 0.25; U < S.OriginX + S.W; U += 0.5)
			{
				const FVector2D P(U * T, V * T);
				const auto G = Geo(P);
				if (G.RiverWeight > 0.95) River.Add({P, G.WaterHeight});
			}
		}
		for (const auto& Pt : River) Best = FMath::Max(Best, Pt.Value);
		for (const auto& Pt : River)
		{
			if (Pt.Value >= Best - 0.02) { Head += Pt.Key; ++N; }
		}
		if (N == 0) { Miss(TEXT("source"), TEXT("no_river")); return; }
		Head /= N;
		// Aval : la plus basse des eaux courantes a trois tuiles.
		FVector2D Down(1, 0);
		double Low = 1.e9;
		for (int32 K = 0; K < 72; ++K)
		{
			const FVector2D P = Head + Dir(K * 5.0) * 3.0 * T;
			const auto G = Geo(P);
			if (G.RiverWeight > 0.5 && G.WaterHeight < Low) { Low = G.WaterHeight; Down = Dir(K * 5.0); }
		}
		bSpring = true;
		SpringHead = Head;
		SpringDown = Down;
		FRandomStream R = Stream(0x5052u);
		const FVector2D Ring = Head + Down * 0.35 * T;
		Begin(EKind::Spring, TEXT("source"), TEXT("La Source aux Pierres"), Ring, 3.4 * T);
		// Onze pierres levees, deux couchees. Le cercle s'ouvre la ou l'eau sort : une pierre
		// tombee dans le lit n'est simplement pas posee.
		const double Start = R.FRandRange(0, 360);
		for (int32 K = 0; K < 11; ++K)
		{
			const double A = Start + K * (360.0 / 11) + R.FRandRange(-6, 6);
			const FVector2D P = Ring + Dir(A) * 1.1 * T * R.FRandRange(0.95, 1.05);
			const bool bFallen = K == 3 || K == 8;
			Put(R, EFamily::RockVertical, P, A + 90 + R.FRandRange(-15, 15), R.FRandRange(2.0, 2.8),
				bFallen ? 0.3 : 0.14, bFallen ? R.FRandRange(72, 84) : R.FRandRange(0, 7), A + R.FRandRange(-40, 40));
		}
		// Au centre, si le sol le permet, un bloc plat : l'autel ou la pierre de seuil.
		Put(R, EFamily::LithosDetached, Ring, R.FRandRange(0, 360), 4.0, 0.2);
		// Le vieil arbre qui veille, en amont, hors du cercle.
		for (int32 Try = 0; Try < 12; ++Try)
		{
			const FVector2D P = Head - Down * (2.4 + 0.15 * Try) * T + FVector2D(-Down.Y, Down.X) * (Try % 3 - 1) * 0.6 * T;
			if (Put(R, EFamily::TreeBroadleafEmergent, P, R.FRandRange(0, 360), 17.0, 0.02))
			{
				Put(R, EFamily::ExposedRoots, P, R.FRandRange(0, 360), 4.5, 0.05);
				Put(R, EFamily::BranchPile, P + Dir(R.FRandRange(0, 360)) * 0.7 * T, R.FRandRange(0, 360), 3.0, 0.05);
				break;
			}
		}
		// Roseaux et touffes sur la frange humide de la source.
		RingOfReeds(R, Head, 0.3 * T, 2.6 * T, 14, 20.0, 200.0);
		End();
	}

	/** Touffes de roseaux par massifs, seulement sur la frange a peine au-dessus de l'eau. */
	void RingOfReeds(FRandomStream& R, const FVector2D& C, double MinR, double MaxR, int32 Clumps, double MinFree, double MaxFree)
	{
		int32 Made = 0;
		for (int32 Try = 0; Try < Clumps * 8 && Made < Clumps; ++Try)
		{
			const FVector2D P = C + Dir(R.FRandRange(0, 360)) * R.FRandRange(MinR, MaxR);
			const double F = Freeboard(P);
			if (F < MinFree || F > MaxFree) continue;
			++Made;
			const int32 Stems = R.RandRange(8, 14);
			for (int32 K = 0; K < Stems; ++K)
			{
				Put(R, EFamily::Reed, Jitter(R, P, 0.2 * T), R.FRandRange(0, 360), R.FRandRange(1.2, 2.0), 0.02, R.FRandRange(0, 9), R.FRandRange(0, 360));
			}
			for (int32 K = 0; K < 3; ++K)
			{
				Put(R, EFamily::ShoreTuft, Jitter(R, P, 0.3 * T), R.FRandRange(0, 360), R.FRandRange(1.6, 2.4), 0.02);
			}
		}
	}

	// ---- LE COL : la selle entre les deux vallees ----------------------------------------

	void Pass()
	{
		if (!bHG) { Miss(TEXT("col"), TEXT("no_human_geography")); return; }
		// Le col est le point le plus haut de ce que la geographie humaine a aplani : le fond
		// des vallees est plus bas que la selle qui les relie.
		double BestZ = -1.e9;
		FVector2D Crest = FVector2D::ZeroVector;
		for (double V = S.OriginY + 0.25; V < S.OriginY + S.H; V += 0.5)
		{
			for (double U = S.OriginX + 0.25; U < S.OriginX + S.W; U += 0.5)
			{
				const FVector2D P(U * T, V * T);
				const auto G = Geo(P);
				double Z;
				if (G.ValleyWeight < 0.97 || G.RiverWeight > 0.05 || !Ground(P, Z)) continue;
				if (Z > BestZ) { BestZ = Z; Crest = P; }
			}
		}
		if (BestZ < -1.e8) { Miss(TEXT("col"), TEXT("no_saddle")); return; }
		// L'axe du passage : la direction ou le terrain aplani continue.
		double BestW = -1;
		FVector2D Axis(1, 0);
		for (int32 K = 0; K < 36; ++K)
		{
			const double W = Geo(Crest + Dir(K * 5.0) * 2.5 * T).ValleyWeight + Geo(Crest - Dir(K * 5.0) * 2.5 * T).ValleyWeight;
			if (W > BestW) { BestW = W; Axis = Dir(K * 5.0); }
		}
		const FVector2D Side(-Axis.Y, Axis.X);
		FRandomStream R = Stream(0x434Fu);
		Begin(EKind::Pass, TEXT("col"), TEXT("Le Col"), Crest, 6.0 * T);
		// Le col borde son propre passage : le fond aplani (poids > 0.8) reste libre pour la
		// future route, mais ses levres, que la reserve generale refuse, lui appartiennent.
		ValleyLimit = 0.8;
		// La porte : deux pierres dressees d'une dizaine de metres a la levre du passage.
		for (const double Sign : {1.0, -1.0})
		{
			for (double D = 0.8; D < 4.0; D += 0.1)
			{
				const FVector2D P = Crest + Side * Sign * D * T;
				if (Put(R, EFamily::RockVertical, P, Heading(Axis) + R.FRandRange(-10, 10), 3.4, 0.1, 4.0, Heading(Side * Sign), false, Sign > 0 ? 0 : 2))
				{
					// Le cairn des voyageurs, au pied de la pierre de droite, cote passage.
					if (Sign > 0) Put(R, EFamily::RockCluster, P + Axis * 0.5 * T + Side * 0.2 * T, R.FRandRange(0, 360), 2.2, 0.15);
					break;
				}
			}
		}
		// Blocs tombes sur les epaules : le col est une breche, pas une prairie.
		for (int32 I = 0; I < 22; ++I)
		{
			const double Sign = (I % 2) ? 1.0 : -1.0;
			const FVector2D P = Crest + Axis * R.FRandRange(-5.0, 5.0) * T + Side * Sign * R.FRandRange(1.6, 5.0) * T;
			const EFamily F = (I % 3 == 0) ? EFamily::RockSplit : (I % 3 == 1 ? EFamily::RockBoulder : EFamily::RockMassive);
			const double Scale = R.FRandRange(2.2, 4.0);
			Put(R, F, P, R.FRandRange(0, 360), Scale, 0.22, R.FRandRange(0, 18), R.FRandRange(0, 360));
		}
		// Un pin seul, penche par le vent qui passe la selle.
		for (double D = 1.5; D < 5.0; D += 0.25)
		{
			if (Put(R, EFamily::TreeConiferEmergent, Crest - Side * D * T - Axis * 0.8 * T, R.FRandRange(0, 360), 13.0, 0.02, 7.0, Heading(Axis)))
			{
				break;
			}
		}
		ValleyLimit = ValleyReserve;
		End();
	}

	// ---- LE MARAIS : l'embouchure de la riviere dans le lac -------------------------------

	void Marsh()
	{
		if (!bHG || !bSpring) { Miss(TEXT("marais"), TEXT("no_river")); return; }
		// Le lac : l'eau REELLEMENT rendue, profonde (des tuiles dont toute la fenetre 5x5 est
		// sous l'eau), qui ne touche pas le bord du monde (la mer). Les tuiles Water
		// de simulation ne suffisent pas : la vallee aplanie a comble d'anciens etangs, et le lit
		// de la riviere est de l'eau sans etre un lac.
		TArray<uint8> Wet, Lake;
		Wet.Init(0, S.Tiles.Num());
		Lake.Init(0, S.Tiles.Num());
		for (int32 I = 0; I < S.Tiles.Num(); ++I) Wet[I] = Freeboard(Center(S.Tiles[I])) < -20.0 ? 1 : 0;
		TArray<uint8> Seen;
		Seen.Init(0, S.Tiles.Num());
		int32 BestInterior = 0;
		for (int32 First = 0; First < S.Tiles.Num(); ++First)
		{
			if (!Wet[First] || Seen[First]) continue;
			TArray<int32> Body{First};
			Seen[First] = 1;
			bool bSea = false;
			int32 Interior = 0;
			TArray<int32> Deep;
			for (int32 Q = 0; Q < Body.Num(); ++Q)
			{
				const int32 X = Body[Q] % S.W, Y = Body[Q] / S.W;
				bSea |= X == 0 || Y == 0 || X == S.W - 1 || Y == S.H - 1;
				int32 WetNeighbours = 0;
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					for (int32 DX = -1; DX <= 1; ++DX)
					{
						const int32 NX = X + DX, NY = Y + DY;
						if ((DX == 0 && DY == 0) || NX < 0 || NY < 0 || NX >= S.W || NY >= S.H) continue;
						const int32 N = NY * S.W + NX;
						if (!Wet[N]) continue;
						++WetNeighbours;
						if (!Seen[N]) { Seen[N] = 1; Body.Add(N); }
					}
				}
				// Coeur profond : toute la fenetre 5x5 sous l'eau. Une riviere de trois tuiles de
				// large a des tuiles entourees d'eau, jamais de coeur profond ; un lac, si.
				bool bDeep = WetNeighbours == 8;
				for (int32 DY = -2; DY <= 2 && bDeep; ++DY)
				{
					for (int32 DX = -2; DX <= 2 && bDeep; ++DX)
					{
						const int32 NX = X + DX, NY = Y + DY;
						bDeep = NX >= 0 && NY >= 0 && NX < S.W && NY < S.H && Wet[NY * S.W + NX];
					}
				}
				if (bDeep) { ++Interior; Deep.Add(Body[Q]); }
			}
			if (bSea || Interior < 4 || Interior <= BestInterior) continue;
			// LE lac : la plus grande etendue interieure. Une riviere peut s'elargir en mare dans
			// la vallee ; le delta, lui, se jette dans le lac.
			BestInterior = Interior;
			// L'eau libre seulement : le coeur profond et deux tuiles autour. Le corps connexe
			// entier comprendrait la riviere qui s'y jette, jusqu'en haut de la vallee.
			Lake.Init(0, S.Tiles.Num());
			for (const int32 I : Deep)
			{
				const int32 X = I % S.W, Y = I / S.W;
				for (int32 DY = -2; DY <= 2; ++DY)
				{
					for (int32 DX = -2; DX <= 2; ++DX)
					{
						if (X + DX >= 0 && Y + DY >= 0 && X + DX < S.W && Y + DY < S.H) Lake[(Y + DY) * S.W + X + DX] = 1;
					}
				}
			}
		}
		auto NearLake = [&](const FVector2D& P, double Radius)
		{
			for (const int32 I : Disc(P, Radius))
			{
				if (Lake[I]) return true;
			}
			return false;
		};
		// L'embouchure : on descend la riviere depuis sa source, toujours vers l'eau plus basse,
		// jusqu'a toucher un lac. C'est la riviere qui designe son delta, pas une distance.
		FVector2D Walk = SpringHead, Heading2 = SpringDown;
		bool bMouth = false;
		for (int32 Step = 0; Step < 400 && !bMouth; ++Step)
		{
			double BestH = 1.e9;
			FVector2D Next = Walk;
			for (int32 K = -12; K <= 12; ++K)
			{
				const FVector2D D = Dir(Heading(Heading2) + K * 5.0);
				const FVector2D Q = Walk + D * 0.5 * T;
				const auto G = Geo(Q);
				if (G.RiverWeight < 0.5) continue;
				// A hauteur d'eau egale (le bief final est plat), garder le cap.
				const double H = G.WaterHeight + 0.0005 * FMath::Abs(K);
				if (H < BestH) { BestH = H; Next = Q; }
			}
			if (Next == Walk) break;
			Heading2 = (Next - Walk).GetSafeNormal();
			Walk = Next;
			// Une source peut sourdre au bord d'un etang : le delta est en aval, pas au depart.
			bMouth = FVector2D::Distance(Walk, SpringHead) > 12.0 * T && NearLake(Walk, 1.6 * T);
		}
		// Le trace peut s'arreter un peu avant la rive du lac qu'il nourrit.
		if (!bMouth) bMouth = FVector2D::Distance(Walk, SpringHead) > 12.0 * T && NearLake(Walk, 4.5 * T);
		if (!bMouth) { Miss(TEXT("marais"), TEXT("no_mouth")); return; }
		const FVector2D Mouth = Walk;
		FRandomStream R = Stream(0x4D41u);
		Begin(EKind::Marsh, TEXT("marais"), TEXT("Le Marais"), Mouth, 4.5 * T);
		const double Radius = 4.2 * T;
		// Roselieres denses sur la frange basse, bois flotte sur la ligne d'eau, bois mort
		// un peu plus haut : l'eau qui monte et redescend a tue quelques arbres.
		// Une roseliere se lit en masse ou pas du tout : beaucoup de touffes, serrees.
		RingOfReeds(R, Mouth, 0.2 * T, Radius, 44, 10.0, 320.0);
		for (int32 Try = 0, Made = 0; Try < 120 && Made < 10; ++Try)
		{
			const FVector2D P = Mouth + Dir(R.FRandRange(0, 360)) * R.FRandRange(0.3, 1.0) * Radius;
			const double F = Freeboard(P);
			if (F < 10.0 || F > 90.0) continue;
			Made += Put(R, EFamily::Driftwood, P, R.FRandRange(0, 360), R.FRandRange(2.2, 3.4), 0.25, 0, 0, true) ? 1 : 0;
		}
		for (int32 Try = 0, Made = 0; Try < 120 && Made < 9; ++Try)
		{
			const FVector2D P = Mouth + Dir(R.FRandRange(0, 360)) * R.FRandRange(0.3, 1.0) * Radius;
			const double F = Freeboard(P);
			if (F < 60.0 || F > 350.0) continue;
			const EFamily Kind = (Made % 3 == 0) ? EFamily::FallenLog : EFamily::Stump;
			Made += Put(R, Kind, P, R.FRandRange(0, 360), Kind == EFamily::FallenLog ? R.FRandRange(2.2, 3.0) : R.FRandRange(2.0, 2.8),
				Kind == EFamily::FallenLog ? 0.25 : 0.1, 0, 0, Kind == EFamily::FallenLog) ? 1 : 0;
		}
		for (int32 Try = 0, Made = 0; Try < 60 && Made < 4; ++Try)
		{
			const FVector2D P = Mouth + Dir(R.FRandRange(0, 360)) * R.FRandRange(0.4, 1.0) * Radius;
			const double F = Freeboard(P);
			if (F < 40.0 || F > 300.0) continue;
			Made += Put(R, EFamily::ExposedRoots, P, R.FRandRange(0, 360), R.FRandRange(2.5, 3.5), 0.05) ? 1 : 0;
		}
		End();
	}

	// ---- L'ANCIEN HAMEAU : les tuiles de ruines lues comme un lieu habite -----------------

	void Hamlet()
	{
		const TArray<FComponent> Ruins = Components(ETileType::Ruin, true);
		if (Ruins.Num() == 0) { Miss(TEXT("hameau"), TEXT("no_ruin")); return; }
		for (int32 Rank = 0; Rank < FMath::Min(4, Ruins.Num()); ++Rank)
		{
			const FComponent& C = Ruins[Rank];
			if (Rank > 0 && C.Tiles.Num() < 5) break;
			FRandomStream R = Stream(0x4841u + Rank * 131u);
			// Le centre est une tuile du lieu, pas le barycentre, qui peut tomber hors du lieu.
			int32 Core = C.Tiles[0];
			for (const int32 I : C.Tiles)
			{
				if (FVector2D::DistSquared(Center(S.Tiles[I]), C.Centroid) < FVector2D::DistSquared(Center(S.Tiles[Core]), C.Centroid)) Core = I;
			}
			const FVector2D Mid = Center(S.Tiles[Core]);
			double Extent = 0;
			for (const int32 I : C.Tiles) Extent = FMath::Max(Extent, FVector2D::Distance(Center(S.Tiles[I]), Mid));
			FVector2D Down;
			Slope(Mid, &Down);
			// Un hameau a une orientation : les maisons se tournent vers la pente, a quelques degres pres.
			const double SiteYaw = Heading(Down) + R.FRandRange(-8, 8);
			if (Rank == 0)
			{
				Begin(EKind::Hamlet, TEXT("hameau"), TEXT("L'Ancien Hameau"), Mid, 0.0);
			}
			else
			{
				Begin(EKind::Vestige, FString::Printf(TEXT("vestiges_%d"), Rank), TEXT("Vestiges"), Mid, 0.0);
			}
			Out.Places[Current].Radius = Extent + T;
			Claim(Grow(C.Tiles));
			// Maisons par echantillonnage du plus lointain : reparties, jamais empilees.
			const int32 Houses = Rank == 0 ? FMath::Clamp(C.Tiles.Num() / 4, 3, 9) : 1;
			TArray<FVector2D> Sites{Mid};
			while (Sites.Num() < Houses)
			{
				double Far = -1;
				FVector2D Next = Mid;
				for (const int32 I : C.Tiles)
				{
					const FVector2D P = Center(S.Tiles[I]);
					double Near = 1.e18;
					for (const FVector2D& Q : Sites) Near = FMath::Min(Near, FVector2D::DistSquared(P, Q));
					if (Near > Far) { Far = Near; Next = P; }
				}
				if (Far < FMath::Square(0.9 * T)) break;
				Sites.Add(Next);
			}
			for (int32 H = 0; H < Sites.Num(); ++H)
			{
				const FVector2D P = Jitter(R, Sites[H], 0.15 * T);
				const double Yaw = SiteYaw + 90.0 * R.RandRange(0, 3) + R.FRandRange(-5, 5);
				const double Scale = R.FRandRange(1.05, 1.25);
				switch ((H + Rank) % 4)
				{
				case 0:
					Put(R, EFamily::RuinSoubassement, P, Yaw, Scale, 0.1, 0, 0, true);
					Put(R, EFamily::RuinAngle, P + Dir(Yaw) * 0.08 * T, Yaw + 180, Scale, 0.06, 0, 0, true);
					break;
				case 1:
					Put(R, EFamily::RuinAngle, P, Yaw, Scale, 0.08, 0, 0, true);
					break;
				case 2:
					Put(R, EFamily::RuinFoyer, P, Yaw, Scale, 0.1, 0, 0, true);
					Put(R, EFamily::RuinMur, P + Dir(Yaw + 90) * 0.22 * T, Yaw, Scale, 0.1, 0, 0, true);
					break;
				default:
					Put(R, EFamily::RuinSoubassement, P, Yaw, Scale, 0.12, 0, 0, true);
					break;
				}
				// La nature reprend : buissons dans les pieces, un arbre dans une maison.
				for (int32 B = 0; B < 2; ++B)
				{
					Put(R, EFamily::BushLow, Jitter(R, P, 0.35 * T), R.FRandRange(0, 360), R.FRandRange(3.0, 4.5), 0.05);
				}
				if (Rank == 0 && H == 1) Put(R, EFamily::TreeBroadleafCanopy, P + Dir(Yaw + 45) * 0.12 * T, R.FRandRange(0, 360), 8.5, 0.02);
			}
			if (Rank == 0)
			{
				// L'enclos : murets bas au bord du hameau, alignes sur son orientation.
				int32 Walls = 0;
				for (const int32 I : C.Tiles)
				{
					if (Walls >= 4) break;
					const FVisualTile& Tl = S.Tiles[I];
					int32 Outside = 0;
					FVector2D Out2 = FVector2D::ZeroVector;
					for (const FIntPoint D : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
					{
						const FVisualTile* Nb = TileAt(Tl.X + D.X, Tl.Y + D.Y);
						if (Nb && Nb->Type != ETileType::Ruin) { ++Outside; Out2 += FVector2D(D.X, D.Y); }
					}
					if (Outside < 2 || R.FRand() > 0.5) continue;
					const FVector2D Normal = Out2.GetSafeNormal();
					// Le muret court le long du bord : perpendiculaire a la normale sortante,
					// arrondi a l'orientation du hameau.
					const double Along = FMath::RoundToDouble((Heading(FVector2D(-Normal.Y, Normal.X)) - SiteYaw) / 90.0) * 90.0 + SiteYaw;
					Walls += Put(R, EFamily::RuinEnclos, Center(Tl) + Normal * 0.35 * T, Along, R.FRandRange(1.2, 1.45), 0.2, 0, 0, true) ? 1 : 0;
				}
			}
			// Pierres de remploi : ce que les suivants viendront chercher.
			const int32 Stones = Rank == 0 ? FMath::Clamp(C.Tiles.Num() / 2, 6, 16) : 3;
			for (int32 K = 0; K < Stones; ++K)
			{
				const FVector2D P = Jitter(R, Center(S.Tiles[C.Tiles[R.RandRange(0, C.Tiles.Num() - 1)]]), 0.5 * T);
				Put(R, EFamily::RuinReemploi, P, R.FRandRange(0, 360), R.FRandRange(0.9, 1.35), 0.35, 0, 0, true);
			}
			End();
		}
	}

	// ---- LES HAUTES PIERRES : le plus grand affleurement devient presque impraticable ------

	void Crags()
	{
		// La pierre de simulation dessine surtout un anneau le long du rempart de bordure : la
		// plus grande composante n'est pas un lieu, c'est un bord. Un chaos se lit la ou la
		// pierre est DENSE et haute, loin du bord du monde.
		TArray<uint8> IsStone;
		IsStone.Init(0, S.Tiles.Num());
		double MinAlt = 1.e9, MaxAlt = -1.e9;
		for (int32 I = 0; I < S.Tiles.Num(); ++I)
		{
			if (S.Tiles[I].Type != ETileType::Stone || Reserved(Center(S.Tiles[I])) || Owner[I] != INDEX_NONE) continue;
			IsStone[I] = 1;
			MinAlt = FMath::Min(MinAlt, S.Tiles[I].Alt);
			MaxAlt = FMath::Max(MaxAlt, S.Tiles[I].Alt);
		}
		if (MaxAlt < MinAlt) { Miss(TEXT("hautes_pierres"), TEXT("no_stone")); return; }
		constexpr int32 Reach = 5;
		TArray<double> Score;
		Score.Init(0.0, S.Tiles.Num());
		for (int32 I = 0; I < S.Tiles.Num(); ++I)
		{
			if (!IsStone[I]) continue;
			const int32 X = I % S.W, Y = I / S.W;
			if (FMath::Min(FMath::Min(X, Y), FMath::Min(S.W - 1 - X, S.H - 1 - Y)) < 6) continue;
			int32 Dense = 0;
			for (int32 DY = -Reach; DY <= Reach; ++DY)
			{
				for (int32 DX = -Reach; DX <= Reach; ++DX)
				{
					const int32 NX = X + DX, NY = Y + DY;
					if (DX * DX + DY * DY <= Reach * Reach && NX >= 0 && NY >= 0 && NX < S.W && NY < S.H) Dense += IsStone[NY * S.W + NX];
				}
			}
			const double Height = (S.Tiles[I].Alt - MinAlt) / FMath::Max(1.e-6, MaxAlt - MinAlt);
			Score[I] = Dense * (0.6 + 0.4 * Height);
		}
		TArray<FVector2D> Cores;
		for (int32 Pick = 0, Minor = 0; Pick < 12 && Minor < 5; ++Pick)
		{
			int32 BestI = INDEX_NONE;
			for (int32 I = 0; I < S.Tiles.Num(); ++I)
			{
				if (Score[I] <= 0 || Owner[I] != INDEX_NONE || (BestI != INDEX_NONE && Score[I] <= Score[BestI])) continue;
				bool bFar = true;
				for (const FVector2D& Q : Cores) bFar &= FVector2D::Distance(Center(S.Tiles[I]), Q) >= 16.0 * T;
				if (bFar) BestI = I;
			}
			if (BestI == INDEX_NONE) break;
			const FVector2D Core = Center(S.Tiles[BestI]);
			const bool bMain = Cores.Num() == 0;
			Cores.Add(Core);
			TArray<int32> Tiles;
			for (const int32 I : Disc(Core, (bMain ? 6.5 : 4.0) * T))
			{
				if (IsStone[I] && Owner[I] == INDEX_NONE) Tiles.Add(I);
			}
			if (Tiles.Num() < (bMain ? 12 : 5)) continue;
			FRandomStream R = Stream(0x4352u + Pick * 97u);
			if (bMain)
			{
				Begin(EKind::Crags, TEXT("hautes_pierres"), TEXT("Les Hautes Pierres"), Core, 0.0);
			}
			else
			{
				++Minor;
				Begin(EKind::Outcrop, FString::Printf(TEXT("affleurement_%d"), Minor), TEXT("Affleurement"), Core, 0.0);
			}
			Out.Places[Current].Radius = (bMain ? 6.5 : 4.0) * T;
			Claim(Grow(Tiles));
			Tiles.StableSort([this](int32 A, int32 B) { return S.Tiles[A].Alt > S.Tiles[B].Alt; });
			if (!bMain)
			{
				// Un affleurement : deux ou trois bancs de roche sur ses points hauts, rien d'autre.
				for (int32 K = 0; K < FMath::Min(3, Tiles.Num()); ++K)
				{
					const FVector2D P = Jitter(R, Center(S.Tiles[Tiles[K]]), 0.3 * T);
					FVector2D Down;
					Slope(P, &Down);
					const EFamily F = K == 0 ? EFamily::RockSplit : (K == 1 ? EFamily::RockCliff : EFamily::RockLow);
					Put(R, F, P, Heading(Down) + R.FRandRange(-20, 20), R.FRandRange(3.2, 4.6), 0.22, R.FRandRange(0, 12), R.FRandRange(0, 360));
					Put(R, EFamily::RockMassive, Jitter(R, P, 0.7 * T), R.FRandRange(0, 360), R.FRandRange(2.4, 3.6), 0.25, R.FRandRange(0, 15), R.FRandRange(0, 360));
				}
				End();
				continue;
			}
			// Des tors, pas un semis : quelques amas serres de tres grands blocs sur les points
			// hauts, espaces ; entre eux, des blocs isoles ; au pied des pentes, les eboulis.
			double Lo = 1.e9, Hi = -1.e9;
			for (const int32 I : Tiles) { Lo = FMath::Min(Lo, S.Tiles[I].Alt); Hi = FMath::Max(Hi, S.Tiles[I].Alt); }
			TArray<FVector2D> Tors{Center(S.Tiles[Tiles[0]])};
			const int32 TorCount = FMath::Clamp(Tiles.Num() / 6, 4, 9);
			while (Tors.Num() < TorCount)
			{
				double BestScore = -1;
				FVector2D Next = FVector2D::ZeroVector;
				for (const int32 I : Tiles)
				{
					const FVector2D P = Center(S.Tiles[I]);
					double Near = 1.e18;
					for (const FVector2D& Q : Tors) Near = FMath::Min(Near, FVector2D::Distance(P, Q));
					if (Near < 2.5 * T) continue;
					const double Sc = Near * (0.5 + (S.Tiles[I].Alt - Lo) / FMath::Max(1.e-6, Hi - Lo));
					if (Sc > BestScore) { BestScore = Sc; Next = P; }
				}
				if (BestScore < 0) break;
				Tors.Add(Next);
			}
			for (const FVector2D& C : Tors)
			{
				FVector2D Down;
				Slope(C, &Down);
				const FVector2D Pivot = Jitter(R, C, 0.15 * T);
				const EFamily Heart = R.FRand() < 0.5 ? EFamily::RockMassive : (R.FRand() < 0.5 ? EFamily::RockSplit : EFamily::RockVertical);
				Put(R, Heart, Pivot, R.FRandRange(0, 360), R.FRandRange(4.5, 6.5), 0.15, R.FRandRange(0, 10), R.FRandRange(0, 360));
				const int32 Ring = R.RandRange(3, 5);
				const double Start = R.FRandRange(0, 360);
				for (int32 K = 0; K < Ring; ++K)
				{
					const double A = Start + K * 360.0 / Ring + R.FRandRange(-25, 25);
					const FVector2D P = Pivot + Dir(A) * R.FRandRange(0.25, 0.55) * T;
					// Des blocs arrondis, pas les strates Lithos : agrandies, celles-ci lisent comme
					// des caisses empilees (vu en capture). Lithos reste aux eboulis.
					const EFamily F = K % 3 == 0 ? EFamily::RockCliff : (K % 3 == 1 ? EFamily::RockMassive : EFamily::RockVertical);
					Put(R, F, P, R.FRandRange(0, 360), R.FRandRange(3.2, 5.0), 0.2, R.FRandRange(0, 16), R.FRandRange(0, 360));
				}
				Put(R, EFamily::LithosTalus, C + Down * R.FRandRange(0.7, 1.0) * T, Heading(Down), R.FRandRange(6.0, 8.5), 0.2);
			}
			// Blocs erratiques entre les tors : l'impraticable vient de la, pas des tors seuls.
			for (const int32 I : Tiles)
			{
				const FVector2D P = Jitter(R, Center(S.Tiles[I]), 0.4 * T);
				bool bNearTor = false;
				for (const FVector2D& Q : Tors) bNearTor |= FVector2D::Distance(P, Q) < 1.2 * T;
				if (bNearTor || R.FRand() > 0.45) continue;
				const EFamily F = R.FRand() < 0.6 ? EFamily::RockBoulder : EFamily::RockLow;
				Put(R, F, P, R.FRandRange(0, 360), F == EFamily::RockBoulder ? R.FRandRange(1.8, 3.2) : R.FRandRange(2.2, 3.4), 0.22, R.FRandRange(0, 20), R.FRandRange(0, 360));
			}
			End();
		}
	}

	// ---- LE VIEUX CHENE : un arbre seul, au rebord de la vallee ---------------------------

	void OldTree()
	{
		if (!bHG || !In.bBasin) { Miss(TEXT("vieux_chene"), TEXT("no_valley")); return; }
		const FVector2D B(In.Basin);
		double Best = -1.e18;
		FVector2D Site = FVector2D::ZeroVector;
		for (const FVisualTile& Tl : S.Tiles)
		{
			if (Tl.Type != ETileType::Grass && Tl.Type != ETileType::Field && Tl.Type != ETileType::Scrub) continue;
			const FVector2D P = Center(Tl);
			const double Valley = ValleyWeightAt(S, P.X, P.Y);
			const double Dist = FVector2D::Distance(P, B) / T;
			if (Valley < 0.04 || Valley > 0.25 || Dist < 14 || Dist > 32) continue;
			if (Reserved(P) || Claimed(P) || !Dry(P, 150.0) || Slope(P) > 10.0) continue;
			bool bNearWood = false;
			for (int32 DY = -5; DY <= 5 && !bNearWood; ++DY)
			{
				for (int32 DX = -5; DX <= 5 && !bNearWood; ++DX)
				{
					const FVisualTile* Nb = TileAt(Tl.X + DX, Tl.Y + DY);
					bNearWood = Nb && Nb->Type == ETileType::Forest;
				}
			}
			if (bNearWood) continue;
			double Z;
			Ground(P, Z);
			// Le plus haut du rebord se voit depuis tout le fond ; le hash departage les egalites.
			const double Score = Z + 40.0 * ((HashCombineFast(S.Seed, GetTypeHash(FIntPoint(Tl.X, Tl.Y))) & 0xFFFF) / 65535.0);
			if (Score > Best) { Best = Score; Site = P; }
		}
		if (Best < -1.e17) { Miss(TEXT("vieux_chene"), TEXT("no_rim")); return; }
		FRandomStream R = Stream(0x4348u);
		Begin(EKind::OldTree, TEXT("vieux_chene"), TEXT("Le Vieux Chene"), Site, 4.0 * T);
		// Seul, et rien autour : le vide fait partie du lieu.
		Put(R, EFamily::TreeBroadleafEmergent, Site, R.FRandRange(0, 360), 21.0, 0.02);
		Put(R, EFamily::ExposedRoots, Site, R.FRandRange(0, 360), 6.0, 0.04);
		Put(R, EFamily::FallenLog, Site + Dir(R.FRandRange(0, 360)) * 0.9 * T, R.FRandRange(0, 360), 2.4, 0.2, 0, 0, true);
		End();
	}

	// ---- LA VIEILLE FORET : du bois mort dans le plus grand massif ------------------------

	void OldWood()
	{
		const TArray<FComponent> Woods = Components(ETileType::Forest, true);
		if (Woods.Num() == 0) { Miss(TEXT("vieille_foret"), TEXT("no_forest")); return; }
		for (int32 Rank = 0; Rank < FMath::Min(6, Woods.Num()); ++Rank)
		{
			const FComponent& C = Woods[Rank];
			if (C.Tiles.Num() < 8) break;
			FRandomStream R = Stream(0x464Fu + Rank * 61u);
			const bool bMain = Rank == 0;
			double Extent = 0;
			for (const int32 I : C.Tiles) Extent = FMath::Max(Extent, FVector2D::Distance(Center(S.Tiles[I]), C.Centroid));
			if (bMain) Begin(EKind::OldWood, TEXT("vieille_foret"), TEXT("La Vieille Foret"), C.Centroid, 0.0);
			else Begin(EKind::OldWood, FString::Printf(TEXT("bois_%d"), Rank), TEXT("Bois"), C.Centroid, 0.0);
			// La foret ne reserve rien : servie en dernier, elle se glisse entre les autres lieux.
			Out.Places[Current].Radius = Extent + 0.5 * T;
			if (bMain)
			{
				// Les veterans : quelques geants bien au-dessus de la canopee, au coeur du massif,
				// la ou la lisiere est le plus loin. C'est eux qu'on reconnait depuis la vallee.
				TArray<TPair<int32, int32>> Depth;
				for (const int32 I : C.Tiles)
				{
					const FVisualTile& Tl = S.Tiles[I];
					int32 D = 0;
					for (int32 Ring = 1; Ring <= 4 && D == 0; ++Ring)
					{
						for (int32 DY = -Ring; DY <= Ring && D == 0; ++DY)
						{
							for (int32 DX = -Ring; DX <= Ring; ++DX)
							{
								const FVisualTile* Nb = TileAt(Tl.X + DX, Tl.Y + DY);
								if (!Nb || Nb->Type != ETileType::Forest) { D = Ring; break; }
							}
						}
					}
					Depth.Add({I, D == 0 ? 5 : D});
				}
				Depth.StableSort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B) { return A.Value > B.Value; });
				TArray<FVector2D> Giants;
				for (const TPair<int32, int32>& Dp : Depth)
				{
					if (Giants.Num() >= 7 || Dp.Value < 2) break;
					const FVisualTile& Tl = S.Tiles[Dp.Key];
					const FVector2D P = Jitter(R, Center(Tl), 0.3 * T);
					bool bSpaced = true;
					for (const FVector2D& Q : Giants) bSpaced &= FVector2D::Distance(P, Q) > 3.0 * T;
					if (!bSpaced) continue;
					// Ombre et altitude favorisent le resineux, comme le fait la grammaire forestiere.
					const EFamily F = Tl.Shade > 0.45 ? EFamily::TreeConiferEmergent : EFamily::TreeBroadleafEmergent;
					if (Put(R, F, P, R.FRandRange(0, 360), R.FRandRange(15.0, 19.0), 0.02))
					{
						Giants.Add(P);
						Put(R, EFamily::ExposedRoots, P, R.FRandRange(0, 360), R.FRandRange(3.5, 4.5), 0.04);
					}
				}
			}
			const double Deadwood = bMain ? 0.55 : 0.12;
			for (const int32 I : C.Tiles)
			{
				const FVisualTile& Tl = S.Tiles[I];
				if (R.FRand() < Deadwood)
				{
					const FVector2D P = Jitter(R, Center(Tl), 0.45 * T);
					const float Pick = R.FRand();
					if (Pick < 0.35) Put(R, EFamily::FallenLog, P, R.FRandRange(0, 360), R.FRandRange(2.0, 2.8), 0.18, 0, 0, true);
					else if (Pick < 0.6) Put(R, EFamily::Stump, P, R.FRandRange(0, 360), R.FRandRange(1.7, 2.4), 0.08);
					else if (Pick < 0.8) Put(R, EFamily::BranchPile, P, R.FRandRange(0, 360), R.FRandRange(2.4, 3.2), 0.05);
					else Put(R, EFamily::ExposedRoots, P, R.FRandRange(0, 360), R.FRandRange(2.2, 3.0), 0.05);
				}
				if (!bMain) continue;
				// Lisiere : buissons et jeunes arbres du cote ouvert, la ou la lumiere entre.
				for (const FIntPoint D : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
				{
					const FVisualTile* Nb = TileAt(Tl.X + D.X, Tl.Y + D.Y);
					if (!Nb || Nb->Type == ETileType::Forest || Nb->Type == ETileType::Water) continue;
					// FOREST_TERRAIN_P3 : plus de rang le long du bord de tuile. Profondeur et position
					// le long du bord tirees, un tiers des bords sans rien : un ourlet, pas une haie.
					if (R.FRand() < 0.33) continue;
					const FVector2D Across(-D.Y, D.X);
					const FVector2D Edge = Center(Tl) + FVector2D(D.X, D.Y) * R.FRandRange(0.1, 0.75) * T
						+ Across * R.FRandRange(-0.45, 0.45) * T;
					Put(R, EFamily::BushLow, Jitter(R, Edge, 0.2 * T), R.FRandRange(0, 360), R.FRandRange(3.0, 4.6), 0.05);
					if (R.FRand() < 0.4) Put(R, EFamily::Sapling, Jitter(R, Edge, 0.3 * T), R.FRandRange(0, 360), R.FRandRange(3.5, 5.0), 0.02);
				}
			}
			End();
		}
	}
};
}

double AnastasisPlaces::ValleyWeightAt(const AnastasisWorldView::FWorldVisualSnapshot& Source, double X, double Y)
{
	if (!Source.bHumanGeography || Source.Seed != AnastasisWorldView::ReferenceSeed) return 0.0;
	const double T = AnastasisWorldView::TileWorldSize * Source.SpatialScale;
	return AnastasisHumanGeography::Evaluate(X / T, Y / T, 0.0).ValleyWeight;
}

bool AnastasisPlaces::Compose(const AnastasisPlaces::FInputs& In, AnastasisPlaces::FPlan& Out, FString& OutError)
{
	Out = AnastasisPlaces::FPlan();
	if (!In.Source) { OutError = TEXT("places: no source snapshot"); return false; }
	const auto& S = *In.Source;
	if (S.W <= 0 || S.H <= 0 || S.Tiles.Num() != S.W * S.H) { OutError = TEXT("places: malformed snapshot"); return false; }
	if (!FMath::IsFinite(S.SpatialScale) || S.SpatialScale <= 0) { OutError = TEXT("places: invalid spatial scale"); return false; }
	if (!In.Ground) { OutError = TEXT("places: no ground sampler"); return false; }
	FComposer(In, Out).Run();
	return true;
}

bool AnastasisPlaces::SupersedesTile(const AnastasisPlaces::FPlan& Plan, int32 SourceIndex)
{
	if (!Plan.TileOwner.IsValidIndex(SourceIndex)) return false;
	const int32 Owner = Plan.TileOwner[SourceIndex];
	return Plan.Places.IsValidIndex(Owner)
		&& (Plan.Places[Owner].Kind == EKind::Hamlet || Plan.Places[Owner].Kind == EKind::Vestige);
}

int32 AnastasisPlaces::VariantCount(EFamily Family) { return FMath::Max(1, Spec(Family).Variants); }

const TCHAR* AnastasisPlaces::FamilyName(EFamily Family)
{
	const TCHAR* Stem = Spec(Family).Stem;
	const TCHAR* Slash = FCString::Strrchr(Stem, TEXT('/'));
	return Slash ? Slash + 1 : Stem;
}

FString AnastasisPlaces::MeshPath(EFamily Family, int32 Variant)
{
	const FSpec& Sp = Spec(Family);
	// Variants 0 : l'asset n'a pas de suffixe (les arbres de la grammaire sont en _01 fixe).
	const FString Name = Sp.Variants == 0
		? FString::Printf(TEXT("%s_01"), Sp.Stem)
		: FString::Printf(TEXT("%s_%02d"), Sp.Stem, FMath::Clamp(Variant, 0, Sp.Variants - 1) + 1);
	const FString Leaf = FPaths::GetCleanFilename(Name);
	return FString::Printf(TEXT("/Game/Anastasis/%s.%s"), *Name, *Leaf);
}

AnastasisPlaces::FEmbodyResult AnastasisPlaces::Embody(AActor& Owner, const AnastasisPlaces::FPlan& Plan, const AnastasisPlaces::FInputs& In,
	UMaterialInterface* ShapeMaterial, TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Components)
{
	// Composants REUTILISES d'une incarnation a l'autre, retrouves par mesh. Le nom passe
	// par MakeUniqueObjectName et peut recevoir un suffixe : le rechercher par nom de base
	// recreait les HISM a chaque incarnation. Detruire puis
	// recreer un HISM du meme nom remplace l'objet en place pendant que son arbre asynchrone
	// se construit encore : assertion InstanceReorderTable, editeur tue (vu le 2026-09-29).
	// Meme regle que GetOrCreateDressingMesh : on vide, on ne detruit pas.
	TMap<UStaticMesh*, UHierarchicalInstancedStaticMeshComponent*> Existing;
	for (UHierarchicalInstancedStaticMeshComponent* C : Components)
	{
		if (!IsValid(C)) continue;
		C->ClearInstances();
		if (UStaticMesh* Mesh = C->GetStaticMesh())
		{
			// Components only contains Places meshes; retain the first usable component
			// if an older session already accumulated duplicates.
			if (!Existing.Contains(Mesh)) Existing.Add(Mesh, C);
		}
	}
	Components.RemoveAll([](const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& C) { return !IsValid(C); });
	AnastasisPlaces::FEmbodyResult Result;
	TMap<uint32, UHierarchicalInstancedStaticMeshComponent*> ByKey;
	TMap<UHierarchicalInstancedStaticMeshComponent*, TArray<FTransform>> Batches;
	UMaterialInstanceDynamic* Tints[3] = {};
	auto Tint = [&](EMat Mat) -> UMaterialInterface*
	{
		if (Mat == EMat::Own || !ShapeMaterial) return nullptr;
		UMaterialInstanceDynamic*& Mid = Tints[static_cast<int32>(Mat)];
		if (!Mid)
		{
			Mid = UMaterialInstanceDynamic::Create(ShapeMaterial, &Owner);
			Mid->SetVectorParameterValue(TEXT("Color"), Mat == EMat::Stone ? StoneTint : RuinTint);
		}
		return Mid;
	};
	auto Ground = [&](double X, double Y, double& Z) { return In.Ground(X, Y, Z); };

	for (const AnastasisPlaces::FPiece& P : Plan.Pieces)
	{
		const uint32 Key = (static_cast<uint32>(P.Family) << 8) | P.Variant;
		UHierarchicalInstancedStaticMeshComponent** Found = ByKey.Find(Key);
		if (!Found)
		{
			UHierarchicalInstancedStaticMeshComponent* Made = nullptr;
			if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath(P.Family, P.Variant)))
			{
				const FSpec& Sp = Spec(P.Family);
				const FName Name(*FString::Printf(TEXT("Place_%s_%d"), FamilyName(P.Family), P.Variant));
				if (UHierarchicalInstancedStaticMeshComponent** Reused = Existing.Find(Mesh))
				{
					Made = *Reused;
				}
				else
				{
					Made = NewObject<UHierarchicalInstancedStaticMeshComponent>(&Owner,
						MakeUniqueObjectName(&Owner, UHierarchicalInstancedStaticMeshComponent::StaticClass(), Name));
					// Comme tout le dressing : regenere a chaque chargement, jamais serialise dans la map.
					Made->SetFlags(RF_Transient);
					Made->SetupAttachment(Owner.GetRootComponent());
					Made->SetMobility(EComponentMobility::Movable);
					Made->SetCollisionEnabled(Sp.bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
					if (Sp.bCollide) Made->SetCollisionProfileName(TEXT("BlockAll"));
					Made->SetGenerateOverlapEvents(false);
					Made->SetCanEverAffectNavigation(false);
					Made->SetCastShadow(true);
					Made->RegisterComponent();
					Components.Add(Made);
				}
				Made->SetStaticMesh(Mesh);
				if (UMaterialInterface* M = Tint(Sp.Mat)) Made->SetMaterial(0, M);
			}
			else
			{
				++Result.MissingMeshes;
			}
			Found = &ByKey.Add(Key, Made);
		}
		UHierarchicalInstancedStaticMeshComponent* Hism = *Found;
		if (!Hism) continue;

		FQuat Q(FVector::UpVector, FMath::DegreesToRadians(P.Yaw));
		if (P.Tilt > 0)
		{
			const FVector Toward(Dir(P.TiltToward), 0.0);
			Q = FQuat(FVector::CrossProduct(FVector::UpVector, Toward).GetSafeNormal(), FMath::DegreesToRadians(P.Tilt)) * Q;
		}
		const FBox Local = Hism->GetStaticMesh()->GetBoundingBox();
		FBox Box = Local.TransformBy(FTransform(Q, FVector::ZeroVector, FVector(P.Scale)));
		const double Reach = 0.45 * FMath::Max(Box.GetExtent().X, Box.GetExtent().Y) * 2.0;
		double Center;
		if (!Ground(P.XY.X, P.XY.Y, Center)) { ++Result.Ungrounded; continue; }
		double Samples[4];
		double Lowest = Center;
		const FVector2D Axes[4] = {{Reach, 0}, {-Reach, 0}, {0, Reach}, {0, -Reach}};
		for (int32 K = 0; K < 4; ++K)
		{
			Samples[K] = Center;
			if (Ground(P.XY.X + Axes[K].X, P.XY.Y + Axes[K].Y, Samples[K])) Lowest = FMath::Min(Lowest, Samples[K]);
		}
		double Base = Lowest;
		if (P.bFollowSlope && Reach > 1.0)
		{
			// Epouse le plan du sol sous l'empreinte, sans jamais basculer au-dela de 18 degres.
			FVector N(-(Samples[0] - Samples[1]) / (2 * Reach), -(Samples[2] - Samples[3]) / (2 * Reach), 1.0);
			N = N.GetSafeNormal();
			if (FMath::RadiansToDegrees(FMath::Acos(N.Z)) > 18.0) N = FMath::Lerp(FVector::UpVector, N, 0.5).GetSafeNormal();
			Q = FQuat::FindBetweenNormals(FVector::UpVector, N) * Q;
			Box = Local.TransformBy(FTransform(Q, FVector::ZeroVector, FVector(P.Scale)));
			Base = Center;
		}
		const double Height = Box.Max.Z - Box.Min.Z;
		const double Z = Base - Box.Min.Z - P.Sink * Height;
		Batches.FindOrAdd(Hism).Add(FTransform(Q, FVector(P.XY.X, P.XY.Y, Z), FVector(P.Scale)));
		++Result.Instances;
	}
	// Un ajout par composant : un seul arbre a batir, pas un par instance.
	for (TPair<UHierarchicalInstancedStaticMeshComponent*, TArray<FTransform>>& B : Batches) B.Key->AddInstances(B.Value, false);
	for (UHierarchicalInstancedStaticMeshComponent* C : Components) C->MarkRenderStateDirty();
	for (const AnastasisPlaces::FPlace& Pl : Plan.Places)
	{
		double Z = 0;
		Ground(Pl.Center.X, Pl.Center.Y, Z);
		Result.PlaceLocations.Add(FVector(Pl.Center, Z));
	}
	return Result;
}
