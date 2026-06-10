#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/PPTypes.h"
#include "PPBasketCharacter.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class USceneComponent;
class APPPlayerState;

/**
 * One of a player's TWO peach characters in Peach Basket. A wobbly server-simulated physics capsule
 * (no uprighting -> it tips, rotates and bumps the others = the intended chaos). Not possessed:
 * the owning player's single input drives both of their characters via APPPeachBasketGame.
 *
 * THROW = CHARGE: while Space is held the character "charges" — ArmAngle rises slowly from 0 toward
 * MaxArmAngle. The angle at RELEASE sets the throw elevation: too low (just grabbed) = weak/short,
 * near the sweet spot = a clean arc into the basket. The horizontal direction is the body's current
 * (possibly tilted) facing, so you also have to be oriented right — that's the chaos.
 *
 * Server simulates physics + advances ArmAngle; clients are proxies. Only bCharging replicates, so
 * clients animate the arm raise locally; the precise angle is server-only and authoritative.
 */
UCLASS()
class PEACHPARTY_API APPBasketCharacter : public AActor
{
	GENERATED_BODY()

public:
	APPBasketCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** SERVER. Bind owner/team. */
	void InitCharacter(APPPlayerState* InOwner, EPPTeam InTeam);

	/** SERVER. Jump along the current (possibly tilted) body orientation. */
	void DoJump(float UpImpulse, float ForwardImpulse);

	/** SERVER. Begin charging: arms start rising from 0. (Also call DoJump on the same press.) */
	void StartCharge();

	/** SERVER. Stop charging; returns the arm angle (deg) reached, then lowers arms. */
	float StopCharge();

	/** SERVER. Teleport to a transform, zero velocities, lower arms (post-score reset). */
	void ResetTo(const FTransform& Transform);

	/** Arms are up (charging) -> this character can grab / hold / steal. */
	bool IsCharging() const { return bCharging; }

	/** Server-authoritative arm angle in degrees (0 = down). */
	float GetArmAngleDeg() const { return ArmAngleDeg; }

	EPPTeam GetTeam() const { return Team; }
	APPPlayerState* GetOwningPlayer() const { return OwningPlayer; }

	USceneComponent* GetHandPoint() const { return HandPoint; }
	FVector GetHandLocation() const;

	/** Horizontal facing (body forward projected to ground), used as throw azimuth. */
	FVector GetThrowDirection() const;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float MaxArmAngleDeg = 80.f;

	/** "Slowly" — ~0.6s of holding to reach a good angle. Tune to taste. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float ArmRaiseRateDegPerSec = 120.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	UCapsuleComponent* Body;        // simulating physics root

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	UStaticMeshComponent* Visual;   // cosmetic, non-colliding

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	USceneComponent* HandPoint;     // ball anchor / grab measure point

	UPROPERTY(ReplicatedUsing = OnRep_Charging)
	bool bCharging = false;

	/** SERVER only. Continuous; not replicated (clients animate from bCharging). */
	float ArmAngleDeg = 0.f;

	UPROPERTY()
	APPPlayerState* OwningPlayer = nullptr;

	EPPTeam Team = EPPTeam::None;

	UFUNCTION()
	void OnRep_Charging();

	/** BP hook: start/stop the local arm-raise animation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Basket")
	void BP_OnChargingChanged(bool bNowCharging);
};
