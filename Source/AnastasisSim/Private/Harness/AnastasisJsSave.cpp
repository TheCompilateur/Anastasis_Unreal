#include "Harness/AnastasisJsSave.h"

#include <cmath>

#include "Core/AnastasisJsNumeric.h"

// Reference: src/sim/save.js de `anastasis-ref-p3` (fee66ae) — serialize,
// tileDiff, applyTileDiff, packActor; src/sim/pristineWorld.js;
// src/ai/algorithmic/mealReservation.js (serializeMealLedger).

namespace AnastasisJsSave
{
	namespace
	{
		using AnastasisJson::FValue;
		using AnastasisWorld::ECropId;
		using AnastasisWorld::EResource;
		using AnastasisWorld::ETileType;

		// --- Vocabulaire de la reference --------------------------------------

		const TCHAR* JsTileTypeName(ETileType Type)
		{
			switch (Type)
			{
			case ETileType::Grass: return TEXT("grass");
			case ETileType::Water: return TEXT("water");
			case ETileType::Stone: return TEXT("stone");
			case ETileType::Ruin: return TEXT("ruin");
			case ETileType::Forest: return TEXT("forest");
			case ETileType::Scrub: return TEXT("scrub");
			case ETileType::Field: return TEXT("field");
			case ETileType::Road: return TEXT("road");
			}
			return nullptr;
		}

		bool JsTileTypeFromName(const FString& Name, ETileType& Out)
		{
			for (uint8 Raw = 0; Raw <= static_cast<uint8>(ETileType::Road); ++Raw)
			{
				const ETileType Type = static_cast<ETileType>(Raw);
				if (Name.Equals(JsTileTypeName(Type), ESearchCase::CaseSensitive))
				{
					Out = Type;
					return true;
				}
			}
			return false;
		}

		/** nullptr = `null` (pas de ressource). */
		const TCHAR* JsResourceName(EResource Resource)
		{
			switch (Resource)
			{
			case EResource::None: return nullptr;
			case EResource::Stone: return TEXT("stone");
			case EResource::Wood: return TEXT("wood");
			case EResource::Food: return TEXT("food");
			}
			return nullptr;
		}

		bool JsResourceFromName(const FString& Name, EResource& Out)
		{
			for (EResource R : { EResource::Stone, EResource::Wood, EResource::Food })
			{
				if (Name.Equals(JsResourceName(R), ESearchCase::CaseSensitive))
				{
					Out = R;
					return true;
				}
			}
			return false;
		}

		/** nullptr = `null` (pas de culture). */
		const TCHAR* JsCropName(ECropId Crop)
		{
			switch (Crop)
			{
			case ECropId::None: return nullptr;
			case ECropId::Grain: return TEXT("grain");
			case ECropId::Greens: return TEXT("greens");
			case ECropId::Fruit: return TEXT("fruit");
			case ECropId::Fallow: return TEXT("fallow");
			}
			return nullptr;
		}

		bool JsCropFromName(const FString& Name, ECropId& Out)
		{
			for (ECropId C : { ECropId::Grain, ECropId::Greens, ECropId::Fruit, ECropId::Fallow })
			{
				if (Name.Equals(JsCropName(C), ESearchCase::CaseSensitive))
				{
					Out = C;
					return true;
				}
			}
			return false;
		}

		/** `Math.max(0, Math.min(1, v))`, -0 compris: Math.max(0, -0) rend +0. */
		double Clamp01Js(double Value)
		{
			const double Low = Value < 1.0 ? Value : 1.0;
			return Low > 0.0 ? Low : 0.0;
		}

		/** `tileDiff` emet `x || 0`: un -0 (falsy) y devient +0. */
		double OrZero(double Value)
		{
			return Value == 0.0 ? 0.0 : Value;
		}

		FValue Num(double Value) { return FValue::MakeNumber(Value); }
		FValue Str(const FString& Value) { return FValue::MakeString(Value); }
		/** Identifiant C++ -> JSON: vide = null. */
		FValue IdOrNull(const FString& Value) { return Value.IsEmpty() ? FValue() : FValue::MakeString(Value); }
		FValue NameOrNull(const TCHAR* Name) { return Name ? FValue::MakeString(Name) : FValue(); }

		// --- Lecture typee, avec le chemin de l'erreur -------------------------

		struct FReader
		{
			FString Error;

			bool Fail(const FString& Where, const FString& What)
			{
				if (Error.IsEmpty())
				{
					Error = FString::Printf(TEXT("%s : %s"), *Where, *What);
				}
				return false;
			}

			static FString At(const FString& Where, const TCHAR* Key) { return Where + TEXT(".") + Key; }

			/** Nombre requis. */
			bool Double(const FValue& Obj, const TCHAR* Key, const FString& Where, double& Out)
			{
				const FValue* V = Obj.Find(Key);
				if (!V || !V->IsNumber()) return Fail(At(Where, Key), TEXT("nombre attendu"));
				Out = V->Number;
				return true;
			}

			/** Nombre optionnel: absent = inchange. */
			bool OptDouble(const FValue& Obj, const TCHAR* Key, const FString& Where, double& Out)
			{
				const FValue* V = Obj.Find(Key);
				if (!V) return true;
				if (!V->IsNumber()) return Fail(At(Where, Key), TEXT("nombre attendu"));
				Out = V->Number;
				return true;
			}

			/**
			 * Entier tenu en int32 cote C++. Refuse ce que l'aller-retour ne
			 * rendrait pas a l'identique: fraction, hors bornes, et -0 (relu
			 * en 0, il reviendrait +0).
			 */
			bool CheckInt(double V, const FString& Where, int32& Out)
			{
				if (!FMath::IsFinite(V) || V != FMath::FloorToDouble(V) || V < -2147483648.0 || V > 2147483647.0)
				{
					return Fail(Where, FString::Printf(TEXT("entier 32 bits attendu, lu %.17g"), V));
				}
				if (V == 0.0 && std::signbit(V))
				{
					return Fail(Where, TEXT("-0 : un entier C++ le rendrait +0"));
				}
				Out = static_cast<int32>(V);
				return true;
			}

			bool Int(const FValue& Obj, const TCHAR* Key, const FString& Where, int32& Out)
			{
				double V = 0.0;
				return Double(Obj, Key, Where, V) && CheckInt(V, At(Where, Key), Out);
			}

			bool OptInt(const FValue& Obj, const TCHAR* Key, const FString& Where, int32& Out)
			{
				const FValue* V = Obj.Find(Key);
				if (!V) return true;
				if (!V->IsNumber()) return Fail(At(Where, Key), TEXT("nombre attendu"));
				return CheckInt(V->Number, At(Where, Key), Out);
			}

			bool Uint32(const FValue& Obj, const TCHAR* Key, const FString& Where, uint32& Out)
			{
				double V = 0.0;
				if (!Double(Obj, Key, Where, V)) return false;
				if (!FMath::IsFinite(V) || V != FMath::FloorToDouble(V) || V < 0.0 || V > 4294967295.0 || std::signbit(V))
				{
					return Fail(At(Where, Key), FString::Printf(TEXT("entier non signe 32 bits attendu, lu %.17g"), V));
				}
				Out = static_cast<uint32>(V);
				return true;
			}

			bool String(const FValue& Obj, const TCHAR* Key, const FString& Where, FString& Out)
			{
				const FValue* V = Obj.Find(Key);
				if (!V || !V->IsString()) return Fail(At(Where, Key), TEXT("chaine attendue"));
				Out = V->String;
				return true;
			}

			bool OptString(const FValue& Obj, const TCHAR* Key, const FString& Where, FString& Out)
			{
				const FValue* V = Obj.Find(Key);
				if (!V) return true;
				if (!V->IsString()) return Fail(At(Where, Key), TEXT("chaine attendue"));
				Out = V->String;
				return true;
			}

			/** Identifiant ou null (null = vide). Une chaine vide ne se distinguerait plus de null: refusee. */
			bool OptId(const FValue& Obj, const TCHAR* Key, const FString& Where, FString& Out)
			{
				const FValue* V = Obj.Find(Key);
				if (!V) return true;
				if (V->IsNull())
				{
					Out.Reset();
					return true;
				}
				if (!V->IsString() || V->String.IsEmpty())
				{
					return Fail(At(Where, Key), TEXT("identifiant (chaine non vide) ou null attendu"));
				}
				Out = V->String;
				return true;
			}

			bool OptBool(const FValue& Obj, const TCHAR* Key, const FString& Where, bool& Out)
			{
				const FValue* V = Obj.Find(Key);
				if (!V) return true;
				if (!V->IsBool()) return Fail(At(Where, Key), TEXT("booleen attendu"));
				Out = V->bBool;
				return true;
			}

			/** `{x, y}`. */
			bool Point(const FValue& V, const FString& Where, AnastasisVillage::FPoint& Out)
			{
				if (!V.IsObject()) return Fail(Where, TEXT("point {x, y} attendu"));
				return Double(V, TEXT("x"), Where, Out.X) && Double(V, TEXT("y"), Where, Out.Y);
			}
		};

		// --- Le monde -----------------------------------------------------------

		/** `applyTileDiff`, lignes au format courant (>= 9 colonnes). */
		bool ApplyTileDiff(FReader& R, const FValue& Diff, FState& S)
		{
			if (!Diff.IsArray()) return R.Fail(TEXT("tileDiff"), TEXT("tableau attendu"));
			for (int32 RowIndex = 0; RowIndex < Diff.Items.Num(); ++RowIndex)
			{
				const FValue& Row = Diff.Items[RowIndex];
				const FString Where = FString::Printf(TEXT("tileDiff[%d]"), RowIndex);
				if (!Row.IsArray() || Row.Items.Num() < 9)
				{
					// Le JS infere encore pads et clairieres des lignes anciennes;
					// ce lecteur ne lit que le format que `serialize` ecrit aujourd'hui.
					return R.Fail(Where, TEXT("ligne de moins de 9 colonnes (format ancien) : non lue"));
				}
				// Colonne absente = defaut de la destructuration JS (bridge 0, le reste null).
				auto Col = [&Row](int32 K) -> const FValue* { return K < Row.Items.Num() ? &Row.Items[K] : nullptr; };
				auto IsNum = [](const FValue* V) { return V && V->IsNumber() && FMath::IsFinite(V->Number); };
				auto Truthy = [](const FValue* V)
				{
					if (!V || V->IsNull()) return false;
					if (V->IsBool()) return V->bBool;
					if (V->IsNumber()) return V->Number != 0.0 && !FMath::IsNaN(V->Number);
					if (V->IsString()) return !V->String.IsEmpty();
					return true;
				};

				int32 Index = 0;
				if (!Col(0)->IsNumber() || !R.CheckInt(Col(0)->Number, Where + TEXT("[0]"), Index)) return R.Fail(Where, TEXT("index"));
				if (Index < 0 || Index >= S.World.Tiles.Num())
				{
					return R.Fail(Where, FString::Printf(TEXT("index %d hors de la carte"), Index));
				}
				AnastasisWorld::FTile& Tile = S.World.Tiles[Index];
				FTileExtra& Extra = S.TileExtras[Index];

				if (!Col(1)->IsString() || !JsTileTypeFromName(Col(1)->String, Tile.Type))
				{
					return R.Fail(Where + TEXT("[1]"), TEXT("type de tuile inconnu"));
				}
				if (Col(2)->IsNull())
				{
					Tile.Resource = EResource::None;
				}
				else if (!Col(2)->IsString() || !JsResourceFromName(Col(2)->String, Tile.Resource))
				{
					return R.Fail(Where + TEXT("[2]"), TEXT("ressource inconnue"));
				}
				if (!Col(3)->IsNumber() || !R.CheckInt(Col(3)->Number, Where + TEXT("[3]"), Tile.Amount)) return false;

				Extra.bBridge = Truthy(Col(4));
				// `tile.roadClass = roadClass` puis `now.roadClass || null`.
				Extra.RoadClass.Reset();
				if (const FValue* V = Col(5); V && !V->IsNull())
				{
					if (!V->IsString()) return R.Fail(Where + TEXT("[5]"), TEXT("roadClass : chaine ou null"));
					Extra.RoadClass = V->String;
				}

				// cropId: garde meme hors champ ; champ sans culture -> tirage de la case.
				if (const FValue* V = Col(6); V && V->IsString() && !V->String.IsEmpty())
				{
					if (!JsCropFromName(V->String, Tile.CropId)) return R.Fail(Where + TEXT("[6]"), TEXT("culture inconnue"));
				}
				else if (Tile.Type == ETileType::Field)
				{
					Tile.CropId = AnastasisWorld::PickFieldCropId(Tile.X, Tile.Y, 47u);
				}
				else
				{
					Tile.CropId = ECropId::None;
				}

				// clairiere: null = egal au vierge, et la generation n'en pose pas (0).
				Extra.Clearing = IsNum(Col(7)) ? Clamp01Js(Col(7)->Number) : 0.0;
				Extra.bColonizationPad = Truthy(Col(8));
				if (IsNum(Col(9))) Extra.Crown = Clamp01Js(Col(9)->Number);
				if (IsNum(Col(10))) Tile.Wetness = Col(10)->Number;
				if (IsNum(Col(11))) Tile.Alt = Col(11)->Number;

				Extra.RoadSurface.Reset();
				if (Truthy(Col(12)))
				{
					if (!Col(12)->IsString()) return R.Fail(Where + TEXT("[12]"), TEXT("roadSurface : chaine ou null"));
					Extra.RoadSurface = Col(12)->String;
				}
				Extra.bHasRoadBuiltDay = IsNum(Col(13));
				if (Extra.bHasRoadBuiltDay) Extra.RoadBuiltDay = AnastasisJs::ToInt32(Col(13)->Number);
				Extra.bHasRoadBuildEffort = IsNum(Col(14));
				Extra.RoadBuildEffort = Extra.bHasRoadBuildEffort ? Col(14)->Number : 0.0;
				Extra.bHasRoadUpgradeEffort = IsNum(Col(15));
				Extra.RoadUpgradeEffort = Extra.bHasRoadUpgradeEffort ? Col(15)->Number : 0.0;
				Extra.bHasRoadPaveEffort = IsNum(Col(16));
				Extra.RoadPaveEffort = Extra.bHasRoadPaveEffort ? Col(16)->Number : 0.0;
			}
			return true;
		}

		/** `tileDiff(sim)`: ce qui differe de la generation, recalcule depuis le monde C++. */
		FValue ProjectTileDiff(const FState& S)
		{
			// `pristineReference` est memoisee dans la reference ; ici, a la lecture.
			const AnastasisWorld::FWorld Generated = S.Pristine.IsValid() ? AnastasisWorld::FWorld() : AnastasisWorld::GenerateWorld(S.Seed, S.W, S.H);
			const AnastasisWorld::FWorld& Pristine = S.Pristine.IsValid() ? *S.Pristine : Generated;
			// `pristine.clearing[i] = t.clearing || 0` dans un Float32Array: la
			// generation ne pose pas de clairiere, la reference vierge vaut 0.
			constexpr double PristineClearing = 0.0;
			FValue Diff = FValue::MakeArray();
			for (int32 I = 0; I < S.World.Tiles.Num(); ++I)
			{
				const AnastasisWorld::FTile& Now = S.World.Tiles[I];
				const AnastasisWorld::FTile& Then = Pristine.Tiles[I];
				const FTileExtra& X = S.TileExtras[I];
				const bool bClearingChanged = FMath::Abs(X.Clearing - PristineClearing) > 0.04;
				if (Now.Type == Then.Type
					&& Now.Resource == Then.Resource
					&& Now.Amount == Then.Amount
					&& !X.bBridge
					&& X.RoadClass.IsEmpty()
					&& X.RoadSurface.IsEmpty()
					&& !(X.bHasRoadBuiltDay && X.RoadBuiltDay != 0)
					&& !(X.bHasRoadBuildEffort && X.RoadBuildEffort != 0.0)
					&& !(X.bHasRoadUpgradeEffort && X.RoadUpgradeEffort != 0.0)
					&& !(X.bHasRoadPaveEffort && X.RoadPaveEffort != 0.0)
					&& Now.CropId == Then.CropId
					&& !bClearingChanged
					&& !X.bColonizationPad)
				{
					continue;
				}
				FValue Row = FValue::MakeArray();
				Row.Items.Add(Num(static_cast<double>(I)));
				Row.Items.Add(Str(JsTileTypeName(Now.Type)));
				Row.Items.Add(NameOrNull(JsResourceName(Now.Resource)));
				Row.Items.Add(Num(static_cast<double>(Now.Amount)));
				Row.Items.Add(Num(X.bBridge ? 1.0 : 0.0));
				Row.Items.Add(IdOrNull(X.RoadClass));
				Row.Items.Add(NameOrNull(JsCropName(Now.CropId)));
				Row.Items.Add(Num(OrZero(X.Clearing)));
				Row.Items.Add(X.bColonizationPad ? Num(1.0) : FValue());
				Row.Items.Add(Num(OrZero(X.Crown)));
				Row.Items.Add(Num(Now.Wetness));
				Row.Items.Add(Num(Now.Alt));
				Row.Items.Add(IdOrNull(X.RoadSurface));
				Row.Items.Add(X.bHasRoadBuiltDay ? Num(static_cast<double>(X.RoadBuiltDay)) : FValue());
				Row.Items.Add(X.bHasRoadBuildEffort ? Num(X.RoadBuildEffort) : FValue());
				Row.Items.Add(X.bHasRoadUpgradeEffort ? Num(X.RoadUpgradeEffort) : FValue());
				Row.Items.Add(X.bHasRoadPaveEffort ? Num(X.RoadPaveEffort) : FValue());
				Diff.Items.Add(MoveTemp(Row));
			}
			return Diff;
		}

		// --- Batiments ----------------------------------------------------------
		// Lus: id, type, x, y, progress, createdDay, owner, builderId, housePhase,
		// piecesPlaced, accessPoints, stock.food.{physical, reserved}.

		bool ReadBuilding(FReader& R, const FValue& B, const FString& Where, AnastasisVillage::FBuilding& Out)
		{
			if (!B.IsObject()) return R.Fail(Where, TEXT("objet attendu"));
			if (!R.String(B, TEXT("id"), Where, Out.Id) || !R.String(B, TEXT("type"), Where, Out.Type)) return false;
			if (!R.Double(B, TEXT("x"), Where, Out.X) || !R.Double(B, TEXT("y"), Where, Out.Y)) return false;
			if (!R.OptDouble(B, TEXT("progress"), Where, Out.Progress)) return false;
			if (!R.OptInt(B, TEXT("createdDay"), Where, Out.CreatedDay)) return false;
			if (!R.OptId(B, TEXT("owner"), Where, Out.Owner)) return false;
			if (!R.OptId(B, TEXT("builderId"), Where, Out.BuilderId)) return false;
			if (!R.OptInt(B, TEXT("housePhase"), Where, Out.HousePhase)) return false;
			if (const FValue* P = B.Find(TEXT("piecesPlaced")); P && !P->IsNull())
			{
				if (!R.OptInt(B, TEXT("piecesPlaced"), Where, Out.PiecesPlaced)) return false;
			}
			if (const FValue* Access = B.Find(TEXT("accessPoints")))
			{
				if (!Access->IsArray()) return R.Fail(FReader::At(Where, TEXT("accessPoints")), TEXT("tableau attendu"));
				for (int32 K = 0; K < Access->Items.Num(); ++K)
				{
					AnastasisVillage::FPoint P;
					if (!R.Point(Access->Items[K], FString::Printf(TEXT("%s.accessPoints[%d]"), *Where, K), P)) return false;
					Out.AccessPoints.Add(P);
				}
			}
			if (const FValue* Stock = B.Find(TEXT("stock")))
			{
				if (const FValue* Food = Stock->Find(TEXT("food")))
				{
					const FString W = Where + TEXT(".stock.food");
					if (!R.Int(*Food, TEXT("physical"), W, Out.FoodPhysical) || !R.Int(*Food, TEXT("reserved"), W, Out.FoodReserved)) return false;
				}
			}
			return true;
		}

		/** Ecrit `Key` si l'objet de base l'a (ou si l'entite est nouvelle). */
		void Put(FValue& Obj, const TCHAR* Key, const FValue& Value, bool bNew)
		{
			if (bNew || Obj.Find(Key)) Obj.Set(Key, Value);
		}

		FValue ProjectBuilding(const AnastasisVillage::FBuilding& B, const FValue* Base)
		{
			const bool bNew = Base == nullptr;
			FValue Out = Base ? *Base : FValue::MakeObject();
			Put(Out, TEXT("id"), Str(B.Id), bNew);
			Put(Out, TEXT("type"), Str(B.Type), bNew);
			Put(Out, TEXT("x"), Num(B.X), bNew);
			Put(Out, TEXT("y"), Num(B.Y), bNew);
			Put(Out, TEXT("progress"), Num(B.Progress), bNew);
			Put(Out, TEXT("createdDay"), Num(B.CreatedDay), bNew);
			Put(Out, TEXT("owner"), IdOrNull(B.Owner), bNew);
			Put(Out, TEXT("builderId"), IdOrNull(B.BuilderId), bNew);
			if (B.Type == AnastasisVillage::HouseType) Put(Out, TEXT("housePhase"), Num(B.HousePhase), bNew);
			if (const FValue* P = Out.Find(TEXT("piecesPlaced")); P && !P->IsNull()) Out.Set(TEXT("piecesPlaced"), Num(B.PiecesPlaced));
			if (bNew || Out.Find(TEXT("accessPoints")))
			{
				const FValue* Old = Out.Find(TEXT("accessPoints"));
				FValue Access = FValue::MakeArray();
				for (int32 K = 0; K < B.AccessPoints.Num(); ++K)
				{
					FValue P = (Old && Old->IsArray() && K < Old->Items.Num() && Old->Items[K].IsObject()) ? Old->Items[K] : FValue::MakeObject();
					P.Set(TEXT("x"), Num(B.AccessPoints[K].X));
					P.Set(TEXT("y"), Num(B.AccessPoints[K].Y));
					Access.Items.Add(MoveTemp(P));
				}
				Out.Set(TEXT("accessPoints"), Access);
			}
			if (FValue* Stock = Out.Find(TEXT("stock")))
			{
				if (FValue* Food = Stock->Find(TEXT("food")))
				{
					Food->Set(TEXT("physical"), Num(B.FoodPhysical));
					Food->Set(TEXT("reserved"), Num(B.FoodReserved));
				}
			}
			return Out;
		}

		// --- Habitants ----------------------------------------------------------
		// Lus: id, x, y, speed, les huit besoins, goal, activity, goalSince,
		// workTimer, jobId, workplaceId, homeId, shelterId, skill,
		// skills.{gather, trade, craft}, inventory.food, target, pathStep,
		// pathCooldown, pathFailed, stuckTimer, doorStuckAt, doorApproachAt,
		// talkWithId, talkUntil, inside (null seulement), _simBudgetAccum (cle presente
		// ou non : la reference ne l'ecrit qu'apres un passage hors de la bande near),
		// phenotype, conditioning, et genome.loci quand le phenotype manque.

		/** `genome.loci` : `{ <locus>: [a, b] }`. Le reste du genome n'entre pas dans le phenotype. */
		bool ReadGenomeLoci(FReader& R, const FValue& G, const FString& Where, AnastasisGenome::FGenome& Out)
		{
			if (!G.IsObject()) return R.Fail(Where, TEXT("objet attendu"));
			const FValue* Loci = G.Find(TEXT("loci"));
			if (!Loci || !Loci->IsObject()) return R.Fail(FReader::At(Where, TEXT("loci")), TEXT("objet attendu"));
			for (int32 L = 0; L < AnastasisGenome::NumLoci; ++L)
			{
				const TCHAR* Name = AnastasisGenome::LocusName(static_cast<AnastasisGenome::ELocus>(L));
				const FValue* Pair = Loci->Find(Name);
				const FString W = FReader::At(FReader::At(Where, TEXT("loci")), Name);
				if (!Pair || !Pair->IsArray() || Pair->Items.Num() != 2 || !Pair->Items[0].IsNumber() || !Pair->Items[1].IsNumber())
				{
					return R.Fail(W, TEXT("paire de nombres attendue"));
				}
				Out.Alleles[L][0] = Pair->Items[0].Number;
				Out.Alleles[L][1] = Pair->Items[1].Number;
			}
			return true;
		}

		/** `npc.phenotype` : les douze loci et les six derivees ; un champ absent garde son defaut. */
		bool ReadPhenotype(FReader& R, const FValue& P, const FString& Where, AnastasisGenome::FPhenotype& Out)
		{
			if (!P.IsObject()) return R.Fail(Where, TEXT("objet attendu"));
			for (int32 L = 0; L < AnastasisGenome::NumLoci; ++L)
			{
				if (!R.OptDouble(P, AnastasisGenome::LocusName(static_cast<AnastasisGenome::ELocus>(L)), Where, Out.Loci[L])) return false;
			}
			return R.OptDouble(P, TEXT("hydrationLossMultiplier"), Where, Out.HydrationLossMultiplier)
				&& R.OptDouble(P, TEXT("heatDissipationEfficiency"), Where, Out.HeatDissipationEfficiency)
				&& R.OptDouble(P, TEXT("metabolicDemandMultiplier"), Where, Out.MetabolicDemandMultiplier)
				&& R.OptDouble(P, TEXT("metabolicPeakRecoveryMultiplier"), Where, Out.MetabolicPeakRecoveryMultiplier)
				&& R.OptDouble(P, TEXT("fatigueRecoveryMultiplier"), Where, Out.FatigueRecoveryMultiplier)
				&& R.OptDouble(P, TEXT("fatigueRecoveryStrainCost"), Where, Out.FatigueRecoveryStrainCost);
		}

		/** `npc.conditioning` ; un champ absent garde le neutre de `ensureConditioning`. */
		bool ReadConditioning(FReader& R, const FValue& C, const FString& Where, AnastasisConditioning::FConditioning& Out)
		{
			if (!C.IsObject()) return R.Fail(Where, TEXT("objet attendu"));
			return R.OptInt(C, TEXT("version"), Where, Out.Version)
				&& R.OptDouble(C, TEXT("workConditioning"), Where, Out.WorkConditioning)
				&& R.OptDouble(C, TEXT("fatigueAdaptation"), Where, Out.FatigueAdaptation)
				&& R.OptDouble(C, TEXT("recoveryConditioning"), Where, Out.RecoveryConditioning);
		}

		/**
		 * `npc.lifestyle` : `{ id, sinceDay, rhythmScore, lastNotedDay }`, avec les `??=`
		 * d'`ensureLifestyle` (0, 0, 1). Un mode de vie absent ou inconnu, `deserialize` le TIRE
		 * dans `sim.rng` (`save.js`) : ce tirage a la lecture n'est pas reproduit, il est refuse.
		 */
		bool ReadLifestyle(FReader& R, const FValue& L, const FString& Where, AnastasisLifestyle::FLifestyle& Out)
		{
			if (!L.IsObject()) return R.Fail(Where, TEXT("objet attendu"));
			if (!R.String(L, TEXT("id"), Where, Out.Id)) return false;
			AnastasisLifestyle::ELifestyle Known;
			if (!AnastasisLifestyle::LifestyleFromId(Out.Id, Known))
			{
				return R.Fail(FReader::At(Where, TEXT("id")), TEXT("mode de vie inconnu : deserialize le retirerait dans sim.rng, non reproduit"));
			}
			return R.OptDouble(L, TEXT("sinceDay"), Where, Out.SinceDay)
				&& R.OptDouble(L, TEXT("rhythmScore"), Where, Out.RhythmScore)
				&& R.OptDouble(L, TEXT("lastNotedDay"), Where, Out.LastNotedDay);
		}

		bool ReadActor(FReader& R, const FValue& A, const FString& Where, uint32 WorldSeed, AnastasisVillage::FNpc& Out)
		{
			if (!A.IsObject()) return R.Fail(Where, TEXT("objet attendu"));
			if (!R.String(A, TEXT("id"), Where, Out.Id)) return false;
			if (!R.Double(A, TEXT("x"), Where, Out.X) || !R.Double(A, TEXT("y"), Where, Out.Y)) return false;
			if (!R.OptDouble(A, TEXT("speed"), Where, Out.Speed)) return false;
			AnastasisNeeds::FNeeds& N = Out.Needs;
			if (!R.OptDouble(A, TEXT("hunger"), Where, N.Hunger) || !R.OptDouble(A, TEXT("thirst"), Where, N.Thirst)
				|| !R.OptDouble(A, TEXT("energy"), Where, N.Energy) || !R.OptDouble(A, TEXT("social"), Where, N.Social)
				|| !R.OptDouble(A, TEXT("leisure"), Where, N.Leisure) || !R.OptDouble(A, TEXT("hygiene"), Where, N.Hygiene)
				|| !R.OptDouble(A, TEXT("health"), Where, N.Health) || !R.OptDouble(A, TEXT("morale"), Where, N.Morale))
			{
				return false;
			}
			if (!R.OptString(A, TEXT("goal"), Where, Out.Goal) || !R.OptString(A, TEXT("activity"), Where, Out.Activity)) return false;
			if (!R.OptDouble(A, TEXT("goalSince"), Where, Out.GoalSince) || !R.OptDouble(A, TEXT("workTimer"), Where, Out.WorkTimer)) return false;
			if (!R.OptString(A, TEXT("jobId"), Where, Out.JobId)) return false;
			if (!R.OptId(A, TEXT("workplaceId"), Where, Out.WorkplaceId) || !R.OptId(A, TEXT("homeId"), Where, Out.HomeId)
				|| !R.OptId(A, TEXT("shelterId"), Where, Out.ShelterId))
			{
				return false;
			}
			if (!R.OptDouble(A, TEXT("skill"), Where, Out.Skill)) return false;
			if (const FValue* Skills = A.Find(TEXT("skills")); Skills && Skills->IsObject())
			{
				const FString W = Where + TEXT(".skills");
				if (!R.OptDouble(*Skills, TEXT("gather"), W, Out.SkillGather) || !R.OptDouble(*Skills, TEXT("trade"), W, Out.SkillTrade)
					|| !R.OptDouble(*Skills, TEXT("craft"), W, Out.SkillCraft))
				{
					return false;
				}
			}
			if (const FValue* Inventory = A.Find(TEXT("inventory")); Inventory && Inventory->IsObject())
			{
				if (!R.OptInt(*Inventory, TEXT("food"), Where + TEXT(".inventory"), Out.InventoryFood)) return false;
			}
			if (const FValue* Target = A.Find(TEXT("target")); Target && !Target->IsNull())
			{
				if (!R.Point(*Target, FReader::At(Where, TEXT("target")), Out.Target)) return false;
				Out.bHasTarget = true;
			}
			if (!R.OptInt(A, TEXT("pathStep"), Where, Out.PathStep) || !R.OptDouble(A, TEXT("pathCooldown"), Where, Out.PathCooldown)) return false;
			if (!R.OptBool(A, TEXT("pathFailed"), Where, Out.bPathFailed) || !R.OptDouble(A, TEXT("stuckTimer"), Where, Out.StuckTimer)) return false;
			if (!R.OptDouble(A, TEXT("doorStuckAt"), Where, Out.DoorStuckAt) || !R.OptDouble(A, TEXT("doorApproachAt"), Where, Out.DoorApproachAt)) return false;
			if (!R.OptId(A, TEXT("talkWithId"), Where, Out.TalkWithId) || !R.OptDouble(A, TEXT("talkUntil"), Where, Out.TalkUntil)) return false;
			// Absents d'une sauvegarde au repos (< 0 et vide cote C++) ; la reference les ecrit au
			// premier tick ou l'habitant pense.
			if (!R.OptDouble(A, TEXT("aiThinkAt"), Where, Out.AiThinkAt) || !R.OptString(A, TEXT("villagePhase"), Where, Out.VillagePhase)) return false;
			if (A.Find(TEXT("_simBudgetAccum")))
			{
				if (!R.OptDouble(A, TEXT("_simBudgetAccum"), Where, Out.SimBudgetAccum)) return false;
				Out.bHasSimBudgetAccum = true;
			}
			// `deserialize` complete chaque habitant par `ensureGenome(actor, sim.seed)` puis
			// `ensureConditioning(actor)` : un phenotype manquant se derive du genome (cree s'il
			// manque, depuis la graine du monde et l'identifiant), un conditionnement manquant est
			// neutre. Seuls phenotype et conditioning sont gardes : ce sont eux que les besoins lisent.
			TOptional<AnastasisGenome::FGenome> Genome;
			if (const FValue* P = A.Find(TEXT("phenotype")); P && !P->IsNull())
			{
				if (!ReadPhenotype(R, *P, FReader::At(Where, TEXT("phenotype")), Out.Phenotype.Emplace())) return false;
			}
			else if (const FValue* G = A.Find(TEXT("genome")); G && !G->IsNull())
			{
				if (!ReadGenomeLoci(R, *G, FReader::At(Where, TEXT("genome")), Genome.Emplace())) return false;
			}
			AnastasisGenome::EnsureGenome(Genome, Out.Phenotype, WorldSeed, Out.Id);
			Out.Conditioning.Emplace();
			if (const FValue* C = A.Find(TEXT("conditioning")); C && !C->IsNull())
			{
				if (!ReadConditioning(R, *C, FReader::At(Where, TEXT("conditioning")), Out.Conditioning.GetValue())) return false;
			}
			const FValue* Lifestyle = A.Find(TEXT("lifestyle"));
			if (!Lifestyle || Lifestyle->IsNull())
			{
				return R.Fail(FReader::At(Where, TEXT("lifestyle")), TEXT("absent : deserialize le tirerait dans sim.rng, non reproduit"));
			}
			if (!ReadLifestyle(R, *Lifestyle, FReader::At(Where, TEXT("lifestyle")), Out.Lifestyle.Emplace())) return false;
			if (const FValue* Inside = A.Find(TEXT("inside")); Inside && !Inside->IsNull())
			{
				return R.Fail(FReader::At(Where, TEXT("inside")), TEXT("habitant a l'interieur : non lu par ce lecteur (inside non nul)"));
			}
			return true;
		}

		FValue ProjectActor(const AnastasisVillage::FNpc& N, const FValue* Base)
		{
			const bool bNew = Base == nullptr;
			FValue Out = Base ? *Base : FValue::MakeObject();
			Put(Out, TEXT("id"), Str(N.Id), bNew);
			Put(Out, TEXT("x"), Num(N.X), bNew);
			Put(Out, TEXT("y"), Num(N.Y), bNew);
			Put(Out, TEXT("speed"), Num(N.Speed), bNew);
			Put(Out, TEXT("hunger"), Num(N.Needs.Hunger), bNew);
			Put(Out, TEXT("thirst"), Num(N.Needs.Thirst), bNew);
			Put(Out, TEXT("energy"), Num(N.Needs.Energy), bNew);
			Put(Out, TEXT("social"), Num(N.Needs.Social), bNew);
			Put(Out, TEXT("leisure"), Num(N.Needs.Leisure), bNew);
			Put(Out, TEXT("hygiene"), Num(N.Needs.Hygiene), bNew);
			Put(Out, TEXT("health"), Num(N.Needs.Health), bNew);
			Put(Out, TEXT("morale"), Num(N.Needs.Morale), bNew);
			Put(Out, TEXT("goal"), Str(N.Goal), bNew);
			Put(Out, TEXT("activity"), Str(N.Activity), bNew);
			Put(Out, TEXT("goalSince"), Num(N.GoalSince), bNew);
			Put(Out, TEXT("workTimer"), Num(N.WorkTimer), bNew);
			Put(Out, TEXT("jobId"), Str(N.JobId), bNew);
			Put(Out, TEXT("workplaceId"), IdOrNull(N.WorkplaceId), bNew);
			Put(Out, TEXT("homeId"), IdOrNull(N.HomeId), bNew);
			Put(Out, TEXT("shelterId"), IdOrNull(N.ShelterId), bNew);
			Put(Out, TEXT("skill"), Num(N.Skill), bNew);
			if (FValue* Skills = Out.Find(TEXT("skills")); Skills && Skills->IsObject())
			{
				if (Skills->Find(TEXT("gather"))) Skills->Set(TEXT("gather"), Num(N.SkillGather));
				if (Skills->Find(TEXT("trade"))) Skills->Set(TEXT("trade"), Num(N.SkillTrade));
				if (Skills->Find(TEXT("craft"))) Skills->Set(TEXT("craft"), Num(N.SkillCraft));
			}
			if (FValue* Inventory = Out.Find(TEXT("inventory")); Inventory && Inventory->IsObject() && Inventory->Find(TEXT("food")))
			{
				Inventory->Set(TEXT("food"), Num(N.InventoryFood));
			}
			if (bNew || Out.Find(TEXT("target")))
			{
				if (N.bHasTarget)
				{
					const FValue* Old = Out.Find(TEXT("target"));
					FValue T = (Old && Old->IsObject()) ? *Old : FValue::MakeObject();
					T.Set(TEXT("x"), Num(N.Target.X));
					T.Set(TEXT("y"), Num(N.Target.Y));
					Out.Set(TEXT("target"), T);
				}
				else
				{
					Out.Set(TEXT("target"), FValue());
				}
			}
			Put(Out, TEXT("pathStep"), Num(N.PathStep), bNew);
			Put(Out, TEXT("pathCooldown"), Num(N.PathCooldown), bNew);
			Put(Out, TEXT("pathFailed"), FValue::MakeBool(N.bPathFailed), bNew);
			Put(Out, TEXT("stuckTimer"), Num(N.StuckTimer), bNew);
			Put(Out, TEXT("doorStuckAt"), Num(N.DoorStuckAt), bNew);
			Put(Out, TEXT("doorApproachAt"), Num(N.DoorApproachAt), bNew);
			Put(Out, TEXT("talkWithId"), IdOrNull(N.TalkWithId), bNew);
			Put(Out, TEXT("talkUntil"), Num(N.TalkUntil), bNew);
			// La cle apparait des que la cadence l'ecrit, qu'elle soit ou non dans la sauvegarde.
			if (N.bHasSimBudgetAccum)
			{
				Out.Set(TEXT("_simBudgetAccum"), Num(N.SimBudgetAccum));
			}
			// `aiThinkAt` et `villagePhase` apparaissent quand l'habitant pense : comme `_simBudgetAccum`,
			// la cle est ecrite des que le C++ la tient, presente ou non au depart.
			if (N.AiThinkAt >= 0.0) Out.Set(TEXT("aiThinkAt"), Num(N.AiThinkAt));
			if (!N.VillagePhase.IsEmpty()) Out.Set(TEXT("villagePhase"), Str(N.VillagePhase));
			// Le conditionnement avance a chaque tick ; le phenotype et le genome, jamais : ils
			// restent ceux du depart. `version` n'est pas reecrite par `tickConditioning`.
			if (N.Conditioning.IsSet() && (bNew || Out.Find(TEXT("conditioning"))))
			{
				const AnastasisConditioning::FConditioning& C = N.Conditioning.GetValue();
				const FValue* Old = Out.Find(TEXT("conditioning"));
				const bool bNewObject = !(Old && Old->IsObject());
				FValue Obj = bNewObject ? FValue::MakeObject() : *Old;
				if (bNewObject) Obj.Set(TEXT("version"), Num(C.Version));
				Obj.Set(TEXT("workConditioning"), Num(C.WorkConditioning));
				Obj.Set(TEXT("fatigueAdaptation"), Num(C.FatigueAdaptation));
				Obj.Set(TEXT("recoveryConditioning"), Num(C.RecoveryConditioning));
				Out.Set(TEXT("conditioning"), Obj);
			}
			// Le mode de vie : son score et le jour note bougent une fois par jour, l'identifiant jamais.
			if (N.Lifestyle.IsSet() && (bNew || Out.Find(TEXT("lifestyle"))))
			{
				const AnastasisLifestyle::FLifestyle& L = N.Lifestyle.GetValue();
				const FValue* Old = Out.Find(TEXT("lifestyle"));
				FValue Obj = (Old && Old->IsObject()) ? *Old : FValue::MakeObject();
				Obj.Set(TEXT("id"), Str(L.Id));
				Obj.Set(TEXT("sinceDay"), Num(L.SinceDay));
				Obj.Set(TEXT("rhythmScore"), Num(L.RhythmScore));
				Obj.Set(TEXT("lastNotedDay"), Num(L.LastNotedDay));
				Out.Set(TEXT("lifestyle"), Obj);
			}
			// `inside` non nul n'est pas lu (ReadActor refuse) : un habitant lu est dehors.
			Put(Out, TEXT("inside"), FValue(), bNew);
			return Out;
		}

		// --- Reservations de repas ------------------------------------------------
		// Lus: seq ; par reservation id, npcId, buildingId, source, amount,
		// createdAt, expiresAt, absoluteExpiresAt, renewals, lastProgressAt,
		// lastDistance. `resource` et `status` restent ceux du depart.

		bool ReadReservation(FReader& R, const FValue& V, const FString& Where, AnastasisVillage::FMealReservation& Out)
		{
			if (!V.IsObject()) return R.Fail(Where, TEXT("objet attendu"));
			return R.String(V, TEXT("id"), Where, Out.Id)
				&& R.String(V, TEXT("npcId"), Where, Out.NpcId)
				&& R.OptId(V, TEXT("buildingId"), Where, Out.BuildingId)
				&& R.OptString(V, TEXT("source"), Where, Out.Source)
				&& R.OptInt(V, TEXT("amount"), Where, Out.Amount)
				&& R.OptDouble(V, TEXT("createdAt"), Where, Out.CreatedAt)
				&& R.OptDouble(V, TEXT("expiresAt"), Where, Out.ExpiresAt)
				&& R.OptDouble(V, TEXT("absoluteExpiresAt"), Where, Out.AbsoluteExpiresAt)
				&& R.OptInt(V, TEXT("renewals"), Where, Out.Renewals)
				&& R.OptDouble(V, TEXT("lastProgressAt"), Where, Out.LastProgressAt)
				&& R.OptDouble(V, TEXT("lastDistance"), Where, Out.LastDistance);
		}

		FValue ProjectReservation(const AnastasisVillage::FMealReservation& M, const FValue* Base)
		{
			const bool bNew = Base == nullptr;
			FValue Out = Base ? *Base : FValue::MakeObject();
			Put(Out, TEXT("id"), Str(M.Id), bNew);
			Put(Out, TEXT("npcId"), Str(M.NpcId), bNew);
			Put(Out, TEXT("buildingId"), IdOrNull(M.BuildingId), bNew);
			Put(Out, TEXT("source"), Str(M.Source), bNew);
			Put(Out, TEXT("amount"), Num(M.Amount), bNew);
			Put(Out, TEXT("createdAt"), Num(M.CreatedAt), bNew);
			Put(Out, TEXT("expiresAt"), Num(M.ExpiresAt), bNew);
			Put(Out, TEXT("absoluteExpiresAt"), Num(M.AbsoluteExpiresAt), bNew);
			Put(Out, TEXT("renewals"), Num(M.Renewals), bNew);
			Put(Out, TEXT("lastProgressAt"), Num(M.LastProgressAt), bNew);
			Put(Out, TEXT("lastDistance"), Num(M.LastDistance), bNew);
			return Out;
		}

		/** L'element d'un tableau d'objets dont `id` vaut `Id`, ou nullptr. */
		const FValue* FindById(const FValue* Array, const FString& Id)
		{
			if (!Array || !Array->IsArray()) return nullptr;
			for (const FValue& Item : Array->Items)
			{
				const FValue* V = Item.Find(TEXT("id"));
				if (V && V->IsString() && V->String.Equals(Id, ESearchCase::CaseSensitive)) return &Item;
			}
			return nullptr;
		}
	}

	const TArray<FString>& PortedSections()
	{
		static const TArray<FString> Sections = {
			TEXT("seed"), TEXT("rng"), TEXT("w"), TEXT("h"), TEXT("time"), TEXT("day"),
			TEXT("tileDiff"), TEXT("buildings"), TEXT("actors"), TEXT("mealReservations"),
		};
		return Sections;
	}

	bool Read(const AnastasisJson::FValue& Save, FState& Out, FString& OutError)
	{
		FReader R;
		Out = FState();
		if (!Save.IsObject())
		{
			OutError = TEXT("save : objet attendu");
			return false;
		}
		const FString Root = TEXT("save");
		if (!R.Uint32(Save, TEXT("seed"), Root, Out.Seed) || !R.Uint32(Save, TEXT("rng"), Root, Out.RngState)
			|| !R.Int(Save, TEXT("w"), Root, Out.W) || !R.Int(Save, TEXT("h"), Root, Out.H)
			|| !R.Double(Save, TEXT("time"), Root, Out.Time) || !R.Int(Save, TEXT("day"), Root, Out.Day))
		{
			OutError = R.Error;
			return false;
		}
		if (Out.W <= 0 || Out.H <= 0)
		{
			OutError = FString::Printf(TEXT("save : carte %d x %d"), Out.W, Out.H);
			return false;
		}

		// Le monde: la generation de la graine, puis ce que la partie a change.
		Out.World = AnastasisWorld::GenerateWorld(Out.Seed, Out.W, Out.H);
		if (Out.World.Tiles.Num() != Out.W * Out.H)
		{
			OutError = TEXT("save : la generation n'a pas rendu w x h tuiles");
			return false;
		}
		Out.Pristine = MakeShared<const AnastasisWorld::FWorld>(Out.World);
		Out.TileExtras.SetNum(Out.World.Tiles.Num());
		const FValue* Diff = Save.Find(TEXT("tileDiff"));
		if (!Diff || !ApplyTileDiff(R, *Diff, Out))
		{
			OutError = Diff ? R.Error : TEXT("save.tileDiff : absent");
			return false;
		}

		const FValue* Buildings = Save.Find(TEXT("buildings"));
		if (!Buildings || !Buildings->IsArray())
		{
			OutError = TEXT("save.buildings : tableau attendu");
			return false;
		}
		for (int32 K = 0; K < Buildings->Items.Num(); ++K)
		{
			AnastasisVillage::FBuilding B;
			if (!ReadBuilding(R, Buildings->Items[K], FString::Printf(TEXT("buildings[%d]"), K), B))
			{
				OutError = R.Error;
				return false;
			}
			Out.Buildings.Add(MoveTemp(B));
		}

		const FValue* Actors = Save.Find(TEXT("actors"));
		if (!Actors || !Actors->IsArray())
		{
			OutError = TEXT("save.actors : tableau attendu");
			return false;
		}
		for (int32 K = 0; K < Actors->Items.Num(); ++K)
		{
			AnastasisVillage::FNpc N;
			if (!ReadActor(R, Actors->Items[K], FString::Printf(TEXT("actors[%d]"), K), Out.Seed, N))
			{
				OutError = R.Error;
				return false;
			}
			Out.Actors.Add(MoveTemp(N));
		}

		const FValue* Meals = Save.Find(TEXT("mealReservations"));
		if (!Meals || Meals->IsNull())
		{
			Out.Meals.bNull = true;
		}
		else
		{
			const FString W = TEXT("mealReservations");
			if (!Meals->IsObject() || !R.Int(*Meals, TEXT("seq"), W, Out.Meals.Seq))
			{
				OutError = R.Error.IsEmpty() ? W + TEXT(" : objet attendu") : R.Error;
				return false;
			}
			const FValue* List = Meals->Find(TEXT("reservations"));
			if (!List || !List->IsArray())
			{
				OutError = W + TEXT(".reservations : tableau attendu");
				return false;
			}
			for (int32 K = 0; K < List->Items.Num(); ++K)
			{
				AnastasisVillage::FMealReservation M;
				if (!ReadReservation(R, List->Items[K], FString::Printf(TEXT("mealReservations.reservations[%d]"), K), M))
				{
					OutError = R.Error;
					return false;
				}
				Out.Meals.Reservations.Add(MoveTemp(M));
			}
		}

		Out.Source = Save;
		return true;
	}

	bool Project(const FState& State, const FString& Section, AnastasisJson::FValue& Out, FString& OutError)
	{
		using AnastasisJson::FValue;
		if (Section == TEXT("seed")) { Out = FValue::MakeNumber(static_cast<double>(State.Seed)); return true; }
		if (Section == TEXT("rng")) { Out = FValue::MakeNumber(static_cast<double>(State.RngState)); return true; }
		if (Section == TEXT("w")) { Out = FValue::MakeNumber(static_cast<double>(State.W)); return true; }
		if (Section == TEXT("h")) { Out = FValue::MakeNumber(static_cast<double>(State.H)); return true; }
		if (Section == TEXT("time")) { Out = FValue::MakeNumber(State.Time); return true; }
		if (Section == TEXT("day")) { Out = FValue::MakeNumber(static_cast<double>(State.Day)); return true; }
		if (Section == TEXT("tileDiff")) { Out = ProjectTileDiff(State); return true; }
		if (Section == TEXT("buildings"))
		{
			const FValue* Source = State.Source.Find(TEXT("buildings"));
			Out = FValue::MakeArray();
			for (const AnastasisVillage::FBuilding& B : State.Buildings.GetItems())
			{
				Out.Items.Add(ProjectBuilding(B, FindById(Source, B.Id)));
			}
			return true;
		}
		if (Section == TEXT("actors"))
		{
			const FValue* Source = State.Source.Find(TEXT("actors"));
			Out = FValue::MakeArray();
			for (const AnastasisVillage::FNpc& N : State.Actors.GetItems())
			{
				Out.Items.Add(ProjectActor(N, FindById(Source, N.Id)));
			}
			return true;
		}
		if (Section == TEXT("mealReservations"))
		{
			if (State.Meals.bNull && State.Meals.Reservations.Num() == 0)
			{
				Out = FValue();
				return true;
			}
			const FValue* Source = State.Source.Find(TEXT("mealReservations"));
			Out = (Source && Source->IsObject()) ? *Source : FValue::MakeObject();
			Out.Set(TEXT("seq"), FValue::MakeNumber(static_cast<double>(State.Meals.Seq)));
			const FValue* OldList = Source ? Source->Find(TEXT("reservations")) : nullptr;
			FValue List = FValue::MakeArray();
			for (const AnastasisVillage::FMealReservation& M : State.Meals.Reservations)
			{
				List.Items.Add(ProjectReservation(M, FindById(OldList, M.Id)));
			}
			Out.Set(TEXT("reservations"), List);
			return true;
		}
		OutError = FString::Printf(TEXT("section `%s` : non projetee par ce lecteur"), *Section);
		return false;
	}
}
