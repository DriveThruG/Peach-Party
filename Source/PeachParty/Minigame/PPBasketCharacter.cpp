#include "Minigame/PPBasketCharacter.h"
#include "Minigame/PPVisual.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

APPBasketCharacter::APPBasketCharacter()
{
	PrimaryActorTick.bCanEverTick = true; // server raises arms; everyone eases the visual arms
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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Flat body+head quad: thin along Y (the camera depth axis) so it reads as a 2D sprite.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Body);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(0.5f, 0.1f, 1.6f));
	if (CubeMesh.Succeeded()) { BodyMesh->SetStaticMesh(CubeMesh.Object); }

	// Separate arms quad — rotates up while charging.
	ArmsMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmsMesh"));
	ArmsMesh->SetupAttachment(Body);
	ArmsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArmsMesh->SetRelativeLocation(FVector(20.f, 0.f, 45.f)); // shoulder, slightly forward
	ArmsMesh->SetRelativeScale3D(FVector(0.6f, 0.12f, 0.16f));
	if (CubeMesh.Succeeded()) { ArmsMesh->SetStaticMesh(CubeMesh.Object); }

	HandPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HandPoint"));
	HandPoint->SetupAttachment(Body);
	HandPoint->SetRelativeLocation(FVector(45.f, 0.f, 70.f));
}

void APPBasketCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPBasketCharacter, bCharging);
	DOREPLIFETIME(APPBasketCharacter, Team);
}

void APPBasketCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// SERVER: raise the gameplay arm angle while charging.
	if (HasAuthority() && bCharging)
	{
		ArmAngleDeg = FMath::Min(MaxArmAngleDeg, ArmAngleDeg + ArmRaiseRateDegPerSec * DeltaSeconds);
	}

	// ALL machines: ease the visual arms toward raised/lowered (from the replicated bCharging).
	const float Target = bCharging ? 75.f : 0.f;
	VisualArmAngle = FMath::FInterpTo(VisualArmAngle, Target, DeltaSeconds, 8.f);
	if (ArmsMesh)
	{
		ArmsMesh->SetRelativeRotation(FRotator(VisualArmAngle, 0.f, 0.f)); // pitch about Y -> swings in view
	}
}

void APPBasketCharacter::InitCharacter(APPPlayerState* InOwner, EPPTeam InTeam)
{
	OwningPlayer = InOwner;
	Team = InTeam;
	ApplyTeamColor();   // server
	OnRep_Team();       // host mirror
}

void APPBasketCharacter::DoJump(float UpImpulse, float ForwardImpulse)
{
	if (!HasAuthority())
	{
		return;
	}
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
	FVector Fwd = GetActorForwardVector();
	Fwd.Z = 0.f;
	return Fwd.GetSafeNormal();
}

void APPBasketCharacter::ApplyTeamColor()
{
	PPVisual::Tint(BodyMesh, PPVisual::TeamColor(Team));
	PPVisual::Tint(ArmsMesh, PPVisual::TeamColor(Team) * 1.3f); // arms a touch brighter
}

void APPBasketCharacter::OnRep_Charging()
{
	BP_OnChargingChanged(bCharging);
}

void APPBasketCharacter::OnRep_Team()
{
	ApplyTeamColor();
}
