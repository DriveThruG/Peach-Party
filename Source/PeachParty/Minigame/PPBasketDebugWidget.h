#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PPBasketDebugWidget.generated.h"

/**
 * Full-screen debug overlay drawn purely in C++ (no UMG asset needed). Reads the live basket state from
 * the owning player's viewed match and paints:
 *   - the hoop "rim" boxes (red outline, yellow solid side walls = the bounce edges),
 *   - a marker at the ball (orange),
 *   - the arm line Shoulder->Hand for each char (green) — so you can place the real arm image on it.
 * Toggle with the console var `pp.basket.debug 0/1`.
 */
UCLASS()
class PEACHPARTY_API UPPBasketDebugWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
