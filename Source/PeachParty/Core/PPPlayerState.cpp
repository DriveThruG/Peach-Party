#include "Core/PPPlayerState.h"
#include "Core/PPGameState.h"
#include "Engine/World.h"
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
	DOREPLIFETIME(APPPlayerState, SelectedClass);
	DOREPLIFETIME(APPPlayerState, Wetness);
	DOREPLIFETIME(APPPlayerState, bIsSlipping);
}

void APPPlayerState::SetSelectedClass(EPPClass NewClass)
{
	// Only the server, and only while down/respawning — prevents mid-life stat swapping.
	if (!HasAuthority() || !bIsSlipping || SelectedClass == NewClass)
	{
		return;
	}
	SelectedClass = NewClass;
	OnRep_Class();
}

bool APPPlayerState::AddWetness(float Amount)
{
	if (!HasAuthority() || bIsSlipping || Amount <= 0.f)
	{
		return false;
	}
	// +10% wetness capacity reward raises the slip threshold (100 -> 110).
	float Threshold = 100.f;
	const APPGameState* GS = GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
	if (GS && GS->GetTeamReward(Team) == EPPReward::Health)
	{
		Threshold *= PPReward::Bonus;
	}
	Wetness = FMath::Clamp(Wetness + Amount, 0.f, Threshold);
	OnRep_Wetness();
	if (Wetness >= Threshold)
	{
		bIsSlipping = true; // caller (character/gamemode) reacts: ragdoll + schedule respawn
		return true;
	}
	return false;
}

void APPPlayerState::ResetForRespawn()
{
	if (!HasAuthority())
	{
		return;
	}
	Wetness = 0.f;
	bIsSlipping = false;
	OnRep_Wetness();
}

void APPPlayerState::OnRep_Class()
{
	BP_OnClassChanged(SelectedClass);
}

void APPPlayerState::OnRep_Wetness()
{
	BP_OnWetnessChanged(Wetness);
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
