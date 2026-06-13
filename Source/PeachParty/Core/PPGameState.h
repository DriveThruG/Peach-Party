#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/PPTypes.h"
#include "PPGameState.generated.h"

class APPMinigameBase;

/**
 * Replicated, everyone-visible match state for the Peach-Basket-only build:
 * the current phase, the two team scores, and the live match (so spectators/UI can find it).
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
	int32 NumPlayers() const;

	UFUNCTION(BlueprintPure, Category = "PeachParty|Match")
	int32 GetTeamScore(EPPTeam Team) const;

	/** All currently-live matches (0 or 1 here). Read by HUD if needed. */
	const TArray<APPMinigameBase*>& GetActiveMinigames() const { return ActiveMinigames; }

	// ---- SERVER only mutators (called by APPGameMode) ----
	void SetPhase(EMatchPhase NewPhase);
	void AddTeamScore(EPPTeam Team, int32 Delta);
	void AddActiveMinigame(APPMinigameBase* Minigame);
	void RemoveActiveMinigame(APPMinigameBase* Minigame);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "PeachParty|Match")
	EMatchPhase CurrentPhase = EMatchPhase::WaitingForPlayers;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Match")
	int32 TeamAScore = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Match")
	int32 TeamBScore = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Match")
	TArray<APPMinigameBase*> ActiveMinigames;

	UFUNCTION()
	void OnRep_Phase();

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Match")
	void BP_OnPhaseChanged(EMatchPhase NewPhase);
};
