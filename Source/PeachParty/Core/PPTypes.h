#pragma once

#include "CoreMinimal.h"
#include "PPTypes.generated.h"

/**
 * High-level match flow for the stripped Peach-Basket-only build. Owned + advanced by APPGameMode
 * (server), replicated to all clients via APPGameState::CurrentPhase.
 */
UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	None				UMETA(DisplayName = "None"),
	WaitingForPlayers	UMETA(DisplayName = "Waiting For Players"),	// not enough players yet
	Playing				UMETA(DisplayName = "Playing")				// the 1v1 basket match is live
};

/** Two teams. EPPTeam::None means "not yet assigned". */
UENUM(BlueprintType)
enum class EPPTeam : uint8
{
	None	UMETA(DisplayName = "Unassigned"),
	TeamA	UMETA(DisplayName = "Team A"),
	TeamB	UMETA(DisplayName = "Team B")
};
