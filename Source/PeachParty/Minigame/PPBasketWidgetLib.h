#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PPBasketWidgetLib.generated.h"

class UWidget;

/** Helpers so the UMG basket widget is just a few nodes: map normalised 0..1 play coords -> canvas pixels. */
UCLASS()
class PEACHPARTY_API UPPBasketWidgetLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Position a widget inside its Canvas Panel from a NORMALISED play-field position (X,Y in 0..1).
	 * Y is flipped (UMG grows downward). Element must be a direct child of a Canvas Panel.
	 * CanvasSize = the canvas pixel size (Self -> Get Cached Geometry -> Get Local Size).
	 */
	UFUNCTION(BlueprintCallable, Category = "PeachParty|Basket")
	static void SetCanvasPos(UWidget* Element, FVector2D NormPos, FVector2D CanvasSize);

	/** Radians -> degrees, for feeding char Lean into Set Render Transform Angle. */
	UFUNCTION(BlueprintPure, Category = "PeachParty|Basket")
	static float RadToDeg(float Radians) { return Radians * 57.29578f; }

	/** Angle (deg, for Set Render Transform Angle) of the arm pointing Shoulder -> Hand. Anchor the arm
	 *  image at the Shoulder (SetCanvasPos) with its render-transform pivot at the LEFT-centre (0, 0.5),
	 *  then rotate by this so it points at the Hand. */
	UFUNCTION(BlueprintPure, Category = "PeachParty|Basket")
	static float ArmAngleDeg(FVector2D Shoulder, FVector2D Hand, FVector2D CanvasSize);

	/** Pixel distance between two normalised points — feed into the arm's width/scale so it reaches the Hand. */
	UFUNCTION(BlueprintPure, Category = "PeachParty|Basket")
	static float SegmentLenPx(FVector2D A, FVector2D B, FVector2D CanvasSize);

	/**
	 * ONE-CALL arm placement. Stretches + rotates an arm image so it runs from Shoulder to Hand and
	 * follows the Hand's distance (so it grows/shrinks like the debug line, not a fixed length).
	 * Requirements on the arm Image:
	 *   - it is a DIRECT child of a Canvas Panel,
	 *   - its canvas-slot "Size To Content" is OFF (so the explicit size sticks),
	 *   - its texture is drawn pointing RIGHT with the shoulder end at the LEFT edge.
	 * Thickness = arm width in pixels.
	 */
	UFUNCTION(BlueprintCallable, Category = "PeachParty|Basket")
	static void SetArm(UWidget* Arm, FVector2D Shoulder, FVector2D Hand, FVector2D CanvasSize, float Thickness = 18.f);
};
