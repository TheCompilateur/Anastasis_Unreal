#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "Village/AnastasisVillageFabric.h"
#include "AnastasisVillageFabricSubsystem.generated.h"

class AAnastasisVillageFabric;

/**
 * VILLAGE_FABRIC_001 -- tient le tissu du village a jour. Lit les batiments de la simulation (case,
 * type, acces) ; quand l'ensemble change, refait la grammaire sur le relief rendu et remplace la
 * geometrie. Ne decide rien pour la simulation, ne touche ni aux acteurs batiments ni a leur Sync.
 *
 * `anastasis.Village.Fabric` 0 = pas de tissu (herbe rendue), 1 = tissu (defaut).
 * `anastasis.Village.FabricClear` 0 = l'herbe reste sous la calade (A/B), 1 = defrichee (defaut).
 * `Anastasis.Village.FabricReport` : ligne ANASTASIS_FABRIC + JSON au journal.
 */
UCLASS()
class UAnastasisVillageFabricSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	/** Etat en JSON pour les preuves : rapport de la grammaire, acteur, defrichement, cle. */
	FString StatusJson() const;

	/** Force une reconstruction au prochain Tick (meme village). */
	void Invalidate() { LastKey = 0; }

private:
	void Rebuild(uint32 Key);
	void TearDown();

	TWeakObjectPtr<AAnastasisVillageFabric> Actor;
	AnastasisVillageFabric::FFabric Last;
	uint32 LastKey = 0;
	int32 Rebuilds = 0;
	int32 ClearedLast = 0;
	double BuildMs = 0.0;
	float Wait = 0.0f;
};

/** Lecteur pour les scripts de preuve Python (un sous-systeme de monde n'est pas atteignable depuis Python). */
UCLASS()
class UAnastasisVillageFabricLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** JSON du tissu courant, `{}` sans sous-systeme. Lecture seule. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetVillageFabricStatus(const UObject* WorldContextObject);
};
