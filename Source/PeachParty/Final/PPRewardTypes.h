#pragma once

#include "CoreMinimal.h"
#include "PPRewardTypes.generated.h"

/**
 * Team reward chosen after the minigames (applies to the WHOLE team for the entire final phase).
 * Each is a flat +10% modifier. Kept as an enum (simple, replicated, no DataTable needed).
 */
UENUM(BlueprintType)
enum class EPPReward : uint8
{
	None	UMETA(DisplayName = "None"),
	Speed	UMETA(DisplayName = "+10% Speed"),
	Ammo	UMETA(DisplayName = "+10% Ammo"),
	Wetness	UMETA(DisplayName = "+10% Wetness per hit"),   // "damage"
	Health	UMETA(DisplayName = "+10% Wetness capacity")    // can take more water before slipping
};

namespace PPReward
{
	constexpr float Bonus = 1.10f; // +10%
}
