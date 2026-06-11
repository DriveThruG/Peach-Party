#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/PPTypes.h"
#include "PPTank.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class APPPlayerState;

/**
 * A player's tank. Kinematic (no physics — moved by SetActorLocation), which keeps turn-based state
 * trivially deterministic and easy to replicate. All player-facing state replicates for the HUD.
 * Only the active player's input mutates it, all on the SERVER (driven by APPPeachArtilleryGame).
 */
UCLASS()
class PEACHPARTY_API APPTank : public AActor
{
	GENERATED_BODY()

public:
	APPTank();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** SERVER. FacingSign = +1 (faces +X) or -1 (faces -X). */
	void InitTank(APPPlayerState* InOwner, EPPTeam InTeam, float InFacingSign);

	// ---- SERVER actions (pre-shot decisions) ----
	void MoveStep(float InputDir);   // +1 / -1; consumes fuel
	void AimStep(float DeltaDeg);    // adjust elevation
	void PowerStep(float DeltaPct);  // adjust power
	void SetWeaponIndex(int32 Index, int32 NumWeapons);
	void NextWeapon(int32 NumWeapons);

	/** SERVER. Apply damage (game computes the amount from the explosion). */
	void ApplyDamage(int32 Amount);

	bool IsAlive() const { return Health > 0; }
	int32 GetHealth() const { return Health; }
	int32 GetWeaponIndex() const { return WeaponIndex; }
	float GetAimAngleDeg() const { return AimAngleDeg; }
	float GetPowerPercent() const { return PowerPercent; }
	APPPlayerState* GetOwningPlayer() const { return OwningPlayer; }

	FVector GetMuzzleLocation() const;

	/** Launch direction: facing, pitched up by the current aim angle. */
	FVector GetLaunchDirection() const;

	// ---- Tunables ----
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Artillery")
	int32 MaxHealth = 100;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Artillery")
	float MaxFuel = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Artillery")
	float MoveStepDistance = 35.f;   // cm moved per Left/Right press

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Artillery")
	float FuelPerStep = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Artillery")
	float AimStepDeg = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Artillery")
	float PowerStepPct = 5.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Artillery")
	UStaticMeshComponent* Body;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Artillery")
	UStaticMeshComponent* Barrel;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Artillery")
	USceneComponent* Muzzle;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Artillery")
	int32 Health = 100;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Artillery")
	float Fuel = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Aim, BlueprintReadOnly, Category = "PeachParty|Artillery")
	float AimAngleDeg = 45.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Artillery")
	float PowerPercent = 60.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Artillery")
	int32 WeaponIndex = 0;

	UPROPERTY()
	APPPlayerState* OwningPlayer = nullptr;

	UPROPERTY(Replicated)
	float FacingSign = 1.f;

	UPROPERTY(ReplicatedUsing = OnRep_Team)
	EPPTeam Team = EPPTeam::None;

	UFUNCTION()
	void OnRep_Aim();

	UFUNCTION()
	void OnRep_Team();

	void ApplyTeamColor();

	/** Keep the barrel visual matching the aim (server + clients via the replicated angle). */
	void UpdateBarrelVisual();
};
