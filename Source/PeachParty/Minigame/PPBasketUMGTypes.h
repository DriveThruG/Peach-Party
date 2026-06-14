#pragma once

#include "CoreMinimal.h"
#include "PPBasketUMGTypes.generated.h"

/**
 * One drawable peach character in the UMG basket game. All positions are NORMALISED play-field coords:
 * X 0..1 = left..right, Y 0..1 = bottom..top. The widget maps these onto its canvas
 * (canvasX = Pos.X * Width, canvasY = (1 - Pos.Y) * Height) — flip Y because UMG's Y grows downward.
 */
USTRUCT(BlueprintType)
struct FPPBasketChar
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Basket") FVector2D Pos = FVector2D(0.5, 0.2);
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Basket") int32 Team = 0;     // 1 = A, 2 = B
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Basket") int32 Variant = 1;  // 1..4 art variant
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Basket") bool  bCharging = false;
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Basket") float ArmAngle = 0.f; // 0..1 (raise amount)
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Basket") float Lean = 0.f;     // radians, +/- = tilt (rotate the sprite by this)
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Basket") bool  bHoldsBall = false;
};

/** The full replicated state of one UMG basket match — the widget reads this every frame and draws it. */
USTRUCT(BlueprintType)
struct FPPBasketState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Basket") TArray<FPPBasketChar> Chars;
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Basket") FVector2D Ball = FVector2D(0.5, 0.5);
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Basket") FVector2D HoopLeft = FVector2D(0.10, 0.62);
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Basket") FVector2D HoopRight = FVector2D(0.90, 0.62);
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Basket") int32 ScoreA = 0; // team A (left)
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Basket") int32 ScoreB = 0; // team B (right)
};
