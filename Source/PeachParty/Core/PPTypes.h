#pragma once

#include "CoreMinimal.h"
#include "PPTypes.generated.h"

/**
 * High-level match flow. Owned + advanced by APPGameMode (server),
 * replicated to all clients via APPGameState::CurrentPhase.
 */
UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	None		UMETA(DisplayName = "None"),
	Lobby		UMETA(DisplayName = "Lobby"),		// players in hub, sit at PCs to ready up
	Minigame	UMETA(DisplayName = "Minigame"),	// 2D minigame at the PC screens
	Reward		UMETA(DisplayName = "Reward"),		// team advantages handed out
	Final		UMETA(DisplayName = "Final"),		// first-person combat phase
	PostMatch	UMETA(DisplayName = "PostMatch")
};

/** Two balanced teams. EPPTeam::None means "not yet assigned". */
UENUM(BlueprintType)
enum class EPPTeam : uint8
{
	None	UMETA(DisplayName = "Unassigned"),
	TeamA	UMETA(DisplayName = "Team A"),
	TeamB	UMETA(DisplayName = "Team B")
};
