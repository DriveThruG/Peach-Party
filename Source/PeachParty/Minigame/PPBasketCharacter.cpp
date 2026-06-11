#include "Minigame/PPBasketCharacter.h"
#include "Minigame/PPVisual.h"
#include "Components/CapsuleComponent.h"
#include "PaperSpriteComponent.h"
#include "Engine/Texture2D.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

APPBasketCharacter::APPBasketCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);

	Body = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->InitCapsuleSize(22.f, 88.f); // thinner hitbox (less bumping); visual is the sprite
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetSimulatePhysics(true);
	Body->SetCenterOfMass(FVector(0.f, 0.f, -70.f)); // at the feet: pivots + self-rights around the feet
	Body->SetAngularDamping(0.5f);
	Body->SetLinearDamping(0.1f);
	// Lock to the X-Z plane: a true 2D side-view (move in X/Z, only roll about Y to wobble/tip).
	Body->BodyInstance.DOFMode = EDOFMode::XZPlane;

	// --- Paper2D sprite layers (back arm, body, front arm). Y offsets order them vs the camera (-Y). ---
	SpriteBody = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("SpriteBody"));
	SpriteBody->SetupAttachment(Body);
	SpriteBody->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 0.f), SpriteFacing);
	SpriteBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Pivot at the SHOULDER (arms rotate from here). The arm textures are full-canvas layers (same frame
	// as the body), so each arm sprite is offset by -ShoulderZ FROM the pivot -> at rest it lands back on
	// the body's origin = perfectly aligned with the body art. Pitching ArmPivot then swings the arm
	// around the shoulder. Only ±2 in Y separates front/back for draw order.
	// Pivot + arm rest-offset scale with SpriteScale so the shoulder stays at the (now-bigger) shoulder.
	ArmPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ArmPivot"));
	ArmPivot->SetupAttachment(Body);
	ArmPivot->SetRelativeLocation(FVector(0.f, 0.f, ShoulderZ * SpriteScale)); // shoulder joint height in the art

	SpriteFront = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("SpriteFront"));
	SpriteFront->SetupAttachment(ArmPivot);
	SpriteFront->SetRelativeLocationAndRotation(FVector(0.f, -2.f, -ShoulderZ * SpriteScale), SpriteFacing); // back onto the body, in front
	SpriteFront->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SpriteBack = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("SpriteBack"));
	SpriteBack->SetupAttachment(ArmPivot);
	SpriteBack->SetRelativeLocationAndRotation(FVector(0.f, 2.f, -ShoulderZ * SpriteScale), SpriteFacing); // back onto the body, behind
	SpriteBack->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HandPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HandPoint"));
	HandPoint->SetupAttachment(Body);
	HandPoint->SetRelativeLocation(FVector(45.f, 0.f, 70.f));

	// Reference the user's imported TEXTURES by path (exact filenames, incl. the PLayer01 typo).
	// Sprites are built from these at runtime in ApplySprites().
	static ConstructorHelpers::FObjectFinder<UTexture2D> B1(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player01_Body.Player01_Body"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> B2(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player02_Body.Player02_Body"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> B3(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player03_Body.Player03_Body"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> B4(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player04_Body.Player04_Body"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> A1(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/PLayer01_Arm.PLayer01_Arm"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> A2(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player02_Arm.Player02_Arm"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> A3(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player03_Arm.Player03_Arm"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> A4(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player04_Arm.Player04_Arm"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Back(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Arm_Left.Arm_Left"));

	BodyTextures[0] = B1.Object; BodyTextures[1] = B2.Object; BodyTextures[2] = B3.Object; BodyTextures[3] = B4.Object;
	ArmTextures[0]  = A1.Object; ArmTextures[1]  = A2.Object; ArmTextures[2]  = A3.Object; ArmTextures[3]  = A4.Object;
	BackArmTexture  = Back.Object;
}

void APPBasketCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPBasketCharacter, bCharging);
	DOREPLIFETIME(APPBasketCharacter, SpriteVariant);
	DOREPLIFETIME(APPBasketCharacter, Team);
}

void APPBasketCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		if (bCharging)
		{
			ArmAngleDeg = FMath::Min(MaxArmAngleDeg, ArmAngleDeg + ArmRaiseRateDegPerSec * DeltaSeconds);
		}

		// Weeble self-righting: spring the body back to upright (rotation about Y, the 2D plane).
		const FVector Up = GetActorUpVector();
		const float Lean = FMath::Atan2(Up.X, Up.Z);                 // 0 = upright; + = leaning toward +X
		const float AngVelY = Body->GetPhysicsAngularVelocityInRadians().Y;
		const float Torque = -UprightStrength * Lean - UprightDamping * AngVelY;
		Body->AddTorqueInRadians(FVector(0.f, Torque, 0.f), NAME_None, /*bAccelChange=*/true);
	}

	// Ease the visual arm raise everywhere (from replicated bCharging). Pitch the shoulder pivot.
	const float Target = bCharging ? ArmRaisedDeg : 0.f;
	VisualArmAngle = FMath::FInterpTo(VisualArmAngle, Target, DeltaSeconds, 8.f);
	if (ArmPivot)
	{
		ArmPivot->SetRelativeRotation(FRotator(VisualArmAngle, 0.f, 0.f));
	}
}

void APPBasketCharacter::InitCharacter(APPPlayerState* InOwner, EPPTeam InTeam, int32 InVariant)
{
	OwningPlayer = InOwner;
	Team = InTeam;
	FacingSign = (InTeam == EPPTeam::TeamA) ? 1.f : -1.f;
	SpriteVariant = FMath::Clamp(InVariant, 1, 4);
	ApplySprites();   // server
	OnRep_Variant();  // host mirror
}

void APPBasketCharacter::DoJump(float UpImpulse, float ForwardImpulse)
{
	if (!HasAuthority() || !IsGrounded())
	{
		return; // no air jumps — must be standing on something
	}
	// Jump along the body's CURRENT (tilted) up vector -> wobble produces angled jumps. Small bias toward the enemy.
	const FVector Impulse = GetActorUpVector() * UpImpulse + FVector(FacingSign, 0.f, 0.f) * (ForwardImpulse * 0.4f);
	Body->AddImpulse(Impulse, NAME_None, /*bVelChange=*/true);
}

bool APPBasketCharacter::IsGrounded() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, 88.f + 15.f); // capsule half-height + margin
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	return World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
}

void APPBasketCharacter::StartCharge()
{
	if (!HasAuthority())
	{
		return;
	}
	ArmAngleDeg = 0.f;
	if (!bCharging)
	{
		bCharging = true;
		OnRep_Charging();
	}
}

float APPBasketCharacter::StopCharge()
{
	const float Reached = ArmAngleDeg;
	if (HasAuthority())
	{
		ArmAngleDeg = 0.f;
		if (bCharging)
		{
			bCharging = false;
			OnRep_Charging();
		}
	}
	return Reached;
}

void APPBasketCharacter::ResetTo(const FTransform& Transform)
{
	if (!HasAuthority())
	{
		return;
	}
	StopCharge();
	SetActorTransform(Transform, false, nullptr, ETeleportType::ResetPhysics);
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
}

FVector APPBasketCharacter::GetHandLocation() const
{
	return HandPoint ? HandPoint->GetComponentLocation() : GetActorLocation();
}

FVector APPBasketCharacter::GetThrowDirection() const
{
	// Throw horizontally toward the enemy basket (elevation comes from the arm angle).
	return FVector(FacingSign, 0.f, 0.f);
}

void APPBasketCharacter::ApplySprites()
{
	const int32 Idx = FMath::Clamp(SpriteVariant - 1, 0, 3);
	// FLIPPED (user request 2026-06-11): all players mirrored vs before. Left team (1&2) now default,
	// right team (3&4) mirrored. Arms are a bit longer (Z 1.3).
	const float FlipX = (SpriteVariant <= 2) ? 1.f : -1.f;
	// Arms are full-canvas layers -> same scale as the body so they overlay 1:1. SpriteScale sizes the
	// whole character uniformly (FlipX still mirrors via its sign).
	const float S = SpriteScale;
	if (SpriteBody)  { SpriteBody->SetSprite(PPVisual::SpriteFromTexture(this, BodyTextures[Idx]));  SpriteBody->SetRelativeScale3D(FVector(FlipX * S, 1.f, S)); }
	if (SpriteFront) { SpriteFront->SetSprite(PPVisual::SpriteFromTexture(this, ArmTextures[Idx]));  SpriteFront->SetRelativeScale3D(FVector(FlipX * S, 1.f, S)); }
	if (SpriteBack)  { SpriteBack->SetSprite(PPVisual::SpriteFromTexture(this, BackArmTexture));     SpriteBack->SetRelativeScale3D(FVector(FlipX * S, 1.f, S)); }
}

void APPBasketCharacter::OnRep_Charging()
{
	BP_OnChargingChanged(bCharging);
}

void APPBasketCharacter::OnRep_Variant()
{
	ApplySprites();
}
