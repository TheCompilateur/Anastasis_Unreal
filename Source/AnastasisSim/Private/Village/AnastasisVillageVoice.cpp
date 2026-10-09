// voix-conseil-001 -- une voix au conseil : le joueur entre dans les decisions du village.
//
// EXTENSION — ecart n°50. Mandat d'Alexandre (2026-10-09) : le joueur est un habitant, pas le seigneur ; il vote au
// conseil, on lui demande de l'aide et il en demande, avec les memes regles que les autres (`EvaluateHelp`,
// `EvaluateWelcome`) ; il arrive en etranger a eprouver (demi-voix, `nouveau`), et le village le juge sur ses actes,
// en face (la raison d'un refus) et dans son dos (ses votes et ses refus deviennent des histoires qui courent).
// Sans joueur incarne, aucune de ces fonctions ne change rien.

#include "Village/AnastasisVillage.h"

namespace AnastasisVillage
{
	bool FVillage::IsUnproven(const FNpc& Npc) const
	{
		// Arrive apres la fondation : le joueur, ou un groupe venu par la route.
		const FFamily* Family = Npc.FamilyId.IsEmpty() ? nullptr : FindFamily(Npc.FamilyId);
		const bool bNewcomer = Npc.Id == PlayerPersonId || (Family && Family->ArrivedDay > 0);
		if (!bNewcomer) return false;
		// Eprouve : il a pose au moins une piece sur la maison achevee d'une autre famille.
		for (const FBuilding& Done : Buildings.GetItems())
		{
			if (Done.Progress < 1.0 || Done.OwnerFamilyId.IsEmpty() || Done.OwnerFamilyId == Npc.FamilyId) continue;
			for (const TPair<FString, int32>& Worker : Done.Workers)
			{
				if (Worker.Key == Npc.Id && Worker.Value > 0) return false;
			}
		}
		return true;
	}

	FString FVillage::CastPlayerVote(bool bYes)
	{
		if (PlayerPersonId.IsEmpty() || !Actors.FindById(PlayerPersonId)) return FString();
		for (const FFamily& Family : Families)
		{
			if (!Family.bGuest || Family.bLeft) continue;
			PlayerVoteFamilyId = Family.Id;
			bPlayerVoteYes = bYes;
			return Family.Id;
		}
		return FString();
	}

	bool FVillage::AnswerPlayerAsk(bool bYes)
	{
		FPlayerAsk* Ask = PlayerAsks.FindByPredicate([](const FPlayerAsk& A) { return !A.bAnswered; });
		if (!Ask || PlayerPersonId.IsEmpty()) return false;
		Ask->bAnswered = true;
		Ask->bAccepted = bYes;
		FHelpAnswer Answer;
		Answer.Day = Day();
		Answer.FromId = Ask->FromId;
		Answer.ToId = PlayerPersonId;
		Answer.SiteId = Ask->SiteId;
		Answer.bAccepted = bYes;
		Answer.Reason = TEXT("joueur");
		HelpLog.Add(Answer);
		if (bYes)
		{
			if (FBuilding* Site = Buildings.FindById(Ask->SiteId)) Site->AllowedBuilders.AddUnique(PlayerPersonId);
		}
		else if (Actors.FindById(Ask->FromId))
		{
			// Le refus se retient, comme celui de n'importe qui : il courra.
			FEpisodeOptions Refusal;
			Refusal.AboutId = PlayerPersonId;
			Refusal.RootId = FString::Printf(TEXT("refus-%s-%s"), *Ask->SiteId, *PlayerPersonId);
			RecordEpisode(Ask->FromId, TEXT("refusedHelp"), Refusal);
		}
		return true;
	}

	FString FVillage::PlayerBuildHome(const FString& FamilyName)
	{
		const FNpc* Player = PlayerPersonId.IsEmpty() ? nullptr : Actors.FindById(PlayerPersonId);
		if (!Player) return FString();
		FString FamilyId = Player->FamilyId;
		if (FamilyId.IsEmpty())
		{
			FamilyId = AddFamily(FamilyName.IsEmpty() ? FString(TEXT("la maison du nouveau")) : FamilyName);
			JoinFamily(PlayerPersonId, FamilyId, true, TEXT("chef"));
		}
		const FFamily* Family = FindFamily(FamilyId);
		if (!Family || !Family->HomeId.IsEmpty()) return FString();
		for (const FBuilding& Building : Buildings.GetItems())
		{
			if (Building.OwnerFamilyId == FamilyId && Building.Progress < 1.0) return FString();
		}
		return OpenFamilySiteNear(FamilyId, *Actors.FindById(PlayerPersonId));
	}

	bool FVillage::PlayerAskHelp(const FString& NpcId, FHelpAnswer& OutAnswer)
	{
		const FNpc* Player = PlayerPersonId.IsEmpty() ? nullptr : Actors.FindById(PlayerPersonId);
		const FNpc* Asked = Actors.FindById(NpcId);
		if (!Player || !Asked || Asked == Player || Player->FamilyId.IsEmpty()) return false;
		FBuilding* Site = nullptr;
		for (FBuilding& Building : Buildings.GetItemsMutable())
		{
			if (Building.OwnerFamilyId == Player->FamilyId && Building.Progress < 1.0) Site = &Building;
		}
		if (!Site) return false;
		OutAnswer = EvaluateHelp(*Player, *Asked, *Site);
		Site->AskedIds.AddUnique(NpcId);
		HelpLog.Add(OutAnswer);
		if (OutAnswer.bAccepted)
		{
			Site->AllowedBuilders.AddUnique(NpcId);
		}
		else
		{
			FEpisodeOptions Refusal;
			Refusal.AboutId = NpcId;
			Refusal.RootId = FString::Printf(TEXT("refus-%s-%s"), *Site->Id, *NpcId);
			RecordEpisode(PlayerPersonId, TEXT("refusedHelp"), Refusal);
		}
		return true;
	}
}
