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
};
