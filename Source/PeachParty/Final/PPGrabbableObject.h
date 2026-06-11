#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/PPTypes.h"
#include "PPGrabbableObject.generated.h"

class UStaticMeshComponent;
class APPCharacter;

/**
 * A throwable physics object (a school item — monitor, chair, ...). Server-authoritative.
 * Grab = physics off + attached to the holder's hold point. Throw = physics on + impulse; on a fast
 * enemy hit it converts impact speed into wetness (+ knockback). Friendly hits do nothing.
 */
UCLASS()
class PEACHPARTY_API APPGrabbableObject : public AActor
{
	GENERATED_BODY()

public:
	APPGrabbableObject();

	bool IsHeld() const { return Holder != nullptr; }

	/** SERVER. Pick up: stop simulating + attach to the hold point. */
	void Grab(APPCharacter* NewHolder, USceneComponent* HoldPoint);

	/** SERVER. Drop straight down (no throw). */
	void Drop();

	/** SERVER. Throw with an impulse; arms wetness-on-impact for the thrower's enemies. */
	void ThrowWithImpulse(const FVector& Impulse, EPPTeam ThrowerTeam);

protected:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	/** Cosmetic impact reaction (spawn dust/sound) on clients. */
	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Object")
	void BP_OnImpact(FVector Location);

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Object")
	UStaticMeshComponent* Mesh;

	/** Wetness per (cm/s) of impact speed above the threshold, clamped. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Object")
	float WetnessPerSpeed = 0.04f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Object")
	float MinImpactSpeed = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Object")
	float MaxWetnessHit = 35.f;

	UPROPERTY()
	APPCharacter* Holder = nullptr;

	EPPTeam ThrowerTeam = EPPTeam::None;
	bool bThrown = false;
};
