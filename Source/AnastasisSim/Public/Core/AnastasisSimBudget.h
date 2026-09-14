// Directeur de budget de simulation — noyau causal de `src/sim/simulationBudget.js`.
//
// --- CE QUI EST PORTE, ET CE QUI NE L'EST PAS -------------------------------
//
// C'est la reference qui trace la ligne, pas moi. Son en-tete porte une loi
// datee du 10/08/2026:
//
//   « LA MACHINE PEUT CHANGER LA VITESSE A LAQUELLE LE MONDE EST CALCULE.
//     ELLE NE DOIT PAS CHANGER LE MONDE QUI EST CALCULE. »
//
// Elle vient d'un bug mesure: `budget.pressure` etait une moyenne mobile du
// temps mur, et la pression decide QUELS PNJ pensent et A QUELLE FREQUENCE.
// Resultat, meme graine et meme nombre de ticks: population 6 contre 5, tresor
// 328 contre 321. Le temps que met la machine entrait dans le monde.
//
// Depuis, `pressure` n'accepte qu'une source deterministe. Tout ce qui touche
// au chronometre — la moyenne mobile, `noteSimulationBudgetFrame`,
// `frameWallMs`, `simMs`, `observedTier`, le HUD — est declare observation
// seule. Ces fonctions ne decident plus rien, donc elles n'ont rien a faire
// dans un module dont le contrat est le determinisme: leur place est dans la
// couche de presentation, et elles ne sont pas ici.
//
// Ce qui reste est exactement ce qui decide: le palier de pression, les
// multiplicateurs de budget, la bande de cadence d'un PNJ, son intervalle.
//
// --- LE PIEGE ---------------------------------------------------------------
//
// `ClassifyNpcBand` passe par `Math.hypot`. V8 l'implemente par mise a
// l'echelle sur le max puis sommation compensee de Kahan; un
// `sqrt(dx*dx+dy*dy)` rend un resultat voisin mais different, et au bord d'un
// rayon cela fait changer un PNJ de bande — donc de frequence de pensee.
// D'ou `AnastasisMath::JsHypot`, jamais `FMath::Sqrt`.

#pragma once

#include "CoreMinimal.h"

namespace AnastasisBudget
{
	/** Palier lisible d'une pression deterministe. */
	enum class ETier : uint8
	{
		Normal = 0,
		Lean = 1,
		Severe = 2,
	};

	/** Bande de cadence d'un habitant, du plus simule au moins simule. */
	enum class EBand : uint8
	{
		Near = 0,
		Medium = 1,
		Far = 2,
		Invisible = 3,
	};

	/** Cadences et rayons — `NPC_SIMULATION_CADENCE`. */
	namespace Cadence
	{
		inline constexpr double NearHz = 60.0;
		inline constexpr double MediumHz = 10.0;
		inline constexpr double FarHz = 1.0;
		inline constexpr double InvisibleHz = 0.25;
		inline constexpr double NearRadius = 18.0;
		inline constexpr double MediumRadius = 42.0;
		inline constexpr double FarRadius = 84.0;
	}

	/** Seuils de pression. Testes de part et d'autre, pas seulement au milieu. */
	inline constexpr double LeanPressure = 0.38;
	inline constexpr double SeverePressure = 0.72;

	/**
	 * L'etat du directeur, reduit a ce qui decide.
	 *
	 * `Pressure` ne doit venir que de l'ETAT DE SIMULATION — population,
	 * profondeur de file, nombre d'entites. Jamais d'un chronometre. C'est la
	 * loi ci-dessus, et elle a deja coute un bug.
	 *
	 * La vue est une commande JOUEUR (ou une vue epinglee sur un banc), donc
	 * reproductible: elle peut legitimement decider quels PNJ sont proches.
	 */
	struct FDirector
	{
		double Pressure = 0.0;
		double ViewX = 0.0;
		double ViewY = 0.0;
		bool bViewPinned = false;
	};

	/** Le strict necessaire pour classer un habitant. */
	struct FNpcView
	{
		double X = 0.0;
		double Y = 0.0;
		/** A l'interieur d'un batiment: toujours `Medium`, ou qu'il soit. */
		bool bInside = false;
	};

	struct FMultipliers
	{
		double PathBudgetMul = 1.0;
		double AnimalTickMul = 1.0;
		double TransportTickMul = 1.0;
	};

	/** Resultat d'une consommation de cadence — `consumeNpcSimulationCadence`. */
	struct FCadenceStep
	{
		bool bRun = true;
		double Dt = 0.0;
		EBand Band = EBand::Near;
		bool bSymbolic = false;
	};

	ANASTASISSIM_API ETier PressureTier(double Pressure);
	ANASTASISSIM_API FMultipliers Multipliers(double Pressure);
	ANASTASISSIM_API EBand ClassifyNpcBand(const FDirector& Director, const FNpcView& Npc);
	ANASTASISSIM_API double IntervalForBand(double Pressure, EBand Band);

	/**
	 * `consumeNpcSimulationCadence`. `Accumulator` est le `_simBudgetAccum` du
	 * JS, porte par l'habitant; il est lu ET ecrit.
	 */
	ANASTASISSIM_API FCadenceStep ConsumeCadence(
		const FDirector& Director,
		const FNpcView& Npc,
		double Dt,
		double& Accumulator,
		bool bCritical = false);

	/** Noms de la reference — ce sont eux qui traversent une sauvegarde. */
	ANASTASISSIM_API const TCHAR* TierName(ETier Tier);
	ANASTASISSIM_API const TCHAR* BandName(EBand Band);

	/**
	 * Une bande inconnue devient `Invisible`.
	 *
	 * Ce n'est pas de la tolerance: le JS ecrit trois `if` puis un `return`, si
	 * bien que toute valeur non reconnue tombe dans la branche invisible. Le
	 * type C++ ferme le cas, la fonction le rouvre a l'identique pour que le
	 * comportement soit le meme au chargement d'une sauvegarde ancienne.
	 */
	ANASTASISSIM_API EBand BandFromName(const FString& Name);
}
