#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PPCharacter.generated.h"

class UCameraComponent;

/**
 * Minimal player pawn for the basket-only build. There is no 3D world to walk in: the pawn exists
 * purely to receive the single basket input (Space = "Primary") and forward it to the active match,
 * and to be a fallback camera before the match starts. The match itself is drawn by a full-screen
 * UMG widget, so movement/look are intentionally absent.
 */
UCLASS()
class PEACHPARTY_API APPCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APPCharacter();

	virtual void PawnClientRestart() override; // owning client: force game input + hide cursor
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	// Basket input (Space): pressed = jump/charge, released = throw. Forwarded to the active match.
	void OnMG_PrimaryPressed();
	void OnMG_PrimaryReleased();

	/** True if this player is currently in a match (reads replicated PlayerState). */
	bool IsInMinigame() const;

	/** Send an action to the server only when actually in a match. */
	void ForwardMinigameInput(FName Action, bool bPressed);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FirstPersonCamera;
};
