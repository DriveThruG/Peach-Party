#include "Minigame/PPProjectile.h"
#include "Minigame/PPPeachArtilleryGame.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

APPProjectile::APPProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true); // server integrates the arc; clients interpolate

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetNotifyRigidBodyCollision(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
		Mesh->SetWorldScale3D(FVector(0.25f));
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->bShouldBounce = false;
	Movement->ProjectileGravityScale = 1.f;
	Movement->bAutoActivate = false; // activate on Launch
	Movement->bRotationFollowsVelocity = true;

	InitialLifeSpan = 15.f; // safety: never linger if it somehow never hits
}

void APPProjectile::Launch(const FVector& Velocity, APPPeachArtilleryGame* InGame, int32 InWeaponIndex, AActor* IgnoredShooter)
{
	if (!HasAuthority())
	{
		return;
	}

	OwnerGame = InGame;
	WeaponIndex = InWeaponIndex;

	if (IgnoredShooter)
	{
		Collision->IgnoreActorWhenMoving(IgnoredShooter, true);
	}

	Collision->OnComponentHit.AddDynamic(this, &APPProjectile::OnHit);
	Movement->Velocity = Velocity;
	Movement->Activate();
}

void APPProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || bImpacted)
	{
		return;
	}
	bImpacted = true;

	if (OwnerGame)
	{
		OwnerGame->OnProjectileImpact(GetActorLocation(), WeaponIndex);
	}
	Destroy();
}

void APPProjectile::Destroyed()
{
	// If we expired (lifespan) without ever hitting, treat it as a miss so the turn still advances.
	if (HasAuthority() && !bImpacted && OwnerGame)
	{
		bImpacted = true;
		OwnerGame->OnProjectileImpact(GetActorLocation(), WeaponIndex);
	}
	Super::Destroyed();
}
