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
	DOREPLIFETIME(APPPlayerState, SelectedClass);
	DOREPLIFETIME(APPPlayerState, bHasChosenClass);
	DOREPLIFETIME(APPPlayerState, Wetness);
	DOREPLIFETIME(APPPlayerState, bIsSlipping);
}

bool APPPlayerState::SetSelectedClass(EPPClass NewClass)
{
	if (!HasAuthority())
	{
		return false;
	}

	// Allowed only while choosing (pre-fight) or while down/respawning — never mid-life.
	const APPGameState* GS = GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
	const bool bSelecting = GS && GS->GetCurrentPhase() == EMatchPhase::ClassSelect;
	if (!bSelecting && !bIsSlipping)
	{
		return false;
	}

	SelectedClass = NewClass;
	if (bSelecting)
	{
		bHasChosenClass = true; // confirms the pick + closes the menu (replicated)
	}
	OnRep_Class();
	return true;
}

bool APPPlayerState::AddWetness(float Amount)
{
	if (!HasAuthority() || bIsSlipping || Amount <= 0.f)
	{
		return false;
	}
	// +10% wetness capacity reward raises the slip threshold (100 -> 110); None = flat 100.
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
		bIsSlipping = true; // caller (character/gamemode) reacts: slip + schedule respawn
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

void APPPlayerState::SetTeam(EPPTeam NewTeam)
{
	if (!HasAuthority() || Team == NewTeam)
	{
		return;
	}

	Team = NewTeam;
	OnRep_Team(); // server-side: OnRep doesn't auto-fire on the authority, so call it manually.
}

void APPPlayerState::OnRep_Team()
{
	BP_OnTeamChanged(Team);
}
