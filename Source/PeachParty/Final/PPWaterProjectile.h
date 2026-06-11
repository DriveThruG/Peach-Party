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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	/** Tint the water blob to the shooter's team colour (runs on every client). */
	UFUNCTION()
	void OnRep_Team();

	/** Cosmetic team-coloured splash on impact (runs on all clients). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastImpact(FVector Location);

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Combat")
	void BP_OnImpact(FVector Location, EPPTeam Team);

	UPROPERTY(VisibleAnywhere) USphereComponent* Collision;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
	UPROPERTY(VisibleAnywhere) UProjectileMovementComponent* Movement;

	UPROPERTY(ReplicatedUsing = OnRep_Team)
	EPPTeam InstigatorTeam = EPPTeam::None;

	float WetnessAmount = 8.f;
	bool bImpacted = false;
};
