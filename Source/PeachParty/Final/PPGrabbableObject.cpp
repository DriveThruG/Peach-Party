#include "Final/PPGrabbableObject.h"
#include "Core/PPCharacter.h"
#include "Core/PPPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

APPGrabbableObject::APPGrabbableObject()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true); // server simulates, clients interpolate

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
		Mesh->SetWorldScale3D(FVector(0.6f, 0.6f, 0.6f)); // placeholder "school item"
	}
}

void APPGrabbableObject::Grab(APPCharacter* NewHolder, USceneComponent* HoldPoint)
{
	if (!HasAuthority() || !NewHolder || !HoldPoint || Holder)
	{
		return;
	}
	Holder = NewHolder;
	bThrown = false;
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // don't shove the holder while carried
	AttachToComponent(HoldPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
}

void APPGrabbableObject::Drop()
{
	if (!HasAuthority())
	{
		return;
	}
	Holder = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
}

void APPGrabbableObject::ThrowWithImpulse(const FVector& Impulse, EPPTeam InThrowerTeam)
{
	if (!HasAuthority())
	{
		return;
	}
	Holder = nullptr;
	ThrowerTeam = InThrowerTeam;
	bThrown = true;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->OnComponentHit.AddDynamic(this, &APPGrabbableObject::OnHit);
	Mesh->AddImpulse(Impulse, NAME_None, /*bVelChange=*/true);
}

void APPGrabbableObject::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || !bThrown)
	{
		return;
	}

	const float Speed = Mesh->GetPhysicsLinearVelocity().Size();
	if (Speed >= MinImpactSpeed)
	{
		if (APPCharacter* HitChar = Cast<APPCharacter>(OtherActor))
		{
			const APPPlayerState* PS = HitChar->GetPlayerState<APPPlayerState>();
			if (PS && PS->GetTeam() != ThrowerTeam) // friendly throws don't wet
			{
				const float Wet = FMath::Clamp((Speed - MinImpactSpeed) * WetnessPerSpeed, 0.f, MaxWetnessHit);
				HitChar->ApplyWetness(Wet, ThrowerTeam);
				const FVector KnockDir = (HitChar->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
				HitChar->LaunchCharacter(KnockDir * 600.f + FVector(0.f, 0.f, 300.f), true, true); // knockback
			}
		}
		BP_OnImpact(GetActorLocation());
	}

	// One impact arms-down: stop reacting after the first significant hit.
	bThrown = false;
	Mesh->OnComponentHit.RemoveDynamic(this, &APPGrabbableObject::OnHit);
}
