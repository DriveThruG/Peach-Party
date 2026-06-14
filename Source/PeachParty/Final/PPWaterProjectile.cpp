#include "Final/PPWaterProjectile.h"
#include "Core/PPCharacter.h"
#include "Core/PPPlayerState.h"
#include "Minigame/PPVisual.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

APPWaterProjectile::APPWaterProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(10.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetNotifyRigidBodyCollision(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) { Mesh->SetStaticMesh(Sphere.Object); Mesh->SetWorldScale3D(FVector(0.2f)); }

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->bShouldBounce = false;
	Movement->ProjectileGravityScale = 0.25f; // light arc — water guns are fairly flat
	Movement->bAutoActivate = false;

	InitialLifeSpan = 5.f;
}

void APPWaterProjectile::Launch(const FVector& Velocity, EPPTeam InInstigatorTeam, float InWetness, AActor* IgnoredShooter)
{
	if (!HasAuthority())
	{
		return;
	}
	InstigatorTeam = InInstigatorTeam;
	WetnessAmount = InWetness;
	OnRep_Team(); // tint on the server/listen host too (OnRep only fires on remote clients)
	if (IgnoredShooter)
	{
		Collision->IgnoreActorWhenMoving(IgnoredShooter, true);
	}
	Collision->OnComponentHit.AddDynamic(this, &APPWaterProjectile::OnHit);
	Movement->Velocity = Velocity;
	Movement->Activate();
}

void APPWaterProjectile::OnRep_Team()
{
	PPVisual::Tint(Mesh, PPVisual::TeamColor(InstigatorTeam));
}

void APPWaterProjectile::MulticastImpact_Implementation(FVector Location)
{
	BP_OnImpact(Location, InstigatorTeam);
}

void APPWaterProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPWaterProjectile, InstigatorTeam);
}

void APPWaterProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || bImpacted)
	{
		return;
	}
	bImpacted = true;

	if (APPCharacter* HitChar = Cast<APPCharacter>(OtherActor))
	{
		const APPPlayerState* PS = HitChar->GetPlayerState<APPPlayerState>();
		// Friendly water does nothing; only wet down enemies.
		if (PS && PS->GetTeam() != InstigatorTeam)
		{
			HitChar->ApplyWetness(WetnessAmount, InstigatorTeam);
		}
	}

	// Team-coloured splash on all clients, then despawn (short delay so the multicast lands).
	MulticastImpact(GetActorLocation());
	Movement->Deactivate();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetVisibility(false);
	SetLifeSpan(0.1f);
}
