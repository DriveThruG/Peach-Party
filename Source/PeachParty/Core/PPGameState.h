#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/PPTypes.h"
#include "PPGameState.generated.h"

class APPMinigameBase;

/**
 * Replicated, everyone-visible match state. The single source of truth that clients read
 * to drive HUD, phase banners, scoreboards and countdowns.
 *
 * Replication strategy:
 *  - CurrentPhase: ReplicatedUsing=OnRep_Phase -> every client (and the host) reacts to
 *    drive phase UI / enable-disable input. Set ONLY by APPGameMode (server).
 *  - Team scores: plain Replicated ints (low churn).
 *  - PhaseEndServerTime: server world-time (seconds) at which the current phase's timer
 *    ends. Clients compute remaining = PhaseEndServerTime - GetServerWorldTimeSeconds().
 *    This is bandwidth-free countdown: one replicated float instead of ticking a counter.
 */
UCLASS()
class PEACHPARTY_API APPGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "PeachParty|Match")
	EMatchPhase GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Match")
	int32 GetCurrentMinigameIndex() const { return CurrentMinigameIndex; }

	/** Seconds left in the current timed phase (0 if untimed). Safe to call on clients. */
	UFUNCTION(BlueprintPure, Category = "PeachParty|Match")
	float GetPhaseTimeRemaining() const;

	/** Counts currently-connected players whose PlayerState reports ready. */
	UFUNCTION(BlueprintPure, Category = "PeachParty|Match")
	int32 NumReadyPlayers() const;

	UFUNCTION(BlueprintPure, Category = "PeachParty|Match")
	int32 NumPlayers() const;

	/** All currently-live 1v1 matches. Read by spectator camera + scoreboard UI. */
	const TArray<APPMinigameBase*>& GetActiveMinigames() const { return ActiveMinigames; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Match")
	int32 GetTeamScore(EPPTeam Team) const;

	// ---- Final phase (frontline) ----
	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	EPPTeam GetAttackingTeam() const { return AttackingTeam; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	int32 GetActiveRoomIndex() const { return ActiveRoomIndex; }

	void SetAttackingTeam(EPPTeam Team);
	void SetActiveRoomIndex(int32 Index);

	// ---- SERVER only mutators (called by APPGameMode) ----
	void SetPhase(EMatchPhase NewPhase);
	void SetCurrentMinigameIndex(int32 Index);
	void SetPhaseEndTime(float ServerWorldTime);
	void AddTeamScore(EPPTeam Team, int32 Delta);
	void AddActiveMinigame(APPMinigameBase* Minigame);
	void RemoveActiveMinigame(APPMinigameBase* Minigame);
	void ClearActiveMinigames();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "PeachParty|Match")
	EMatchPhase CurrentPhase = EMatchPhase::Lobby;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Match")
	int32 CurrentMinigameIndex = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Match")
	int32 TeamAScore = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Match")
	int32 TeamBScore = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Match")
	float PhaseEndServerTime = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Match")
	TArray<APPMinigameBase*> ActiveMinigames;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Final")
	EPPTeam AttackingTeam = EPPTeam::TeamA;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Final")
	int32 ActiveRoomIndex = 0; // 0 = none yet; 1..3 active room

	UFUNCTION()
	void OnRep_Phase();

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Match")
	void BP_OnPhaseChanged(EMatchPhase NewPhase);
};
