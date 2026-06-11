#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/PPTypes.h"
#include "PPBasketCharacter.generated.h"

class UCapsuleComponent;
class UPaperSpriteComponent;
class UTexture2D;
class USceneComponent;
class APPPlayerState;

/**
 * One of a player's TWO peach characters in Peach Basket. Physics is a server-simulated capsule
 * locked to the X-Z plane (a true 2D side-view: it can move left/right + up, and only rolls about Y
 * to wobble/tip). The VISUAL is three stacked Paper2D sprites: a shared back arm (Arm_Left), the body
 * (Player0X_Body), and the front arm (Player0X_Arm). Both arms rotate up while charging.
 *
 * SpriteVariant (1..4) picks which Player0X art to show (already team-coloured in the textures, so no
 * tint needed). Replicated so clients show the right sprites. THROW = CHARGE: arm angle at release
 * sets elevation; horizontal direction is toward the enemy basket (FacingSign).
 */
UCLASS()
class PEACHPARTY_API APPBasketCharacter : public AActor
{
	GENERATED_BODY()

public:
	APPBasketCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** SERVER. Bind owner/team and which Player0X art variant (1..4) to show. */
	void InitCharacter(APPPlayerState* InOwner, EPPTeam InTeam, int32 InVariant);

	void DoJump(float UpImpulse, float ForwardImpulse);

	/** True if standing on something (short downward trace) — used to block air jumps. */
	bool IsGrounded() const;

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

	/** Self-righting (weeble) spring: torque about the Y axis to return to upright. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float UprightStrength = 22.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float UprightDamping = 6.f;

	/** How the sprites face the camera. 0 = Paper2D default (faces -Y, toward the side camera). */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	FRotator SpriteFacing = FRotator(0.f, 0.f, 0.f);

	/** Shoulder pivot height (arms hinge here). Capsule centre = 0, top ≈ +88. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float ShoulderZ = 58.f;

	/** How far the arm sprite hangs below the shoulder pivot ≈ half the arm sprite's world height,
	 *  so the arm's TOP edge sits at the shoulder joint. Lower this if the arms float above the body. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket")
	float ArmDropZ = 30.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	UCapsuleComponent* Body;          // simulating physics root (collision only), locked to X-Z plane

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	USceneComponent* ArmPivot;           // shoulder pivot; pitched to raise both arms

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	UPaperSpriteComponent* SpriteBack;   // shared back arm (Arm_Left)

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	UPaperSpriteComponent* SpriteBody;   // Player0X_Body

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	UPaperSpriteComponent* SpriteFront;  // Player0X_Arm (front)

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Basket")
	USceneComponent* HandPoint;

	UPROPERTY(ReplicatedUsing = OnRep_Charging)
	bool bCharging = false;

	UPROPERTY(ReplicatedUsing = OnRep_Variant)
	int32 SpriteVariant = 1;

	UPROPERTY(Replicated)
	EPPTeam Team = EPPTeam::None;

	float ArmAngleDeg = 0.f;     // SERVER gameplay angle
	float VisualArmAngle = 0.f;  // LOCAL eased visual angle (all machines)
	float FacingSign = 1.f;      // +1 = throws toward +X, -1 = toward -X

	UPROPERTY()
	APPPlayerState* OwningPlayer = nullptr;

	// User textures loaded by path (same on every machine); sprites are built from them at runtime.
	UPROPERTY()
	UTexture2D* BodyTextures[4] = { nullptr, nullptr, nullptr, nullptr };

	UPROPERTY()
	UTexture2D* ArmTextures[4] = { nullptr, nullptr, nullptr, nullptr };

	UPROPERTY()
	UTexture2D* BackArmTexture = nullptr;

	UFUNCTION()
	void OnRep_Charging();

	UFUNCTION()
	void OnRep_Variant();

	void ApplySprites();

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Basket")
	void BP_OnChargingChanged(bool bNowCharging);
};
