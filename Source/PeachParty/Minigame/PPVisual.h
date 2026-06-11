#pragma once

#include "CoreMinimal.h"
#include "Core/PPTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

/**
 * Tiny filler-visual helpers. The minigames are meant to look 2D; until real Paper2D sprite assets
 * exist, we use thin camera-facing quads (flattened cubes) tinted with a dynamic material so they
 * read as flat coloured shapes from the side camera. Swap each visual UStaticMeshComponent for a
 * UPaperSpriteComponent later — call sites only set the tint, so the swap is local.
 *
 * NOTE: tinting relies on the mesh material exposing a "Color"/"BaseColor" parameter. The engine
 * BasicShapes material may not, in which case shapes show in their default colour — assign a coloured
 * material in a BP child if so. The colours still help; team A = blue, team B = red.
 */
namespace PPVisual
{
	inline void Tint(UStaticMeshComponent* Comp, const FLinearColor& Color)
	{
		if (!Comp)
		{
			return;
		}
		if (UMaterialInstanceDynamic* MID = Comp->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
		}
	}

	inline FLinearColor TeamColor(EPPTeam Team)
	{
		switch (Team)
		{
		case EPPTeam::TeamA: return FLinearColor(0.10f, 0.35f, 1.00f); // blue
		case EPPTeam::TeamB: return FLinearColor(1.00f, 0.20f, 0.15f); // red
		default:             return FLinearColor(0.60f, 0.60f, 0.60f);
		}
	}
}
