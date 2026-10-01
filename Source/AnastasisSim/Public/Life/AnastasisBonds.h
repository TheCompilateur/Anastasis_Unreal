// Liens — la branche AVEC compagnon de `socialize()` (reference `fee66ae`).
//
//   life/bonds.js        companionAffinity, bondTalkGain, bumpRelation (paliers)
//   life/talk.js         canStartTalk, speakWorthFor, shouldSpeakNow (FNV), talkMaxTurnsFor,
//                        talkHoldDurationFor, replyStanceChance (le refus), bondKindBetween
//   ai/socialMemory.js   notePerson / noteMeeting, promoteTag, trimPeople,
//                        socialMemoryBias, socialCompanionBonus, pickRememberedSeek
//   ai/socialCognition   observeMind, trimToM, estimatedGoalOf (theorie de l'esprit)
//   life/moodlets.js     stampMoodlet, pruneMoodlets, tickMoodlets, moodletGoalBias (newFriend)
//   life/needs.js        dominantNeedLabel
//
// Adultes sans famille ni partenaire, nature moyenne (coeur 1), sans colonie
// (pas de sceau), chronique vide (pas de ragot). Prouve par
// `Anastasis.Sim.Parite.Liens`. L'etat (relations, fiches, sessions) vit dans
// le village : Village/AnastasisVillage.h.

#pragma once

#include "CoreMinimal.h"
#include "Life/AnastasisNeeds.h"

namespace AnastasisBonds
{
	/** `BONDS`. */
	inline constexpr double FriendAt = 45.0;
	inline constexpr double MaxOutdoor = 5.2;
	inline constexpr double MaxIndoor = 6.5;
	inline constexpr double SeekKinRange = 14.0;
	inline constexpr double SeekSocialRange = 18.0;
	inline constexpr double LonelySocialAt = 62.0;
	inline constexpr double FriendScale = 0.42;
	inline constexpr double JobBonus = 14.0;
	inline constexpr double GrudgePenalty = 55.0;
	inline constexpr double GrudgeAt = -18.0;
	inline constexpr double StrangerFloor = -6.0;
	inline constexpr double DistWeight = 3.2;
	inline constexpr double MutualSocialBonus = 36.0;

	/** `TALK`. */
	inline constexpr double PairCooldownSeconds = 18.0;
	inline constexpr double SessionSeconds = 3.0;
	inline constexpr double LeadInSeconds = 0.32;
	inline constexpr double TurnSeconds = 2.2;
	inline constexpr int32 MinTurns = 3;
	inline constexpr int32 MaxTurns = 4;
	inline constexpr double SessionSecondsUrgent = 0.45;
	inline constexpr double SessionSecondsWork = 1.1;
	inline constexpr int32 MaxConcurrentTalkers = 4;
	inline constexpr double ConcurrentTalkFrac = 0.4;
	inline constexpr double SessionMaxDistance = 2.4;
	inline constexpr double FatigueWindowSeconds = 60.0;
	inline constexpr int32 FatigueBlockCount = 2;
	inline constexpr double SpeakWorthThreshold = 0.52;
	inline constexpr double SpeakWorthAmbientChance = 0.08;
	inline constexpr double VillageEmitWindow = 22.0;
	inline constexpr int32 VillageEmitLimit = 4;
	/** Une replique n'ecrase le dernier propos de l'auditeur qu'au-dela de 4 s. */
	inline constexpr double ReplyQuietSeconds = 4.0;
	/** `STANDING.grudge` : en dessous, une vraie rancune. */
	inline constexpr double RivalAt = -45.0;

	/** `SOCIAL_MEM` / `SOCIAL_COGNITION`. */
	inline constexpr int32 PeopleCapacity = 14;
	inline constexpr int32 PeopleForgetAfterDays = 26;
	inline constexpr double MeetTrust = 2.5;
	inline constexpr double AllyAt = 22.0;
	inline constexpr double PersonRivalAt = -18.0;
	inline constexpr double BiasCap = 16.0;
	inline constexpr double CompanionWeight = 0.55;
	inline constexpr double SeekRange = 22.0;
	inline constexpr double SeekMinTrust = 16.0;
	inline constexpr double SeekBias = 11.0;
	inline constexpr int32 TomCapacity = 6;
	inline constexpr int32 TomForgetAfterDays = 18;
	inline constexpr double TomMinConfidence = 0.35;

	/** `MOODLET` et le profil `newFriend`. */
	inline constexpr int32 MoodletMaxSlots = 2;
	inline constexpr double MoodletMoralePerSecondCap = 0.04;
	inline constexpr double NewFriendSeconds = 72.0;
	inline constexpr double NewFriendMoraleOnStamp = 3.0;
	inline constexpr double NewFriendMoralePerSecond = 0.02;
	inline constexpr double NewFriendSocializeBias = 8.0;

	/** `bondKindBetween` pour deux adultes sans famille ni partenaire. */
	enum class EBondKind : uint8 { Stranger, Coworker, Friend, Rival };

	/** Etiquettes de fiche portees (`ally`, `rival` ; les autres viennent d'episodes). */
	enum class EPersonTag : uint8 { None, Ally, Rival };

	/** `mind.people[id]` — une fiche de personne, vecue directement. */
	struct FPersonRow
	{
		FString Id;
		double Trust = 0.0;
		EPersonTag Tag = EPersonTag::None;
		int32 Day = 0;
		int32 Meets = 0;
	};

	/** `mind.tom[id]` — ce qu'on croit de l'autre (profondeur 1). */
	struct FTomEntry
	{
		FString Id;
		FString EstimatedGoal;
		double Attitude = 0.0;
		double Confidence = 0.0;
		int32 Day = 0;
	};

	/** `npc.moodlets[]` — seul `newFriend` est pose dans cette tranche. */
	struct FMoodlet
	{
		FString Id;
		double At = 0.0;
		double Until = 0.0;
	};

	/** `dominantNeedLabel` : le premier besoin de plus forte pression (tri stable). */
	struct FDominantNeed
	{
		const TCHAR* Id = TEXT("hunger");
		double Value = 0.0;
	};

	// --- Conversation (talk.js) -------------------------------------------------

	/** `hashTalk(a, b, salt)` : FNV-1a 32 bits de `"<a.id>:<b.id>:<salt>"`. */
	ANASTASISSIM_API uint32 HashTalk(const FString& AId, const FString& BId, int64 Salt);

	/** `chance(seed, salt, rate)` : FNV-1a de `"<seed>:<salt>"`, `% 1000 / 1000 < rate`. */
	ANASTASISSIM_API bool Chance(uint32 Seed, int32 Salt, double Rate);

	ANASTASISSIM_API bool IsTalkUrgent(const AnastasisNeeds::FNeeds& N);
	ANASTASISSIM_API bool IsTalkWorkBusy(const FString& Goal, bool bInside);
	ANASTASISSIM_API EBondKind BondKindBetween(double RelAB, double RelBA, const FString& JobA, const FString& JobB);
	ANASTASISSIM_API FDominantNeed DominantNeed(const AnastasisNeeds::FNeeds& N);

	/** `speakWorthFor` sans sceau, sans joueur, sans ragot. */
	ANASTASISSIM_API double SpeakWorth(const AnastasisNeeds::FNeeds& Speaker, const AnastasisNeeds::FNeeds& Listener, bool bSpeakerChain, EBondKind Kind);

	/** `shouldSpeakNow` : RecentEmits = paroles de rue dans les 22 dernieres secondes. */
	ANASTASISSIM_API bool ShouldSpeakNow(double Worth, int32 RecentEmits, const FString& SpeakerId, const FString& ListenerId, double Now);

	/** `talkHoldDurationFor`. */
	ANASTASISSIM_API double TalkHoldDuration(bool bUrgent, bool bWorkBusy);

	/** `talkMaxTurnsFor(speaker, listener, { fatigue })`. */
	ANASTASISSIM_API int32 TalkMaxTurns(const FString& SpeakerId, const FString& ListenerId, bool bUrgent, bool bWorkBusy, EBondKind Kind, int32 Fatigue);

	/**
	 * Le refus de `replyUtteranceFor` : faim ou fatigue >= 70 -> replique de besoin,
	 * jamais un refus ; sinon `chance(seed, 19, replyStanceChance(kind, "refuse", fatigue))`.
	 */
	ANASTASISSIM_API bool RefusesReply(const AnastasisNeeds::FNeeds& Replier, EBondKind Kind, int32 Fatigue, uint32 Seed);

	// --- Liens (bonds.js) -------------------------------------------------------

	/** `bondTalkGain` : 6 (8 entre amis) x moyenne des `talk` (nature moyenne), arrondi, >= 3. */
	ANASTASISSIM_API int32 BondTalkGain(bool bFriend);

	/** `companionAffinity` : relation, rancune, collegue, disponible, puis `socialCompanionBonus`. */
	ANASTASISSIM_API double CompanionAffinity(double Rel, double Trust, EPersonTag Tag, bool bSameJob, bool bOtherAvailable);

	/** `bondStageFromRelation(rel).rank` : rival -2, inconnu 0, familier 1, ami 2, proche 3, intrigue 4. */
	ANASTASISSIM_API int32 BondStageRank(double Rel);

	// --- Memoire sociale (socialMemory.js, socialCognition.js) ------------------

	/** `notePerson(sim, npc, other, { trustDelta, meet: true })` direct, puis tag et trim. */
	ANASTASISSIM_API void NotePersonDirect(TArray<FPersonRow>& People, const FString& OtherId, int32 Day, double TrustDelta);

	/** `forgetStalePeople`. */
	ANASTASISSIM_API void ForgetStalePeople(TArray<FPersonRow>& People, int32 Day);

	/** `observeMind(sim, npc, other, { from: "meeting" })`. */
	ANASTASISSIM_API void ObserveMind(TArray<FTomEntry>& Tom, const FString& OtherId, const FString& OtherGoal, double Attitude, int32 Day);

	/** `estimatedGoalOf(npc, otherId)` (confiance >= 0,35), vide sinon. */
	ANASTASISSIM_API FString EstimatedGoalOf(const TArray<FTomEntry>& Tom, const FString& OtherId);

	/** `socialMemoryBias(npc, "socialize")`. */
	ANASTASISSIM_API double SocialMemoryBiasSocialize(const TArray<FPersonRow>& People);

	/** `trustOf` / `personTag`. */
	ANASTASISSIM_API const FPersonRow* FindPerson(const TArray<FPersonRow>& People, const FString& Id);

	/**
	 * `pickRememberedSeek` : Locate rend la position de l'acteur vivant (false s'il
	 * n'existe plus). Rend l'identifiant choisi, vide sinon.
	 */
	ANASTASISSIM_API FString PickRememberedSeek(const TArray<FPersonRow>& People, const TArray<FTomEntry>& Tom,
		const FString& SelfId, double SelfX, double SelfY, double SelfSocial,
		TFunctionRef<bool(const FString&, double&, double&)> Locate);

	// --- Moodlets (moodlets.js) -------------------------------------------------

	ANASTASISSIM_API void PruneMoodlets(TArray<FMoodlet>& List, double Now);
	/** `stampMoodlet(npc, "newFriend", { at })` : moral +3 a la pose (base `|| 50`). */
	ANASTASISSIM_API void StampNewFriend(TArray<FMoodlet>& List, double& Morale, double Now);
	ANASTASISSIM_API void TickMoodlets(TArray<FMoodlet>& List, double& Morale, double Dt, double Now);
	ANASTASISSIM_API double MoodletGoalBias(TArray<FMoodlet>& List, const FString& Goal, double Now);
}
