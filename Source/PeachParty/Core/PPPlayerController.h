#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PPPlayerController.generated.h"

class APPPCStation;

/**
 * Owns the interaction request path and the local camera switch.
 *
 * Interaction flow:
 *   1. Owning client traces for an IPPInteractable (done in APPCharacter).
 *   2. Client calls ServerRequestInteract(TargetActor) (Server RPC, validated).
 *   3. Server runs the interactable's authoritative ServerInteract.
 *   4. Server sets SeatedStation on this controller (replicated to owner only).
 *   5. OnRep_View on the owning client blends the camera to/from the PC.
 *
 * Camera is purely local & cosmetic. Only the seating *state* is networked, so there is
 * no camera RPC spam and no authority needed for the view target.
 */
UCLASS()
class PEACHPARTY_API APPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Client -> Server: "I want to interact with this actor." Server re-validates. */
	UFUNCTION(Server, Reliable)
	void ServerRequestInteract(AActor* TargetActor);

	/** Client -> Server: "Stand up from whatever PC I'm at." Fast exit back to the 3D world. */
	UFUNCTION(Server, Reliable)
	void ServerLeaveStation();

	/** SERVER only. Set by APPPCStation when (un)seating this player. */
	void SetSeatedStation(APPPCStation* NewStation);

	/**
	 * SERVER only. Highest-priority view target: the active minigame (while playing) or a
	 * spectated instance's minigame. Null = fall back to the seated PC, then the pawn.
	 */
	void SetServerViewTarget(AActor* NewTarget);

	/** Client -> Server: cycle the spectator camera (Dir = +1/-1). Gated: own match must be done. */
	UFUNCTION(Server, Reliable)
	void ServerCycleSpectate(int32 Dir);

	/** Client -> Server: routed gameplay input for the minigame this player is currently in. */
	UFUNCTION(Server, Reliable)
	void ServerMinigameInput(FName Action, bool bPressed);

	UFUNCTION(BlueprintPure, Category = "PeachParty|Interaction")
	APPPCStation* GetSeatedStation() const { return SeatedStation; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Interaction")
	bool IsSeated() const { return SeatedStation != nullptr; }

protected:
	/** Replicated to the OWNER only — nobody else needs our local camera state. */
	UPROPERTY(ReplicatedUsing = OnRep_View)
	APPPCStation* SeatedStation = nullptr;

	/** Higher-priority view target (minigame / spectated minigame). Owner-only. */
	UPROPERTY(ReplicatedUsing = OnRep_View)
	AActor* ServerViewTarget = nullptr;

	UFUNCTION()
	void OnRep_View();

	/** Pick the right camera from current state and blend to it. Priority below. */
	void RefreshViewTarget(float BlendTime = 0.35f);

	/** Server-side transient: index into GameState's instance list while spectating. */
	int32 SpectateIndex = 0;
};
