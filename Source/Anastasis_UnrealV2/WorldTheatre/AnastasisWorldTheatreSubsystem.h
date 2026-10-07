#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldTheatre/AnastasisWorldTheatre.h"
#include "AnastasisWorldTheatreSubsystem.generated.h"

class AActor;
class AAnastasisWorldEmbodiment;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UProceduralMeshComponent;

/**
 * WORLD_THEATRE_001 -- mise en scene geographique, cote monde.
 *
 * Lit l'incarnation SANS la modifier (AnastasisWorldReading), puis drape le plan versionne
 * (AnastasisWorldTheatre::CanonicalPlan) sur le sol reellement rendu. Aucun systeme d'un autre proprietaire
 * (sol, eau, vegetation, villages, lumiere) n'est touche : tout ce que la couche pose vit dans UN acteur
 * transitoire qui lui appartient (couche d'editeur DL_WORLD_THEATRE), sans collision ni navigation.
 *
 * `anastasis.Theatre 0` retire la couche en entier (A/B propre, a chaud) ; 1 la rebatit.
 * Le niveau n'est pas World Partition et le monde est transitoire : la Data Layer native ne s'applique pas,
 * la CVar en tient lieu (meme role : une couche qu'on allume ou coupe d'un geste).
 *
 * v2.1, la lumiere (`anastasis.Theatre.Light 0/1`, independante des masses) : un post-traitement non borne pose sur
 * le meme acteur assombrit, desature et refroidit ce qui est loin (M_WorldTheatreDistance) ; le village reste la
 * poche de lumiere. `anastasis.Theatre.DepthProbe 1` ecrit la profondeur en gris (instrument de mesure).
 *
 * v2.2, la menace (`anastasis.Theatre.Threat 0/1`) : fumees et feux de signaux places par l'analyse, allumes par le
 * monde exterieur simule (AnastasisWorldTheatreThreat) ; l'effroi du village assombrit davantage le lointain.
 *
 * Commandes : `anastasis.Theatre.Read <dossier>` (releve pour l'analyse), `anastasis.Theatre.Rebuild`,
 * `anastasis.Theatre.Status`.
 */
UCLASS()
class UAnastasisWorldTheatreSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Releve le monde rendu et l'ecrit dans Dir. Faux = rien a lire (journalise pourquoi). */
	bool ReadAndDump(const FString& Dir);
	/** Rebatit la couche depuis l'incarnation courante. Faux = rien pose (journalise pourquoi). */
	bool Rebuild();
	/** Retire tout ce que la couche a pose. */
	void Clear();
	void LogStatus() const;

	const AnastasisWorldTheatre::FBuildReport& GetReport() const { return LastReport; }
	bool IsBuilt() const { return bBuilt; }

private:
	AAnastasisWorldEmbodiment* FindEmbodiment() const;
	uint64 EmbodimentSignature(const AAnastasisWorldEmbodiment& Embodiment) const;
	AActor* EnsureActor();
	UMaterialInterface* ResolveMaterial();
	/** v2.1 : pose ou retire le post-traitement de distance et la sonde, et y recopie les CVars. */
	void UpdateLight();
	/** v2.2 : lit la carte de menace et allume, eteint, dose chaque signe. */
	void UpdateThreat();
	bool BuildThreatSigns();
	float ReadSunElevationDeg() const;

public:
	/** Journalise la carte de menace et chaque signe (preuve PIE : lignes WORLD_THREAT). */
	void LogThreat() const;
	/** Meme etat en JSON, lu par la preuve PIE (world-theatre-threat-pie.py). */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|WorldTheatre")
	FString GetThreatStatusJson() const;

private:

	UPROPERTY(Transient)
	TObjectPtr<AActor> HostActor;
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> Material;
	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> LightVolume;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DistanceMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ProbeMaterial;
	int32 LoggedLight = -1;

	/** Un composant par site de menace (meme ordre que CanonicalPlan().ThreatSites). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> ThreatMeshes;
	/** Section 0 de chaque site (fumee ou feu), section 1 des feux (fumee claire du jour). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ThreatMain;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ThreatDay;
	TArray<float> ThreatIntensity;
	float Dread = 0.0f;
	float SunElevationDeg = 45.0f;
	bool bThreatLoaded = false;
	double ThreatDay_ = 0.0;
	double ThreatVillageExcess = 0.0;
	int32 LoggedThreat = -1;

	AnastasisWorldTheatre::FBuildReport LastReport;
	bool bBuilt = false;
	uint64 BuiltSignature = 0;
	uint64 PendingSignature = 0;
	int32 PendingPolls = 0;
	int32 BuiltEnabled = -1;
	float PollClock = 0.0f;
};

/** Acces Python / Blueprint au theatre (le sous-systeme de monde n'est pas joignable directement depuis Python ici). */
UCLASS()
class UAnastasisWorldTheatreLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Etat de la carte de menace et des signes, en JSON (vide si pas de theatre dans ce monde). */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|WorldTheatre", meta = (WorldContext = "WorldContextObject"))
	static FString GetThreatStatus(const UObject* WorldContextObject);
};
