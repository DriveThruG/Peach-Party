#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPRefillStation.generated.h"

class UStaticMeshComponent;

/**
 * Refill station: stand in its radius to refill water ammo over time (no manual reload anywhere else).
 * Server-authoritative; the player is stationary/exposed while refilling. Runner refills faster
 * (class RefillSpeedMul). Place these around the map.
 */
UCLASS()
class PEACHPARTY_API APPRefillStation : public AActor
{
	GENERATED_BODY()

public:
	APPRefillStation();

	virtual void BeginPlay() override;

protected:
	/** SERVER. Top up ammo for everyone standing in the radius. */
	void RefillTick();

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Refill")
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Refill")
	float Radius = 260.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Refill")
	int32 AmmoPerInterval = 10; // x4/sec at the default interval

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Refill")
	float RefillInterval = 0.25f;

	FTimerHandle RefillTimer;
};
