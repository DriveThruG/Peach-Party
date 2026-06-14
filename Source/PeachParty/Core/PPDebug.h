#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"

/**
 * Tiny debug-print helper: writes to the Output Log AND the on-screen debug area.
 * Header-only so any .cpp can include it without a new module dependency.
 *
 * Key: -1 = append a new line; >=0 = update that fixed slot in place (use for per-frame values
 * like capture progress / ammo so they don't spam the screen).
 */
namespace PPDebug
{
	inline void Print(const FString& Msg, const FColor& Color = FColor::White, float Time = 4.f, int32 Key = -1)
	{
		UE_LOG(LogTemp, Log, TEXT("[PP] %s"), *Msg);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(Key, Time, Color, FString::Printf(TEXT("[PP] %s"), *Msg));
		}
	}
}
