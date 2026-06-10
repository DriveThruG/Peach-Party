#pragma once

#include "CoreMinimal.h"
#include "PPMinigameTypes.generated.h"

/** Which concrete minigame an AMinigameBase actor is. */
UENUM(BlueprintType)
enum class EMinigameType : uint8
{
	None			UMETA(DisplayName = "None"),
	PeachBasket		UMETA(DisplayName = "Peach Basket"),
	PeachArtillery	UMETA(DisplayName = "Peach Artillery")
};

/**
 * Outcome of a single minigame OR of a whole 1v1 instance.
 * Player1/Player2 refer to the instance's two slots (slot 1 = Team A side, slot 2 = Team B side).
 */
UENUM(BlueprintType)
enum class EMatchResult : uint8
{
	Undecided	UMETA(DisplayName = "Undecided"),
	Player1		UMETA(DisplayName = "Player 1"),
	Player2		UMETA(DisplayName = "Player 2"),
	Draw		UMETA(DisplayName = "Draw")
};
