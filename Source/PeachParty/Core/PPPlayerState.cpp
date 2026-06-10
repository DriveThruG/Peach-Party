#include "Core/PPPlayerState.h"
#include "Net/UnrealNetwork.h"

APPPlayerState::APPPlayerState()
{
	// PlayerState already ticks/replicates appropriately; nothing special needed here.
}

void APPPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APPPlayerState, Team);
	DOREPLIFETIME(APPPlayerState, bIsReady);
	DOREPLIFETIME(APPPlayerState, MatchScore);
	DOREPLIFETIME(APPPlayerState, CurrentMinigame);
}

void APPPlayerState::SetCurrentMinigame(APPMinigameBase* Minigame)
{
	if (HasAuthority())
	{
		CurrentMinigame = Minigame;
	}
}

void APPPlayerState::SetTeam(EPPTeam NewTeam)
{
	if (!HasAuthority() || Team == NewTeam)
	{
		return;
	}

	Team = NewTeam;
	OnRep_Team(); // server-side: OnRep doesn't auto-fire on the authority, so call it manually.
}

void APPPlayerState::SetReady(bool bNewReady)
{
	if (!HasAuthority() || bIsReady == bNewReady)
	{
		return;
	}

	bIsReady = bNewReady;
	OnRep_Ready();
}

void APPPlayerState::AddMatchScore(int32 Delta)
{
	if (!HasAuthority())
	{
		return;
	}

	MatchScore += Delta;
}

void APPPlayerState::OnRep_Team()
{
	BP_OnTeamChanged(Team);
}

void APPPlayerState::OnRep_Ready()
{
	BP_OnReadyChanged(bIsReady);
}
