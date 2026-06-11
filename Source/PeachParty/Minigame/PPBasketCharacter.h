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
 * One of a player's TWO peach characters in Peach Basket. Physics is a wobbly server-simulated capsule
 * (no uprighting); the VISUAL is 2D filler: a flat body+head quad plus a separate arms quad that
 * rotates up while charging (matching the requested two-part sprite layout). Team-tinted (A blue / B red).
 *
 * THROW = CHARGE: while Space is held ArmAngleDeg rises (server). The angle at RELEASE sets the throw
 * elevation. Horizontal direction = body facing (tilt = chaos). Only bCharging + Team replicate; the
 * precise angle is server-only and the client animates the arm quad locally from bCharging.
 */
UCLASS()
class PEACHPARTY_API APPBasketCharacter : public AActor
{
	GENERATED_BODY()

public:
	APPBasketCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** SERVER. Bind owner/team (also tints the body on the server). */
	void InitCharacter(APPPlayerState* InOwner, EPPTeam InTeam);

	void DoJump(float UpImpulse, float ForwardImpulse);
	void StartCharge();
	float StopCharge();
	void ResetTo(const FTransform& Transform);

	bool IsCharging() const { return bCharging; }
	float GetArmAngleDeg() const { return ArmAngleDeg; }
	EPPTeam GetTeam() const { return Team; }
	APPPlayerState* GetOwningPlayer() const { return OwningPlayer; }

	USceneComponent* GetHandPoint() const { return HandPoint; }
	FVector GetHandLocation() const;
	FVector GetThrowDirection() const;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float MaxArmAngleDeg = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float ArmRaiseRateDegPerSec = 120.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	UCapsuleComponent* Body;          // simulating physics root (collision only)

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	UStaticMeshComponent* BodyMesh;   // flat body+head quad (visual)

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	UStaticMeshComponent* ArmsMesh;   // flat arms quad (rotates while charging)

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	USceneComponent* HandPoint;

	UPROPERTY(ReplicatedUsing = OnRep_Charging)
	bool bCharging = false;

	/** Replicated so clients can team-tint. */
	UPROPERTY(ReplicatedUsing = OnRep_Team)
	EPPTeam Team = EPPTeam::None;

	float ArmAngleDeg = 0.f;     // SERVER gameplay angle
	float VisualArmAngle = 0.f;  // LOCAL eased visual angle (all machines)

	UPROPERTY()
	APPPlayerState* OwningPlayer = nullptr;

	UFUNCTION()
	void OnRep_Charging();

	UFUNCTION()
	void OnRep_Team();

	void ApplyTeamColor();

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Basket")
	void BP_OnChargingChanged(bool bNowCharging);
};
