#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/PPTypes.h"
#include "PPPlayerState.generated.h"

class APPMinigameBase;

/**
 * Per-player replicated match data: team + the match the player is currently in.
 * PlayerState already replicates to everyone, so scoreboards "just work".
 */
UCLASS()
class PEACHPARTY_API APPPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "PeachParty|Player")
	EPPTeam GetTeam() const { return Team; }

	/** The 1v1 match this player is currently in (null when none). */
	UFUNCTION(BlueprintPure, Category = "PeachParty|Player")
	APPMinigameBase* GetCurrentMinigame() const { return CurrentMinigame; }

	/** SERVER only. */
	void SetTeam(EPPTeam NewTeam);
	void SetCurrentMinigame(APPMinigameBase* Minigame);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Team, BlueprintReadOnly, Category = "PeachParty|Player")
	EPPTeam Team = EPPTeam::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Player")
	APPMinigameBase* CurrentMinigame = nullptr;

	UFUNCTION()
	void OnRep_Team();

	/** UI/VFX hook reacting to the replicated team change. */
	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Player")
	void BP_OnTeamChanged(EPPTeam NewTeam);
};
