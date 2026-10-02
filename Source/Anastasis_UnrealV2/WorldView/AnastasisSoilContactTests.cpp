#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WorldView/AnastasisSoilContact.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisSoilContactSupport, "Anastasis.Terrain.SoilContact.Support",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisSoilContactSupport::RunTest(const FString&)
{
 namespace SC=AnastasisSoilContact;
 SC::FInputs In;
 In.Search=FBox2D(FVector2D(-800,-1200),FVector2D(1200,1200));
 In.Ground=[](double X,double Y,double& Z){Z=X<0?X*0.15:X*0.60;return true;};
 In.Water=[](double X,double Y,double& Z){Z=-60;return true;};
 const auto A=SC::Build(In),B=SC::Build(In);
 if(!TestTrue(TEXT("supported cliff foot found"),A.bValid)) return false;
 TestTrue(TEXT("bounded nonempty pilot"),A.Stones.Num()>0 && A.Stones.Num()<=162);
 TestEqual(TEXT("deterministic count"),A.Stones.Num(),B.Stones.Num());
 for(int32 I=0;I<A.Stones.Num();++I)
 {
  const auto& P=A.Stones[I];
  TestTrue(TEXT("stable location"),P.Foot.Equals(B.Stones[I].Foot,0.001));
  TestTrue(TEXT("local footprint"),FVector2D::Distance(FVector2D(P.Foot),FVector2D(A.Anchor))<=1650);
  double G;In.Ground(P.Foot.X,P.Foot.Y,G);
  TestTrue(TEXT("supported, never floating"),P.Foot.Z<=G+0.001);
  TestTrue(TEXT("large rocks stay out of water"),P.Kind==2 || G>=-60);
 }
 In.Water=[](double,double,double&){return false;};
 TestFalse(TEXT("no fake river when water unavailable"),SC::Build(In).bValid);
 In.Water=[](double,double,double& Z){Z=10;return true;};
 In.Ground=[](double,double,double& Z){Z=0;return true;};
 TestFalse(TEXT("submerged flat rejected"),SC::Build(In).bValid);
 In.Water=[](double,double,double& Z){Z=-100;return true;};
 TestFalse(TEXT("dry flat rejected"),SC::Build(In).bValid);
 return true;
}
#endif
