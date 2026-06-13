#include "Core/PPGameState.h"
#include "Core/PPPlayerState.h"
#include "Minigame/PPMinigameBase.h"
#include "Net/UnrealNetwork.h"

void APPGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPGameState, CurrentPhase);
	DOREPLIFETIME(APPGameState, TeamAScore);
	DOREPLIFETIME(APPGameState, TeamBScore);
	DOREPLIFETIME(APPGameState, ActiveMinigames);
}

int32 APPGameState::NumPlayers() const
{
	return PlayerArray.Num();
}

int32 APPGameState::GetTeamScore(EPPTeam Team) const
{
	if (Team == EPPTeam::TeamA) return TeamAScore;
	if (Team == EPPTeam::TeamB) return TeamBScore;
	return 0;
}

void APPGameState::SetPhase(EMatchPhase NewPhase)
{
	if (!HasAuthority() || CurrentPhase == NewPhase)
	{
		return;
	}
	CurrentPhase = NewPhase;
	OnRep_Phase(); // listen-server host mirror
}

void APPGameState::AddTeamScore(EPPTeam Team, int32 Delta)
{
	if (!HasAuthority())
	{
		return;
	}
	if (Team == EPPTeam::TeamA) { TeamAScore += Delta; }
	else if (Team == EPPTeam::TeamB) { TeamBScore += Delta; }
}

void APPGameState::AddActiveMinigame(APPMinigameBase* Minigame)
{
	if (HasAuthority() && Minigame)
	{
		ActiveMinigames.AddUnique(Minigame);
	}
}

void APPGameState::RemoveActiveMinigame(APPMinigameBase* Minigame)
{
	if (HasAuthority())
	{
		ActiveMinigames.Remove(Minigame);
	}
}

void APPGameState::OnRep_Phase()
{
	BP_OnPhaseChanged(CurrentPhase);
}
