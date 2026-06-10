#include "Minigame/PPBasketCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

APPBasketCharacter::APPBasketCharacter()
{
	PrimaryActorTick.bCanEverTick = true; // server advances the arm charge
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);

	Body = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->InitCapsuleSize(34.f, 88.f);
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetSimulatePhysics(true);
	// Tippy on purpose: high centre of mass + low angular damping => constant wobble, easy to tip.
	Body->SetCenterOfMass(FVector(0.f, 0.f, 50.f));
	Body->SetAngularDamping(0.5f);
	Body->SetLinearDamping(0.1f);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Body);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CapsuleVis(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CapsuleVis.Succeeded())
	{
		Visual->SetStaticMesh(CapsuleVis.Object);
		Visual->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.8f));
		Visual->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
	}

	// Hand sits in front and up — where the ball attaches and where grab proximity is measured.
	HandPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HandPoint"));
	HandPoint->SetupAttachment(Body);
	HandPoint->SetRelativeLocation(FVector(45.f, 0.f, 70.f));
}

void APPBasketCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPBasketCharacter, bCharging);
}

void APPBasketCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// SERVER: raise the arms while charging (the release angle decides the throw).
	if (HasAuthority() && bCharging)
	{
		ArmAngleDeg = FMath::Min(MaxArmAngleDeg, ArmAngleDeg + ArmRaiseRateDegPerSec * DeltaSeconds);
	}
}

void APPBasketCharacter::InitCharacter(APPPlayerState* InOwner, EPPTeam InTeam)
{
	OwningPlayer = InOwner;
	Team = InTeam;
}

void APPBasketCharacter::DoJump(float UpImpulse, float ForwardImpulse)
{
	if (!HasAuthority())
	{
		return;
	}
	// Direction follows the body's CURRENT orientation, so a tilted peach jumps sideways.
	const FVector Impulse = GetActorForwardVector() * ForwardImpulse + FVector::UpVector * UpImpulse;
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
		OnRep_Charging(); // host mirror
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
	FVector Fwd = GetActorForwardVector();
	Fwd.Z = 0.f;
	return Fwd.GetSafeNormal();
}

void APPBasketCharacter::OnRep_Charging()
{
	BP_OnChargingChanged(bCharging);
}
