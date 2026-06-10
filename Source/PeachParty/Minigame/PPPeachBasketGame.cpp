#include "Minigame/PPPeachBasketGame.h"
#include "Minigame/PPBasketBall.h"
#include "Minigame/PPBasketCharacter.h"
#include "Minigame/PPBasket.h"
#include "Core/PPPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

APPPeachBasketGame::APPPeachBasketGame()
{
	PrimaryActorTick.bCanEverTick = true; // server runs grab/steal/score each tick
	MinigameType = EMinigameType::PeachBasket;
	Duration = 45.f; // fallback cap; usually ends earlier on TargetScore

	GameCamera->SetRelativeLocation(FVector(0.f, -1500.f, 600.f));
	GameCamera->SetRelativeRotation(FRotator(-18.f, 90.f, 0.f));

	BallClass = APPBasketBall::StaticClass();
	CharacterClass = APPBasketCharacter::StaticClass();
	BasketClass = APPBasket::StaticClass();

	BuildArenaGeometry();
}

void APPPeachBasketGame::BuildArenaGeometry()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	auto MakeBlock = [&](const TCHAR* Name, const FVector& Loc, const FVector& Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(SceneRoot);
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Scale);
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetSimulatePhysics(false);
		if (CubeMesh.Succeeded()) { C->SetStaticMesh(CubeMesh.Object); }
		return C;
	};

	Floor = MakeBlock(TEXT("Floor"), FVector(0.f, 0.f, -20.f), FVector(18.f, 9.f, 0.4f));

	Walls.Add(MakeBlock(TEXT("WallPosX"), FVector( 900.f, 0.f, 200.f), FVector(0.4f, 9.f, 4.f)));
	Walls.Add(MakeBlock(TEXT("WallNegX"), FVector(-900.f, 0.f, 200.f), FVector(0.4f, 9.f, 4.f)));
	Walls.Add(MakeBlock(TEXT("WallPosY"), FVector(0.f,  450.f, 200.f), FVector(18.f, 0.4f, 4.f)));
	Walls.Add(MakeBlock(TEXT("WallNegY"), FVector(0.f, -450.f, 200.f), FVector(18.f, 0.4f, 4.f)));
}

// ----------------------------------------------------------------- spawn ----

void APPPeachBasketGame::OnMinigameStarted()
{
	if (HasAuthority())
	{
		SpawnPlay();
	}
}

void APPPeachBasketGame::SpawnPlay()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector O = GetActorLocation();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.Owner = this;

	auto SpawnChar = [&](const FVector& Off, float Yaw, APPPlayerState* InOwner, EPPTeam InTeam) -> APPBasketCharacter*
	{
		APPBasketCharacter* C = World->SpawnActor<APPBasketCharacter>(
			CharacterClass, O + Off, FRotator(0.f, Yaw, 0.f), P);
		if (C) { C->InitCharacter(InOwner, InTeam); }
		return C;
	};

	APPPlayerState* P1 = GetPlayer1();
	APPPlayerState* P2 = GetPlayer2();

	Player1Chars.Reset();
	Player2Chars.Reset();
	Player1Chars.Add(SpawnChar(FVector(-400.f, -150.f, 120.f),   0.f, P1, EPPTeam::TeamA));
	Player1Chars.Add(SpawnChar(FVector(-400.f,  150.f, 120.f),   0.f, P1, EPPTeam::TeamA));
	Player2Chars.Add(SpawnChar(FVector( 400.f, -150.f, 120.f), 180.f, P2, EPPTeam::TeamB));
	Player2Chars.Add(SpawnChar(FVector( 400.f,  150.f, 120.f), 180.f, P2, EPPTeam::TeamB));

	// Baskets: P1 scores into the one on P2's side, and vice-versa.
	BasketForP1 = World->SpawnActor<APPBasket>(BasketClass, O + FVector( 780.f, 0.f, 300.f), FRotator::ZeroRotator, P);
	BasketForP2 = World->SpawnActor<APPBasket>(BasketClass, O + FVector(-780.f, 0.f, 300.f), FRotator::ZeroRotator, P);
	if (BasketForP1) { BasketForP1->SetScorer(P1); }
	if (BasketForP2) { BasketForP2->SetScorer(P2); }

	Ball = World->SpawnActor<APPBasketBall>(BallClass, O + FVector(0.f, 0.f, 160.f), FRotator::ZeroRotator, P);

	// Capture start transforms for the post-score reset.
	BallStart = Ball ? Ball->GetActorTransform() : FTransform(O + FVector(0, 0, 160));
	Char1Starts.Reset();
	Char2Starts.Reset();
	for (APPBasketCharacter* C : Player1Chars) { Char1Starts.Add(C ? C->GetActorTransform() : FTransform::Identity); }
	for (APPBasketCharacter* C : Player2Chars) { Char2Starts.Add(C ? C->GetActorTransform() : FTransform::Identity); }
}

// ------------------------------------------------------------------ tick ----

void APPPeachBasketGame::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && !IsFinished())
	{
		UpdateGrabAndSteal();
		UpdateScoring();
	}
}

void APPPeachBasketGame::UpdateGrabAndSteal()
{
	if (!Ball)
	{
		return;
	}

	const FVector BallLoc = Ball->GetActorLocation();

	if (!Ball->IsHeld())
	{
		// Free ball: nearest charging hand within reach grabs it.
		APPBasketCharacter* Best = nullptr;
		float BestDistSq = FMath::Square(GrabRadius);
		ForEachChar([&](APPBasketCharacter* C)
		{
			if (C && C->IsCharging())
			{
				const float D = FVector::DistSquared(C->GetHandLocation(), BallLoc);
				if (D < BestDistSq) { BestDistSq = D; Best = C; }
			}
		});
		if (Best) { Ball->GrabBy(Best, Best->GetHandPoint()); }
	}
	else
	{
		// Held: an enemy charging hand within steal range takes it.
		APPBasketCharacter* Holder = Ball->GetHolder();
		const EPPTeam HolderTeam = Holder ? Holder->GetTeam() : EPPTeam::None;
		APPBasketCharacter* Thief = nullptr;
		float BestDistSq = FMath::Square(StealRadius);
		ForEachChar([&](APPBasketCharacter* C)
		{
			if (C && C->IsCharging() && C->GetTeam() != HolderTeam && C != Holder)
			{
				const float D = FVector::DistSquared(C->GetHandLocation(), BallLoc);
				if (D < BestDistSq) { BestDistSq = D; Thief = C; }
			}
		});
		if (Thief) { Ball->GrabBy(Thief, Thief->GetHandPoint()); }
	}
}

void APPPeachBasketGame::UpdateScoring()
{
	if (!Ball || Ball->IsHeld())
	{
		return;
	}

	const FVector BallLoc = Ball->GetActorLocation();

	if (BasketForP1 && FVector::Dist(BallLoc, BasketForP1->GetMouthLocation()) < ScoreRadius)
	{
		RegisterScore(BasketForP1->GetScorer());
	}
	else if (BasketForP2 && FVector::Dist(BallLoc, BasketForP2->GetMouthLocation()) < ScoreRadius)
	{
		RegisterScore(BasketForP2->GetScorer());
	}
}

void APPPeachBasketGame::RegisterScore(APPPlayerState* Scorer)
{
	if (!Scorer)
	{
		return;
	}

	AddScore(Scorer, 1); // base updates Player1Score / Player2Score

	if (Player1Score >= TargetScore)
	{
		FinishWithResult(EMatchResult::Player1);
	}
	else if (Player2Score >= TargetScore)
	{
		FinishWithResult(EMatchResult::Player2);
	}
	else
	{
		ResetPositions();
	}
}

// ----------------------------------------------------------------- input ----

void APPPeachBasketGame::HandleInput(APPPlayerState* Player, FName Action, bool bPressed)
{
	if (Action != FName(TEXT("Primary")))
	{
		return; // Peach Basket has exactly one input
	}

	const TArray<APPBasketCharacter*>& Chars = CharsOf(Player);

	if (bPressed)
	{
		// Jump + start charging the arms on BOTH of the player's characters.
		for (APPBasketCharacter* C : Chars)
		{
			if (C) { C->DoJump(JumpUpImpulse, JumpForwardImpulse); C->StartCharge(); }
		}
	}
	else
	{
		// Release: throw if holding (using the angle reached), then lower arms.
		for (APPBasketCharacter* C : Chars)
		{
			if (!C) continue;
			if (Ball && Ball->GetHolder() == C) { ThrowFrom(C); }
			C->StopCharge();
		}
	}
}

void APPPeachBasketGame::ThrowFrom(APPBasketCharacter* Holder)
{
	if (!Holder || !Ball)
	{
		return;
	}

	// Arm angle -> launch elevation. Low (just grabbed) = short/weak; sweet spot = into the basket.
	const float MaxArm = FMath::Max(1.f, Holder->MaxArmAngleDeg);
	const float Alpha = FMath::Clamp(Holder->GetArmAngleDeg() / MaxArm, 0.f, 1.f);
	float ElevationDeg = FMath::Lerp(MinThrowElevationDeg, MaxThrowElevationDeg, Alpha);

	// "Not perfectly accurate."
	ElevationDeg += FMath::FRandRange(-ThrowSpreadDeg, ThrowSpreadDeg);
	const float YawJitter = FMath::FRandRange(-ThrowSpreadDeg, ThrowSpreadDeg);

	FVector Horiz = Holder->GetThrowDirection().RotateAngleAxis(YawJitter, FVector::UpVector);
	if (Horiz.IsNearlyZero()) { Horiz = FVector::ForwardVector; }

	const float ElevRad = FMath::DegreesToRadians(ElevationDeg);
	const FVector LaunchDir = (Horiz * FMath::Cos(ElevRad) + FVector::UpVector * FMath::Sin(ElevRad)).GetSafeNormal();

	Ball->ThrowWithImpulse(LaunchDir * ThrowSpeed);
}

// ----------------------------------------------------------------- reset ----

void APPPeachBasketGame::ResetPositions()
{
	if (Ball) { Ball->ResetTo(BallStart); }
	for (int32 i = 0; i < Player1Chars.Num(); ++i)
	{
		if (Player1Chars[i] && Char1Starts.IsValidIndex(i)) { Player1Chars[i]->ResetTo(Char1Starts[i]); }
	}
	for (int32 i = 0; i < Player2Chars.Num(); ++i)
	{
		if (Player2Chars[i] && Char2Starts.IsValidIndex(i)) { Player2Chars[i]->ResetTo(Char2Starts[i]); }
	}
}

// --------------------------------------------------------------- teardown ----

void APPPeachBasketGame::OnMinigameFinished()
{
	if (Ball) { Ball->Destroy(); Ball = nullptr; }
	ForEachChar([](APPBasketCharacter* C) { if (C) { C->Destroy(); } });
	Player1Chars.Reset();
	Player2Chars.Reset();
	if (BasketForP1) { BasketForP1->Destroy(); BasketForP1 = nullptr; }
	if (BasketForP2) { BasketForP2->Destroy(); BasketForP2 = nullptr; }
}

// ----------------------------------------------------------------- utils ----

const TArray<APPBasketCharacter*>& APPPeachBasketGame::CharsOf(const APPPlayerState* Player) const
{
	static const TArray<APPBasketCharacter*> Empty;
	if (Player && Player == GetPlayer1()) { return Player1Chars; }
	if (Player && Player == GetPlayer2()) { return Player2Chars; }
	return Empty;
}

void APPPeachBasketGame::ForEachChar(TFunctionRef<void(APPBasketCharacter*)> Fn) const
{
	for (APPBasketCharacter* C : Player1Chars) { Fn(C); }
	for (APPBasketCharacter* C : Player2Chars) { Fn(C); }
}
