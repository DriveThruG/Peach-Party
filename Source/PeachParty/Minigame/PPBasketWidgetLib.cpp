#include "Minigame/PPBasketWidgetLib.h"
#include "Components/Widget.h"
#include "Components/CanvasPanelSlot.h"

void UPPBasketWidgetLib::SetCanvasPos(UWidget* Element, FVector2D NormPos, FVector2D CanvasSize)
{
	if (!Element)
	{
		return;
	}
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Element->Slot))
	{
		const FVector2D Pixel(NormPos.X * CanvasSize.X, (1.0 - NormPos.Y) * CanvasSize.Y);
		CanvasSlot->SetPosition(Pixel);
	}
}
