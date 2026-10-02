#pragma once
#include "CoreMinimal.h"
class AActor;
namespace AnastasisSoilContact
{
struct FInputs
{
 TFunction<bool(double,double,double&)> Ground;
 TFunction<bool(double,double,double&)> Water;
 FBox2D Search = FBox2D(FVector2D(27000,12000),FVector2D(46000,28000));
 int32 Seed=12345;
};
struct FStone { FVector Foot; double Diameter=0; double Yaw=0; double Sink=0; int32 Kind=0; int32 Variant=0; };
struct FPlan { FVector Anchor=FVector::ZeroVector; FVector2D Uphill=FVector2D::ZeroVector; TArray<FStone> Stones; bool bValid=false; };
// Presentation-only pilot: rendered slope break with actual submerged ground nearby.
FPlan Build(const FInputs& In);
void Apply(AActor& Owner,int32 Seed,double SpatialScale);
}
