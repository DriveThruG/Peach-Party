#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/PPTypes.h"
#include "Minigame/PPMinigameTypes.h"
#include "PPGameMode.generated.h"

class APPGameState;
class APPMinigameBase;
class APPPlayerState;

/**
 * SERVER ONLY. Stripped to a single job: as soon as TWO players are connected, assign them to
 * Team A / Team B and drop both straight into ONE Peach Basket match — no lobby, no menus, no
 * hosting flow. When a match ends it auto-restarts after a short pause so play is continuous.
 *
 * (bDebugSoloBasket: a single player drops into a free-play basket for live tuning.)
 */
UCLASS(Config=Game)
class PEACHPARTY_API APPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APPGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Called by a basket match when it is decided: score it, then restart after a pause. */
	void NotifyMinigameFinished(APPMinigameBase* Match, EMatchResult Result);

	// ---- Rules ----
	/** The basket game class. Defaults to the C++ class; auto-prefers BP_PeachBasketUMG if it exists. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	TSubclassOf<APPMinigameBase> BasketGameClass;

	/** Pause after a match ends before the next one starts. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	float RestartDelaySeconds = 3.f;

	/** SOLO tuning preview: drop a SINGLE player into a free-play basket (no opponent, no timer). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "PeachParty|Debug")
	bool bDebugSoloBasket = false;

protected:
	EPPTeam PickJoinTeam() const;     // smaller team (A on a tie) for a joining player
	void TryStartMatch();             // start a match if the player requirement is met and none is live
	void StartTwoPlayerMatch(APPPlayerState* TeamAPlayer, APPPlayerState* TeamBPlayer);
	void StartSoloBasketPreview();
	APPMinigameBase* SpawnBasket();   // spawns the basket actor (server), AddActiveMinigame
	void EndCurrentMatch();           // tears down the live match + clears player bindings
	void RestartMatch();              // timer target: end + try to start a fresh one

	APPGameState* GetPPGameState() const;

	UPROPERTY()
	APPMinigameBase* ActiveMatch = nullptr;

	FTimerHandle RestartTimer;
};
