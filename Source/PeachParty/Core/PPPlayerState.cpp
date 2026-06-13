#include "Core/PPPlayerState.h"
#include "Minigame/PPMinigameBase.h"
#include "Net/UnrealNetwork.h"

void APPPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPPlayerState, Team);
	DOREPLIFETIME(APPPlayerState, CurrentMinigame);
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

void APPPlayerState::SetCurrentMinigame(APPMinigameBase* Minigame)
{
	if (HasAuthority())
	{
		CurrentMinigame = Minigame;
	}
}

void APPPlayerState::OnRep_Team()
{
	BP_OnTeamChanged(Team);
}
