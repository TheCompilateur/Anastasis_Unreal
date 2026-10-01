#include "WorldView/AnastasisTerrainHorizon.h"

#include "Math/NumericLimits.h"
#include "World/AnastasisWorldNoise.h"

namespace
{
// Nom propre a ce fichier : en build unity, il partage l'unite de compilation avec
// AnastasisTerrainForge.cpp, dont le namespace anonyme a deja un SmoothStep.
double HorizonSmoothStep(double Edge0, double Edge1, double X)
{
	const double T = FMath::Clamp((X - Edge0) / FMath::Max(Edge1 - Edge0, 1.e-8), 0.0, 1.0);
	return T * T * (3.0 - 2.0 * T);
}

/**
 * Altitude lointaine RENDUE au-dessus de la mer, en unites d'altitude (x AltitudeScale x
 * SpatialScale pour des uu). Pas d'exageration a appliquer : la forge exagere l'altitude
 * de simulation, l'anneau n'en a pas.
 *
 *  - collines : bruit a ~500 m de longueur d'onde, 7 a 60 m -- l'ordre du relief de la
 *    carte elle-meme (point haut 57 m), pour que le raccord ne change pas d'echelle. Elles
 *    s'aplatissent au-dela de 1.2 km : les anneaux y sont trop espaces pour les porter
 *    (une colline de 500 m echantillonnee tous les 300 m devient du bruit de facettes) ;
 *  - chaine lointaine : montee entre RangeStart et RangeFull, 100 a 300 m, crete de bruit
 *    "ridged". Son minimum est ce qui ferme l'horizon ; le test Horizon.Closed le mesure
 *    depuis le point le plus haut de la carte.
 */
double FarAltitude(double TileX, double TileY, double DistTiles, uint32 Seed)
{
	const double N1 = AnastasisWorldNoise::Fbm(TileX / 26.0, TileY / 26.0, static_cast<double>(Seed) + 1301.0);
	const double N2 = AnastasisWorldNoise::Fbm(TileX / 85.0, TileY / 85.0, static_cast<double>(Seed) + 1777.0);
	const double Hills = 0.15 + 1.1 * N1 * N1 * (1.0 - 0.7 * HorizonSmoothStep(60.0, 160.0, DistTiles));
	const double Ridge = 1.0 - FMath::Abs(2.0 * N2 - 1.0);
	const double Range = HorizonSmoothStep(AnastasisTerrainHorizon::RangeStartTiles, AnastasisTerrainHorizon::RangeFullTiles, DistTiles)
		* (2.0 + 4.0 * Ridge * Ridge);
	return Hills + Range;
}

/** Une teinte de sol avec ses canaux morphologiques, tels que le materiau de sol les lit. */
struct FHorizonPaletteEntry
{
	FLinearColor Color = FLinearColor(0.f, 0.f, 0.f, 0.f);
	FVector2D UV0 = FVector2D::ZeroVector;
	FVector2D UV1 = FVector2D::ZeroVector;

	static FHorizonPaletteEntry Lerp(const FHorizonPaletteEntry& A, const FHorizonPaletteEntry& B, double T)
	{
		FHorizonPaletteEntry Out;
		Out.Color = FMath::Lerp(A.Color, B.Color, static_cast<float>(T));
		Out.UV0 = FMath::Lerp(A.UV0, B.UV0, T);
		Out.UV1 = FMath::Lerp(A.UV1, B.UV1, T);
		return Out;
	}
};

/**
 * Prairies de la carte : sommets de terre dont l'herbe domine (1 - Rock - Litter - Worked
 * >= 0.6) hors des zones humides (Wetness < 0.35). Tries par luminance ; Lush = moyenne de la
 * moitie sombre, Dry = moyenne de la moitie claire. Renvoie le nombre de donneurs. Moins de
 * 64 donneurs (carte sans prairie) : repli sur toute la terre, les deux palettes egales.
 */
int32 BuildHorizonPalette(const AnastasisTerrainSurface::FGeometry& Src, FHorizonPaletteEntry& OutLush, FHorizonPaletteEntry& OutDry)
{
	auto Entry = [&Src](int32 I)
	{
		FHorizonPaletteEntry E;
		E.Color = Src.Colors[I];
		E.Color.A = 0.f;
		E.UV0 = Src.UV0.IsValidIndex(I) ? Src.UV0[I] : FVector2D::ZeroVector;
		E.UV1 = Src.UV1.IsValidIndex(I) ? Src.UV1[I] : FVector2D::ZeroVector;
		return E;
	};
	auto Mean = [&Entry](const TArray<int32>& Indices, int32 Begin, int32 End)
	{
		FHorizonPaletteEntry Sum;
		for (int32 K = Begin; K < End; ++K)
		{
			const FHorizonPaletteEntry E = Entry(Indices[K]);
			Sum.Color += E.Color;
			Sum.UV0 += E.UV0;
			Sum.UV1 += E.UV1;
		}
		const double N = static_cast<double>(FMath::Max(1, End - Begin));
		Sum.Color /= static_cast<float>(N);
		Sum.UV0 /= N;
		Sum.UV1 /= N;
		Sum.Color.A = 0.f;
		return Sum;
	};

	TArray<int32> Meadow, Land;
	for (int32 I = 0; I < Src.Vertices.Num(); ++I)
	{
		if (Src.Colors[I].A >= 0.5f)
		{
			continue;
		}
		Land.Add(I);
		const FHorizonPaletteEntry E = Entry(I);
		const double Grass = 1.0 - E.UV0.X - E.UV0.Y - E.UV1.X;
		if (Grass >= 0.6 && E.UV1.Y < 0.35)
		{
			Meadow.Add(I);
		}
	}
	TArray<int32>& Donors = Meadow.Num() >= 64 ? Meadow : Land;
	if (Donors.Num() == 0)
	{
		return 0;
	}
	if (&Donors == &Land)
	{
		OutLush = OutDry = Mean(Land, 0, Land.Num());
		return Land.Num();
	}
	Donors.Sort([&Src](int32 A, int32 B) { return Src.Colors[A].GetLuminance() < Src.Colors[B].GetLuminance(); });
	const int32 Half = Donors.Num() / 2;
	OutLush = Mean(Donors, 0, Half);
	OutDry = Mean(Donors, Half, Donors.Num());
	return Donors.Num();
}

/** Somme circulaire par prefixes : moyenne d'une fenetre de 2R+1 colonnes en deux lectures. */
template <typename T>
struct TCircularMean
{
	TArray<T> Sum;
	int32 P = 0;

	void Init(const TArray<T>& Values)
	{
		P = Values.Num();
		Sum.SetNumZeroed(P + 1);
		for (int32 I = 0; I < P; ++I)
		{
			Sum[I + 1] = Sum[I] + Values[I];
		}
	}

	/** Somme (pas moyenne) de [I-R, I+R], R borne pour ne jamais compter deux fois. */
	T Total(int32 I, int32& R) const
	{
		R = FMath::Min(R, (P - 1) / 2);
		// Sum[0] et pas T() : un FVector4 construit par defaut vaut (0,0,0,1).
		T Out = Sum[0];
		int32 Lo = I - R, Hi = I + R;
		if (Lo < 0) { Out += Sum[P] - Sum[P + Lo]; Lo = 0; }
		if (Hi >= P) { Out += Sum[Hi - P + 1]; Hi = P - 1; }
		Out += Sum[Hi + 1] - Sum[Lo];
		return Out;
	}

	T Mean(int32 I, int32 R) const
	{
		const T S = Total(I, R);
		return S * (1.0 / static_cast<double>(2 * R + 1));
	}
};
}

TArray<int32> AnastasisTerrainHorizon::PerimeterIndices(int32 FineW, int32 FineH)
{
	TArray<int32> Out;
	if (FineW < 2 || FineH < 2)
	{
		return Out;
	}
	Out.Reserve(2 * (FineW - 1) + 2 * (FineH - 1));
	for (int32 X = 0; X < FineW - 1; ++X)
	{
		Out.Add(X);
	}
	for (int32 Y = 0; Y < FineH - 1; ++Y)
	{
		Out.Add(Y * FineW + FineW - 1);
	}
	for (int32 X = FineW - 1; X > 0; --X)
	{
		Out.Add((FineH - 1) * FineW + X);
	}
	for (int32 Y = FineH - 1; Y > 0; --Y)
	{
		Out.Add(Y * FineW);
	}
	return Out;
}

int32 AnastasisTerrainHorizon::FRing::RingOf(int32 Vertex) const
{
	int32 K = 0;
	while (K + 1 < RingStart.Num() && RingStart[K + 1] <= Vertex)
	{
		++K;
	}
	return K;
}

bool AnastasisTerrainHorizon::Build(const AnastasisTerrainForge::FMesh& Forge, uint32 Seed, FRing& Out,
	const AnastasisTerrainSurface::FGeometry* Rendered)
{
	Out = FRing{};
	const AnastasisTerrainSurface::FGeometry& Src = Forge.Geometry;
	const int32 FineW = Forge.FineW, FineH = Forge.FineH;
	if (FineW < 2 || FineH < 2 || Forge.Subdiv < 1 || Src.Vertices.Num() != FineW * FineH
		|| Src.Normals.Num() != Src.Vertices.Num() || Src.Colors.Num() != Src.Vertices.Num()
		|| !FMath::IsFinite(Forge.SpatialScale) || Forge.SpatialScale <= 0.0)
	{
		return false;
	}

	const TArray<int32> Edge = PerimeterIndices(FineW, FineH);
	const int32 P = Edge.Num();
	const double TileStep = AnastasisWorldView::TileWorldSize * Forge.SpatialScale;
	const double FineStep = TileStep / static_cast<double>(Forge.Subdiv);
	const double AltStep = AnastasisWorldView::AltitudeScale * Forge.SpatialScale;
	const double SeaZ = AnastasisTerrainSurface::WaterPlaneZ;

	const double MinX = Src.Vertices[0].X, MaxX = Src.Vertices[FineW - 1].X;
	const double MinY = Src.Vertices[0].Y, MaxY = Src.Vertices[(FineH - 1) * FineW].Y;
	const double CX = 0.5 * (MinX + MaxX), CY = 0.5 * (MinY + MaxY);
	const double SX = 0.5 * (MaxX - MinX), SY = 0.5 * (MaxY - MinY);
	if (SX <= 0.0 || SY <= 0.0)
	{
		return false;
	}

	// Anneaux : le premier au pas fin de la forge (le raccord garde la meme densite),
	// puis une suite geometrique jusqu'a OuterTiles. Un dernier anneau "jupe" repousse
	// le bord de l'anneau a 8x plus loin, a la meme altitude : vu d'en haut, c'est lui
	// qui cache le sol de planete au ras de l'horizon.
	TArray<double>& D = Out.Distances;
	D.Add(0.0);
	const double Outer = OuterTiles * TileStep;
	double Step = FineStep;
	while (D.Last() < Outer)
	{
		D.Add(FMath::Min(D.Last() + Step, Outer));
		Step *= RingGrowth;
	}
	D.Add(Outer * 8.0);
	const int32 Rings = D.Num();

	// Paliers de detail. Chaque anneau garde toutes les colonnes du bord (1 520), mais son
	// pas radial grandit de 15 % par anneau : a 2 km, une cellule fait 15 m de large pour
	// 300 m de long. L'ombrage a facettes du low-poly y dessine des rayures qui partent de
	// la carte. Un anneau divise donc son nombre de colonnes par deux des que son pas radial
	// depasse deux fois son pas tangentiel, un palier a la fois : les cellules restent
	// presque carrees.
	int32 MaxLevel = 0;
	while (MaxLevel < MaxDecimation && (P % (1 << (MaxLevel + 1))) == 0)
	{
		++MaxLevel;
	}
	const double HalfSize = FMath::Min(SX, SY);
	Out.RingLevel.Add(0);
	for (int32 K = 1; K < Rings; ++K)
	{
		const int32 Prev = Out.RingLevel.Last();
		const double Radial = D[K] - D[K - 1];
		const double Tangential = FineStep * (1 << Prev) * (HalfSize + D[K]) / HalfSize;
		// L'anneau 1 reste au pas fin : sa bande est celle du raccord.
		const bool bCoarsen = K > 1 && K < Rings - 1 && Prev < MaxLevel && Radial > 2.0 * Tangential;
		Out.RingLevel.Add(bCoarsen ? Prev + 1 : Prev);
	}

	// Teinte et canaux lointains : empruntes aux PRAIRIES de la carte, pas inventes.
	//
	// HORIZON_BLEND_001. La premiere version prenait la moyenne de toute la terre forgee.
	// Elle inclut le sol des forets (famille Litter, brune) : la moyenne tombait sur un taupe
	// qui n'existe nulle part sur la carte, et l'anneau se lisait comme un desert autour d'un
	// pays vert. L'anneau n'a pas d'arbres ; ce qu'il montre est un sol nu, donc un sol de
	// prairie : sommets dont l'herbe (1 - Rock - Litter - Worked) domine, hors des zones
	// humides. Deux palettes, la moitie sombre (prairie verte) et la moitie claire (prairie
	// seche), melangees en mosaique a grande echelle. Le materiau de sol reste celui de la
	// carte ; la roche des montagnes vient de sa lecture de pente, plus un peu de famille Rock
	// sur la chaine.
	FHorizonPaletteEntry Lush, Dry;
	Out.PaletteDonors = BuildHorizonPalette(Src, Lush, Dry);
	Out.LushColor = Lush.Color;
	Out.DryColor = Dry.Color;

	// Profil de bord lisse le long du pourtour, par fenetre circulaire qui s'elargit avec
	// la distance. Sans lui, chaque anneau recopie le bord forge colonne par colonne --
	// ses pentes TANGENTIELLES (une berge de rivière qui sort de la carte y descend a 50
	// degres) comme ses taches de couleur -- et les etire en rayons jusqu'a la fin du fondu.
	//
	// L'eau du bord se lit comme la forge la rend : sommet sous SA nappe. Human_Geography_V2
	// donne a la rivière une nappe propre (son exutoire ouvert sort de la carte), plus haute
	// que la mer et sans alpha d'eau dans la couleur ; la couleur ne dit donc pas ou est l'eau.
	const bool bWaterLevels = Src.WaterVertices.Num() == Src.Vertices.Num();
	TArray<uint8> EdgeWet;
	EdgeWet.SetNumZeroed(P);
	TArray<double> EdgeZ, EdgeWetF, EdgeLevel, EdgeWetZ;
	TArray<FVector4> EdgeColor, EdgeUV;
	EdgeZ.SetNum(P);
	EdgeWetF.SetNum(P);
	EdgeLevel.SetNum(P);
	EdgeWetZ.SetNum(P);
	EdgeColor.SetNum(P);
	EdgeUV.SetNum(P);
	for (int32 I = 0; I < P; ++I)
	{
		const int32 S = Edge[I];
		const double Level = bWaterLevels ? Src.WaterVertices[S].Z : SeaZ;
		EdgeWet[I] = bWaterLevels ? (Src.Vertices[S].Z < Level ? 1 : 0) : (Src.Colors[S].A > 0.5f ? 1 : 0);
		EdgeZ[I] = Src.Vertices[S].Z;
		EdgeWetF[I] = EdgeWet[I];
		EdgeLevel[I] = EdgeWet[I] ? Level : 0.0;
		EdgeWetZ[I] = EdgeWet[I] ? Src.Vertices[S].Z : 0.0;
		const FLinearColor& C = Src.Colors[S];
		EdgeColor[I] = FVector4(C.R, C.G, C.B, 0.0);
		const FVector2D U0 = Src.UV0.IsValidIndex(S) ? Src.UV0[S] : FVector2D::ZeroVector;
		const FVector2D U1 = Src.UV1.IsValidIndex(S) ? Src.UV1[S] : FVector2D::ZeroVector;
		EdgeUV[I] = FVector4(U0.X, U0.Y, U1.X, U1.Y);
	}
	TCircularMean<double> ZMean, WetMean, LevelSum, WetZSum;
	TCircularMean<FVector4> ColorMean, UVMean;
	ZMean.Init(EdgeZ);
	WetMean.Init(EdgeWetF);
	LevelSum.Init(EdgeLevel);
	WetZSum.Init(EdgeWetZ);
	ColorMean.Init(EdgeColor);
	UVMean.Init(EdgeUV);
	Out.EdgeWater = static_cast<int32>(WetMean.Sum[P] + 0.5);

	// Nappe et lit de la rivière prolongee, par colonne : moyenne des sommets d'eau du bord
	// dans la fenetre. Le lit reste au moins 3 m (a l'echelle 5) sous la nappe, pour que
	// l'eau reste visible jusqu'a ce que le lit remonte. Sans eau dans la fenetre : la mer.
	const double MinDepth = 60.0 * Forge.SpatialScale;
	auto WaterAt = [&](int32 I, int32 R, double& OutLevel, double& OutBed)
	{
		int32 RW = R;
		const double Wet = WetMean.Total(I, RW);
		if (Wet <= 0.0)
		{
			OutLevel = SeaZ;
			OutBed = SeaZ - MinDepth;
			return;
		}
		int32 RL = R, RZ = R;
		OutLevel = LevelSum.Total(I, RL) / Wet;
		OutBed = FMath::Min(OutLevel - MinDepth, WetZSum.Total(I, RZ) / Wet);
	};

	// Position d'un sommet de bord E repousse a la distance Dist du bord.
	//
	// Loin : homothetie de centre la carte -- les anneaux restent des carres emboites et les
	// coins s'ouvrent en diagonale. Pres du bord : decalage PERPENDICULAIRE au cote. Une
	// homothetie seule y part en biais (jusqu'a 45 degres pres des coins), et un triangle
	// dont un cote est un segment de bord en pente puis un cote de biais est plus raide que
	// le segment lui-meme : une berge de 50 degres y montait a 58. Les deux placements
	// coincident exactement aux coins et gardent Y (ou X) commun le long d'un cote : leur
	// melange ne peut pas replier l'anneau.
	auto Place = [&](const FVector& E, double Dist) -> FVector2D
	{
		const double NX = E.X <= MinX ? -1.0 : (E.X >= MaxX ? 1.0 : 0.0);
		const double NY = E.Y <= MinY ? -1.0 : (E.Y >= MaxY ? 1.0 : 0.0);
		const FVector2D Normal(E.X + NX * Dist, E.Y + NY * Dist);
		const FVector2D Scaled(CX + (E.X - CX) * (SX + Dist) / SX, CY + (E.Y - CY) * (SY + Dist) / SY);
		return FMath::Lerp(Normal, Scaled, HorizonSmoothStep(0.0, PerpendicularTiles * TileStep, Dist));
	};

	AnastasisTerrainSurface::FGeometry& G = Out.Geometry;
	int32 N = 0;
	for (int32 K = 0; K < Rings; ++K)
	{
		Out.RingStart.Add(N);
		N += P >> Out.RingLevel[K];
	}
	G.Vertices.SetNumUninitialized(N);
	G.Colors.SetNumUninitialized(N);
	G.UV0.SetNumUninitialized(N);
	G.UV1.SetNumUninitialized(N);
	G.WaterVertices.SetNumUninitialized(N);
	TArray<int32> VertexColumn;
	VertexColumn.SetNumUninitialized(N);
	double MinZ = TNumericLimits<double>::Max();
	double MaxZ = TNumericLimits<double>::Lowest();

	for (int32 K = 0; K < Rings; ++K)
	{
		const double Dist = D[K];
		const int32 Stride = 1 << Out.RingLevel[K];
		const int32 Columns = P / Stride;
		const double Blend = HorizonSmoothStep(0.0, BlendTiles * TileStep, Dist);
		const double Range = HorizonSmoothStep(RangeStartTiles, RangeFullTiles, Dist / TileStep);
		// Fenetre proportionnelle a la distance au bord, pas au rang de l'anneau : une
		// colonne de plus tous les SmoothFineStepsPerColumn pas fins. Pres du bord c'est
		// une extrusion pure (meme pente que la derniere rangee forgee), puis le profil
		// s'adoucit sans jamais changer plus vite qu'il ne s'eloigne. Jamais plus etroite
		// que le palier : un anneau decime doit moyenner les colonnes qu'il saute.
		const int32 R = FMath::Max(Stride / 2, FMath::Min(MaxSmoothColumns,
			static_cast<int32>(Dist / (SmoothFineStepsPerColumn * FineStep))));
		for (int32 C = 0; C < Columns; ++C)
		{
			const int32 I = C * Stride;
			const int32 S = Edge[I];
			const FVector& E = Src.Vertices[S];
			const int32 V = Out.RingStart[K] + C;
			VertexColumn[V] = I;
			if (K == 0)
			{
				// Copie exacte : c'est le seul moyen de garantir une couture nulle.
				G.Vertices[V] = E;
				G.Colors[V] = Src.Colors[S];
				G.UV0[V] = Src.UV0.IsValidIndex(S) ? Src.UV0[S] : FVector2D::ZeroVector;
				G.UV1[V] = Src.UV1.IsValidIndex(S) ? Src.UV1[S] : FVector2D::ZeroVector;
				G.WaterVertices[V] = bWaterLevels ? Src.WaterVertices[S] : FVector(E.X, E.Y, SeaZ);
			}
			else
			{
				const FVector2D At = Place(E, Dist);
				// La jupe reprend l'altitude lointaine de son voisin interieur, pas du bruit
				// echantillonne 160 km plus loin : elle doit rester au niveau de la crete.
				const bool bSkirt = K == Rings - 1;
				const double NDist = bSkirt ? D[K - 1] : Dist;
				const FVector2D NoiseAt = bSkirt ? Place(E, NDist) : At;
				const double Far = SeaZ + AltStep * FarAltitude(NoiseAt.X / TileStep - 0.5, NoiseAt.Y / TileStep - 0.5, NDist / TileStep, Seed);
				const double Base = ZMean.Mean(I, R);
				const double River = WetMean.Mean(I, R)
					* (1.0 - HorizonSmoothStep(RiverStartTiles, RiverEndTiles, NDist / TileStep));
				double Level = SeaZ, Bed = SeaZ;
				WaterAt(I, R, Level, Bed);
				const double Target = FMath::Lerp(Far, Bed, River);
				G.Vertices[V] = FVector(At.X, At.Y, FMath::Lerp(Base, Target, Blend));
				G.WaterVertices[V] = FVector(At.X, At.Y, Level);

				const FVector4 EC = ColorMean.Mean(I, R);
				const FVector4 EU = UVMean.Mean(I, R);
				// Mosaique de prairie : ~900 m de longueur d'onde (45 tuiles), bornes douces
				// pour que les plages se fondent au lieu de dessiner des taches nettes.
				const double Mosaic = HorizonSmoothStep(0.38, 0.62, AnastasisWorldNoise::Fbm(
					NoiseAt.X / TileStep / 45.0, NoiseAt.Y / TileStep / 45.0, static_cast<double>(Seed) + 2203.0));
				const FHorizonPaletteEntry Ground = FHorizonPaletteEntry::Lerp(Lush, Dry, Mosaic);
				const FVector2D FarUV0(FMath::Clamp(Ground.UV0.X + 0.45 * Range, 0.0, 1.0), Ground.UV0.Y * (1.0 - Range));
				G.Colors[V] = FMath::Lerp(FLinearColor(EC.X, EC.Y, EC.Z, 0.f), Ground.Color, static_cast<float>(Blend));
				G.Colors[V].A = 0.f;
				G.UV0[V] = FMath::Lerp(FVector2D(EU.X, EU.Y), FarUV0, Blend);
				// Worked (champs) n'a pas de sens hors de la carte : il tombe a zero ; l'humidite
				// suit la palette.
				G.UV1[V] = FMath::Lerp(FVector2D(EU.Z, EU.W), FVector2D(0.0, Ground.UV1.Y), Blend);
			}
			MinZ = FMath::Min(MinZ, G.Vertices[V].Z);
			MaxZ = FMath::Max(MaxZ, G.Vertices[V].Z);
		}
	}

	// Meme sens de face que la forge (horaire vu de dessus, dans le plan XY) : le pourtour
	// tourne dans le sens direct et l'exterieur est a droite de la marche. Premier sommet
	// de chaque triangle toujours sur l'anneau interieur : c'est ce qui date un triangle.
	for (int32 K = 0; K < Rings - 1; ++K)
	{
		const int32 In = Out.RingStart[K], Ou = Out.RingStart[K + 1];
		const int32 CIn = P >> Out.RingLevel[K], COu = P >> Out.RingLevel[K + 1];
		if (COu == CIn)
		{
			for (int32 C = 0; C < CIn; ++C)
			{
				const int32 C1 = (C + 1) % CIn;
				G.Triangles.Append({In + C, In + C1, Ou + C, In + C1, Ou + C1, Ou + C});
			}
		}
		else
		{
			// Palier : deux colonnes interieures pour une exterieure, trois triangles.
			for (int32 C = 0; C < COu; ++C)
			{
				const int32 I0 = In + 2 * C, I1 = In + 2 * C + 1, I2 = In + (2 * C + 2) % CIn;
				const int32 O0 = Ou + C, O1 = Ou + (C + 1) % COu;
				G.Triangles.Append({I0, I1, O0, I1, O1, O0, I1, I2, O1});
			}
		}
	}

	// Normales : meme formule que la forge. L'anneau 0 reprend ensuite celles du bord forge,
	// sinon l'eclairage sauterait a la couture meme avec des positions identiques.
	G.Normals.Init(FVector::ZeroVector, N);
	for (int32 T = 0; T < G.Triangles.Num(); T += 3)
	{
		const int32 A = G.Triangles[T], B = G.Triangles[T + 1], C = G.Triangles[T + 2];
		const FVector Nrm = FVector::CrossProduct(G.Vertices[C] - G.Vertices[A], G.Vertices[B] - G.Vertices[A]);
		G.Normals[A] += Nrm;
		G.Normals[B] += Nrm;
		G.Normals[C] += Nrm;
	}
	for (FVector& Nrm : G.Normals)
	{
		Nrm = Nrm.GetSafeNormal();
	}
	for (int32 I = 0; I < P; ++I)
	{
		G.Normals[I] = Src.Normals[Edge[I]];
	}

	// Nappe d'eau : meme regle que Human_Geography_V2 (d'eau des qu'un sommet passe sous sa
	// nappe), par triangle. Anneau 0 : le drapeau et la nappe du bord forge, pour que les
	// deux nappes se touchent exactement le long du bord.
	TArray<uint8> Wet;
	Wet.SetNumZeroed(N);
	for (int32 V = 0; V < N; ++V)
	{
		Wet[V] = V < P ? EdgeWet[V] : (G.Vertices[V].Z < G.WaterVertices[V].Z ? 1 : 0);
	}
	G.WaterUV0.SetNumUninitialized(N);
	G.WaterUV1.SetNumUninitialized(N);
	const bool bSeamChannels = Rendered && Rendered->WaterUV0.Num() == Src.Vertices.Num() && Rendered->WaterUV1.Num() == Src.Vertices.Num();
	for (int32 V = 0; V < N; ++V)
	{
		const FVector& X = G.Vertices[V];
		if (V < P && bSeamChannels)
		{
			G.WaterUV0[V] = Rendered->WaterUV0[Edge[V]];
			G.WaterUV1[V] = Rendered->WaterUV1[Edge[V]];
		}
		else
		{
			// (Depth, Flatness), (Flow, 0) -- meme lecture que FillShorelineChannels ; le
			// courant de la rivière sortante s'eteint avant que son lit ne remonte.
			const double Depth = FMath::Clamp((G.WaterVertices[V].Z - X.Z) / AnastasisTerrainSurface::ShoreDepthSpan, 0.0, 1.0);
			const double Flat = FMath::Clamp(static_cast<double>(G.Normals[V].Z), 0.0, 1.0);
			const double Flow = bSeamChannels
				? Rendered->WaterUV1[Edge[VertexColumn[V]]].X * (1.0 - HorizonSmoothStep(0.0, RiverStartTiles * TileStep, D[Out.RingOf(V)]))
				: 0.0;
			G.WaterUV0[V] = FVector2D(Depth, Flat);
			G.WaterUV1[V] = FVector2D(Flow, 0.0);
		}
	}
	for (int32 T = 0; T < G.Triangles.Num(); T += 3)
	{
		const int32 A = G.Triangles[T], B = G.Triangles[T + 1], C = G.Triangles[T + 2];
		if (Wet[A] || Wet[B] || Wet[C])
		{
			G.WaterTriangles.Append({A, B, C});
		}
	}
	G.WaterNormals.Init(FVector::UpVector, G.WaterVertices.Num());

	Out.VertexColumn = MoveTemp(VertexColumn);
	Out.Perimeter = P;
	Out.Rings = Rings;
	Out.MinZ = MinZ;
	Out.MaxZ = MaxZ;
	return true;
}
