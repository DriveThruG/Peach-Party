#include "Core/PPTeamPlayerStart.h"
#include "Components/ArrowComponent.h"

APPTeamPlayerStart::APPTeamPlayerStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

APPTeamAPlayerStart::APPTeamAPlayerStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = EPPTeam::TeamA;
}

APPTeamBPlayerStart::APPTeamBPlayerStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = EPPTeam::TeamB;
}

#if WITH_EDITOR
void APPTeamPlayerStart::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Editor-only: tint the direction arrow by team so placed starts are easy to tell apart.
	if (UArrowComponent* Arrow = GetArrowComponent())
	{
		const FColor C = (Team == EPPTeam::TeamA) ? FColor(40, 90, 255)   // blue
		               : (Team == EPPTeam::TeamB) ? FColor(255, 50, 40)   // red
		               : FColor(160, 160, 160);                            // grey (None)
		Arrow->SetArrowColor(FLinearColor(C));
	}
}
#endif
