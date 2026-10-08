#pragma once

#include "CoreMinimal.h"
#include "WorldView/AnastasisPresentationRegistry.h"

/**
 * Which portrait a simulated villager wears (VILLAGER_PNG_001).
 *
 * PRESENTATION ONLY. The simulation decides nothing here and is told nothing: the choice is
 * a pure function of the villager's identifier (`npc-N`) and of the registry. It consumes no
 * simulation RNG and writes no simulation field, so the village digest cannot depend on it.
 *
 * The simulation's villagers are all adults (deviation 8 of AnastasisVillage.h): only adult
 * and elder portraits are handed out. Children exist in the registry and on the lineup board;
 * they are drawn in the village the day the simulation has children.
 */
namespace AnastasisVillagerLooks
{
	/**
	 * Card canvas, shared with SourceArt/Characters/villager-population.json ("canvas"): 512 x 1024 px
	 * for 128 x 256 cm. Taller than a person on purpose: a guard's spear rises ~60 cm above his head.
	 */
	inline constexpr double CanvasWidthCm = 128.0;
	inline constexpr double CanvasHeightCm = 256.0;
	/** Feet sit 16 px above the bottom of a 1024 px canvas. */
	inline constexpr double FootMarginCm = 16.0 * CanvasHeightCm / 1024.0;

	/** May a portrait of this category be handed to a simulated villager today (bInGame aside). */
	bool IsAssignableInVillage(EAnastasisVillagerCategory Category);

	/**
	 * Target make-up of a village, by category: 35 % men, 35 % women, 15 % old men, 15 % old women.
	 * Interleaving in proportion to the PORTRAIT counts gave a village half old (the settler pool
	 * holds as many elders as adults); the share is the village's, not the sheets'.
	 */
	double VillageShare(EAnastasisVillagerCategory Category);

	/**
	 * Indices into `Looks` of the portraits a villager of this simulated job may wear: the job is in
	 * Look.Jobs, the pose is a walking one (bInGame), the category is assignable, the portrait is set.
	 * Each category is ordered by CRC of LookId (independent of the asset order); the categories are
	 * then interleaved by VillageShare, so any first N villagers mirror the village's make-up.
	 */
	TArray<int32> VillagePool(const TArray<FAnastasisVillagerLook>& Looks, FName Job);

	/**
	 * Index into `Looks` for this villager, INDEX_NONE if the pool is empty.
	 * `npc-N` takes the pool's N-th portrait modulo its size: the first Pool.Num() villagers
	 * are all different. Any other identifier falls back to its CRC.
	 */
	int32 PickLook(const TArray<int32>& Pool, const FString& NpcId);

	/**
	 * familles-feu-001 -- la categorie de portrait qu'un habitant doit porter quand la simulation connait son
	 * sexe (« male » / « female ») et son age : enfant sous treize ans, ancien a partir de cinquante-huit, adulte
	 * sinon. Faux si la simulation ne les connait pas (habitant anonyme) : le portrait se tire comme avant.
	 */
	bool CategoryFor(const FString& Gender, double Age, EAnastasisVillagerCategory& OutCategory);

	/**
	 * Les portraits de cette categorie que ce metier peut porter (meme ordre et memes regles que VillagePool).
	 * Un enfant n'a pas de metier : tous les portraits d'enfant en jeu lui vont.
	 */
	TArray<int32> PersonPool(const TArray<FAnastasisVillagerLook>& Looks, FName Job, EAnastasisVillagerCategory Category);

	/**
	 * How the 3D body of a villager is dressed (VILLAGER_BODY_3D_001). Colours are linear; heights
	 * are fractions of the body's own height on the bind pose (the material reads the pre-skinned
	 * position, so a band stays on the same piece of body while it walks).
	 */
	struct FBodyLook
	{
		bool bFemale = false;
		/** Uniform scale of the mannequin: Epic's mannequins stand ~180 cm, an ancient villager less. */
		float Scale = 1.0f;
		FLinearColor Skin = FLinearColor::White;
		FLinearColor Hair = FLinearColor::Black;
		FLinearColor Garment = FLinearColor::White;
		/** Belt and hem border. */
		FLinearColor Trim = FLinearColor::Black;
		/** Lowest point of the garment: knee for a man's chiton, ankle for a woman's peplos. */
		float Hem = 0.3f;
		/** Lowest point of the hair at the back of the head. */
		float HairLow = 0.86f;
		/** Locomotion play-rate jitter, so a crowd does not walk in step. */
		float PlayRate = 1.0f;
	};

	/**
	 * A pure function of the portrait id and its category: a portrait is always dressed the same
	 * way, and no simulation RNG is consumed. Mostly undyed wool and linen, some natural dyes
	 * (madder, faded woad, olive, dark brown) -- what a village of refugees would wear -- and never
	 * a garment within MinGarmentContrast of the skin it covers. Elders get grey or white hair;
	 * women a long garment and long hair.
	 */
	FBodyLook BodyLookFor(FName LookId, EAnastasisVillagerCategory Category);

	/**
	 * The same, then the colours measured on the portrait (FAnastasisVillagerLook::Body*) where they
	 * exist: the body wears what the card wears. Measured skin loses a third of its saturation (painted
	 * light); a measured garment too close to the skin is pushed lighter or darker, keeping its hue.
	 */
	FBodyLook BodyLookFor(const FAnastasisVillagerLook& Portrait);

	/** Largest sRGB channel difference, 0..255: below MinGarmentContrast a garment vanishes into the skin. */
	int32 ColourContrast(const FColor& A, const FColor& B);
	inline constexpr int32 MinGarmentContrast = 48;
}
