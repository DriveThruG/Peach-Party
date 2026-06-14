#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "Core/PPTypes.h"
#include "PPTeamPlayerStart.generated.h"

/**
 * A PlayerStart tagged with a team. PLACE THESE IN THE LEVEL (Place Actors panel → search
 * "PPTeamPlayerStart", or drag from the Content Browser's C++ Classes folder) and set `Team` = A or B
 * per instance in the Details panel. APPGameMode::ChoosePlayerStart spawns each player at a start that
 * matches their team. Put as many as you like per team; players are distributed across them.
 *
 * In the editor the direction arrow is tinted by team (A = blue, B = red) so you can tell them apart.
 */
UCLASS()
class PEACHPARTY_API APPTeamPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	APPTeamPlayerStart(const FObjectInitializer& ObjectInitializer);

	/** Which team spawns here. Set per placed instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PeachParty")
	EPPTeam Team = EPPTeam::TeamA;

#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
#endif
};

/** Drag this into the level for a TEAM A spawn (Place Actors → "PPTeamAPlayerStart"). Arrow = blue. */
UCLASS()
class PEACHPARTY_API APPTeamAPlayerStart : public APPTeamPlayerStart
{
	GENERATED_BODY()
public:
	APPTeamAPlayerStart(const FObjectInitializer& ObjectInitializer);
};

/** Drag this into the level for a TEAM B spawn (Place Actors → "PPTeamBPlayerStart"). Arrow = red. */
UCLASS()
class PEACHPARTY_API APPTeamBPlayerStart : public APPTeamPlayerStart
{
	GENERATED_BODY()
public:
	APPTeamBPlayerStart(const FObjectInitializer& ObjectInitializer);
};
