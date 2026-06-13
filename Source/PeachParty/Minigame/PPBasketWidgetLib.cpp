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

float UPPBasketWidgetLib::ArmAngleDeg(FVector2D Shoulder, FVector2D Hand, FVector2D CanvasSize)
{
	// Screen-space delta (UMG Y grows DOWN, so flip Y). Angle is clockwise from +X, i.e. what
	// Set Render Transform Angle expects. Pixel deltas (not normalised) so non-square canvases look right.
	const double Dx = (Hand.X - Shoulder.X) * CanvasSize.X;
	const double Dy = (Shoulder.Y - Hand.Y) * CanvasSize.Y; // flip
	return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Dy, Dx)));
}

float UPPBasketWidgetLib::SegmentLenPx(FVector2D A, FVector2D B, FVector2D CanvasSize)
{
	const double Dx = (B.X - A.X) * CanvasSize.X;
	const double Dy = (B.Y - A.Y) * CanvasSize.Y;
	return static_cast<float>(FMath::Sqrt(Dx * Dx + Dy * Dy));
}

void UPPBasketWidgetLib::SetArm(UWidget* Arm, FVector2D Shoulder, FVector2D Hand, FVector2D CanvasSize, float Thickness)
{
	if (!Arm)
	{
		return;
	}
	UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Arm->Slot);
	if (!Slot)
	{
		return;
	}

	const FVector2D ShoulderPx(Shoulder.X * CanvasSize.X, (1.0 - Shoulder.Y) * CanvasSize.Y);

	Slot->SetAlignment(FVector2D(0.f, 0.5f));                                  // pivot the slot at the shoulder end
	Slot->SetPosition(ShoulderPx);
	Slot->SetSize(FVector2D(SegmentLenPx(Shoulder, Hand, CanvasSize), Thickness));

	Arm->SetRenderTransformPivot(FVector2D(0.f, 0.5f));                        // rotate around the shoulder end
	Arm->SetRenderTransformAngle(ArmAngleDeg(Shoulder, Hand, CanvasSize));
}
