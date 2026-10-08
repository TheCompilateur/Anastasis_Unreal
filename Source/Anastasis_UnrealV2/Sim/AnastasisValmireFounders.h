#pragma once

// familles-feu-001 -- les fondateurs de Valmire (ecart n°44) : quatre familles, une par matrice d'origine
// de la Bible, et le moine Arsenios, qui pose les trois questions au feu le premier soir.
//
// L'autorite est Content/Anastasis/Scenario/valmire-fondateurs.json, valide par Alexandre le 2026-10-08.
// Ce module le lit, pose les fondateurs dans la simulation (identite, foyers) autour du premier puits, sur
// des cases d'ou le puits est atteignable, et compose la scene du feu. Le passe de chacun reste ici, dans
// l'hote : la simulation ne connait que les noms, le sexe, l'age et les foyers.

#include "CoreMinimal.h"

class FAnastasisSimulation;

namespace AnastasisDialogue
{
	class FLibrary;
}

namespace AnastasisFounders
{
	struct FMember
	{
		FString Key;
		FString Given;
		FString Byname;
		bool bFemale = false;
		double Age = 0.0;
		/** chef, epouse, fils, fille, frere, pupille, engage ; vide pour le moine. */
		FString Link;
	};

	/** Une reponse possible a une question du feu : de quoi on parle (l'objet, l'absent) et ce qu'on en dit. */
	struct FAnswer
	{
		FString Subject;
		FString Text;
	};

	struct FFamilyDef
	{
		FString Key;
		FString Name;
		FString Matrix;
		FString Respondent;
		FString Presentation;
		TArray<FMember> Members;
		TArray<FString> Where;
		TArray<FAnswer> Carried;
		TArray<FAnswer> Missing;
	};

	struct ANASTASIS_UNREALV2_API FScenario
	{
		FMember Monk;
		TArray<FFamilyDef> Families;
		/** Cles des personnes dans l'ordre de pose : il donne les metiers d'ouverture. */
		TArray<FString> PoseOrder;

		bool Parse(const FString& Json, FString& OutError);
		int32 PeopleCount() const;
		/** La personne de cette cle, et sa famille (INDEX_NONE pour le moine). */
		const FMember* FindMember(const FString& Key, int32& OutFamily) const;

		/** Le scenario livre, lu une fois. Nul si le fichier manque ou ne se lit pas (un avertissement au log). */
		static const FScenario* Get();
	};

	/** Un fondateur pose : son habitant dans la simulation, sa cle, sa famille (INDEX_NONE pour le moine). */
	struct FFounder
	{
		FString NpcId;
		FString Key;
		int32 Family = INDEX_NONE;
		FString FamilyId;
	};

	/**
	 * Pose les fondateurs autour du puits `WellId`, dans l'ordre de pose, chaque famille groupee de son cote,
	 * sur une case libre d'ou un chemin mene au puits : identite (`SetIdentity`) et foyers (`AddFamily`,
	 * `JoinFamily`). Le moine se tient pres du puits. Rend les fondateurs poses, dans l'ordre.
	 */
	ANASTASIS_UNREALV2_API TArray<FFounder> Seed(FAnastasisSimulation& Sim, const FString& WellId, const FScenario& Scenario);

	/** Quelle variante d'une reponse cette partie raconte : la meme graine, la meme histoire. */
	ANASTASIS_UNREALV2_API int32 Variant(uint32 Seed, const FString& FamilyKey, const TCHAR* Question, int32 Count);

	/** Une replique de la scene : qui parle (identifiant d'habitant) et ce qu'il dit. */
	struct FLine
	{
		FString SpeakerId;
		FString Text;
	};

	/** Le premier soir au feu : le moine ouvre, interroge chaque famille, chacune repond, il benit. */
	ANASTASIS_UNREALV2_API TArray<FLine> FireScene(const FScenario& Scenario, const TArray<FFounder>& Founders, uint32 Seed,
		const AnastasisDialogue::FLibrary& Lines);

	/** Deux phrases par famille : qui la compose, et ce qu'elle est. */
	ANASTASIS_UNREALV2_API FString FamilyIntro(const FFamilyDef& Family);

	/** « Konstantinos le Scribe » : le prenom et le surnom s'il y en a un. */
	ANASTASIS_UNREALV2_API FString FullName(const FMember& Member);
}
