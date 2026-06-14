#pragma once

#include "CoreMinimal.h"
#include "PPArtilleryTypes.generated.h"

/**
 * One artillery weapon. Kept deliberately small + data-only for stability — no special behaviours
 * wired yet (multi-shot/guided/terrain destruction are noted as future work, not implemented).
 */
USTRUCT(BlueprintType)
struct FPPWeaponData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName Name = TEXT("Shell");

	/** Damage at a direct hit; falls off to 0 at ExplosionRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	int32 Damage = 40;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float ExplosionRadius = 220.f;

	/** Launch speed at 100% power (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float MaxSpeed = 1700.f;
};
