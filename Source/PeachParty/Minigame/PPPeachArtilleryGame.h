#pragma once

#include "CoreMinimal.h"
#include "Minigame/PPMinigameBase.h"
#include "PPPeachArtilleryGame.generated.h"

class APPPlayerState;

/**
 * Peach Artillery — turn-based tank duel. Players alternate aim+power shots; a hit damages the
 * opponent. Winner = whoever still has health (early finish on KO), else higher health at time-out.
 *
 * Demonstrates the modular override: this game scores by HEALTH, not points, so it overrides
 * ForceResolve(). Everything else (camera, lifecycle, reporting) comes free from the base.
 *
 * GAMEPLAY TODO: spawn two tanks + terrain in OnMinigameStarted; drive turns from input; call
 * ApplyDamage() on a hit. KO triggers an immediate FinishWithResult.
 */
UCLASS()
class PEACHPARTY_API APPPeachArtilleryGame : public APPMinigameBase
{
	GENERATED_BODY()

public:
	APPPeachArtilleryGame();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** SERVER. Apply damage to a player's tank; auto-finishes on KO. */
	UFUNCTION(BlueprintCallable, Category = "PeachParty|Minigame")
	void ApplyDamage(APPPlayerState* Target, int32 Amount);

	/** Winner decided by remaining health (time-out) — overrides the score-based base rule. */
	virtual EMatchResult ForceResolve() const override;

protected:
	virtual void OnMinigameStarted() override;
	virtual void OnMinigameFinished() override;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Minigame")
	int32 MaxHealth = 100;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Minigame")
	int32 Player1Health = 100;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Minigame")
	int32 Player2Health = 100;
};
