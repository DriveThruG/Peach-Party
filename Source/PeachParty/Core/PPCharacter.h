#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PPCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class IPPInteractable;

/**
 * The player's body in the 3D hub. Third-person in the hub; the final phase swaps to the
 * first-person camera (toggle CameraMode / detach the spring arm in the Final phase).
 *
 * Interaction is CLIENT-driven discovery + SERVER-authoritative execution:
 *  - Tick (owning client only) traces ahead for an IPPInteractable and tracks focus.
 *  - Interact() sends the focused actor to the server via the controller's Server RPC,
 *    OR, if already seated, asks the server to stand up. One key does both.
 */
UCLASS()
class PEACHPARTY_API APPCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APPCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	// ---- Movement / look (legacy axis bindings; see DefaultInput.ini) ----
	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnYaw(float Value);
	void LookPitch(float Value);

	/** Interact pressed: sit at the focused PC, or stand up if already seated. */
	void OnInteractPressed();

	/** Spectator camera cycling (only effective once your own 1v1 is finished). */
	void OnSpectateNext();
	void OnSpectatePrev();

	/** Owning-client trace that updates the currently focused interactable. */
	void UpdateFocus();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FollowCamera;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Interaction")
	float InteractTraceDistance = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Interaction")
	float InteractTraceRadius = 25.f;

private:
	/** Client-only focus bookkeeping. */
	TWeakObjectPtr<AActor> FocusedActor;
};
