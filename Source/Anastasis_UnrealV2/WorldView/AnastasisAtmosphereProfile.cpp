#include "WorldView/AnastasisAtmosphereProfile.h"

UAnastasisAtmosphereProfile* UAnastasisAtmosphereProfile::CreateCodeDefaults(UObject* Outer)
{
	// Every value the UPROPERTY initialisers already carry IS the observe-slice.py rig plus
	// the new fog block, so the fallback is a plain default-constructed profile. Spelling the
	// rig values out a second time here would create two places to drift; the parity test
	// asserts the defaults directly instead.
	return NewObject<UAnastasisAtmosphereProfile>(Outer ? Outer : GetTransientPackage());
}
