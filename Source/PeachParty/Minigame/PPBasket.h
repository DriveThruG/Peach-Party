#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPBasket.generated.h"

class UStaticMeshComponent;
class UPaperSpriteComponent;
class UTexture2D;
class USceneComponent;
class APPPlayerState;

/**
 * A scoring target. Cosmetic mesh + a server-side note of WHO scores when the ball drops in here.
 * Scoring itself is a proximity check in APPPeachBasketGame's tick (no overlap-channel setup).
 */
UCLASS()
class PEACHPARTY_API APPBasket : public AActor
{
	GENERATED_BODY()

public:
	APPBasket();

	virtual void BeginPlay() override;

	/** SERVER. The player who gets the point when the ball enters this basket. */
	void SetScorer(APPPlayerState* InScorer) { Scorer = InScorer; }
	APPPlayerState* GetScorer() const { return Scorer; }

	/** Where the ball must arrive to score (the hoop mouth). */
	FVector GetMouthLocation() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	UStaticMeshComponent* Ring;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	UPaperSpriteComponent* Sprite;

	UPROPERTY()
	UTexture2D* HoopTexture = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	USceneComponent* Mouth;

	UPROPERTY()
	APPPlayerState* Scorer = nullptr;
};
