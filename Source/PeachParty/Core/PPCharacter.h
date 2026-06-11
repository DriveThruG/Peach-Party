#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PPCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class IPPInteractable;

/**
 * First-person player character for Peach Party.
 *
 * Movement: walk / sprint / jump / crouch, all networked. ACharacter + CharacterMovementComponent
 * give server-authoritative, client-predicted movement out of the box (position, jump and crouch
 * replicate automatically); we only add a replicated sprint flag on top. The capsule blocks world
 * geometry, so walls/objects can't be walked through.
 *
 * Camera: a first-person UCameraComponent at eye height; the body yaws with the controller.
 *
 * Interaction: client-driven discovery (a trace each Tick on the owning client) + server-authoritative
 * execution (the focused actor is sent to the server via the PlayerController's Server RPC).
 *
 * Minigame input is forwarded to the active match only while playing (see ForwardMinigameInput).
 */
UCLASS()
class PEACHPARTY_API APPCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APPCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

	/** Server RPC so the authority also applies the sprint speed. */
	UFUNCTION(Server, Reliable)
	void ServerSetSprint(bool bNewSprinting);

	void ApplyMovementSpeed();

	/** Interact pressed: sit at the focused PC, or stand up if already seated. */
	void OnInteractPressed();

	/** Spectator camera cycling (only effective once your own 1v1 is finished). */
	void OnSpectateNext();
	void OnSpectatePrev();

	// ---- Minigame input forwarding (active only while in a match) ----
	void OnMG_PrimaryPressed();
	void OnMG_PrimaryReleased();
	void OnMG_Left();
	void OnMG_Right();
	void OnMG_Up();
	void OnMG_Down();
	void OnMG_PowerUp();
	void OnMG_PowerDown();
	void OnMG_Weapon();

	/** True if the local player is currently in a 1v1 match (reads replicated PlayerState). */
	bool IsInMinigame() const;

	/** Send an action to the server only when actually in a match (avoids RPC spam in the hub). */
	void ForwardMinigameInput(FName Action, bool bPressed);

	/** Owning-client trace that updates the currently focused interactable. */
	void UpdateFocus();

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

	// ---- Interaction ----
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Interaction")
	float InteractTraceDistance = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Interaction")
	float InteractTraceRadius = 25.f;

private:
	/** Client-only focus bookkeeping. */
	TWeakObjectPtr<AActor> FocusedActor;
};
