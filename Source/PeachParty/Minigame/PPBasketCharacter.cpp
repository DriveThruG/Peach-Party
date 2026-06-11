#include "Minigame/PPBasketCharacter.h"
#include "Components/CapsuleComponent.h"
#include "PaperSpriteComponent.h"
#include "PaperSprite.h"
#include "PhysicsEngine/BodyInstance.h"
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
	Body->InitCapsuleSize(34.f, 88.f);
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetSimulatePhysics(true);
	Body->SetCenterOfMass(FVector(0.f, 0.f, 50.f)); // tippy on purpose
	Body->SetAngularDamping(0.5f);
	Body->SetLinearDamping(0.1f);
	// Lock to the X-Z plane: a true 2D side-view (move in X/Z, only roll about Y to wobble/tip).
	Body->BodyInstance.DOFMode = EDOFMode::XZPlane;

	// --- Paper2D sprite layers (back arm, body, front arm). Y offsets order them vs the camera (-Y). ---
	SpriteBody = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("SpriteBody"));
	SpriteBody->SetupAttachment(Body);
	SpriteBody->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 0.f), SpriteFacing);
	SpriteBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ArmPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ArmPivot"));
	ArmPivot->SetupAttachment(Body);
	ArmPivot->SetRelativeLocation(FVector(0.f, 0.f, 45.f)); // shoulder height

	SpriteFront = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("SpriteFront"));
	SpriteFront->SetupAttachment(ArmPivot);
	SpriteFront->SetRelativeLocationAndRotation(FVector(0.f, -2.f, 0.f), SpriteFacing); // closest to camera
	SpriteFront->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SpriteBack = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("SpriteBack"));
	SpriteBack->SetupAttachment(ArmPivot);
	SpriteBack->SetRelativeLocationAndRotation(FVector(0.f, 2.f, 0.f), SpriteFacing); // behind
	SpriteBack->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HandPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HandPoint"));
	HandPoint->SetupAttachment(Body);
	HandPoint->SetRelativeLocation(FVector(45.f, 0.f, 70.f));

	// Load the sprite assets by path (user creates them via right-click texture -> Create Sprite;
	// default sprite name is <Texture>_Sprite in the same folder). Null until they exist.
	static ConstructorHelpers::FObjectFinder<UPaperSprite> B1(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player01_Body_Sprite.Player01_Body_Sprite"));
	static ConstructorHelpers::FObjectFinder<UPaperSprite> B2(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player02_Body_Sprite.Player02_Body_Sprite"));
	static ConstructorHelpers::FObjectFinder<UPaperSprite> B3(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player03_Body_Sprite.Player03_Body_Sprite"));
	static ConstructorHelpers::FObjectFinder<UPaperSprite> B4(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player04_Body_Sprite.Player04_Body_Sprite"));
	static ConstructorHelpers::FObjectFinder<UPaperSprite> A1(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/PLayer01_Arm_Sprite.PLayer01_Arm_Sprite"));
	static ConstructorHelpers::FObjectFinder<UPaperSprite> A2(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player02_Arm_Sprite.Player02_Arm_Sprite"));
	static ConstructorHelpers::FObjectFinder<UPaperSprite> A3(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player03_Arm_Sprite.Player03_Arm_Sprite"));
	static ConstructorHelpers::FObjectFinder<UPaperSprite> A4(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Player04_Arm_Sprite.Player04_Arm_Sprite"));
	static ConstructorHelpers::FObjectFinder<UPaperSprite> Back(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Arm_Left_Sprite.Arm_Left_Sprite"));

	BodySprites[0] = B1.Object; BodySprites[1] = B2.Object; BodySprites[2] = B3.Object; BodySprites[3] = B4.Object;
	ArmSprites[0]  = A1.Object; ArmSprites[1]  = A2.Object; ArmSprites[2]  = A3.Object; ArmSprites[3]  = A4.Object;
	BackArmSprite  = Back.Object;
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

	if (HasAuthority() && bCharging)
	{
		ArmAngleDeg = FMath::Min(MaxArmAngleDeg, ArmAngleDeg + ArmRaiseRateDegPerSec * DeltaSeconds);
	}

	// Ease the visual arm raise everywhere (from replicated bCharging). Pitch the shoulder pivot.
	const float Target = bCharging ? 75.f : 0.f;
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
	if (!HasAuthority())
	{
		return;
	}
	// Jump along the body's CURRENT (tilted) up vector -> wobble produces angled jumps. Small bias toward the enemy.
	const FVector Impulse = GetActorUpVector() * UpImpulse + FVector(FacingSign, 0.f, 0.f) * (ForwardImpulse * 0.4f);
	Body->AddImpulse(Impulse, NAME_None, /*bVelChange=*/true);
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
	if (SpriteBody)  { SpriteBody->SetSprite(BodySprites[Idx]); }
	if (SpriteFront) { SpriteFront->SetSprite(ArmSprites[Idx]); }
	if (SpriteBack)  { SpriteBack->SetSprite(BackArmSprite); }
}

void APPBasketCharacter::OnRep_Charging()
{
	BP_OnChargingChanged(bCharging);
}

void APPBasketCharacter::OnRep_Variant()
{
	ApplySprites();
}
