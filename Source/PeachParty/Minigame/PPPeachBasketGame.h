#pragma once

#include "CoreMinimal.h"
#include "Minigame/PPMinigameBase.h"
#include "PPPeachBasketGame.generated.h"

/**
 * Peach Basket — real-time, physics-based. Each player flings/guides peaches into their basket;
 * most peaches in the basket when the timer ends wins. Scoring is the base-class score model,
 * so ForceResolve() (higher score wins, equal = draw) is exactly right — no override needed.
 *
 * GAMEPLAY TODO: spawn the basket + spawn-area + physics peaches in OnMinigameStarted (relative
 * to this actor's arena origin), and call AddScore(Owner, +1) from the basket's overlap handler.
 */
UCLASS()
class PEACHPARTY_API APPPeachBasketGame : public APPMinigameBase
{
	GENERATED_BODY()

public:
	APPPeachBasketGame();

protected:
	virtual void OnMinigameStarted() override;
	virtual void OnMinigameFinished() override;
};
