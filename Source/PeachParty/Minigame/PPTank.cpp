#include "Minigame/PPTank.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

APPTank::APPTank()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true); // kinematic moves replicate as transform updates

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionProfileName(TEXT("BlockAll")); // blocks the shell; tanks don't simulate
	Body->SetSimulatePhysics(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Body->SetStaticMesh(CubeMesh.Object);
		Body->SetWorldScale3D(FVector(1.4f, 1.0f, 0.6f));
	}

	Barrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Barrel"));
	Barrel->SetupAttachment(Body);
	Barrel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh.Succeeded())
	{
		Barrel->SetStaticMesh(CubeMesh.Object);
		// thin forward bar; pivot from the body front
		Barrel->SetRelativeScale3D(FVector(0.06f, 0.06f, 0.6f));
		Barrel->SetRelativeLocation(FVector(60.f, 0.f, 30.f));
	}

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(Body);
	Muzzle->SetRelativeLocation(FVector(110.f, 0.f, 60.f));
}

void APPTank::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPTank, Health);
	DOREPLIFETIME(APPTank, Fuel);
	DOREPLIFETIME(APPTank, AimAngleDeg);
	DOREPLIFETIME(APPTank, PowerPercent);
	DOREPLIFETIME(APPTank, WeaponIndex);
	DOREPLIFETIME(APPTank, FacingSign);
}

void APPTank::InitTank(APPPlayerState* InOwner, EPPTeam InTeam, float InFacingSign)
{
	OwningPlayer = InOwner;
	Team = InTeam;
	FacingSign = (InFacingSign >= 0.f) ? 1.f : -1.f;
	Health = MaxHealth;
	Fuel = MaxFuel;

	// Face the enemy; forward vector then encodes facing, barrel pitch is local.
	SetActorRotation(FRotator(0.f, (FacingSign > 0.f) ? 0.f : 180.f, 0.f));
	UpdateBarrelVisual();
}

void APPTank::MoveStep(float InputDir)
{
	if (!HasAuthority() || Fuel <= 0.f || InputDir == 0.f)
	{
		return;
	}
	const float Dir = (InputDir > 0.f) ? 1.f : -1.f; // screen-right = +X (side-view camera at -Y)
	AddActorWorldOffset(FVector(Dir * MoveStepDistance, 0.f, 0.f));
	Fuel = FMath::Max(0.f, Fuel - FuelPerStep);
}

void APPTank::AimStep(float DeltaDeg)
{
	if (!HasAuthority())
	{
		return;
	}
	AimAngleDeg = FMath::Clamp(AimAngleDeg + DeltaDeg, 0.f, 89.f);
	UpdateBarrelVisual();
}

void APPTank::PowerStep(float DeltaPct)
{
	if (!HasAuthority())
	{
		return;
	}
	PowerPercent = FMath::Clamp(PowerPercent + DeltaPct, 10.f, 100.f);
}

void APPTank::SetWeaponIndex(int32 Index, int32 NumWeapons)
{
	if (HasAuthority() && NumWeapons > 0)
	{
		WeaponIndex = ((Index % NumWeapons) + NumWeapons) % NumWeapons;
	}
}

void APPTank::NextWeapon(int32 NumWeapons)
{
	SetWeaponIndex(WeaponIndex + 1, NumWeapons);
}

void APPTank::ApplyDamage(int32 Amount)
{
	if (HasAuthority() && Amount > 0)
	{
		Health = FMath::Max(0, Health - Amount);
	}
}

FVector APPTank::GetMuzzleLocation() const
{
	return Muzzle ? Muzzle->GetComponentLocation() : GetActorLocation();
}

FVector APPTank::GetLaunchDirection() const
{
	const FVector Fwd = GetActorForwardVector(); // (+/-X) from facing yaw
	const float Rad = FMath::DegreesToRadians(AimAngleDeg);
	return (Fwd * FMath::Cos(Rad) + FVector::UpVector * FMath::Sin(Rad)).GetSafeNormal();
}

void APPTank::OnRep_Aim()
{
	UpdateBarrelVisual();
}

void APPTank::UpdateBarrelVisual()
{
	if (Barrel)
	{
		// Pitch the barrel up by the aim angle (local space; actor yaw already faces the enemy).
		Barrel->SetRelativeRotation(FRotator(-AimAngleDeg, 0.f, 0.f));
	}
}
