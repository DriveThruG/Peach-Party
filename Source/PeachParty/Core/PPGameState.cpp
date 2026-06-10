#include "Core/PPGameState.h"
#include "Core/PPPlayerState.h"
#include "Net/UnrealNetwork.h"

void APPGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APPGameState, CurrentPhase);
	DOREPLIFETIME(APPGameState, CurrentMinigameIndex);
	DOREPLIFETIME(APPGameState, TeamAScore);
	DOREPLIFETIME(APPGameState, TeamBScore);
	DOREPLIFETIME(APPGameState, PhaseEndServerTime);
	DOREPLIFETIME(APPGameState, ActiveMinigames);
}

int32 APPGameState::GetTeamScore(EPPTeam Team) const
{
	if (Team == EPPTeam::TeamA) return TeamAScore;
	if (Team == EPPTeam::TeamB) return TeamBScore;
	return 0;
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

void APPGameState::ClearActiveMinigames()
{
	if (HasAuthority())
	{
		ActiveMinigames.Reset();
	}
}

float APPGameState::GetPhaseTimeRemaining() const
{
	if (PhaseEndServerTime <= 0.f)
	{
		return 0.f;
	}
	return FMath::Max(0.f, PhaseEndServerTime - GetServerWorldTimeSeconds());
}

int32 APPGameState::NumReadyPlayers() const
{
	int32 Count = 0;
	for (APlayerState* PS : PlayerArray)
	{
		const APPPlayerState* PPPS = Cast<APPPlayerState>(PS);
		if (PPPS && PPPS->IsReady())
		{
			++Count;
		}
	}
	return Count;
}

int32 APPGameState::NumPlayers() const
{
	int32 Count = 0;
	for (APlayerState* PS : PlayerArray)
	{
		if (PS)
		{
			++Count;
		}
	}
	return Count;
}

void APPGameState::SetPhase(EMatchPhase NewPhase)
{
	if (!HasAuthority() || CurrentPhase == NewPhase)
	{
		return;
	}

	CurrentPhase = NewPhase;
	OnRep_Phase(); // authority doesn't auto-fire OnRep; mirror the client path on the host.
}

void APPGameState::SetCurrentMinigameIndex(int32 Index)
{
	if (HasAuthority())
	{
		CurrentMinigameIndex = Index;
	}
}

void APPGameState::SetPhaseEndTime(float ServerWorldTime)
{
	if (HasAuthority())
	{
		PhaseEndServerTime = ServerWorldTime;
	}
}

void APPGameState::AddTeamScore(EPPTeam Team, int32 Delta)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Team == EPPTeam::TeamA)
	{
		TeamAScore += Delta;
	}
	else if (Team == EPPTeam::TeamB)
	{
		TeamBScore += Delta;
	}
}

void APPGameState::OnRep_Phase()
{
	BP_OnPhaseChanged(CurrentPhase);
}
