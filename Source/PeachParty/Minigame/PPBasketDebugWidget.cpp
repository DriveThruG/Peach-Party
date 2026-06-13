#include "Minigame/PPBasketDebugWidget.h"
#include "Minigame/PPPeachBasketUMGGame.h"
#include "Core/PPPlayerController.h"
#include "Rendering/DrawElements.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarBasketDebug(
	TEXT("pp.basket.debug"), 1,
	TEXT("Draw the basket rim boxes / ball / arm-line debug overlay (1 = on, 0 = off)."),
	ECVF_Default);

int32 UPPBasketDebugWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (CVarBasketDebug.GetValueOnGameThread() == 0)
	{
		return Layer;
	}

	const APPPlayerController* PC = Cast<APPPlayerController>(GetOwningPlayer());
	const APPPeachBasketUMGGame* B = PC ? Cast<APPPeachBasketUMGGame>(PC->GetViewedMinigame()) : nullptr;
	if (!B)
	{
		return Layer;
	}
	const FPPBasketState& S = B->GetState();

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return Layer;
	}
	const FPaintGeometry PG = AllottedGeometry.ToPaintGeometry();
	++Layer;

	// Normalised (0..1, Y up) -> local widget pixels (Y down).
	auto ToPx = [&](double Nx, double Ny) { return FVector2D(Nx * Size.X, (1.0 - Ny) * Size.Y); };

	auto Line = [&](const FVector2D& A, const FVector2D& C, const FLinearColor& Col, float Thick)
	{
		TArray<FVector2D> Pts;
		Pts.Add(A);
		Pts.Add(C);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, PG, Pts, ESlateDrawEffect::None, Col, true, Thick);
	};

	auto DrawHoop = [&](const FVector2D& C, double HW, double HH)
	{
		const FVector2D TL = ToPx(C.X - HW, C.Y + HH);
		const FVector2D TR = ToPx(C.X + HW, C.Y + HH);
		const FVector2D BR = ToPx(C.X + HW, C.Y - HH);
		const FVector2D BL = ToPx(C.X - HW, C.Y - HH);
		// Outline (red), open top hinted by drawing the box; solid side walls in thick yellow (= bounce edges).
		Line(TL, TR, FLinearColor::Red, 2.f);
		Line(BL, BR, FLinearColor::Red, 2.f);
		Line(TL, BL, FLinearColor::Yellow, 4.f);
		Line(TR, BR, FLinearColor::Yellow, 4.f);
	};

	DrawHoop(S.RimLeft, S.HoopHalfW, S.HoopHalfH);
	DrawHoop(S.RimRight, S.HoopHalfW, S.HoopHalfH);

	// Ball marker (orange box of the collision radius).
	{
		const double r = S.BallRadius;
		const FVector2D TL = ToPx(S.Ball.X - r, S.Ball.Y + r);
		const FVector2D TR = ToPx(S.Ball.X + r, S.Ball.Y + r);
		const FVector2D BR = ToPx(S.Ball.X + r, S.Ball.Y - r);
		const FVector2D BL = ToPx(S.Ball.X - r, S.Ball.Y - r);
		const FLinearColor Orange(1.f, 0.55f, 0.f, 1.f);
		Line(TL, TR, Orange, 2.f); Line(TR, BR, Orange, 2.f); Line(BR, BL, Orange, 2.f); Line(BL, TL, Orange, 2.f);
	}

	// Arm line Shoulder -> Hand for each char (green) + a hand dot.
	for (const FPPBasketChar& Ch : S.Chars)
	{
		const FVector2D Sh = ToPx(Ch.Shoulder.X, Ch.Shoulder.Y);
		const FVector2D Hd = ToPx(Ch.Hand.X, Ch.Hand.Y);
		Line(Sh, Hd, FLinearColor::Green, 3.f);
	}

	return Layer;
}
