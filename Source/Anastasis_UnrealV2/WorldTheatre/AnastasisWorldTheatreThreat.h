#pragma once

#include "CoreMinimal.h"
#include "WorldTheatre/AnastasisWorldTheatre.h"

namespace AnastasisGeo { class FGeoWorld; }

/**
 * WORLD_THEATRE v2.2 -- la menace, lue dans la simulation.
 *
 * Aucune menace n'est inventee ici (PONT-HIS-02). La seule source est le monde exterieur simule
 * (geopolitical-world-001, scenario geo-pontos-1204.json) : pressions d'insecurite (raids, enlevements, bandes) et
 * militaires (troupes, requisitions) qui voyagent de noeud en noeud, et nouvelles qui voyagent plus vite qu'elles.
 * Sans scenario charge (`Anastasis.Geo.Load`), la carte de menace est vide et aucun signe ne s'allume.
 *
 * Ce qui se voit, et pourquoi :
 *  - FUMEE, evenement physique : la pression vraie AU-DESSUS DE SA BASE chez un voisin (un jour de route), puis la
 *    pression en route vers le village, qui se rapproche (etape 1 a mi-chemin, 2 pres du village), puis l'exposition
 *    du village elle-meme. Un noeud a plus d'un jour ne se voit pas : son danger se montre en arrivant chez un voisin.
 *  - FEU DE SIGNAUX, information : une nouvelle de raid ou de troupes en route vers le village allume la chaine de
 *    collines du plus loin au plus pres, au fil de son trajet ; elle part plus vite que la pression, d'ou l'avance.
 *  - EFFROI : l'exposition du village (ce qui l'a atteint) assombrit davantage le lointain (lumiere v2.1).
 */
namespace AnastasisWorldTheatreThreat
{
/** Une pression (insecurite ou troupes) ou une nouvelle en route d'un voisin vers le village. */
struct FTransit
{
	FString FromNode;
	/** [0, 1] : ampleur de la pression, ou ampleur rapportee x fiabilite pour une nouvelle. */
	double Magnitude = 0.0;
	/** [0, 1] : part du trajet faite, en jours continus. */
	double Progress = 0.0;
};

/** La carte de menace : ce que le monde exterieur simule rend visible depuis le village, a cet instant. */
struct FThreatMap
{
	bool bLoaded = false;
	FString VillageNode;
	/** Jour continu de la simulation (1 + Time / DayLength). */
	double DayFloat = 0.0;
	/** Noeud -> pression vraie combinee (insecurite, troupes) au-dessus de sa base, [0, 1]. */
	TMap<FString, double> NodeExcess;
	/** Exposition du village au-dessus de sa base (insecurite, troupes combinees), [0, 1]. */
	double VillageExcess = 0.0;
	TArray<FTransit> PressureToVillage;
	TArray<FTransit> NewsToVillage;
};

/** Seuils et gains, dans [0, 1] : une valeur de pression qui allume un signe plein. */
struct FRules
{
	double SmokeFull = 0.35;
	double TransitFull = 0.3;
	double NewsFull = 0.3;
	double DreadFull = 0.4;
	/** Une nouvelle sous cette ampleur x fiabilite n'allume rien. */
	double NewsMin = 0.08;
	/** Ampleur minimale (insecurite ou troupes) d'un choc pour que ses nouvelles comptent comme menace. */
	double ThreatShockMin = 0.15;
};

/** Combine deux intensites independantes : 1 - (1 - A)(1 - B), comme le module geopolitique. */
inline double NoisyOr(double A, double B) { return 1.0 - (1.0 - FMath::Clamp(A, 0.0, 1.0)) * (1.0 - FMath::Clamp(B, 0.0, 1.0)); }

/** Lit le monde exterieur. Faux (carte vide) si aucun scenario n'est charge. */
bool ReadThreatMap(const AnastasisGeo::FGeoWorld& Geo, double DayFloat, FThreatMap& Out, const FRules& Rules = FRules());

struct FSignals
{
	/** Une intensite [0, 1] par site du plan, meme ordre. */
	TArray<float> Site;
	/** [0, 1] : effroi du village (assombrit le lointain). */
	float Dread = 0.0f;
};

/** Decide l'intensite de chaque site. Pur : meme carte, memes sites, meme resultat. */
FSignals Evaluate(const FThreatMap& Map, const TArray<AnastasisWorldTheatre::FThreatSite>& Sites, const FRules& Rules = FRules());

/**
 * Colonne de fumee : tube effile qui s'elargit et se couche avec le vent, Height de haut, sur l'origine. UV : U autour,
 * V le long (0 au pied, 1 au sommet). Alpha de sommet : densite (monte vite au pied, s'efface au sommet).
 */
AnastasisWorldTheatre::FMeshData BuildSmokeColumn(double Height, const FVector2D& LeanPerUnitHeight, double BaseRadius, double TopRadius);

/** Feu : trois plans croises (billboard sans camera), Size de haut, sur l'origine. UV : (0..1, 0..1). */
AnastasisWorldTheatre::FMeshData BuildFire(double Size);
}
