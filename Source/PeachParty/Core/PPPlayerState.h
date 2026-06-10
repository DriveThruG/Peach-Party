#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/PPTypes.h"
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

	/** SERVER only. */
	void SetTeam(EPPTeam NewTeam);
	void SetReady(bool bNewReady);
	void AddMatchScore(int32 Delta);
	void SetCurrentMinigame(APPMinigameBase* Minigame);

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
