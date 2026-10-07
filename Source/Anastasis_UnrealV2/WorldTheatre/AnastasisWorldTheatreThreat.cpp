#include "WorldTheatre/AnastasisWorldTheatreThreat.h"

#include "Geo/AnastasisGeo.h"

namespace AnastasisWorldTheatreThreat
{
using AnastasisGeo::EPressure;
using AnastasisWorldTheatre::EThreatSign;
using AnastasisWorldTheatre::FMeshData;
using AnastasisWorldTheatre::FThreatSite;

namespace
{
double Excess(const AnastasisGeo::FGeoWorld& Geo, const AnastasisGeo::FNode& Node, EPressure P)
{
	const double Base = Node.Baseline[static_cast<int32>(P)];
	return FMath::Clamp(Geo.GetTruePressure(Node.Id, P) - Base, 0.0, 1.0);
}

double Progress(double DayFloat, int32 Departure, int32 Arrival)
{
	const double Span = FMath::Max(1, Arrival - Departure);
	return FMath::Clamp((DayFloat - Departure) / Span, 0.0, 1.0);
}

bool IsThreatPressure(EPressure P) { return P == EPressure::Insecurity || P == EPressure::Military; }
}

bool ReadThreatMap(const AnastasisGeo::FGeoWorld& Geo, double DayFloat, FThreatMap& Out, const FRules& Rules)
{
	Out = FThreatMap();
	Out.DayFloat = DayFloat;
	if (!Geo.IsLoaded()) return false;
	const AnastasisGeo::FScenario& S = Geo.GetScenario();
	Out.bLoaded = true;
	Out.VillageNode = S.VillageNodeId;
	for (const AnastasisGeo::FNode& Node : S.Nodes)
	{
		const double E = NoisyOr(Excess(Geo, Node, EPressure::Insecurity), Excess(Geo, Node, EPressure::Military));
		if (Node.Id == S.VillageNodeId) Out.VillageExcess = E;
		else Out.NodeExcess.Add(Node.Id, E);
	}
	// L'exposition est ce que le village a reellement recu ; elle prime sur la verite du noeud si plus forte.
	const AnastasisGeo::FVillageExposure& X = Geo.GetVillageExposure();
	// FScenario::FindNode n'est pas exporte par AnastasisSim : on cherche le noeud soi-meme.
	const AnastasisGeo::FNode* V = S.Nodes.FindByPredicate([&S](const AnastasisGeo::FNode& N) { return N.Id == S.VillageNodeId; });
	if (V)
	{
		const double Ins = FMath::Clamp(X.Pressure[static_cast<int32>(EPressure::Insecurity)] - V->Baseline[static_cast<int32>(EPressure::Insecurity)], 0.0, 1.0);
		const double Mil = FMath::Clamp(X.Pressure[static_cast<int32>(EPressure::Military)] - V->Baseline[static_cast<int32>(EPressure::Military)], 0.0, 1.0);
		Out.VillageExcess = FMath::Max(Out.VillageExcess, NoisyOr(Ins, Mil));
	}
	for (const AnastasisGeo::FPressurePacket& P : Geo.GetPressureInTransit())
	{
		if (P.ToNode != S.VillageNodeId || !IsThreatPressure(P.Pressure)) continue;
		Out.PressureToVillage.Add({ P.FromNode, FMath::Clamp(P.Magnitude, 0.0, 1.0), Progress(DayFloat, P.DepartureDay, P.ArrivalDay) });
	}
	// Une nouvelle compte comme menace si le choc qui l'a fait naitre porte de l'insecurite ou des troupes.
	TSet<FString> ThreatCauses;
	for (const AnastasisGeo::FShockDef& Shock : Geo.GetShocks())
	{
		for (const AnastasisGeo::FEmission& E : Shock.Emissions)
		{
			if (IsThreatPressure(E.Pressure) && E.Magnitude >= Rules.ThreatShockMin) { ThreatCauses.Add(Shock.Id); break; }
		}
	}
	for (const AnastasisGeo::FInformationPacket& I : Geo.GetInformationInTransit())
	{
		if (I.ToNode != S.VillageNodeId || !ThreatCauses.Contains(I.RootCauseId)) continue;
		const double M = FMath::Clamp(I.ReportedMagnitude * I.Reliability, 0.0, 1.0);
		if (M < Rules.NewsMin) continue;
		Out.NewsToVillage.Add({ I.FromNode, M, Progress(DayFloat, I.DepartureDay, I.ArrivalDay) });
	}
	return true;
}

FSignals Evaluate(const FThreatMap& Map, const TArray<FThreatSite>& Sites, const FRules& Rules)
{
	FSignals Out;
	Out.Site.Init(0.0f, Sites.Num());
	if (!Map.bLoaded) return Out;
	Out.Dread = static_cast<float>(FMath::Clamp(Map.VillageExcess / Rules.DreadFull, 0.0, 1.0));

	// Longueur de chaque chaine de feux, par noeud.
	TMap<FString, int32> ChainLength;
	for (const FThreatSite& S : Sites)
	{
		if (S.Sign == EThreatSign::Beacon) ChainLength.FindOrAdd(S.NodeId) = FMath::Max(ChainLength.FindRef(S.NodeId), S.Stage + 1);
	}
	// Le voisin d'ou vient le danger arrive : celui qui porte le plus d'exces ou de pression en route, parmi les noeuds
	// QUI ONT DES SITES (les voisins visibles). Preuve PIE du 2026-10-07 : Paipert (exces 0,75, sans site, a trois jours)
	// l'emportait, et a l'arrivee les feux s'eteignaient et la fumee proche ne prenait pas.
	TSet<FString> Visible;
	for (const FThreatSite& S : Sites) Visible.Add(S.NodeId);
	FString Source;
	double SourceWeight = 0.0;
	for (const TPair<FString, double>& N : Map.NodeExcess)
	{
		if (!Visible.Contains(N.Key)) continue;
		double W = N.Value;
		for (const FTransit& T : Map.PressureToVillage) if (T.FromNode == N.Key) W = NoisyOr(W, T.Magnitude);
		if (W > SourceWeight) { SourceWeight = W; Source = N.Key; }
	}

	for (int32 K = 0; K < Sites.Num(); ++K)
	{
		const FThreatSite& S = Sites[K];
		const FString Node(S.NodeId);
		double I = 0.0;
		if (S.Sign == EThreatSign::Smoke)
		{
			if (S.Stage == 0) I = Map.NodeExcess.FindRef(Node) / Rules.SmokeFull;
			for (const FTransit& T : Map.PressureToVillage)
			{
				if (T.FromNode != Node) continue;
				// La pression en route se montre a l'etape qu'elle traverse : mi-chemin, puis pres du village.
				const int32 Stage = T.Progress < 0.5 ? 1 : 2;
				if (S.Stage == Stage) I = FMath::Max(I, T.Magnitude / Rules.TransitFull);
			}
			// Arrivee : l'exposition du village brule pres de lui, du cote d'ou le danger est venu.
			if (S.Stage == 2 && Node == Source && Map.VillageExcess > 0.0) I = FMath::Max(I, Map.VillageExcess / Rules.SmokeFull);
		}
		else
		{
			const int32 Len = FMath::Max(1, ChainLength.FindRef(Node));
			for (const FTransit& N : Map.NewsToVillage)
			{
				if (N.FromNode != Node) continue;
				// Le premier feu s'allume au depart de la nouvelle ; les suivants a mesure qu'elle avance.
				const int32 Lit = FMath::Clamp(1 + FMath::FloorToInt32(N.Progress * Len), 1, Len);
				if (S.Stage < Lit) I = FMath::Max(I, N.Magnitude / Rules.NewsFull);
			}
			// La nouvelle arrivee, les feux restent tenus tant que le danger pese sur ce cote.
			if (Node == Source && SourceWeight > 0.0 && Map.VillageExcess > 0.0) I = FMath::Max(I, Map.VillageExcess / Rules.NewsFull);
		}
		Out.Site[K] = static_cast<float>(FMath::Clamp(I, 0.0, 1.0));
	}
	return Out;
}

FMeshData BuildSmokeColumn(double Height, const FVector2D& LeanPerUnitHeight, double BaseRadius, double TopRadius)
{
	FMeshData M;
	constexpr int32 Around = 10;
	constexpr int32 Rings = 14;
	for (int32 R = 0; R <= Rings; ++R)
	{
		const double V = double(R) / Rings;
		// La fumee monte droit puis se couche : derive en V^1,6 (la colonne chaude, puis le vent).
		const double Z = V * Height;
		const FVector2D Drift = LeanPerUnitHeight * Height * FMath::Pow(V, 1.6);
		const double Radius = FMath::Lerp(BaseRadius, TopRadius, FMath::Pow(V, 0.8));
		// Densite : monte vite au pied, plein vers un tiers, s'efface vers le sommet.
		const float Alpha = static_cast<float>(FMath::SmoothStep(0.0, 0.08, V) * (1.0 - FMath::SmoothStep(0.55, 1.0, V)));
		for (int32 A = 0; A <= Around; ++A)
		{
			const double U = double(A) / Around;
			const double Ang = U * 2.0 * PI;
			const FVector P(Drift.X + Radius * FMath::Cos(Ang), Drift.Y + Radius * FMath::Sin(Ang), Z);
			M.Vertices.Add(P);
			M.Normals.Add(FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.0));
			M.Colours.Add(FLinearColor(1.0f, 1.0f, 1.0f, Alpha));
			M.UVs.Add(FVector2D(U, V));
		}
	}
	constexpr int32 Row = Around + 1;
	for (int32 R = 0; R < Rings; ++R)
	{
		for (int32 A = 0; A < Around; ++A)
		{
			const int32 I0 = R * Row + A, I1 = I0 + 1, I2 = I0 + Row, I3 = I2 + 1;
			// Materiau double face : l'ordre n'importe que pour la coherence.
			M.Triangles.Append({ I0, I2, I1, I1, I2, I3 });
		}
	}
	return M;
}

FMeshData BuildFire(double Size)
{
	FMeshData M;
	const double H = Size * 0.5;
	for (int32 K = 0; K < 3; ++K)
	{
		const double Ang = K * PI / 3.0;
		const FVector D(FMath::Cos(Ang) * H, FMath::Sin(Ang) * H, 0.0);
		const FVector N(-FMath::Sin(Ang), FMath::Cos(Ang), 0.0);
		const int32 Base = M.Vertices.Num();
		const FVector Corners[4] = { -D, D, D + FVector(0, 0, Size), -D + FVector(0, 0, Size) };
		const FVector2D UV[4] = { {0, 1}, {1, 1}, {1, 0}, {0, 0} };
		for (int32 C = 0; C < 4; ++C)
		{
			M.Vertices.Add(Corners[C]);
			M.Normals.Add(N);
			M.Colours.Add(FLinearColor::White);
			M.UVs.Add(UV[C]);
		}
		M.Triangles.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
	}
	return M;
}
}
