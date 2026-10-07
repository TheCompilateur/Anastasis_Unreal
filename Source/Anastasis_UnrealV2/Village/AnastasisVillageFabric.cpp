#include "Village/AnastasisVillageFabric.h"

namespace AnastasisVillageFabric
{
	namespace
	{
		/** Un batiment pose : centre monde, axe d'acces (+Y local), corps ou disque du puits. */
		struct FBody
		{
			const FPlot* Plot = nullptr;
			FVector2D Centre = FVector2D::ZeroVector;
			/** Unitaire, du centre vers l'acces. */
			FVector2D Front = FVector2D(0.0, 1.0);
			bool bWell = false;

			/** Repere local : +Y = Front, +X = Front tourne de -90 deg. */
			FVector2D ToLocal(const FVector2D& P) const
			{
				const FVector2D D = P - Centre;
				const FVector2D Side(Front.Y, -Front.X);
				return FVector2D(FVector2D::DotProduct(D, Side), FVector2D::DotProduct(D, Front));
			}
			FVector2D ToWorld(const FVector2D& L) const
			{
				const FVector2D Side(Front.Y, -Front.X);
				return Centre + Side * L.X + Front * L.Y;
			}
			bool InSquare(const FVector2D& P, double Half) const
			{
				const FVector2D L = ToLocal(P);
				return FMath::Abs(L.X) <= Half && FMath::Abs(L.Y) <= Half;
			}
		};

		struct FGrid
		{
			FVector2D Origin = FVector2D::ZeroVector;
			double Step = 100.0;
			int32 W = 0;
			int32 H = 0;
			TArray<double> Z;
			TBitArray<> HasZ;
			TBitArray<> Free;

			/** Pente propre du terrain au noeud (differences centrees, sur les voisins qui ont un sol). */
			double Fall(int32 Idx) const
			{
				const int32 I = Idx % W;
				const int32 J = Idx / W;
				const auto Slope = [&](int32 DI, int32 DJ)
				{
					const int32 A = Inside(I - DI, J - DJ) && HasZ[Index(I - DI, J - DJ)] ? Index(I - DI, J - DJ) : Idx;
					const int32 B = Inside(I + DI, J + DJ) && HasZ[Index(I + DI, J + DJ)] ? Index(I + DI, J + DJ) : Idx;
					const int32 Span = (A != Idx) + (B != Idx);
					return Span > 0 ? (Z[B] - Z[A]) / (Span * Step) : 0.0;
				};
				return FVector2D(Slope(1, 0), Slope(0, 1)).Size();
			}

			int32 Index(int32 I, int32 J) const { return J * W + I; }
			bool Inside(int32 I, int32 J) const { return I >= 0 && J >= 0 && I < W && J < H; }
			FVector2D At(int32 Idx) const
			{
				return Origin + FVector2D((Idx % W + 0.5) * Step, (Idx / W + 0.5) * Step);
			}
			int32 Nearest(const FVector2D& P) const
			{
				const int32 I = FMath::Clamp(FMath::FloorToInt32((P.X - Origin.X) / Step), 0, W - 1);
				const int32 J = FMath::Clamp(FMath::FloorToInt32((P.Y - Origin.Y) / Step), 0, H - 1);
				return Index(I, J);
			}
		};

		/** FNV-1a sur des entiers : le hachage du tissu ne depend que des valeurs arrondies au cm. */
		void Mix(uint32& Hash, int64 Value)
		{
			for (int32 Byte = 0; Byte < 8; ++Byte)
			{
				Hash ^= static_cast<uint32>((Value >> (Byte * 8)) & 0xFF);
				Hash *= 16777619u;
			}
		}

		double Median(TArray<double> Values)
		{
			if (Values.Num() == 0)
			{
				return 0.0;
			}
			Values.Sort();
			const int32 Mid = Values.Num() / 2;
			return (Values.Num() % 2) ? Values[Mid] : 0.5 * (Values[Mid - 1] + Values[Mid]);
		}

		/** Deux passes de Chaikin : l'escalier de la grille devient une courbe, extremites gardees. */
		TArray<FVector2D> Smooth(const TArray<FVector2D>& In)
		{
			TArray<FVector2D> Line = In;
			for (int32 Pass = 0; Pass < 2 && Line.Num() > 2; ++Pass)
			{
				TArray<FVector2D> Next;
				Next.Add(Line[0]);
				for (int32 I = 0; I + 1 < Line.Num(); ++I)
				{
					Next.Add(FMath::Lerp(Line[I], Line[I + 1], 0.25));
					Next.Add(FMath::Lerp(Line[I], Line[I + 1], 0.75));
				}
				Next.Add(Line.Last());
				Line = MoveTemp(Next);
			}
			return Line;
		}

		/** Reechantillonne une polyligne tous les `Step` cm (extremites comprises). */
		TArray<FVector2D> Resample(const TArray<FVector2D>& Line, double Step)
		{
			TArray<FVector2D> Out;
			if (Line.Num() == 0)
			{
				return Out;
			}
			Out.Add(Line[0]);
			double Carry = 0.0;
			for (int32 I = 0; I + 1 < Line.Num(); ++I)
			{
				const FVector2D A = Line[I];
				const FVector2D B = Line[I + 1];
				const double Len = FVector2D::Distance(A, B);
				double T = Step - Carry;
				while (T < Len)
				{
					Out.Add(FMath::Lerp(A, B, T / Len));
					T += Step;
				}
				Carry = Len - (T - Step);
			}
			if (FVector2D::Distance(Out.Last(), Line.Last()) > 1.0)
			{
				Out.Add(Line.Last());
			}
			return Out;
		}
	}

	uint32 StableHash(const FString& Text)
	{
		uint32 Hash = 2166136261u;
		for (const TCHAR C : Text)
		{
			Hash ^= static_cast<uint32>(C);
			Hash *= 16777619u;
		}
		return Hash;
	}

	bool FFabric::IsPavedAt(double X, double Y) const
	{
		if (GridW <= 0 || GridH <= 0)
		{
			return false;
		}
		const int32 I = FMath::FloorToInt32((X - GridOrigin.X) / GridCm);
		const int32 J = FMath::FloorToInt32((Y - GridOrigin.Y) / GridCm);
		return I >= 0 && J >= 0 && I < GridW && J < GridH && Paved[J * GridW + I];
	}

	FFabric Build(TArray<FPlot> Plots, const double CellCm, FHeightFn Height, const FParams& P)
	{
		FFabric Out;
		Out.GridCm = P.GridCm;
		// L'ordre de la simulation ne doit rien decider : tri par identifiant.
		Plots.Sort([](const FPlot& A, const FPlot& B) { return A.Id < B.Id; });
		Out.Report.Plots = Plots.Num();
		if (Plots.Num() == 0 || CellCm <= 0.0 || P.GridCm <= 0.0)
		{
			return Out;
		}

		// --- Les corps --------------------------------------------------------------------------
		TArray<FBody> Bodies;
		for (const FPlot& Plot : Plots)
		{
			FBody Body;
			Body.Plot = &Plot;
			Body.Centre = FVector2D((Plot.Cell.X + 0.5) * CellCm, (Plot.Cell.Y + 0.5) * CellCm);
			Body.bWell = Plot.Type == TEXT("well");
			const FVector2D Dir(Plot.Access.X - (Plot.Cell.X + 0.5), Plot.Access.Y - (Plot.Cell.Y + 0.5));
			if (Plot.bHasAccess && Dir.SizeSquared() > 1.0e-8)
			{
				Body.Front = Dir.GetSafeNormal();
			}
			Bodies.Add(Body);
		}

		// La placette : le premier puits. Sans puits, la maisonnee la plus centrale sert de racine.
		int32 Hub = Bodies.IndexOfByPredicate([](const FBody& B) { return B.bWell; });
		if (Hub == INDEX_NONE)
		{
			FVector2D Mean = FVector2D::ZeroVector;
			for (const FBody& B : Bodies)
			{
				Mean += B.Centre;
			}
			Mean /= Bodies.Num();
			double Best = TNumericLimits<double>::Max();
			for (int32 I = 0; I < Bodies.Num(); ++I)
			{
				const double D = FVector2D::DistSquared(Bodies[I].Centre, Mean);
				if (D < Best - 1.0)
				{
					Best = D;
					Hub = I;
				}
			}
		}

		// Un village ne pave pas une demi-lieue : au-dela de MaxReachCells de la racine, une maison est un
		// ecart (elle aura son sentier de terre, pas une calade), et elle ne fait pas grossir la grille.
		{
			const FIntPoint HubCell = Bodies[Hub].Plot->Cell;
			TArray<FBody> Kept;
			int32 KeptHub = INDEX_NONE;
			for (int32 I = 0; I < Bodies.Num(); ++I)
			{
				const FIntPoint D = Bodies[I].Plot->Cell - HubCell;
				if (FMath::Max(FMath::Abs(D.X), FMath::Abs(D.Y)) > P.MaxReachCells)
				{
					++Out.Report.Remote;
					continue;
				}
				if (I == Hub)
				{
					KeptHub = Kept.Num();
				}
				Kept.Add(Bodies[I]);
			}
			Bodies = MoveTemp(Kept);
			Hub = KeptHub;
		}
		FIntPoint MinCell(MAX_int32, MAX_int32);
		FIntPoint MaxCell(MIN_int32, MIN_int32);
		for (const FBody& B : Bodies)
		{
			MinCell = FIntPoint(FMath::Min(MinCell.X, B.Plot->Cell.X), FMath::Min(MinCell.Y, B.Plot->Cell.Y));
			MaxCell = FIntPoint(FMath::Max(MaxCell.X, B.Plot->Cell.X), FMath::Max(MaxCell.Y, B.Plot->Cell.Y));
		}

		// --- La grille --------------------------------------------------------------------------
		FGrid Grid;
		Grid.Step = P.GridCm;
		Grid.Origin = FVector2D((MinCell.X - P.MarginCells) * CellCm, (MinCell.Y - P.MarginCells) * CellCm);
		Grid.W = FMath::CeilToInt32((MaxCell.X - MinCell.X + 1 + 2 * P.MarginCells) * CellCm / P.GridCm);
		Grid.H = FMath::CeilToInt32((MaxCell.Y - MinCell.Y + 1 + 2 * P.MarginCells) * CellCm / P.GridCm);
		const int32 N = Grid.W * Grid.H;
		Grid.Z.SetNumZeroed(N);
		Grid.Free.Init(true, N);
		Grid.HasZ.Init(false, N);
		for (int32 Idx = 0; Idx < N; ++Idx)
		{
			const FVector2D C = Grid.At(Idx);
			double Z = 0.0;
			if (!Height(C.X, C.Y, Z))
			{
				Grid.Free[Idx] = false;
				continue;
			}
			Grid.Z[Idx] = Z;
			Grid.HasZ[Idx] = true;
			for (const FBody& B : Bodies)
			{
				const bool bBlocked = B.bWell
					? FVector2D::Distance(C, B.Centre) <= P.WellHalfCm
					: B.InSquare(C, P.BodyHalfCm + P.KeepOutCm);
				if (bBlocked)
				{
					Grid.Free[Idx] = false;
					break;
				}
			}
		}
		Out.GridOrigin = Grid.Origin;
		Out.GridW = Grid.W;
		Out.GridH = Grid.H;
		Out.Paved.Init(false, N);

		// --- La racine : placette du puits, ou seuil de la maisonnee centrale ---------------------
		TArray<int32> TreeParent;
		TreeParent.Init(INDEX_NONE - 1, N); // INDEX_NONE - 1 : hors de l'arbre ; INDEX_NONE : racine.
		TArray<int32> Roots;
		const FBody& HubBody = Bodies[Hub];
		if (HubBody.bWell)
		{
			TArray<double> Levels;
			for (int32 Idx = 0; Idx < N; ++Idx)
			{
				if (Grid.Free[Idx] && FVector2D::Distance(Grid.At(Idx), HubBody.Centre) <= P.PlazaRadiusCm)
				{
					Roots.Add(Idx);
					Levels.Add(Grid.Z[Idx]);
					Out.Paved[Idx] = true;
				}
			}
			if (Roots.Num() > 0)
			{
				FPlaza& Plaza = Out.Plaza;
				Plaza.bValid = true;
				Plaza.WellId = HubBody.Plot->Id;
				Plaza.RadiusCm = P.PlazaRadiusCm;
				Plaza.LevelCm = Median(Levels);
				Plaza.Centre = FVector(HubBody.Centre, Plaza.LevelCm);
			}
		}

		// Les seuils : juste hors de la reserve du corps, dans l'axe de l'acces.
		struct FDoor
		{
			int32 Body = INDEX_NONE;
			int32 Node = INDEX_NONE;
		};
		TArray<FDoor> Doors;
		for (int32 I = 0; I < Bodies.Num(); ++I)
		{
			const FBody& B = Bodies[I];
			if (B.bWell)
			{
				continue;
			}
			const FVector2D Exit = B.ToWorld(FVector2D(0.0, P.BodyHalfCm + P.KeepOutCm + P.GridCm));
			int32 Node = INDEX_NONE;
			double Best = TNumericLimits<double>::Max();
			// Le noeud libre le plus proche de la sortie, dans 4 m.
			const int32 Reach = FMath::CeilToInt32(400.0 / P.GridCm);
			const int32 Seed = Grid.Nearest(Exit);
			for (int32 DJ = -Reach; DJ <= Reach; ++DJ)
			{
				for (int32 DI = -Reach; DI <= Reach; ++DI)
				{
					const int32 GI = Seed % Grid.W + DI;
					const int32 GJ = Seed / Grid.W + DJ;
					if (!Grid.Inside(GI, GJ) || !Grid.Free[Grid.Index(GI, GJ)])
					{
						continue;
					}
					const double D = FVector2D::DistSquared(Grid.At(Grid.Index(GI, GJ)), Exit);
					if (D < Best)
					{
						Best = D;
						Node = Grid.Index(GI, GJ);
					}
				}
			}
			++Out.Report.Doors;
			if (Node == INDEX_NONE)
			{
				continue;
			}
			if (I == Hub)
			{
				Roots.Add(Node);
				Out.Paved[Node] = true;
				++Out.Report.ConnectedDoors;
				continue;
			}
			Doors.Add({ I, Node });
		}
		for (const int32 R : Roots)
		{
			TreeParent[R] = INDEX_NONE;
		}

		// --- L'arbre des ruelles : chaque fois, le seuil le moins couteux rejoint le reseau --------
		// Quatre cardinales, quatre diagonales, huit sauts de cavalier : sur une pente, le cavalier donne une
		// rampe a pente / sqrt(5), la ou la diagonale seule forcerait l'escalier (pente / sqrt(2)).
		const int32 DIR = 16;
		const int32 DI8[DIR] = { 1, -1, 0, 0, 1, 1, -1, -1, 2, 2, -2, -2, 1, 1, -1, -1 };
		const int32 DJ8[DIR] = { 0, 0, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 2, -2, 2, -2 };
		TArray<double> Dist;
		TArray<int32> Came;
		TArray<TArray<int32>> LanePaths;
		TArray<int32> LaneBodies;
		TBitArray<> Joined(false, Doors.Num());
		while (Roots.Num() > 0)
		{
			Dist.Init(TNumericLimits<double>::Max(), N);
			Came.Init(INDEX_NONE, N);
			TArray<TPair<double, int32>> Heap;
			const auto Less = [](const TPair<double, int32>& A, const TPair<double, int32>& B)
			{
				return A.Key < B.Key || (A.Key == B.Key && A.Value < B.Value);
			};
			for (int32 Idx = 0; Idx < N; ++Idx)
			{
				if (TreeParent[Idx] != INDEX_NONE - 1)
				{
					Dist[Idx] = 0.0;
					Heap.HeapPush(TPair<double, int32>(0.0, Idx), Less);
				}
			}
			while (Heap.Num() > 0)
			{
				TPair<double, int32> Top;
				Heap.HeapPop(Top, Less);
				if (Top.Key > Dist[Top.Value])
				{
					continue;
				}
				const int32 CI = Top.Value % Grid.W;
				const int32 CJ = Top.Value / Grid.W;
				for (int32 K = 0; K < DIR; ++K)
				{
					const int32 NI = CI + DI8[K];
					const int32 NJ = CJ + DJ8[K];
					if (!Grid.Inside(NI, NJ))
					{
						continue;
					}
					const int32 Next = Grid.Index(NI, NJ);
					if (!Grid.Free[Next])
					{
						continue;
					}
					// Pas d'angle rogne sur un mur : toutes les cases que le pas effleure doivent etre libres.
					const int32 SI = FMath::Sign(DI8[K]);
					const int32 SJ = FMath::Sign(DJ8[K]);
					if (K >= 4 && (!Grid.Free[Grid.Index(CI + SI, CJ)] || !Grid.Free[Grid.Index(CI, CJ + SJ)]
						|| !Grid.Free[Grid.Index(CI + SI, CJ + SJ)]))
					{
						continue;
					}
					if (K >= 8 && !Grid.Free[Grid.Index(NI - SI * (FMath::Abs(DI8[K]) == 2), NJ - SJ * (FMath::Abs(DJ8[K]) == 2))])
					{
						continue;
					}
					const double Len = P.GridCm * FMath::Sqrt(static_cast<double>(DI8[K] * DI8[K] + DJ8[K] * DJ8[K]));
					const double Grade = FMath::Abs(Grid.Z[Next] - Grid.Z[Top.Value]) / Len;
					double Cost = Len * (1.0 + P.SlopeWeight * Grade * Grade);
					if (Grade > P.MaxGrade)
					{
						Cost *= P.VeryStepCost;
					}
					const double Candidate = Top.Key + Cost;
					if (Candidate < Dist[Next])
					{
						Dist[Next] = Candidate;
						Came[Next] = Top.Value;
						Heap.HeapPush(TPair<double, int32>(Candidate, Next), Less);
					}
				}
			}

			int32 Pick = INDEX_NONE;
			for (int32 D = 0; D < Doors.Num(); ++D)
			{
				if (Joined[D] || Dist[Doors[D].Node] == TNumericLimits<double>::Max())
				{
					continue;
				}
				if (Pick == INDEX_NONE || Dist[Doors[D].Node] < Dist[Doors[Pick].Node])
				{
					Pick = D; // egalite : le premier identifiant (Doors suit l'ordre trie)
				}
			}
			if (Pick == INDEX_NONE)
			{
				break;
			}
			Joined[Pick] = true;
			TArray<int32> Path;
			for (int32 Idx = Doors[Pick].Node; Idx != INDEX_NONE; Idx = Came[Idx])
			{
				// Un saut de cavalier enjambe une case : on la compte dans la ruelle (chaussee continue, arbre sans trou).
				if (Path.Num() > 0)
				{
					const int32 LastI = Path.Last() % Grid.W;
					const int32 LastJ = Path.Last() / Grid.W;
					const int32 DI = Idx % Grid.W - LastI;
					const int32 DJ = Idx / Grid.W - LastJ;
					if (FMath::Abs(DI) == 2 || FMath::Abs(DJ) == 2)
					{
						const int32 Mid = Grid.Index(LastI + (FMath::Abs(DI) == 2 ? DI / 2 : DI), LastJ + (FMath::Abs(DJ) == 2 ? DJ / 2 : DJ));
						if (TreeParent[Mid] == INDEX_NONE - 1 && Mid != Idx)
						{
							Path.Add(Mid);
						}
					}
				}
				Path.Add(Idx);
				if (TreeParent[Idx] != INDEX_NONE - 1)
				{
					break; // jonction
				}
			}
			for (int32 K = 0; K + 1 < Path.Num(); ++K)
			{
				TreeParent[Path[K]] = Path[K + 1];
			}
			LanePaths.Add(Path);
			LaneBodies.Add(Doors[Pick].Body);
			++Out.Report.ConnectedDoors;
		}

		// --- Charge : combien de seuils passent par chaque noeud --------------------------------
		TArray<int32> Usage;
		Usage.Init(0, N);
		for (const TArray<int32>& Path : LanePaths)
		{
			for (int32 Idx = Path[0]; Idx >= 0; Idx = TreeParent[Idx])
			{
				++Usage[Idx];
			}
		}

		// --- Ruelles rendues --------------------------------------------------------------------
		double GradeWeighted = 0.0;
		double FallWeighted = 0.0;
		for (int32 L = 0; L < LanePaths.Num(); ++L)
		{
			const TArray<int32>& Path = LanePaths[L];
			FLane Lane;
			Lane.FromId = Bodies[LaneBodies[L]].Plot->Id;
			for (const int32 Idx : Path)
			{
				Lane.Usage = FMath::Max(Lane.Usage, Usage[Idx]);
			}
			const double Load = FMath::Clamp((Lane.Usage - 1) / 3.0, 0.0, 1.0);
			Lane.WidthCm = FMath::Lerp(P.LaneWidthCm, P.MainLaneWidthCm, Load);

			TArray<FVector2D> Raw;
			for (const int32 Idx : Path)
			{
				Raw.Add(Grid.At(Idx));
			}
			const TArray<FVector2D> Line = Resample(Smooth(Raw), P.GridCm);
			for (int32 K = 0; K < Line.Num(); ++K)
			{
				FLanePoint Point;
				double Z = 0.0;
				if (!Height(Line[K].X, Line[K].Y, Z))
				{
					Z = Grid.Z[Grid.Nearest(Line[K])];
				}
				Point.Position = FVector(Line[K], Z);
				if (K > 0)
				{
					const FVector& Prev = Lane.Points.Last().Position;
					const double Run = FVector2D::Distance(FVector2D(Prev), Line[K]);
					Point.Grade = Run > 1.0 ? FMath::Abs(Z - Prev.Z) / Run : 0.0;
					Point.bStep = Point.Grade > P.StepGrade;
					Lane.LengthCm += Run;
					Lane.MeanGrade += Point.Grade * Run;
					if (Point.bStep)
					{
						Out.Report.StepLengthCm += Run;
					}
					Out.Report.LaneMaxGrade = FMath::Max(Out.Report.LaneMaxGrade, Point.Grade);
				}
				Lane.Points.Add(Point);
			}
			if (Lane.LengthCm > 0.0)
			{
				Lane.MeanGrade /= Lane.LengthCm;
				for (const int32 Idx : Path)
				{
					Lane.FallGrade += Grid.Fall(Idx);
				}
				Lane.FallGrade /= Path.Num();
				GradeWeighted += Lane.MeanGrade * Lane.LengthCm;
				FallWeighted += Lane.FallGrade * Lane.LengthCm;
				Out.Report.LaneLengthCm += Lane.LengthCm;
			}

			// Chaussee : tout noeud a moins d'une demi-largeur de l'axe.
			const int32 Reach = FMath::CeilToInt32(Lane.WidthCm * 0.5 / P.GridCm);
			for (const FLanePoint& Point : Lane.Points)
			{
				const int32 Seed = Grid.Nearest(FVector2D(Point.Position));
				for (int32 DJ = -Reach; DJ <= Reach; ++DJ)
				{
					for (int32 DI = -Reach; DI <= Reach; ++DI)
					{
						const int32 GI = Seed % Grid.W + DI;
						const int32 GJ = Seed / Grid.W + DJ;
						if (Grid.Inside(GI, GJ)
							&& FVector2D::Distance(Grid.At(Grid.Index(GI, GJ)), FVector2D(Point.Position)) <= Lane.WidthCm * 0.5 + P.GridCm * 0.5)
						{
							Out.Paved[Grid.Index(GI, GJ)] = true;
						}
					}
				}
			}
			for (const int32 Idx : Path)
			{
				for (const FBody& B : Bodies)
				{
					if (!B.bWell && B.InSquare(Grid.At(Idx), P.BodyHalfCm))
					{
						++Out.Report.BodyIntrusions;
					}
				}
			}
			Out.Lanes.Add(MoveTemp(Lane));
		}
		Out.Report.Lanes = Out.Lanes.Num();
		if (Out.Report.LaneLengthCm > 0.0)
		{
			Out.Report.LaneMeanGrade = GradeWeighted / Out.Report.LaneLengthCm;
			Out.Report.FallMeanGrade = FallWeighted / Out.Report.LaneLengthCm;
		}

		// L'herbe dans l'emprise d'une maison est defrichee par l'architecture : pas de chaussee a nous la.
		for (int32 Idx = 0; Idx < N; ++Idx)
		{
			if (!Out.Paved[Idx])
			{
				continue;
			}
			for (const FBody& B : Bodies)
			{
				if (!B.bWell && B.InSquare(Grid.At(Idx), P.ArchitectureClearHalfCm))
				{
					Out.Paved[Idx] = false;
					break;
				}
			}
		}

		// --- Terrasses : mur de pierre seche au bord de chaque plate-forme -----------------------
		for (const FBody& B : Bodies)
		{
			if (B.bWell)
			{
				continue;
			}
			TArray<double> Pad;
			for (int32 SJ = 0; SJ < 5; ++SJ)
			{
				for (int32 SI = 0; SI < 5; ++SI)
				{
					const FVector2D L(-P.BodyHalfCm + SI * P.BodyHalfCm * 0.5, -P.BodyHalfCm + SJ * P.BodyHalfCm * 0.5);
					const FVector2D W = B.ToWorld(L);
					double Z = 0.0;
					if (Height(W.X, W.Y, Z))
					{
						Pad.Add(Z);
					}
				}
			}
			if (Pad.Num() == 0)
			{
				continue;
			}
			const double PadZ = Median(Pad);
			const FVector2D DoorEdge = B.ToWorld(FVector2D(0.0, P.TerraceHalfCm));

			// Quatre cotes, dans le sens trigonometrique local : un mur suit le bord sans retour.
			const FVector2D Corners[4] = {
				FVector2D(-P.TerraceHalfCm, -P.TerraceHalfCm), FVector2D(P.TerraceHalfCm, -P.TerraceHalfCm),
				FVector2D(P.TerraceHalfCm, P.TerraceHalfCm), FVector2D(-P.TerraceHalfCm, P.TerraceHalfCm) };
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const FVector2D A = Corners[Side];
				const FVector2D C = Corners[(Side + 1) % 4];
				const int32 Samples = FMath::Max(1, FMath::RoundToInt32(FVector2D::Distance(A, C) / P.WallStepCm));
				FWallRun Run;
				int32 RunKind = 0;
				const auto Flush = [&]()
				{
					if (Run.Bottom.Num() >= 2)
					{
						for (int32 K = 1; K < Run.Bottom.Num(); ++K)
						{
							Run.LengthCm += FVector2D::Distance(FVector2D(Run.Bottom[K - 1]), FVector2D(Run.Bottom[K]));
						}
						Out.Walls.Add(Run);
					}
					Run = FWallRun();
					RunKind = 0;
				};
				for (int32 K = 0; K <= Samples; ++K)
				{
					const FVector2D W = B.ToWorld(FMath::Lerp(A, C, static_cast<double>(K) / Samples));
					double Ground = 0.0;
					int32 Kind = 0;
					if (Height(W.X, W.Y, Ground) && FVector2D::Distance(W, DoorEdge) > P.DoorGapCm * 0.5)
					{
						const double Drop = PadZ - Ground;
						Kind = Drop >= P.WallMinDropCm ? 1 : (-Drop >= P.WallMinDropCm ? -1 : 0);
					}
					if (Kind != RunKind)
					{
						Flush();
					}
					if (Kind == 0)
					{
						continue;
					}
					RunKind = Kind;
					Run.PlotId = B.Plot->Id;
					Run.bRetaining = Kind > 0;
					{
						const FVector2D Mid = (A + C) * 0.5;
						const FVector2D Outward = (B.ToWorld(Mid) - B.Centre).GetSafeNormal();
						Run.Face = Kind > 0 ? Outward : -Outward;
					}
					const double Low = Kind > 0 ? FMath::Max(Ground, PadZ - P.WallMaxCm) : PadZ;
					const double High = Kind > 0 ? PadZ : FMath::Min(Ground, PadZ + P.WallMaxCm);
					Run.Bottom.Add(FVector(W, Low));
					Run.Top.Add(FVector(W, High));
					Run.MaxHeightCm = FMath::Max(Run.MaxHeightCm, High - Low);
				}
				Flush();
			}
		}
		for (const FWallRun& Wall : Out.Walls)
		{
			Out.Report.WallLengthCm += Wall.LengthCm;
			Out.Report.WallMaxHeightCm = FMath::Max(Out.Report.WallMaxHeightCm, Wall.MaxHeightCm);
		}
		Out.Report.Walls = Out.Walls.Num();

		// --- L'ombre de la placette : un platane, sur le bord le plus plat, hors chaussee ---------
		if (Out.Plaza.bValid)
		{
			double Best = TNumericLimits<double>::Max();
			const int32 Turns = 16;
			const double Phase = (StableHash(Out.Plaza.WellId) % 360) * PI / 180.0;
			for (int32 K = 0; K < Turns; ++K)
			{
				const double Angle = Phase + K * 2.0 * PI / Turns;
				const FVector2D Spot = HubBody.Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (P.PlazaRadiusCm + 250.0);
				const int32 Idx = Grid.Nearest(Spot);
				if (!Grid.Free[Idx] || Out.Paved[Idx] || Usage[Idx] > 0)
				{
					continue;
				}
				bool bOnParcel = false;
				for (const FBody& B : Bodies)
				{
					bOnParcel |= !B.bWell && B.InSquare(Spot, P.ArchitectureClearHalfCm);
				}
				if (bOnParcel)
				{
					continue;
				}
				const double Score = FMath::Abs(Grid.Z[Idx] - Out.Plaza.LevelCm);
				if (Score < Best)
				{
					Best = Score;
					Out.Plaza.bHasTree = true;
					Out.Plaza.TreeSpot = FVector(Spot, Grid.Z[Idx]);
				}
			}
		}

		// --- Signature ---------------------------------------------------------------------------
		uint32 Hash = 2166136261u;
		for (const FLane& Lane : Out.Lanes)
		{
			Mix(Hash, StableHash(Lane.FromId));
			Mix(Hash, FMath::RoundToInt64(Lane.WidthCm));
			for (const FLanePoint& Point : Lane.Points)
			{
				Mix(Hash, FMath::RoundToInt64(Point.Position.X));
				Mix(Hash, FMath::RoundToInt64(Point.Position.Y));
				Mix(Hash, FMath::RoundToInt64(Point.Position.Z));
			}
		}
		for (const FWallRun& Wall : Out.Walls)
		{
			Mix(Hash, StableHash(Wall.PlotId));
			Mix(Hash, Wall.bRetaining ? 1 : 0);
			for (int32 K = 0; K < Wall.Bottom.Num(); ++K)
			{
				Mix(Hash, FMath::RoundToInt64(Wall.Bottom[K].X));
				Mix(Hash, FMath::RoundToInt64(Wall.Bottom[K].Y));
				Mix(Hash, FMath::RoundToInt64(Wall.Top[K].Z - Wall.Bottom[K].Z));
			}
		}
		Mix(Hash, FMath::RoundToInt64(Out.Plaza.TreeSpot.X));
		Mix(Hash, FMath::RoundToInt64(Out.Plaza.TreeSpot.Y));
		Out.Report.Signature = Hash;
		return Out;
	}

	FString ToLogLine(const FReport& R)
	{
		return FString::Printf(
			TEXT("ANASTASIS_FABRIC report plots=%d remote=%d doors=%d connected=%d lanes=%d laneLength=%.0fm steps=%.0fm laneGrade=%.3f fallGrade=%.3f maxGrade=%.3f walls=%d wallLength=%.0fm wallMax=%.0fcm intrusions=%d signature=%08x"),
			R.Plots, R.Remote, R.Doors, R.ConnectedDoors, R.Lanes, R.LaneLengthCm / 100.0, R.StepLengthCm / 100.0,
			R.LaneMeanGrade, R.FallMeanGrade, R.LaneMaxGrade, R.Walls, R.WallLengthCm / 100.0,
			R.WallMaxHeightCm, R.BodyIntrusions, R.Signature);
	}

	FString ToJson(const FFabric& F)
	{
		const FReport& R = F.Report;
		TArray<FString> Lanes;
		for (const FLane& L : F.Lanes)
		{
			const FVector A = L.Points.Num() ? L.Points[0].Position : FVector::ZeroVector;
			const FVector B = L.Points.Num() ? L.Points.Last().Position : FVector::ZeroVector;
			Lanes.Add(FString::Printf(
				TEXT("{\"from\":\"%s\",\"width\":%.0f,\"usage\":%d,\"length\":%.0f,\"grade\":%.4f,\"fall\":%.4f,\"a\":[%.0f,%.0f,%.0f],\"b\":[%.0f,%.0f,%.0f]}"),
				*L.FromId, L.WidthCm, L.Usage, L.LengthCm, L.MeanGrade, L.FallGrade, A.X, A.Y, A.Z, B.X, B.Y, B.Z));
		}
		return FString::Printf(
			TEXT("{\"plots\":%d,\"remote\":%d,\"doors\":%d,\"connected\":%d,\"lanes\":%d,\"laneLength\":%.0f,\"stepLength\":%.0f,\"laneGrade\":%.4f,\"fallGrade\":%.4f,\"maxGrade\":%.4f,\"walls\":%d,\"wallLength\":%.0f,\"wallMax\":%.0f,\"intrusions\":%d,\"signature\":\"%08x\",\"plaza\":{\"valid\":%s,\"well\":\"%s\",\"centre\":[%.0f,%.0f,%.0f],\"radius\":%.0f,\"tree\":%s,\"treeSpot\":[%.0f,%.0f,%.0f]},\"laneList\":[%s]}"),
			R.Plots, R.Remote, R.Doors, R.ConnectedDoors, R.Lanes, R.LaneLengthCm, R.StepLengthCm, R.LaneMeanGrade, R.FallMeanGrade,
			R.LaneMaxGrade, R.Walls, R.WallLengthCm, R.WallMaxHeightCm, R.BodyIntrusions, R.Signature,
			F.Plaza.bValid ? TEXT("true") : TEXT("false"), *F.Plaza.WellId, F.Plaza.Centre.X, F.Plaza.Centre.Y, F.Plaza.Centre.Z,
			F.Plaza.RadiusCm, F.Plaza.bHasTree ? TEXT("true") : TEXT("false"), F.Plaza.TreeSpot.X, F.Plaza.TreeSpot.Y, F.Plaza.TreeSpot.Z,
			*FString::Join(Lanes, TEXT(",")));
	}
}
