#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPObjectiveRoom.generated.h"

class UBoxComponent;
class USceneComponent;

/**
 * One of the 3 sequential frontline objectives. Place 3 in the level with RoomIndex 1/2/3.
 *
 * Only the ACTIVE room (set by the GameMode) can be contested, and rooms capture strictly in order.
 * Capture is server-authoritative: while active, the room sums attackers vs defenders standing in its
 * zone each tick. Attackers with no defenders present advance the bar (faster with more attackers and
 * with Engineers — `CaptureSpeedMul`); a defender present stalls/reverses it (interrupt/defuse). At
 * 100% the room reports to the GameMode, which locks it and shifts the frontline to the next room.
 *
 * Carries the attacker/defender spawn points for this stage so the GameMode can reposition both teams.
 */
UCLASS()
class PEACHPARTY_API APPObjectiveRoom : public AActor
{
	GENERATED_BODY()

public:
	APPObjectiveRoom();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	int32 GetRoomIndex() const { return RoomIndex; }
	bool IsActive() const { return bIsActive; }
	bool IsCaptured() const { return bCaptured; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Final")
	float GetCaptureProgress() const { return CaptureProgress; }

	/** SERVER. The GameMode opens exactly one room at a time (frontline). */
	void SetActive(bool bNewActive);

	FTransform GetAttackerSpawn() const;
	FTransform GetDefenderSpawn() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PeachParty|Final")
	int32 RoomIndex = 1; // 1..3, set per placed instance

	// EditAnywhere so you can tune EACH placed room live in the level Details panel.
	UPROPERTY(EditAnywhere, Category = "PeachParty|Final")
	float CaptureRatePerSec = 0.10f;   // 1 attacker, base: ~10s to capture (raise for faster)

	UPROPERTY(EditAnywhere, Category = "PeachParty|Final")
	float DefuseRatePerSec = 0.15f;    // defenders reverse it faster than one attacker fills

	UPROPERTY(EditAnywhere, Category = "PeachParty|Final")
	float CaptureRadius = 400.f;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Final")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Final")
	UBoxComponent* CaptureZone;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Final")
	USceneComponent* AttackerSpawnPoint;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Final")
	USceneComponent* DefenderSpawnPoint;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Final")
	bool bIsActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_Captured, BlueprintReadOnly, Category = "PeachParty|Final")
	bool bCaptured = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Final")
	float CaptureProgress = 0.f; // 0..1

	UFUNCTION()
	void OnRep_Captured();

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Final")
	void BP_OnCaptured();
};
