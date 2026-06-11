#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/PPTypes.h"
#include "Final/PPClassTypes.h"
#include "PPPlayerState.generated.h"

class APPMinigameBase;

/**
 * Per-player replicated match data: team, ready flag, score.
 *
 * Replication strategy:
 *  - Team / bIsReady use ReplicatedUsing so every client (incl. the listen-server host)
 *    reacts via OnRep_* to drive UI. These are low-frequency, server-authoritative.
 *  - MatchScore is a plain Replicated int; UI polls it or reacts on its own cadence.
 *  - PlayerState already replicates to everyone by default, so scoreboards "just work".
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

	UFUNCTION(BlueprintPure, Category = "PeachParty|Player")
	bool IsReady() const { return bIsReady; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Player")
	int32 GetMatchScore() const { return MatchScore; }

	/** The 1v1 match this player is currently in (null when waiting / done / in lobby). */
	UFUNCTION(BlueprintPure, Category = "PeachParty|Player")
	APPMinigameBase* GetCurrentMinigame() const { return CurrentMinigame; }

	// ---- Final phase: class + wetness ----
	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	EPPClass GetSelectedClass() const { return SelectedClass; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	FPPClassStats GetClassStats() const { return PPClass::GetDefaults(SelectedClass); }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	float GetWetness() const { return Wetness; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	bool IsSlipping() const { return bIsSlipping; }

	/** SERVER only. */
	void SetTeam(EPPTeam NewTeam);
	void SetReady(bool bNewReady);
	void AddMatchScore(int32 Delta);
	void SetCurrentMinigame(APPMinigameBase* Minigame);

	/** SERVER. Change class — allowed ONLY while slipping/respawning (anti mid-life-switch). */
	void SetSelectedClass(EPPClass NewClass);

	/** SERVER. Add wetness; at >=100 sets bIsSlipping (caller triggers ragdoll + respawn). Returns true if it just hit 100. */
	bool AddWetness(float Amount);

	/** SERVER. Reset wetness/slipping for a fresh life. */
	void ResetForRespawn();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Team, BlueprintReadOnly, Category = "PeachParty|Player")
	EPPTeam Team = EPPTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_Ready, BlueprintReadOnly, Category = "PeachParty|Player")
	bool bIsReady = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Player")
	int32 MatchScore = 0;

	/** The 1v1 match this player is currently in (null in lobby / waiting / done). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Player")
	APPMinigameBase* CurrentMinigame = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_Class, BlueprintReadOnly, Category = "PeachParty|Final")
	EPPClass SelectedClass = EPPClass::Sprayer;

	UPROPERTY(ReplicatedUsing = OnRep_Wetness, BlueprintReadOnly, Category = "PeachParty|Final")
	float Wetness = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Final")
	bool bIsSlipping = false;

	UFUNCTION()
	void OnRep_Class();

	UFUNCTION()
	void OnRep_Wetness();

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Final")
	void BP_OnClassChanged(EPPClass NewClass);

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Final")
	void BP_OnWetnessChanged(float NewWetness);

	UFUNCTION()
	void OnRep_Team();

	UFUNCTION()
	void OnRep_Ready();

	// ---- Blueprint integration points (UI / VFX react to replicated changes) ----

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Player")
	void BP_OnTeamChanged(EPPTeam NewTeam);

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Player")
	void BP_OnReadyChanged(bool bNewReady);
};
