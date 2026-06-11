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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** SERVER. The player who gets the point when the ball enters this basket. */
	void SetScorer(APPPlayerState* InScorer) { Scorer = InScorer; }
	APPPlayerState* GetScorer() const { return Scorer; }

	/** SERVER. Mirror the hoop sprite horizontally (the left hoop must point right). */
	void SetFlipped(bool bNewFlipped);

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

	/** Uniform size of the hoop sprite (1.0 = native). 1.3 = 30% bigger. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float HoopScale = 1.3f;

	/** Depth (+Y = away from the side camera) of the VISUAL only — pushes the hoop back behind the
	 *  players. The scoring point (Mouth/actor origin) stays in the play plane, so scoring is unaffected. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float VisualDepthOffsetY = 60.f;

	UPROPERTY()
	APPPlayerState* Scorer = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_Flip)
	bool bFlipX = false;

	UFUNCTION()
	void OnRep_Flip();

	void ApplyFlip();
};
