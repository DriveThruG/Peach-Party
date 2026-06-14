#include "Core/PPGameState.h"
#include "Core/PPPlayerState.h"
#include "Core/PPPlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void APPGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APPGameState, CurrentPhase);
	DOREPLIFETIME(APPGameState, TeamAScore);
	DOREPLIFETIME(APPGameState, TeamBScore);
	DOREPLIFETIME(APPGameState, PhaseEndServerTime);
	DOREPLIFETIME(APPGameState, AttackingTeam);
	DOREPLIFETIME(APPGameState, ActiveRoomIndex);
	DOREPLIFETIME(APPGameState, MatchWinner);
	DOREPLIFETIME(APPGameState, TeamAReward);
	DOREPLIFETIME(APPGameState, TeamBReward);
}

EPPReward APPGameState::GetTeamReward(EPPTeam Team) const
{
	if (Team == EPPTeam::TeamA) return TeamAReward;
	if (Team == EPPTeam::TeamB) return TeamBReward;
	return EPPReward::None;
}

void APPGameState::SetAttackingTeam(EPPTeam Team)
{
	if (HasAuthority()) { AttackingTeam = Team; }
}

void APPGameState::SetActiveRoomIndex(int32 Index)
{
	if (HasAuthority()) { ActiveRoomIndex = Index; }
}

void APPGameState::SetMatchWinner(EPPTeam Team)
{
	if (HasAuthority()) { MatchWinner = Team; }
}

int32 APPGameState::GetTeamScore(EPPTeam Team) const
{
	if (Team == EPPTeam::TeamA) return TeamAScore;
	if (Team == EPPTeam::TeamB) return TeamBScore;
	return 0;
}

float APPGameState::GetPhaseTimeRemaining() const
{
	if (PhaseEndServerTime <= 0.f)
	{
		return 0.f;
	}
	return FMath::Max(0.f, PhaseEndServerTime - GetServerWorldTimeSeconds());
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
	BP_OnPhaseChanged(CurrentPhase); // UMG swaps the on-screen panel per phase

	// Smooth fade on every phase transition.
	if (UWorld* World = GetWorld())
	{
		if (APPPlayerController* PC = Cast<APPPlayerController>(World->GetFirstPlayerController()))
		{
			PC->PlayTransitionFade();
		}
	}
}
