using UnrealBuildTool;

/// Socle de simulation Anastasis, porte depuis le simulateur JS (src/sim, src/runtime).
///
/// Regle du module: AUCUNE dependance rendu, aucun Engine. Le determinisme est le
/// contrat — meme seed => meme monde => meme deroulement — et il doit tenir en
/// headless (tests d'automation, serveur, rejeu de bug) exactement comme en jeu.
/// Tout ce qui touche a l'affichage vit dans Anastasis_UnrealV2, pas ici.
///
/// Le portage reproduit la semantique numerique de JavaScript (doubles IEEE754,
/// Math.imul en 32 bits, ToUint32). Ne pas activer de fast-math sur ce module:
/// la reassociation flottante casse l'equivalence bit a bit avec la reference JS.
public class AnastasisSim : ModuleRules
{
	public AnastasisSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
