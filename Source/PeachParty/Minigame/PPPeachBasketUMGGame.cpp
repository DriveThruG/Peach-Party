#include "Minigame/PPPeachBasketUMGGame.h"
#include "Core/PPPlayerState.h"
#include "Net/UnrealNetwork.h"

APPPeachBasketUMGGame::APPPeachBasketUMGGame()
{
	PrimaryActorTick.bCanEverTick = true;
	MinigameType = EMinigameType::PeachBasket;
	Duration = 120.f; // 2 minutes, then most points wins (tie = both win, via base ForceResolve)

	// Default start spots (on the floor): A on the left, B on the right.
	CharStartPositions = {
		FVector2D(0.25, GroundY), FVector2D(0.38, GroundY), // team A
		FVector2D(0.62, GroundY), FVector2D(0.75, GroundY)  // team B
	};
}

void APPPeachBasketUMGGame::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPPeachBasketUMGGame, RepState);
}

void APPPeachBasketUMGGame::OnMinigameStarted()
{
	// Free-play (solo tuning preview): kill the round timer so the field stays up indefinitely. The base
	// arms the Duration timer right AFTER this call, only when Duration > 0 — so zeroing it here suppresses it.
	if (bFreePlay)
	{
		Duration = 0.f;
	}

	if (HasAuthority())
	{
		SetupField();
	}
}

void APPPeachBasketUMGGame::DebugResetField()
{
	// Re-read every layout/start tunable and rebuild the field — live, no PIE restart.
	if (HasAuthority())
	{
		SetupField();
	}
}

void APPPeachBasketUMGGame::SetupField()
{
	const int32 Teams[4]  = { 1, 1, 2, 2 };           // 1 = A (left), 2 = B (right)
	const int32 Variants[4] = { 1, 2, 3, 4 };
	const float Phases[4] = { 0.f, 1.6f, 3.1f, 4.7f };

	RepState.Chars.Reset();
	CharVel.Reset();
	CharPhase.Reset();
	for (int32 i = 0; i < 4; ++i)
	{
		FPPBasketChar C;
		C.Pos = CharStartPositions.IsValidIndex(i) ? CharStartPositions[i] : FVector2D(0.25 + i * 0.15, GroundY);
		C.Team = Teams[i];
		C.Variant = Variants[i];
		RepState.Chars.Add(C);
		CharVel.Add(FVector2D::ZeroVector);
		CharPhase.Add(Phases[i]);
	}

	RepState.HoopLeft  = HoopLeftPos;
	RepState.HoopRight = HoopRightPos;
	RepState.Ball = BallStartPos;
	RepState.ScoreA = 0;
	RepState.ScoreB = 0;

	BallHolder = -1;
	BallVel = FVector2D::ZeroVector;
	ThrowCooldown = 0.f;
	SimTime = 0.f;
}

void APPPeachBasketUMGGame::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && !IsFinished())
	{
		ServerTick(DeltaSeconds);
	}
}

void APPPeachBasketUMGGame::ServerTick(float Dt)
{
	SimTime += Dt;
	ThrowCooldown = FMath::Max(0.f, ThrowCooldown - Dt);

	const float MinX = 0.03f, MaxX = 0.97f;

	// ---- characters ----
	for (int32 i = 0; i < RepState.Chars.Num(); ++i)
	{
		FPPBasketChar& C = RepState.Chars[i];

		// Pendulum lean (drives jump direction + visual tilt).
		C.Lean = MaxLean * FMath::Sin(SimTime * LeanFreq + CharPhase[i]);

		// Arms rise while charging, fall otherwise (controls hand height for grabbing).
		C.ArmAngle = FMath::Clamp(C.ArmAngle + (C.bCharging ? ArmRaiseRate : -ArmRaiseRate) * Dt, 0.f, 1.f);

		// Integrate motion.
		CharVel[i].Y -= Gravity * Dt;
		CharVel[i].X *= FMath::Max(0.f, 1.f - AirDrag * Dt); // always-on horizontal damping (less floaty drift)
		C.Pos += CharVel[i] * Dt;

		// Floor.
		if (C.Pos.Y <= GroundY)
		{
			C.Pos.Y = GroundY;
			if (CharVel[i].Y < 0.0) { CharVel[i].Y = 0.0; }
			CharVel[i].X *= FMath::Max(0.f, 1.f - SlideFriction * Dt); // extra ground friction (less slide)
		}
		// Side walls.
		if (C.Pos.X < MinX) { C.Pos.X = MinX; CharVel[i].X =  FMath::Abs(CharVel[i].X) * 0.3; }
		if (C.Pos.X > MaxX) { C.Pos.X = MaxX; CharVel[i].X = -FMath::Abs(CharVel[i].X) * 0.3; }
	}

	// ---- ball ----
	if (BallHolder >= 0 && RepState.Chars.IsValidIndex(BallHolder))
	{
		RepState.Ball = HandOf(BallHolder);
	}
	else
	{
		BallVel.Y -= Gravity * Dt;
		RepState.Ball += BallVel * Dt;

		if (RepState.Ball.X < MinX) { RepState.Ball.X = MinX; BallVel.X =  FMath::Abs(BallVel.X) * 0.5; }
		if (RepState.Ball.X > MaxX) { RepState.Ball.X = MaxX; BallVel.X = -FMath::Abs(BallVel.X) * 0.5; }
		if (RepState.Ball.Y > 0.98) { RepState.Ball.Y = 0.98; BallVel.Y = -FMath::Abs(BallVel.Y) * 0.35; }
		if (RepState.Ball.Y <= BallFloorY)
		{
			RepState.Ball.Y = BallFloorY;
			BallVel.Y = FMath::Abs(BallVel.Y) * 0.35;        // small bounce
			BallVel.X *= FMath::Max(0.f, 1.f - 3.f * Dt);
			if (BallVel.Y < 0.02) { BallVel.Y = 0.0; }       // settle
		}
	}

	TryGrabSteal(); // hands grab/steal ALWAYS (not gated by Primary)
	TryScore();
}

FVector2D APPPeachBasketUMGGame::HandOf(int32 Index) const
{
	if (!RepState.Chars.IsValidIndex(Index))
	{
		return RepState.Ball;
	}
	const FPPBasketChar& C = RepState.Chars[Index];
	const float HandLen = 0.04f + C.ArmAngle * 0.11f; // arms up -> hand higher
	return C.Pos + UpVec(C.Lean) * HandLen;
}

bool APPPeachBasketUMGGame::IsGrounded(int32 Index) const
{
	if (!CharVel.IsValidIndex(Index) || !RepState.Chars.IsValidIndex(Index))
	{
		return false;
	}
	return RepState.Chars[Index].Pos.Y <= GroundY + 0.006 && FMath::Abs(CharVel[Index].Y) < 0.02;
}

void APPPeachBasketUMGGame::TryGrabSteal()
{
	const float R2 = GrabRange * GrabRange;

	if (BallHolder < 0 && ThrowCooldown <= 0.f)
	{
		int32 Best = -1;
		double BestD = R2;
		for (int32 i = 0; i < RepState.Chars.Num(); ++i)
		{
			const double D = FVector2D::DistSquared(HandOf(i), RepState.Ball);
			if (D < BestD) { BestD = D; Best = i; }
		}
		if (Best >= 0) { BallHolder = Best; BallVel = FVector2D::ZeroVector; }
	}
	else if (BallHolder >= 0 && RepState.Chars.IsValidIndex(BallHolder))
	{
		const int32 HolderTeam = RepState.Chars[BallHolder].Team;
		for (int32 i = 0; i < RepState.Chars.Num(); ++i)
		{
			if (i == BallHolder || RepState.Chars[i].Team == HolderTeam) { continue; }
			if (FVector2D::DistSquared(HandOf(i), RepState.Ball) < R2) { BallHolder = i; break; }
		}
	}

	for (int32 i = 0; i < RepState.Chars.Num(); ++i)
	{
		RepState.Chars[i].bHoldsBall = (i == BallHolder);
	}
}

void APPPeachBasketUMGGame::TryScore()
{
	if (BallHolder >= 0)
	{
		return;
	}
	const float R2 = ScoreRange * ScoreRange;
	if (FVector2D::DistSquared(RepState.Ball, RepState.HoopRight) < R2)      { DoScore(EPPTeam::TeamA); } // A -> right hoop
	else if (FVector2D::DistSquared(RepState.Ball, RepState.HoopLeft) < R2)  { DoScore(EPPTeam::TeamB); }
}

void APPPeachBasketUMGGame::ThrowFrom(int32 Index)
{
	if (!RepState.Chars.IsValidIndex(Index))
	{
		return;
	}
	const FPPBasketChar& C = RepState.Chars[Index];
	const FVector2D Target = (C.Team == 1) ? RepState.HoopRight : RepState.HoopLeft;
	const FVector2D S = RepState.Ball;
	const float Tf = FMath::Max(0.2f, ThrowFlightTime);
	const float Quality = FMath::Clamp(C.ArmAngle, 0.f, 1.f); // arms high at release -> full power -> hits

	// Velocity that EXACTLY reaches Target in Tf, then scaled by quality (low arms -> falls short).
	const double Vx = (Target.X - S.X) / Tf;
	const double Vy = (Target.Y - S.Y + 0.5 * Gravity * Tf * Tf) / Tf;
	FVector2D V(Vx, Vy);
	V *= Quality;
	V.X += FMath::FRandRange(-0.03f, 0.03f); // "not perfectly accurate"
	V.Y += FMath::FRandRange(-0.02f, 0.02f);

	BallVel = V;
	BallHolder = -1;
	ThrowCooldown = 0.3f;
	for (int32 k = 0; k < RepState.Chars.Num(); ++k) { RepState.Chars[k].bHoldsBall = false; }
}

void APPPeachBasketUMGGame::DoScore(EPPTeam ScoringTeam)
{
	AddScore(ScoringTeam == EPPTeam::TeamA ? GetPlayer1() : GetPlayer2(), 1);
	RepState.ScoreA = Player1Score; // base slot1 = team A
	RepState.ScoreB = Player2Score;

	if (bFreePlay)                        { ResetPositions(); } // solo preview: never ends, just resets
	else if (Player1Score >= TargetScore) { FinishWithResult(EMatchResult::Player1); }
	else if (Player2Score >= TargetScore) { FinishWithResult(EMatchResult::Player2); }
	else                                  { ResetPositions(); }
}

void APPPeachBasketUMGGame::ResetPositions()
{
	for (int32 i = 0; i < RepState.Chars.Num(); ++i)
	{
		RepState.Chars[i].Pos = CharStartPositions.IsValidIndex(i) ? CharStartPositions[i] : FVector2D(0.25 + i * 0.15, GroundY);
		RepState.Chars[i].ArmAngle = 0.f;
		RepState.Chars[i].bCharging = false;
		RepState.Chars[i].bHoldsBall = false;
		if (CharVel.IsValidIndex(i)) { CharVel[i] = FVector2D::ZeroVector; }
	}
	RepState.Ball = BallStartPos;
	BallVel = FVector2D::ZeroVector;
	BallHolder = -1;
	ThrowCooldown = 0.f;
}

void APPPeachBasketUMGGame::CharsOfPlayer(const APPPlayerState* Player, int32& OutA, int32& OutB) const
{
	OutA = OutB = -1;
	if (Player && Player == GetPlayer1())      { OutA = 0; OutB = 1; }
	else if (Player && Player == GetPlayer2()) { OutA = 2; OutB = 3; }
}

void APPPeachBasketUMGGame::HandleInput(APPPlayerState* Player, FName Action, bool bPressed)
{
	if (!HasAuthority() || Action != FName(TEXT("Primary")))
	{
		return;
	}

	int32 A, B;
	CharsOfPlayer(Player, A, B);
	const int32 Idxs[2] = { A, B };
	for (int32 Idx : Idxs)
	{
		if (!RepState.Chars.IsValidIndex(Idx))
		{
			continue;
		}
		FPPBasketChar& C = RepState.Chars[Idx];
		if (bPressed)
		{
			C.bCharging = true;
			if (IsGrounded(Idx)) { CharVel[Idx] = UpVec(C.Lean) * JumpImpulse; } // jump ALONG the lean
		}
		else
		{
			if (BallHolder == Idx) { ThrowFrom(Idx); }
			C.bCharging = false;
		}
	}
}
