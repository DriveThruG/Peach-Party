#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PPInteractable.generated.h"

class APPPlayerController;

UINTERFACE(MinimalAPI, BlueprintType)
class UPPInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything a player can walk up to and press Interact on (PC stations, doors, etc.).
 *
 * Contract:
 *  - Focus (OnBeginFocus/OnEndFocus) is CLIENT-side cosmetic (highlight/outline).
 *  - ServerInteract runs on the SERVER only. The owning client requests it through
 *    APPPlayerController::ServerRequestInteract; the server then re-validates and calls
 *    this. Never trust the client for the actual state change.
 */
class IPPInteractable
{
	GENERATED_BODY()

public:
	/** SERVER authoritative. Perform the actual interaction state change here. */
	virtual void ServerInteract(APPPlayerController* InteractingController) = 0;

	/** SERVER authoritative. Cheap gate so we can reject before mutating state. */
	virtual bool CanInteract(APPPlayerController* InteractingController) const { return true; }

	/** CLIENT cosmetic focus hooks. */
	virtual void OnBeginFocus() {}
	virtual void OnEndFocus() {}
};
