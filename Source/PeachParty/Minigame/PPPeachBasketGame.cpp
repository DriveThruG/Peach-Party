#include "Minigame/PPPeachBasketGame.h"
#include "Minigame/PPBasketBall.h"
#include "Minigame/PPBasketCharacter.h"
#include "Minigame/PPBasket.h"
#include "Minigame/PPVisual.h"
#include "Core/PPPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "PaperSpriteComponent.h"
#include "Engine/Texture2D.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

APPPeachBasketGame::APPPeachBasketGame()
{
	PrimaryActorTick.bCanEverTick = true; // server runs grab/steal/score each tick
	MinigameType = EMinigameType::PeachBasket;
	Duration = 45.f; // fallback cap; usually ends earlier on TargetScore

	GameCamera->SetRelativeLocation(FVector(0.f, -470.f, 115.f)); // closer still
	GameCamera->SetRelativeRotation(FRotator(-1.f, 90.f, 0.f));   // head-on
	// Orthographic (set in base). Camera "2x closer" = half the ortho width (smaller = more zoomed in).
	OrthoWidth = 700.f; // was 1400
	GameCamera->SetOrthoWidth(OrthoWidth);

	// Lock exposure: the arena floats in dark empty space, so UE's auto-exposure cranks brightness
	// and blows out the unlit sprites ("overexposed"). Fixing min=max=1 disables eye adaptation.
	GameCamera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	GameCamera->PostProcessSettings.AutoExposureMinBrightness = 1.f;
	GameCamera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	GameCamera->PostProcessSettings.AutoExposureMaxBrightness = 1.f;

	BallClass = APPBasketBall::StaticClass();
	CharacterClass = APPBasketCharacter::StaticClass();
	BasketClass = APPBasket::StaticClass();

	BuildArenaGeometry();

	// Full-screen 2D background behind the action (built from the Background texture in BeginPlay).
	Background = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Background"));
	Background->SetupAttachment(SceneRoot);
	Background->SetRelativeLocation(BackgroundOffset);
	Background->SetRelativeRotation(FRotator(0.f, 0.f, 0.f)); // Paper2D default faces -Y = the side camera
	Background->SetRelativeScale3D(FVector(BackgroundScale));
	Background->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Background->SetTranslucentSortPriority(-100); // draw behind everything

	static ConstructorHelpers::FObjectFinder<UTexture2D> BgTex(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Background.Background"));
	BackgroundTexture = BgTex.Object;
}

void APPPeachBasketGame::BeginPlay()
{
	Super::BeginPlay();

	// Build the sprite ONCE here; the per-frame ScaleBackgroundToView() only does cheap scale/position math.
	if (Background && BackgroundTexture)
	{
		if (UPaperSprite* S = PPVisual::SpriteFromTexture(this, BackgroundTexture))
		{
			Background->SetSprite(S);
		}
	}
	ScaleBackgroundToView();
}

void APPPeachBasketGame::ScaleBackgroundToView()
{
	if (!Background || !GameCamera)
	{
		return;
	}

	// Centre on the camera's view axis: camera X is 0 and (with a ~head-on camera) the screen's vertical
	// centre maps to world Z = camera Z. So sprite centre = screen centre.
	const float CamZ = (float)GameCamera->GetRelativeLocation().Z;
	Background->SetRelativeLocation(FVector(0.f, BackgroundOffset.Y, CamZ));

	// Use the camera's REAL ortho width (can't desync from a stored copy).
	const float OrthoW = GameCamera->OrthoWidth;

	// Under ORTHO the view shows EXACTLY OrthoWidth world-units across the screen width, and
	// OrthoWidth/aspect vertically. So the rectangle we must cover is known exactly.
	// (FVector2D components are double under LWC -> cast to float to avoid FMath::Max ambiguity.)
	FVector2D Viewport(1920.f, 1080.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(Viewport);
	}
	const float VpX = (float)Viewport.X;
	const float VpY = (float)Viewport.Y;
	const float Aspect = VpX / FMath::Max(1.f, VpY);
	const float NeedHalfW = OrthoW * 0.5f;
	const float NeedHalfH = (OrthoW / FMath::Max(0.01f, Aspect)) * 0.5f;

	// Native (scale-1) half-size of the sprite in world units, MEASURED — independent of the sprite's
	// pixels-per-unit. The sprite faces -Y, so its width is along X and its height along Z.
	const FBoxSphereBounds LB = Background->CalcLocalBounds();
	const float NativeHalfW = FMath::Max(1.f, (float)LB.BoxExtent.X);
	const float NativeHalfH = FMath::Max(1.f, (float)LB.BoxExtent.Z);

	// COVER fit: the larger of the two ratios guarantees both dimensions are filled (may crop a little).
	// BackgroundScale (>=1) is an extra bleed margin so edges never show under rounding/resize.
	const float Fit = FMath::Max(NeedHalfW / NativeHalfW, NeedHalfH / NativeHalfH) * FMath::Max(1.f, BackgroundScale);
	Background->SetRelativeScale3D(FVector(Fit, 1.f, Fit));
}

void APPPeachBasketGame::BuildArenaGeometry()
{
	// Pure collision boxes — invisible in game (no mesh), so only the 2D sprites/background show.
	auto MakeBox = [&](const TCHAR* Name, const FVector& Loc, const FVector& Extent) -> UBoxComponent*
	{
		UBoxComponent* C = CreateDefaultSubobject<UBoxComponent>(Name);
		C->SetupAttachment(SceneRoot);
		C->SetRelativeLocation(Loc);
		C->SetBoxExtent(Extent);
		C->SetCollisionProfileName(TEXT("BlockAll"));
		return C;
	};

	Floor = MakeBox(TEXT("Floor"), FVector(0.f, 0.f, -20.f), FVector(900.f, 450.f, 20.f));

	Walls.Add(MakeBox(TEXT("WallPosX"), FVector( 900.f, 0.f, 200.f), FVector(20.f, 450.f, 200.f)));
	Walls.Add(MakeBox(TEXT("WallNegX"), FVector(-900.f, 0.f, 200.f), FVector(20.f, 450.f, 200.f)));
	Walls.Add(MakeBox(TEXT("WallPosY"), FVector(0.f,  450.f, 200.f), FVector(900.f, 20.f, 200.f)));
	Walls.Add(MakeBox(TEXT("WallNegY"), FVector(0.f, -450.f, 200.f), FVector(900.f, 20.f, 200.f)));
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

	auto SpawnChar = [&](const FVector& Off, APPPlayerState* InOwner, EPPTeam InTeam, int32 Variant) -> APPBasketCharacter*
	{
		// Yaw 0: all face the camera (2D side view). Team/throw side comes from the team, not yaw.
		APPBasketCharacter* C = World->SpawnActor<APPBasketCharacter>(
			CharacterClass, O + Off, FRotator::ZeroRotator, P);
		if (C) { C->InitCharacter(InOwner, InTeam, Variant); }
		return C;
	};

	APPPlayerState* P1 = GetPlayer1();
	APPPlayerState* P2 = GetPlayer2();

	// Same depth/ground (Y=0, Z=120), spread along X = the screen horizontal (like the reference).
	Player1Chars.Reset();
	Player2Chars.Reset();
	Player1Chars.Add(SpawnChar(FVector(-420.f, 0.f, 120.f), P1, EPPTeam::TeamA, 1));
	Player1Chars.Add(SpawnChar(FVector(-220.f, 0.f, 120.f), P1, EPPTeam::TeamA, 2));
	Player2Chars.Add(SpawnChar(FVector( 220.f, 0.f, 120.f), P2, EPPTeam::TeamB, 3));
	Player2Chars.Add(SpawnChar(FVector( 420.f, 0.f, 120.f), P2, EPPTeam::TeamB, 4));

	// Hoops pulled IN toward the court and DOWN (user request 2026-06-11) so they sit over the field
	// instead of floating at the screen corners. Both face inward: the texture opens RIGHT, so the LEFT
	// hoop stays default and the RIGHT one mirrors. (±X = how far out, Z = rim height — tune by eye.)
	BasketForP1 = World->SpawnActor<APPBasket>(BasketClass, O + FVector( 250.f, 0.f, 50.f), FRotator::ZeroRotator, P);
	BasketForP2 = World->SpawnActor<APPBasket>(BasketClass, O + FVector(-250.f, 0.f, 50.f), FRotator::ZeroRotator, P);
	if (BasketForP1) { BasketForP1->SetScorer(P1); BasketForP1->SetFlipped(true); } // right hoop mirrored to face the court
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

	ScaleBackgroundToView(); // keep the background filling the screen (self-corrects on resize)

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
