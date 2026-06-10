#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPBasketBall.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class APPBasketCharacter;

/**
 * The peach ball. Server-simulated rigid body; clients are replicated proxies (movement replication).
 * Held = attached to a character's hand point with physics off; thrown = detached + impulse.
 * All state changes happen on the SERVER (driven by APPPeachBasketGame).
 */
UCLASS()
class PEACHPARTY_API APPBasketBall : public AActor
{
	GENERATED_BODY()

public:
	APPBasketBall();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** SERVER. Attach to a hand point and stop simulating. */
	void GrabBy(APPBasketCharacter* NewHolder, USceneComponent* HandPoint);

	/** SERVER. Detach, resume simulating, and launch. */
	void ThrowWithImpulse(const FVector& Impulse);

	/** SERVER. Snap back to a transform with zero velocity (post-score reset). */
	void ResetTo(const FTransform& Transform);

	APPBasketCharacter* GetHolder() const { return Holder; }
	bool IsHeld() const { return Holder != nullptr; }

	UStaticMeshComponent* GetMesh() const { return Mesh; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	UStaticMeshComponent* Mesh;

	/** Replicated so clients can suppress local prediction while it's carried. */
	UPROPERTY(Replicated)
	APPBasketCharacter* Holder = nullptr;
};
