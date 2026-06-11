#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/PPTypes.h"
#include "PPWaterProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * A water gun projectile (NOT hitscan). Server simulates the arc; movement replication shows it fly.
 * On hitting an ENEMY player it adds wetness (friendly water does nothing). Server-authoritative.
 */
UCLASS()
class PEACHPARTY_API APPWaterProjectile : public AActor
{
	GENERATED_BODY()

public:
	APPWaterProjectile();

	/** SERVER. Launch with a velocity, the shooter's team and the class wetness-per-hit. */
	void Launch(const FVector& Velocity, EPPTeam InInstigatorTeam, float InWetness, AActor* IgnoredShooter);

protected:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere) USphereComponent* Collision;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
	UPROPERTY(VisibleAnywhere) UProjectileMovementComponent* Movement;

	EPPTeam InstigatorTeam = EPPTeam::None;
	float WetnessAmount = 8.f;
	bool bImpacted = false;
};
