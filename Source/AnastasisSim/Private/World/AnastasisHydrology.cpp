#include "World/AnastasisHydrology.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimMath.h"
#include "World/AnastasisWorldNoise.h"

#include <limits>

namespace AnastasisHydrology
{
	namespace
	{
		constexpr int32 Cardinals[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

		struct FMinHeap
		{
			TArray<int32> Idx;
			TArray<float> Pri;
			int32 Size = 0;

			explicit FMinHeap(int32 Capacity)
			{
				Idx.SetNumUninitialized(Capacity);
				Pri.SetNumUninitialized(Capacity);
			}

			void Push(int32 Index, float Priority)
			{
				int32 I = Size;
				Size = I + 1;
				while (I > 0)
				{
					const int32 P = (I - 1) >> 1;
					if (Pri[P] <= Priority)
					{
						break;
					}
					Idx[I] = Idx[P];
					Pri[I] = Pri[P];
					I = P;
				}
				Idx[I] = Index;
				Pri[I] = Priority;
			}

			int32 Pop()
			{
				const int32 Root = Idx[0];
				Size -= 1;
				if (Size == 0)
				{
					return Root;
				}
				const int32 Index = Idx[Size];
				const float Priority = Pri[Size];
				int32 I = 0;
				while (true)
				{
					const int32 L = I * 2 + 1;
					if (L >= Size)
					{
						break;
					}
					const int32 R = L + 1;
					const int32 Child = (R < Size && Pri[R] < Pri[L]) ? R : L;
					if (Pri[Child] >= Priority)
					{
						break;
					}
					Idx[I] = Idx[Child];
					Pri[I] = Pri[Child];
					I = Child;
				}
				Idx[I] = Index;
				Pri[I] = Priority;
				return Root;
			}
		};

		void BiasValleys(TArray<float>& Height, int32 W, int32 H, uint32 Seed, double ValleyDig, double BasinDig)
		{
			for (int32 Y = 0; Y < H; ++Y)
			{
				for (int32 X = 0; X < W; ++X)
				{
					const int32 I = Y * W + X;
					const double Warp = AnastasisWorldNoise::Fbm(X / 18.0, Y / 18.0, static_cast<double>(Seed) + WarpSeed);
					const double WX = X / 26.0 + (Warp - 0.5) * 0.9;
					const double WY = Y / 26.0 + (AnastasisWorldNoise::Fbm(X / 19.0, Y / 19.0, static_cast<double>(Seed) + WarpSeed + 17) - 0.5) * 0.9;
					const double Ridge = 1.0 - FMath::Abs(AnastasisWorldNoise::Fbm(WX, WY, static_cast<double>(Seed) + ValleySeed) * 2.0 - 1.0);
					const double Valley = 1.0 - Ridge;
					Height[I] = AnastasisJs::StoreF32(AnastasisJs::LoadF32(Height[I]) - Valley * Valley * 0.085 * ValleyDig);

					const double Basin = AnastasisWorldNoise::Fbm(X / 40.0, Y / 40.0, static_cast<double>(Seed) + BasinSeed);
					const double Bowl = AnastasisMath::Clamp(1.0 - Basin * 1.55, 0.0, 1.0);
					Height[I] = AnastasisJs::StoreF32(AnastasisJs::LoadF32(Height[I]) - Bowl * Bowl * 0.05 * BasinDig);
				}
			}
		}

		TArray<float> FillDepressions(const TArray<float>& Height, int32 W, int32 H)
		{
			const int32 N = W * H;
			TArray<float> Filled;
			Filled.SetNumUninitialized(N);
			TArray<uint8> Closed;
			Closed.SetNumZeroed(N);
			FMinHeap Heap(N);

			for (int32 I = 0; I < N; ++I)
			{
				Filled[I] = std::numeric_limits<float>::infinity();
			}

			for (int32 Y = 0; Y < H; ++Y)
			{
				for (int32 X = 0; X < W; ++X)
				{
					if (X > 0 && Y > 0 && X < W - 1 && Y < H - 1)
					{
						continue;
					}
					const int32 I = Y * W + X;
					Filled[I] = Height[I];
					Closed[I] = 1;
					Heap.Push(I, Height[I]);
				}
			}

			while (Heap.Size > 0)
			{
				const int32 I = Heap.Pop();
				const float Elev = Filled[I];
				const int32 X = I % W;
				const int32 Y = I / W;
				for (int32 D = 0; D < 4; ++D)
				{
					const int32 NX = X + Cardinals[D][0];
					const int32 NY = Y + Cardinals[D][1];
					if (NX < 0 || NY < 0 || NX >= W || NY >= H)
					{
						continue;
					}
					const int32 NI = NY * W + NX;
					if (Closed[NI])
					{
						continue;
					}
					Closed[NI] = 1;
					const float Next = FMath::Max(Height[NI], Elev);
					Filled[NI] = Next;
					Heap.Push(NI, Next);
				}
			}
			return Filled;
		}

		TArray<int32> BuildFlowMap(const TArray<float>& Filled, int32 W, int32 H)
		{
			const int32 N = W * H;
			TArray<int32> FlowTo;
			FlowTo.SetNumUninitialized(N);
			for (int32 I = 0; I < N; ++I)
			{
				const int32 X = I % W;
				const int32 Y = I / W;
				int32 Best = -1;
				float BestElev = Filled[I];
				for (int32 D = 0; D < 4; ++D)
				{
					const int32 NX = X + Cardinals[D][0];
					const int32 NY = Y + Cardinals[D][1];
					if (NX < 0 || NY < 0 || NX >= W || NY >= H)
					{
						continue;
					}
					const int32 NI = NY * W + NX;
					const float Elev = Filled[NI];
					if (Elev < BestElev)
					{
						BestElev = Elev;
						Best = NI;
					}
				}
				FlowTo[I] = Best;
			}
			return FlowTo;
		}

		TArray<float> AccumulateFlow(const TArray<float>& Filled, const TArray<int32>& FlowTo, int32 W, int32 H)
		{
			const int32 N = W * H;
			TArray<int32> Order;
			Order.SetNumUninitialized(N);
			for (int32 I = 0; I < N; ++I)
			{
				Order[I] = I;
			}
			Order.StableSort([&](int32 A, int32 B)
			{
				if (Filled[A] != Filled[B])
				{
					return Filled[A] > Filled[B];
				}
				return A < B;
			});

			TArray<float> Accum;
			Accum.SetNumUninitialized(N);
			for (int32 I = 0; I < N; ++I)
			{
				Accum[I] = 1.0f;
			}
			for (int32 K = 0; K < N; ++K)
			{
				const int32 I = Order[K];
				const int32 Next = FlowTo[I];
				if (Next >= 0)
				{
					Accum[Next] = AnastasisJs::StoreF32(AnastasisJs::LoadF32(Accum[Next]) + AnastasisJs::LoadF32(Accum[I]));
				}
			}
			return Accum;
		}

		void WidenAround(TArray<float>& Height, int32 W, int32 H, int32 X, int32 Y, int32 Radius, double Bed, double Amount)
		{
			for (int32 DY = -Radius; DY <= Radius; ++DY)
			{
				for (int32 DX = -Radius; DX <= Radius; ++DX)
				{
					if (DX == 0 && DY == 0)
					{
						continue;
					}
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) > Radius)
					{
						continue;
					}
					const int32 NX = X + DX;
					const int32 NY = Y + DY;
					if (NX < 0 || NY < 0 || NX >= W || NY >= H)
					{
						continue;
					}
					const int32 NI = NY * W + NX;
					const double H0 = AnastasisJs::LoadF32(Height[NI]);
					if (H0 > Bed)
					{
						Height[NI] = AnastasisJs::StoreF32(H0 + (Bed - H0) * Amount);
					}
				}
			}
		}

		void StampLakeBasins(TArray<float>& Height, const TArray<float>& Filled, double SeaLevel, double LakeDig)
		{
			for (int32 I = 0; I < Height.Num(); ++I)
			{
				const double Ponding = AnastasisJs::LoadF32(Filled[I]) - AnastasisJs::LoadF32(Height[I]);
				if (Ponding < 0.035)
				{
					continue;
				}
				const double Dig = AnastasisMath::Clamp(0.02 + Ponding * 0.7, 0.02, 0.08) * LakeDig;
				const double Bed = SeaLevel - Dig;
				if (AnastasisJs::LoadF32(Height[I]) > Bed)
				{
					Height[I] = AnastasisJs::StoreF32(Bed);
				}
			}
		}

		void CarveChannels(TArray<float>& Height, const TArray<float>& Accum, const TArray<int32>& FlowTo, int32 W, int32 H, double SeaLevel, double ChannelDig)
		{
			double MaxAccum = 1.0;
			for (const float A : Accum)
			{
				if (AnastasisJs::LoadF32(A) > MaxAccum)
				{
					MaxAccum = AnastasisJs::LoadF32(A);
				}
			}
			const double RiverStart = FMath::Max(28.0, MaxAccum * 0.02);
			const double LogSpan = AnastasisJs::Log(FMath::Max(MaxAccum / RiverStart, 1.001));
			TArray<uint8> Carved;
			Carved.SetNumZeroed(Height.Num());

			for (int32 I = 0; I < Accum.Num(); ++I)
			{
				if (AnastasisJs::LoadF32(Accum[I]) < RiverStart)
				{
					continue;
				}
				int32 Cursor = I;
				int32 Guard = 0;
				while (Cursor >= 0 && Guard < W * H)
				{
					Guard += 1;
					if (Carved[Cursor])
					{
						Cursor = FlowTo[Cursor];
						continue;
					}
					Carved[Cursor] = 1;
					const double A = AnastasisJs::LoadF32(Accum[Cursor]);
					const double T = A <= RiverStart ? 0.0 : AnastasisMath::Clamp(AnastasisJs::Log(A / RiverStart) / LogSpan, 0.0, 1.0);
					const double Dig = (0.04 + T * 0.09) * ChannelDig;
					const double Bed = SeaLevel - Dig;
					if (AnastasisJs::LoadF32(Height[Cursor]) > Bed)
					{
						Height[Cursor] = AnastasisJs::StoreF32(Bed);
					}
					if (T >= 0.35 && ChannelDig > 0.45)
					{
						const int32 X = Cursor % W;
						const int32 Y = Cursor / W;
						WidenAround(Height, W, H, X, Y, 1, SeaLevel - Dig * 0.35, 0.85);
						if (T >= 0.7)
						{
							WidenAround(Height, W, H, X, Y, 2, SeaLevel - Dig * 0.18, 0.5);
						}
					}
					Cursor = FlowTo[Cursor];
				}
			}
		}

		void PrunePuddles(TArray<float>& Height, int32 W, int32 H, double SeaLevel, int32 MinSize)
		{
			const int32 N = W * H;
			TArray<uint8> Seen;
			Seen.SetNumZeroed(N);
			TArray<int32> Stack;
			Stack.SetNumUninitialized(N);
			TArray<int32> Component;
			Component.SetNumUninitialized(N);

			for (int32 Start = 0; Start < N; ++Start)
			{
				if (Seen[Start] || AnastasisJs::LoadF32(Height[Start]) >= SeaLevel)
				{
					continue;
				}
				int32 Top = 0;
				int32 Size = 0;
				Stack[Top++] = Start;
				Seen[Start] = 1;
				while (Top > 0)
				{
					const int32 I = Stack[--Top];
					Component[Size++] = I;
					const int32 X = I % W;
					const int32 Y = I / W;
					for (int32 D = 0; D < 4; ++D)
					{
						const int32 NX = X + Cardinals[D][0];
						const int32 NY = Y + Cardinals[D][1];
						if (NX < 0 || NY < 0 || NX >= W || NY >= H)
						{
							continue;
						}
						const int32 NI = NY * W + NX;
						if (Seen[NI] || AnastasisJs::LoadF32(Height[NI]) >= SeaLevel)
						{
							continue;
						}
						Seen[NI] = 1;
						Stack[Top++] = NI;
					}
				}
				if (Size >= MinSize)
				{
					continue;
				}
				const float Raised = AnastasisJs::StoreF32(SeaLevel + 0.02);
				for (int32 K = 0; K < Size; ++K)
				{
					Height[Component[K]] = Raised;
				}
			}
		}
	}

	FResult Apply(
		TArray<float>& Height,
		int32 W,
		int32 H,
		uint32 Seed,
		double SeaLevel,
		double ValleyDig,
		double BasinDig,
		double ChannelDig,
		double LakeDig,
		int32 MinWaterBody)
	{
		BiasValleys(Height, W, H, Seed, ValleyDig, BasinDig);
		TArray<float> Filled = FillDepressions(Height, W, H);
		TArray<int32> FlowTo = BuildFlowMap(Filled, W, H);
		TArray<float> Accum = AccumulateFlow(Filled, FlowTo, W, H);
		StampLakeBasins(Height, Filled, SeaLevel, LakeDig);
		CarveChannels(Height, Accum, FlowTo, W, H, SeaLevel, ChannelDig);
		PrunePuddles(Height, W, H, SeaLevel, MinWaterBody);
		FResult Out;
		Out.Filled = MoveTemp(Filled);
		Out.Accum = MoveTemp(Accum);
		Out.FlowTo = MoveTemp(FlowTo);
		return Out;
	}

	void StampWaterFlowFields(
		TArray<AnastasisWorld::FTile>& Tiles,
		int32 W,
		int32 H,
		const TArray<float>& Accum,
		const TArray<int32>& FlowTo,
		double SeaLevel,
		const TArray<float>& Height)
	{
		double MaxAccum = 1.0;
		for (const float A : Accum)
		{
			if (AnastasisJs::LoadF32(A) > MaxAccum)
			{
				MaxAccum = AnastasisJs::LoadF32(A);
			}
		}
		const double RiverStart = FMath::Max(28.0, MaxAccum * 0.02);
		const double LogSpan = AnastasisJs::Log(FMath::Max(MaxAccum / RiverStart, 1.001));

		auto IsWaterAt = [&](int32 X, int32 Y) -> bool
		{
			if (X < 0 || Y < 0 || X >= W || Y >= H)
			{
				return false;
			}
			return AnastasisJs::LoadF32(Height[Y * W + X]) < SeaLevel;
		};

		auto LandDist = [&](int32 X, int32 Y) -> int32
		{
			for (int32 Radius = 1; Radius <= 4; ++Radius)
			{
				for (int32 DY = -Radius; DY <= Radius; ++DY)
				{
					for (int32 DX = -Radius; DX <= Radius; ++DX)
					{
						if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius)
						{
							continue;
						}
						if (!IsWaterAt(X + DX, Y + DY))
						{
							return Radius - 1;
						}
					}
				}
			}
			return 4;
		};

		auto FlowDirAt = [&](int32 I, double& OutX, double& OutZ)
		{
			double FX = 0.0;
			double FZ = 0.0;
			double Weight = 0.0;
			int32 Cursor = I;
			for (int32 Step = 0; Step < 2; ++Step)
			{
				const int32 Next = FlowTo[Cursor];
				if (Next < 0)
				{
					break;
				}
				const int32 X = Cursor % W;
				const int32 Y = Cursor / W;
				const int32 NX = Next % W;
				const int32 NY = Next / W;
				const double Wgt = Step == 0 ? 1.0 : 0.55;
				FX += (NX - X) * Wgt;
				FZ += (NY - Y) * Wgt;
				Weight += Wgt;
				Cursor = Next;
			}
			if (Weight < 1e-4)
			{
				OutX = 0.0;
				OutZ = 0.0;
				return;
			}
			const double Len = AnastasisMath::JsHypot(FX, FZ);
			const double Denom = Len == 0.0 ? 1.0 : Len;
			OutX = FX / Denom;
			OutZ = FZ / Denom;
		};

		for (int32 I = 0; I < Tiles.Num(); ++I)
		{
			AnastasisWorld::FTile& Tile = Tiles[I];
			if (Tile.Type != AnastasisWorld::ETileType::Water)
			{
				continue;
			}
			const double A = AnastasisJs::LoadF32(Accum[I]);
			const double Accum01 = A <= RiverStart
				? 0.0
				: AnastasisMath::Clamp(AnastasisJs::Log(A / RiverStart) / LogSpan, 0.0, 1.0);
			const double Narrowness = 1.0 - AnastasisMath::Clamp(LandDist(Tile.X, Tile.Y) / 3.0, 0.0, 1.0);
			const int32 Next = FlowTo[I];
			const bool bNextWater = Next >= 0 && IsWaterAt(Next % W, Next / W);
			const double ChannelBoost = bNextWater ? 1.12 : 0.72;
			const double Raw = Accum01 * (0.18 + Narrowness * 0.95) * ChannelBoost;
			double FlowAmt = AnastasisMath::Clamp(
				AnastasisJs::Pow(FMath::Max(0.0, Raw), 0.82) * (0.92 + Narrowness * 0.28),
				0.0,
				1.0);
			double FX = 0.0;
			double FZ = 0.0;
			if (FlowAmt >= WaterFlowAmtGate && Next >= 0)
			{
				FlowDirAt(I, FX, FZ);
				if (FX == 0.0 && FZ == 0.0)
				{
					FlowAmt = 0.0;
				}
			}
			else
			{
				FlowAmt = 0.0;
			}
			Tile.FlowX = AnastasisJs::Round3(FX);
			Tile.FlowZ = AnastasisJs::Round3(FZ);
			Tile.FlowAmt = AnastasisJs::Round3(FlowAmt);
		}

		for (int32 I = 0; I < Tiles.Num(); ++I)
		{
			AnastasisWorld::FTile& Tile = Tiles[I];
			if (Tile.Type != AnastasisWorld::ETileType::Water || Tile.FlowAmt < WaterFlowAmtGate)
			{
				continue;
			}
			const int32 Next = FlowTo[I];
			if (Next < 0)
			{
				continue;
			}
			const AnastasisWorld::FTile& Down = Tiles[Next];
			if (Down.Type != AnastasisWorld::ETileType::Water || Down.FlowAmt < WaterFlowAmtGate)
			{
				continue;
			}
			const double T = 0.35;
			double FX = Tile.FlowX * (1.0 - T) + Down.FlowX * T;
			double FZ = Tile.FlowZ * (1.0 - T) + Down.FlowZ * T;
			const double Len = AnastasisMath::JsHypot(FX, FZ);
			const double Denom = Len == 0.0 ? 1.0 : Len;
			Tile.FlowX = AnastasisJs::Round3(FX / Denom);
			Tile.FlowZ = AnastasisJs::Round3(FZ / Denom);
			Tile.FlowAmt = AnastasisJs::Round3(FMath::Min(1.0, Tile.FlowAmt * (1.0 + Down.FlowAmt * 0.12)));
		}
	}
}
