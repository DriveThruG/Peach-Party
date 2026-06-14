#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/PPTypes.h"
#include "Final/PPClassTypes.h"
#include "PPCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class USceneComponent;
class APPWaterProjectile;
class APPGrabbableObject;

/**
 * First-person player character for the Final fight.
 *
 * Movement: walk / sprint / jump / crouch, all networked (CharacterMovementComponent gives
 * server-authoritative, client-predicted movement). The capsule blocks world geometry.
 *
 * Combat (Final phase only): LMB shoots a water projectile (or throws a held object); RMB grabs/drops
 * an object. Class stats (speed/fire-rate/ammo/…) are applied on spawn from the chosen class.
 */
UCLASS()
class PEACHPARTY_API APPCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APPCharacter();

	virtual void BeginPlay() override;
	virtual void PawnClientRestart() override; // owning client: force game input + hide cursor
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** SERVER. Called by a water projectile that hit this (enemy) player. */
	void ApplyWetness(float Amount, EPPTeam InstigatorTeam);

	/** SERVER. Refill stations top up water ammo (clamped to the class capacity). */
	void AddAmmo(int32 Amount);

	bool IsHoldingObject() const { return bIsHolding; }

protected:
	// ---- Movement / look ----
	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnYaw(float Value);
	void LookPitch(float Value);

	void OnJumpPressed();
	void OnJumpReleased();
	/** Toggle sprint (tap, don't hold) — avoids the W+Shift+Space keyboard-ghosting jam. */
	void ToggleSprint();
	void OnCrouchPressed();
	void OnCrouchReleased();

	// ---- Class pick fallback (keys 1-4) so the flow is testable before WBP_ClassSelect exists ----
	void OnSelectClass1();
	void OnSelectClass2();
	void OnSelectClass3();
	void OnSelectClass4();
	void SelectClass(EPPClass NewClass); // routes to the PlayerController's ServerSelectClass

	/** Server RPC so the authority also applies the sprint speed. */
	UFUNCTION(Server, Reliable)
	void ServerSetSprint(bool bNewSprinting);

	void ApplyMovementSpeed();

	// ---- Final-phase combat ----
	/** True only during the Final phase (combat enabled). */
	bool IsFightLive() const;

	/** Class stats with the team reward applied (None in this build = base stats). */
	FPPClassStats GetEffectiveStats() const;

	/** Apply the player's class stats (movement speed, ammo). Called on spawn. */
	void ApplyClassStats();

	void OnFirePressed();    // LMB down: throw held object, else START autofire
	void OnFireReleased();   // LMB up: stop autofire
	void TryFire();          // one autofire tick (server rate-gates); stops itself if invalid
	void OnGrabPressed();    // RMB: grab an object, or drop the held one

	FTimerHandle AutoFireTimer;

	UFUNCTION(Server, Reliable)
	void ServerFire();

	/** RMB: grab a nearby object via a trace, or drop the one we're holding. */
	UFUNCTION(Server, Reliable)
	void ServerGrabOrDrop();

	/** LMB while holding: throw the object along the aim. */
	UFUNCTION(Server, Reliable)
	void ServerThrow();

	/** Played on all clients when this player slips (wetness 100): cosmetic + lock movement. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSlip();

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Final")
	void BP_OnSlip();

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Combat")
	TSubclassOf<APPWaterProjectile> WaterProjectileClass;

	int32 CurrentAmmo = 100;
	float LastFireServerTime = -100.f;

	// ---- Gravity object (grab/throw) ----
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Object")
	USceneComponent* HoldPoint; // where a grabbed object floats in front of the player

	UPROPERTY(Replicated)
	bool bIsHolding = false; // can't shoot while true

	UPROPERTY()
	APPGrabbableObject* HeldObject = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Object")
	float GrabDistance = 350.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Object")
	float ThrowImpulse = 1600.f;

	// ---- Camera ----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FirstPersonCamera;

	/** Placeholder body so OTHER players can see you (hidden for the local first-person owner). */
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Placeholder")
	UStaticMeshComponent* BodyMesh;

	// ---- Movement tuning ----
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Movement")
	float WalkSpeed = 420.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Movement")
	float SprintSpeed = 720.f;

	/** Replicated so simulated proxies could match (position replicates regardless). */
	UPROPERTY(Replicated)
	bool bIsSprinting = false;
};
