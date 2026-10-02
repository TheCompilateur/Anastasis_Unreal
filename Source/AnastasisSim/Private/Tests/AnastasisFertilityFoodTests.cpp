#include "Misc/AutomationTest.h"
#include "Life/AnastasisNeeds.h"
#include "Sim/AnastasisSimulation.h"
#include "Work/AnastasisFields.h"
#include "Work/AnastasisGather.h"
#include "World/AnastasisPathfinding.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace AnastasisFertilityFoodTest
{
 using namespace AnastasisVillage;
 constexpr double Dt = 1.0 / 60.0;
 constexpr int32 Days = 12;
 constexpr int32 Ticks = Days * 90 * 60;
 constexpr int32 FieldX = 8, FieldY = 11;

 AnastasisWorld::FWorld World(double Fertility)
 {
  AnastasisWorld::FWorld W;
  W.W = W.H = 32;
  W.Tiles.SetNum(32 * 32);
  for (int32 Y = 0; Y < 32; ++Y) for (int32 X = 0; X < 32; ++X)
  {
   auto& T = W.Tiles[Y * 32 + X];
   T.X = X; T.Y = Y; T.Type = AnastasisWorld::ETileType::Grass;
   T.Alt = 0.5; T.Wetness = 0.3;
  }
  auto& T = W.Tiles[FieldY * 32 + FieldX];
  T.Type = AnastasisWorld::ETileType::Field;
  T.Resource = AnastasisWorld::EResource::Food;
  T.Amount = 4; T.CropId = AnastasisWorld::ECropId::Grain;
  T.Fertility = Fertility;
  return W;
 }

 struct FSample
 {
  int64 Regrown = 0;
  int32 Field = 0, Inventory = 0, Stored = 0, Gathered = 0, Delivered = 0, Meals = 0;
  int64 Ledger() const { return Field + Inventory + Stored + Meals - Regrown; }
 };
 FSample Sample(const FVillage& V)
 {
  FSample S;
  const auto T = V.LiveTileAt(FieldX, FieldY);
  S.Field = T.Resource == AnastasisWorld::EResource::Food ? T.Amount : 0;
  S.Regrown = V.GetRegrownFood();
  for (const auto& N : V.GetActors())
  {
   S.Inventory += N.InventoryFood; S.Gathered += N.GatheredFood;
   S.Delivered += N.DeliveredFood; S.Meals += N.MealsTaken;
  }
  for (const auto& B : V.GetBuildings()) S.Stored += B.FoodPhysical;
  return S;
 }
 struct FResult
 {
  FSample Last;
  int64 EmptyTicks = 0, FullTicks = 0, HungryPersonTicks = 0, CriticalFarmerTicks = 0;
  uint64 Digest = 0;
  TArray<FString> Rows;
  bool bValid = false;
 };

 FResult Run(FAutomationTestBase& Test, double Fertility, const TCHAR* Arm)
 {
  FResult R;
  FAnastasisSimulation Sim;
  Sim.ResetFromWorld(12345u, World(Fertility), 90.0 * 0.42, 1);
  auto& V = Sim.GetVillage();
  const FString Granary = V.AddBuilding(GranaryType, 16, 11);
  const FString Well = V.AddBuilding(WellType, 16, 18);
  const FString House = V.AddBuilding(HouseType, 23, 11);
  if (!Test.TestTrue(TEXT("fixture: three buildings"), !Granary.IsEmpty() && !Well.IsEmpty() && !House.IsEmpty())) return R;
  const auto* Depot = V.FindBuilding(Granary);
  if (!Test.TestTrue(TEXT("fixture: granary door"), Depot && Depot->AccessPoints.Num() > 0)) return R;
  const FPoint Door = Depot->AccessPoints[0];
  const AnastasisPath::FWorldNavSource Nav(V.GetNavGrid(), Sim.GetWorld());
  TArray<FPoint> Path;
  if (!Test.TestTrue(TEXT("fixture: field reachable"), AnastasisPath::FindPath(Nav, Door, {FieldX + 0.5, FieldY + 0.5}, {}, Path))) return R;
  FString Farmer;
  for (int32 K = 0; K < 3; ++K)
  {
   AnastasisNeeds::FNeeds N;
   N.Hunger = 20.0 + K * 10.0; N.Thirst = 10.0; N.Energy = 85.0;
   N.Social = N.Leisure = N.Hygiene = 80.0; N.Health = 95.0; N.Morale = 60.0;
   const FString Id = V.SpawnNpc(Door.X, Door.Y, N);
   if (!Test.TestTrue(TEXT("fixture: actor spawned"), !Id.IsEmpty())) return R;
   if (K == 0) { Farmer = Id; if (!Test.TestTrue(TEXT("fixture: farmer assigned"), V.AssignWorkplace(Id, AnastasisGather::JobFarmer, Granary))) return R; }
  }
  const int64 InitialLedger = Sample(V).Ledger();
  int32 PreviousDay = Sim.GetDay();
  for (int32 I = 0; I < Ticks; ++I)
  {
   Sim.Tick(Dt);
   R.Last = Sample(V);
   if (R.Last.Ledger() != InitialLedger || R.Last.Field < 0 || R.Last.Field > AnastasisFields::FieldFoodCap)
   {
    Test.AddError(FString::Printf(TEXT("FERTILITY_AB_INVALID arm=%s tick=%d ledger=%lld expected=%lld field=%d"), Arm, I, R.Last.Ledger(), InitialLedger, R.Last.Field));
    return R;
   }
   R.EmptyTicks += R.Last.Field == 0;
   R.FullTicks += R.Last.Field == AnastasisFields::FieldFoodCap;
   for (const auto& N : V.GetActors())
   {
    R.HungryPersonTicks += N.Needs.Hunger >= AnastasisNeeds::Constants::HungerCritical;
    if (N.Id == Farmer) R.CriticalFarmerTicks += NeedsCritical(N.Needs);
   }
   if (Sim.GetDay() != PreviousDay || I == Ticks - 1)
   {
    const auto& S = R.Last;
    // Cumulative measurements; day is the current day, including its midnight regeneration.
    const FString Row = FString::Printf(TEXT("tick=%d day=%d regrown=%lld field=%d inventory=%d stored=%d gathered=%d delivered=%d meals=%d empty_ticks=%lld full_ticks=%lld hungry_person_ticks=%lld critical_farmer_ticks=%lld"),
     I + 1, Sim.GetDay(), S.Regrown, S.Field, S.Inventory, S.Stored, S.Gathered, S.Delivered, S.Meals,
     R.EmptyTicks, R.FullTicks, R.HungryPersonTicks, R.CriticalFarmerTicks);
    R.Rows.Add(Row);
    Test.AddInfo(FString::Printf(TEXT("FERTILITY_AB_SAMPLE arm=%s fertility=%.2f %s"), Arm, Fertility, *Row));
    PreviousDay = Sim.GetDay();
   }
  }
  R.Digest = V.Digest();
  R.bValid = true;
  return R;
 }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisFertilityFoodABTest,
 "Anastasis.Sim.Village.Fertilite.AlimentationAB",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisFertilityFoodABTest::RunTest(const FString&)
{
 using namespace AnastasisFertilityFoodTest;
 AddInfo(TEXT("FERTILITY_AB_PROTOCOL v1 seed=12345 dt=1/60 duration_days=12 start=37.8 fields=1 initial_food=4 npcs=3 farmers=1 low=0.75 high=1.30; simulation-only, no scene/player claim"));
 const FResult A = Run(*this, 0.75, TEXT("A"));
 const FResult B = Run(*this, 1.30, TEXT("B"));
 const FResult A2 = Run(*this, 0.75, TEXT("A2"));
 if (!A.bValid || !B.bValid || !A2.bValid) return false;
 const bool bRepeat = A.Digest == A2.Digest && A.Rows == A2.Rows;
 if (!TestTrue(TEXT("A/A2 exact trajectory summaries and final digest"), bRepeat)) return false;
 const bool bReachable = A.Last.Gathered > 0 && B.Last.Gathered > 0 && A.Last.Delivered > 0 && B.Last.Delivered > 0 && A.Last.Meals > 0 && B.Last.Meals > 0;
 const bool bSupplyContrast = B.Last.Regrown > A.Last.Regrown;
 const bool bDeliveryContrast = B.Last.Delivered > A.Last.Delivered;
 const bool bMealContrast = B.Last.Meals > A.Last.Meals;
 // Instrument validity is separate from economic benefit. A null/negative result stays visible.
 const TCHAR* Verdict = !bReachable ? TEXT("UNKNOWN_CHAIN_NOT_REACHED")
  : !bSupplyContrast ? TEXT("NO_POSITIVE_REGROWTH_CONTRAST")
  : !bDeliveryContrast ? TEXT("REGROWTH_WITHOUT_MORE_DELIVERY")
  : !bMealContrast ? TEXT("DELIVERY_WITHOUT_MORE_MEALS")
  : TEXT("MORE_REGROWTH_DELIVERY_AND_MEALS");
 AddInfo(FString::Printf(TEXT("FERTILITY_AB_RESULT instrument=VALID effect=%s delta_regrown=%lld delta_delivered=%d delta_meals=%d delta_hungry_person_ticks=%lld; scope=one_fixture_12_days"),
  Verdict, B.Last.Regrown - A.Last.Regrown, B.Last.Delivered - A.Last.Delivered, B.Last.Meals - A.Last.Meals, B.HungryPersonTicks - A.HungryPersonTicks));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisFertilityMaskTest,
 "Anastasis.Sim.Village.Fertilite.PlafondTemoin",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisFertilityMaskTest::RunTest(const FString&)
{
 using namespace AnastasisFertilityFoodTest;
 // Local positive control and saturated negative control. Never substitute for village results.
 for (const int32 Stock : {1, 36, 37})
 {
  auto WA = World(0.75), WB = World(1.30);
  auto& A = WA.Tiles[FieldY * 32 + FieldX];
  auto& B = WB.Tiles[FieldY * 32 + FieldX];
  A.Amount = B.Amount = Stock;
  AnastasisFields::RegrowFieldTile(A, 5, 1);
  AnastasisFields::RegrowFieldTile(B, 5, 1);
  TestEqual(TEXT("low control"), A.Amount - Stock, Stock == 1 ? 4 : 37 - Stock);
  TestEqual(TEXT("high control"), B.Amount - Stock, Stock == 1 ? 7 : 37 - Stock);
 }
 return true;
}
#endif
