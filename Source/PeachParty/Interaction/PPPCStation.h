#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PPInteractable.h"
#include "PPPCStation.generated.h"

class UStaticMeshComponent;
class UCameraComponent;
class USceneComponent;
class APPPlayerState;
class APPPlayerController;

/**
 * A "PC" the players sit at. Reused across phases:
 *   - Lobby:    sitting = ready up. Standing = un-ready.
 *   - Minigame: sitting = play the 2D minigame shown on ScreenMesh.
 *
 * The MinigameCamera frames ScreenMesh; when a player is seated, their controller blends
 * the view to this actor (which auto-uses MinigameCamera). The pawn never leaves the world,
 * so standing up is an instant return to the 3D hub.
 *
 * Replication strategy:
 *  - OccupantPlayerState: ReplicatedUsing=OnRep_Occupant so ALL clients can show who is
 *    sitting here (name plate, screen-on glow). Server authoritative; one pointer.
 *  - Seating mutations happen ONLY on the server (ServerInteract / ServerReleaseOccupant).
 */
UCLASS()
class PEACHPARTY_API APPPCStation : public AActor, public IPPInteractable
{
	GENERATED_BODY()

public:
	APPPCStation();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- IPPInteractable (SERVER) ----
	virtual void ServerInteract(APPPlayerController* InteractingController) override;
	virtual bool CanInteract(APPPlayerController* InteractingController) const override;
	virtual void OnBeginFocus() override;
	virtual void OnEndFocus() override;

	/** SERVER. Free the seat and clear the occupant's seated/ready state. */
	void ServerReleaseOccupant();

	UFUNCTION(BlueprintPure, Category = "PeachParty|Station")
	bool IsOccupied() const { return OccupantPlayerState != nullptr; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Station")
	APPPlayerState* GetOccupant() const { return OccupantPlayerState; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Station")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Station")
	UStaticMeshComponent* DeskMesh;

	/** The quad the 2D minigame is drawn onto (sprites / render target material). */
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Station")
	UStaticMeshComponent* ScreenMesh;

	/** Frames ScreenMesh. Becomes the player's view target while seated. ORTHOGRAPHIC (2D look). */
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Station")
	UCameraComponent* MinigameCamera;

	/** Orthographic width of the seated view = how much around the monitor is visible (smaller = zoomed in). */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Station")
	float SeatedOrthoWidth = 300.f;

	/** Where the pawn is parked while seated (snap point / anim target). */
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Station")
	USceneComponent* SeatPoint;

	UPROPERTY(ReplicatedUsing = OnRep_Occupant, BlueprintReadOnly, Category = "PeachParty|Station")
	APPPlayerState* OccupantPlayerState = nullptr;

	UFUNCTION()
	void OnRep_Occupant();

	// ---- Blueprint integration points ----

	/** Fired on all clients when occupancy changes: screen on/off, name plate, sit anim. */
	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Station")
	void BP_OnOccupantChanged(APPPlayerState* NewOccupant);

	/** Focus highlight (local cosmetic). */
	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Station")
	void BP_OnFocusChanged(bool bFocused);

private:
	/** SERVER helper: seat the given controller and (in Lobby) mark them ready. */
	void SeatController(APPPlayerController* Controller);
};
