#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class APPPeachArtilleryGame;

/**
 * A fired shell. After launch the server's UProjectileMovementComponent owns the trajectory
 * (gravity arc); movement replication shows the arc on clients. On the first blocking hit it
 * reports the impact point + weapon to its owning game, which applies area damage, then destroys.
 *
 * Server-authoritative: only the server reacts to the hit. Clients just watch it fly.
 */
UCLASS()
class PEACHPARTY_API APPProjectile : public AActor
{
	GENERATED_BODY()

public:
	APPProjectile();

	/** SERVER. Launch with a velocity; remember who to report the impact to. IgnoredShooter avoids self-hit. */
	void Launch(const FVector& Velocity, APPPeachArtilleryGame* InGame, int32 InWeaponIndex, AActor* IgnoredShooter);

protected:
	/** Lifespan-expiry safety: if we die without a hit, still report a (harmless) miss so the turn advances. */
	virtual void Destroyed() override;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Artillery")
	USphereComponent* Collision;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Artillery")
	UStaticMeshComponent* Mesh;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Artillery")
	UProjectileMovementComponent* Movement;

	UPROPERTY()
	APPPeachArtilleryGame* OwnerGame = nullptr;

	int32 WeaponIndex = 0;
	bool bImpacted = false;
};
