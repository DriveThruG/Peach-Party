#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Final/PPClassTypes.h"
#include "PPPlayerController.generated.h"

/**
 * Owns the class-pick request path and the local fade transition.
 *
 * Class selection:
 *   - While the match is in ClassSelect, the local client shows WBP_ClassSelect (auto-spawned here).
 *   - The widget's 4 buttons call ServerSelectClass(EPPClass) (BlueprintCallable Server RPC).
 *   - The server records the pick on the PlayerState and, once BOTH players have chosen, starts the fight.
 */
UCLASS()
class PEACHPARTY_API APPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PlayerTick(float DeltaTime) override; // local: shows/hides the class-select widget

	/** Client -> Server: pick a class. Server gates it to ClassSelect / respawn windows and, when both
	 *  players have chosen, kicks off the Final phase. Call this from the WBP_ClassSelect buttons. */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "PeachParty|Final")
	void ServerSelectClass(EPPClass NewClass);

	/** Local fade-to-black-and-back. Called on phase transitions for a clean cut. */
	void PlayTransitionFade(float HalfSeconds = 0.25f, bool bHold = false);

protected:
	FTimerHandle FadeTimer;
	void FadeBackIn(float Seconds);

	// ---- Class-select widget (auto shown/hidden on the local client) ----
	UPROPERTY(Transient)
	class UUserWidget* ClassSelectHud = nullptr;

	bool bClassSelectInputActive = false;

	void UpdateClassSelectHud(); // create/remove WBP_ClassSelect to match the phase + pick state
};
