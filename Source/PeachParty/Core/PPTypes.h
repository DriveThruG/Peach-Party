#pragma once

#include "CoreMinimal.h"
#include "PPTypes.generated.h"

/**
 * High-level match flow. Owned + advanced by APPGameMode (server),
 * replicated to all clients via APPGameState::CurrentPhase.
 *
 * Final-only build: 2 players join -> pick a class -> the frontline fight runs -> a winner.
 */
UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	None		UMETA(DisplayName = "None"),
	ClassSelect	UMETA(DisplayName = "ClassSelect"),	// both players pick a class before the fight
	Final		UMETA(DisplayName = "Final"),		// first-person frontline combat
	PostMatch	UMETA(DisplayName = "PostMatch")	// a team has won
};

/** Two teams. EPPTeam::None means "not yet assigned". */
UENUM(BlueprintType)
enum class EPPTeam : uint8
{
	None	UMETA(DisplayName = "Unassigned"),
	TeamA	UMETA(DisplayName = "Team A"),
	TeamB	UMETA(DisplayName = "Team B")
};
