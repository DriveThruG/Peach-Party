#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Minigame/PPMinigameBase.h"
#include "PPPeachBasketGame.generated.h"

class UStaticMeshComponent;
class APPBasketBall;
class APPBasketCharacter;
class APPBasket;
class APPPlayerState;

/**
 * Peach Basket — real-time physics chaos. Each player drives TWO wobbly characters with ONE input
 * (Space). Hold = jump + charge arms; release angle decides the throw arc into the enemy basket.
 *
 * Server-authoritative: the server simulates physics, advances charges, and runs grab/steal/scoring
 * as proximity checks in Tick (no collision-channel setup -> nothing to silently misconfigure).
 * Clients are replicated proxies (movement replication on ball + characters). First to TargetScore
 * wins; ties / time-out fall back to the base score comparison.
 */
UCLASS()
class PEACHPARTY_API APPPeachBasketGame : public APPMinigameBase
{
	GENERATED_BODY()

public:
	APPPeachBasketGame();

	virtual void Tick(float DeltaSeconds) override;
	virtual void HandleInput(APPPlayerState* Player, FName Action, bool bPressed) override;

protected:
	virtual void OnMinigameStarted() override;
	virtual void OnMinigameFinished() override;

	// ---- Tunables ----
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	int32 TargetScore = 3;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float JumpUpImpulse = 550.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float JumpForwardImpulse = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float GrabRadius = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float StealRadius = 70.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float ScoreRadius = 140.f;

	/** Fixed launch speed; the arm angle only changes the elevation, not the power. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float ThrowSpeed = 1150.f;

	/** Arm angle 0 -> MinElev (short), Max -> MaxElev. The sweet spot lands in the basket. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float MinThrowElevationDeg = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float MaxThrowElevationDeg = 68.f;

	/** "Not perfectly accurate": random spread added at release. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float ThrowSpreadDeg = 4.f;

	// ---- Static arena geometry (this game brings its own floor/walls; arenas are empty space) ----
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	UStaticMeshComponent* Floor;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	TArray<UStaticMeshComponent*> Walls;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	TSubclassOf<APPBasketBall> BallClass;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	TSubclassOf<APPBasketCharacter> CharacterClass;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	TSubclassOf<APPBasket> BasketClass;

private:
	// Spawned actors (server owns gameplay; replicated to clients for view).
	UPROPERTY() APPBasketBall* Ball = nullptr;
	UPROPERTY() TArray<APPBasketCharacter*> Player1Chars;
	UPROPERTY() TArray<APPBasketCharacter*> Player2Chars;
	UPROPERTY() APPBasket* BasketForP1 = nullptr; // P1 scores by landing the ball here
	UPROPERTY() APPBasket* BasketForP2 = nullptr;

	// Start transforms for the post-score reset.
	FTransform BallStart;
	TArray<FTransform> Char1Starts;
	TArray<FTransform> Char2Starts;

	void BuildArenaGeometry();   // constructor helper (floor + walls)
	void SpawnPlay();            // OnMinigameStarted: spawn ball/chars/baskets
	void UpdateGrabAndSteal();   // Tick (server)
	void UpdateScoring();        // Tick (server)
	void ThrowFrom(APPBasketCharacter* Holder);
	void ResetPositions();
	void RegisterScore(APPPlayerState* Scorer);

	const TArray<APPBasketCharacter*>& CharsOf(const APPPlayerState* Player) const;
	void ForEachChar(TFunctionRef<void(APPBasketCharacter*)> Fn) const;
};
