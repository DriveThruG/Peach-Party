#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/PPTypes.h"
#include "Final/PPRewardTypes.h"
#include "PPGameState.generated.h"

/**
 * Replicated, everyone-visible match state. The single source of truth that clients read
 * to drive HUD, phase banners, scoreboards and countdowns.
 *
 * Replication strategy:
 *  - CurrentPhase: ReplicatedUsing=OnRep_Phase -> every client (and the host) reacts to
 *    drive phase UI. Set ONLY by APPGameMode (server).
 *  - PhaseEndServerTime: server world-time (seconds) at which the current phase's timer
 *    ends. Clients compute remaining = PhaseEndServerTime - GetServerWorldTimeSeconds().
 */
UCLASS()
class PEACHPARTY_API APPGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "PeachParty|Match")
	EMatchPhase GetCurrentPhase() const { return CurrentPhase; }

	/** Seconds left in the current timed phase (0 if untimed). Safe to call on clients. */
	UFUNCTION(BlueprintPure, Category = "PeachParty|Match")
	float GetPhaseTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "PeachParty|Match")
	int32 NumPlayers() const;

	UFUNCTION(BlueprintPure, Category = "PeachParty|Match")
	int32 GetTeamScore(EPPTeam Team) const;

	// ---- Final phase (frontline) ----
	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	EPPTeam GetAttackingTeam() const { return AttackingTeam; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	int32 GetActiveRoomIndex() const { return ActiveRoomIndex; }

	/** Set once a team has won (PostMatch). EPPTeam::None until then. */
	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	EPPTeam GetMatchWinner() const { return MatchWinner; }

	/** Per-team Final advantage. Always None in this build (no reward phase) — kept for the
	 *  class/wetness code paths that read it. */
	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	EPPReward GetTeamReward(EPPTeam Team) const;

	// ---- SERVER only mutators (called by APPGameMode) ----
	void SetPhase(EMatchPhase NewPhase);
	void SetPhaseEndTime(float ServerWorldTime);
	void SetAttackingTeam(EPPTeam Team);
	void SetActiveRoomIndex(int32 Index);
	void SetMatchWinner(EPPTeam Team);
	void AddTeamScore(EPPTeam Team, int32 Delta);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "PeachParty|Match")
	EMatchPhase CurrentPhase = EMatchPhase::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Match")
	int32 TeamAScore = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Match")
	int32 TeamBScore = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Match")
	float PhaseEndServerTime = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Final")
	EPPTeam AttackingTeam = EPPTeam::TeamA;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Final")
	int32 ActiveRoomIndex = 0; // 0 = none yet; 1..3 active room

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Final")
	EPPTeam MatchWinner = EPPTeam::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Final")
	EPPReward TeamAReward = EPPReward::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Final")
	EPPReward TeamBReward = EPPReward::None;

	UFUNCTION()
	void OnRep_Phase();

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Match")
	void BP_OnPhaseChanged(EMatchPhase NewPhase);
};
