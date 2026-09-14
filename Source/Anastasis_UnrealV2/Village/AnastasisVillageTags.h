#pragma once

#include "NativeGameplayTags.h"

/**
 * Ontologie village — incarnation spatiale, pas des regles de sim.
 * La sim JS reste l'autorite (besoins, metiers, eco). Unreal expose des slots.
 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Anastasis_Activity_Sleep);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Anastasis_Activity_Eat);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Anastasis_Activity_Drink);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Anastasis_Activity_Work);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Anastasis_Activity_Socialize);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Anastasis_Activity_Storage_Deposit);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Anastasis_Activity_Storage_Take);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Anastasis_Building_House);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Anastasis_Building_Farm);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Anastasis_Building_Tavern);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Anastasis_Building_Well);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Anastasis_Building_Workshop);
