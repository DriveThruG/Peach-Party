#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/PPTypes.h"
#include "Final/PPClassTypes.h"
#include "PPPlayerState.generated.h"

/**
 * Per-player replicated match data: team, chosen class, wetness / slip state.
 *
 * PlayerState already replicates to everyone by default, so scoreboards "just work".
 * Team / class / wetness use ReplicatedUsing so every client (incl. the listen-server host)
 * reacts via OnRep_* to drive UI.
 */
UCLASS()
class PEACHPARTY_API APPPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	APPPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "PeachParty|Player")
	EPPTeam GetTeam() const { return Team; }

	// ---- Final phase: class + wetness ----
	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	EPPClass GetSelectedClass() const { return SelectedClass; }

	/** True once the player confirmed a class in the ClassSelect phase. Drives the menu hide + start gate. */
	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	bool HasChosenClass() const { return bHasChosenClass; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	FPPClassStats GetClassStats() const { return PPClass::GetDefaults(SelectedClass); }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	float GetWetness() const { return Wetness; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	bool IsSlipping() const { return bIsSlipping; }

	/** SERVER only. */
	void SetTeam(EPPTeam NewTeam);

	/** SERVER. Pick a class — allowed during the ClassSelect phase, or while slipping/respawning
	 *  (so a downed player can re-pick). Returns true if the selection took effect. */
	bool SetSelectedClass(EPPClass NewClass);

	/** SERVER. Add wetness; at >=100 sets bIsSlipping (caller triggers slip + respawn). Returns true if it just hit 100. */
	bool AddWetness(float Amount);

	/** SERVER. Reset wetness/slipping for a fresh life. */
	void ResetForRespawn();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Team, BlueprintReadOnly, Category = "PeachParty|Player")
	EPPTeam Team = EPPTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_Class, BlueprintReadOnly, Category = "PeachParty|Final")
	EPPClass SelectedClass = EPPClass::Sprayer;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Final")
	bool bHasChosenClass = false;

	UPROPERTY(ReplicatedUsing = OnRep_Wetness, BlueprintReadOnly, Category = "PeachParty|Final")
	float Wetness = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Final")
	bool bIsSlipping = false;

	UFUNCTION()
	void OnRep_Class();

	UFUNCTION()
	void OnRep_Wetness();

	UFUNCTION()
	void OnRep_Team();

	// ---- Blueprint integration points (UI / VFX react to replicated changes) ----
	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Final")
	void BP_OnClassChanged(EPPClass NewClass);

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Final")
	void BP_OnWetnessChanged(float NewWetness);

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Player")
	void BP_OnTeamChanged(EPPTeam NewTeam);
};
