#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PPPlayerController.generated.h"

/**
 * Minimal controller for the basket-only build: forwards the one minigame input to the server,
 * points the local camera at the active match, and auto-shows the UMG basket widget.
 */
UCLASS()
class PEACHPARTY_API APPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PlayerTick(float DeltaTime) override; // local: shows/hides the UMG minigame widget

	/** The minigame the local player is currently viewing. Bind your UMG widget to this. */
	UFUNCTION(BlueprintPure, Category = "PeachParty|Minigame")
	AActor* GetViewedMinigame() const;

	/** SERVER only. View target = the active minigame; null falls back to the pawn. */
	void SetServerViewTarget(AActor* NewTarget);

	/** Client -> Server: routed gameplay input for the match this player is in. */
	UFUNCTION(Server, Reliable)
	void ServerMinigameInput(FName Action, bool bPressed);

	/** Local fade-to-black-and-back on a view change. */
	void PlayTransitionFade(float HalfSeconds = 0.25f);

protected:
	/** Higher-priority view target (the active match). Owner-only. */
	UPROPERTY(ReplicatedUsing = OnRep_View)
	AActor* ServerViewTarget = nullptr;

	UFUNCTION()
	void OnRep_View();

	void RefreshViewTarget(float BlendTime = 0.35f);

	FTimerHandle FadeTimer;
	void FadeBackIn(float Seconds);

	// ---- UMG minigame widget (auto shown/hidden on the local client) ----
	UPROPERTY(Transient)
	class UUserWidget* MinigameHud = nullptr;

	/** C++ debug overlay drawn on top of the basket HUD (rim boxes / ball / arm lines). */
	UPROPERTY(Transient)
	class UUserWidget* DebugHud = nullptr;

	void UpdateMinigameHud(); // create/remove the widget to match GetViewedMinigame()
};
