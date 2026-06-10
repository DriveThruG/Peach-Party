#pragma once

#include "CoreMinimal.h"
#include "Minigame/PPMinigameBase.h"
#include "Minigame/PPArtilleryTypes.h"
#include "PPPeachArtilleryGame.generated.h"

class UStaticMeshComponent;
class APPTank;
class APPProjectile;
class APPPlayerState;

/**
 * Peach Artillery — strictly turn-based tank duel. Only the active player's pre-shot decisions
 * (move/fuel, aim, power, weapon) and fire are accepted; after firing the shell's physics decide
 * the outcome and the turn passes.
 *
 * Server-authoritative + turn-synchronised: ActiveSlot and bTurnInProgress are server-owned and
 * replicated, so every client agrees on whose turn it is and when input is locked. Tanks + shells
 * replicate their own state. Win = enemy tank destroyed; time-out = higher remaining health.
 */
UCLASS()
class PEACHPARTY_API APPPeachArtilleryGame : public APPMinigameBase
{
	GENERATED_BODY()

public:
	APPPeachArtilleryGame();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void HandleInput(APPPlayerState* Player, FName Action, bool bPressed) override;

	/** Winner decided by remaining health (time-out) — overrides the score-based base rule. */
	virtual EMatchResult ForceResolve() const override;

	/** SERVER. Called by a shell when it lands: apply area damage, then resolve the turn. */
	void OnProjectileImpact(const FVector& ImpactLocation, int32 WeaponIndex);

	UFUNCTION(BlueprintPure, Category = "PeachParty|Artillery")
	int32 GetActiveSlot() const { return ActiveSlot; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Artillery")
	bool IsTurnInProgress() const { return bTurnInProgress; }

protected:
	virtual void OnMinigameStarted() override;
	virtual void OnMinigameFinished() override;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Artillery")
	TArray<FPPWeaponData> Weapons;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Artillery")
	TSubclassOf<APPTank> TankClass;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Artillery")
	TSubclassOf<APPProjectile> ProjectileClass;

	/** This game brings its own ground (arenas are empty space). */
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Artillery")
	UStaticMeshComponent* Floor;

	/** 1 = Player1's turn, 2 = Player2's turn. */
	UPROPERTY(ReplicatedUsing = OnRep_Turn, BlueprintReadOnly, Category = "PeachParty|Artillery")
	int32 ActiveSlot = 1;

	/** True while a shell is in flight: all input is locked. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Artillery")
	bool bTurnInProgress = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Artillery")
	APPTank* Tank1 = nullptr;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Artillery")
	APPTank* Tank2 = nullptr;

	UFUNCTION()
	void OnRep_Turn();

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Artillery")
	void BP_OnTurnChanged(int32 NewActiveSlot);

private:
	APPTank* ActiveTank() const;
	APPPlayerState* ActivePlayer() const;
	void FireActiveTank();
	void SwitchTurn();
	void ApplyExplosion(const FVector& Center, int32 WeaponIndex);
};
