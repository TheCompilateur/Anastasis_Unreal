// Ontologie des interactions villageoises : un noyau, pas un catalogue.
//
// Deux axes seulement, et ils ne se melangent pas :
//
//   Anastasis.Activity.*   ce qu'un PNJ VIENT FAIRE a un endroit
//   Anastasis.Building.*   ce QU'EST cet endroit
//
// Un slot Smart Object porte une activite. Un batiment porte une categorie.
// La requete d'un PNJ interroge l'activite, jamais la categorie : demander
// « ou puis-je dormir » doit trouver le lit d'une auberge aussi bien que
// celui d'une maison. La categorie sert au debug, a la presentation et aux
// futurs filtres sociaux -- pas au routage de l'intention.
//
// CE QUE CES TAGS NE SONT PAS
//
// Ce ne sont pas des besoins. `Activity.Sleep` ne dit pas qu'un PNJ a sommeil ;
// il dit qu'un emplacement du monde sait accueillir l'acte de dormir. La
// decision -- POURQUOI ce PNJ veut dormir maintenant -- appartient a la
// simulation ANASTASIS et n'entre jamais dans cette enumeration. Ajouter ici
// un `Need.Sleep` serait deplacer l'autorite comportementale dans Unreal.
//
// Tags natifs, pas DataTable ni .ini : le village est genere, ces tags doivent
// exister avant tout chargement d'asset, et un depot multi-agent n'a pas besoin
// d'un `Config/DefaultGameplayTags.ini` que trois missions se disputent.

#pragma once

#include "NativeGameplayTags.h"

namespace AnastasisVillageTags
{
	// --- Activites : ce qu'un PNJ vient faire ---------------------------------

	/** Racine des activites. Utilisable comme filtre « n'importe quelle activite ». */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Activity);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Activity_Sleep);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Activity_Eat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Activity_Drink);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Activity_Work);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Activity_Socialize);

	/** Racine du stockage. Deposer et prendre restent distincts : une reserve pleine refuse l'un sans refuser l'autre. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Activity_Storage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Activity_Storage_Deposit);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Activity_Storage_Take);

	// --- Batiments : ce qu'est le lieu ----------------------------------------

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Building);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Building_House);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Building_Farm);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Building_Tavern);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Building_Well);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Building_Workshop);
}
