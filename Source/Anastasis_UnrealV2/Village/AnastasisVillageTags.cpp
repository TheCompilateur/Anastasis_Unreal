#include "Village/AnastasisVillageTags.h"

namespace AnastasisVillageTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Activity, "Anastasis.Activity",
		"Racine des activites villageoises. Un slot Smart Object en porte au moins une.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Activity_Sleep, "Anastasis.Activity.Sleep",
		"Dormir. Un lit, une paillasse, un banc d'auberge.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Activity_Eat, "Anastasis.Activity.Eat",
		"Manger sur place. Une table, un foyer.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Activity_Drink, "Anastasis.Activity.Drink",
		"Boire sur place. Le bord d'un puits, une fontaine, un comptoir.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Activity_Work, "Anastasis.Activity.Work",
		"Travailler a un poste. Le metier lui-meme reste decide par la simulation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Activity_Socialize, "Anastasis.Activity.Socialize",
		"Se tenir la ou d'autres se tiennent.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Activity_Storage, "Anastasis.Activity.Storage",
		"Racine du stockage. Sert de filtre « n'importe quel acces a une reserve ».");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Activity_Storage_Deposit, "Anastasis.Activity.Storage.Deposit",
		"Deposer dans une reserve.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Activity_Storage_Take, "Anastasis.Activity.Storage.Take",
		"Prendre dans une reserve, y compris puiser de l'eau.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Building, "Anastasis.Building",
		"Racine des categories de lieu. Ne route jamais une intention : voir Activity.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Building_House, "Anastasis.Building.House",
		"Habitation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Building_Farm, "Anastasis.Building.Farm",
		"Exploitation agricole.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Building_Tavern, "Anastasis.Building.Tavern",
		"Auberge : boire, manger, dormir, se tenir ensemble.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Building_Well, "Anastasis.Building.Well",
		"Puits.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Building_Workshop, "Anastasis.Building.Workshop",
		"Atelier.");
}
