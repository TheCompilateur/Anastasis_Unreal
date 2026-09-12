using UnrealBuildTool;

/**
 * ANASTASIS Core — frontiere semantique portable, cote Unreal.
 *
 * Ce module est la traduction C++ de la couche INFRASTRUCTURE du simulateur
 * (ANASTASIS_WONDERLAND_PROBE/js/anastasis-world-contract.js,
 *  anastasis-infrastructure.js, anastasis-actor-kinematics.js,
 *  anastasis-time-control.js). Il ne traduit PAS `src/render3d/` : ce dossier
 * est specifique a Three.js et n'a rien a faire ici.
 *
 * LA COUPE DE DEPENDANCE EST APPLIQUEE PAR LE COMPILATEUR.
 * Ne pas ajouter "Engine", "RenderCore", "Slate", "UMG" ou un module de rendu
 * a cette liste. Le pendant JS de ce module n'importe ni Wonderland, ni
 * Three.js, ni le DOM (voir l'en-tete de anastasis-world-contract.js) ; c'est
 * la seule raison pour laquelle il a pu etre porte tel quel. Un `#include`
 * moteur ici reintroduirait exactement la contamination que la carte
 * d'extraction a passe un portage entier a retirer
 * (unreal/UNREAL_EXTRACTION_MAP_V1.md, section OBS).
 *
 * "Json" est present parce que le contrat monde est un format d'echange :
 * c'est par la que les instantanes produits par la source JS entrent, et par
 * la que les checkpoints C++ sortent pour la porte de parite [SCN].
 */
public class AnastasisCore : ModuleRules
{
	public AnastasisCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Json"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
