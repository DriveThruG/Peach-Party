#include "Minigame/PPBasketBall.h"
#include "Minigame/PPVisual.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

APPBasketBall::APPBasketBall()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;          // far-flung arena; see APPMinigameBase note
	SetReplicateMovement(true);      // server simulates, clients interpolate

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);

	// Visible primitive out of the box (editor content); guarded so it still compiles/runs without it.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
		Mesh->SetWorldScale3D(FVector(0.4f));
	}
}

void APPBasketBall::BeginPlay()
{
	Super::BeginPlay();
	PPVisual::Tint(Mesh, FLinearColor(1.0f, 0.55f, 0.05f)); // basketball orange
}

void APPBasketBall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPBasketBall, Holder);
}

void APPBasketBall::GrabBy(APPBasketCharacter* NewHolder, USceneComponent* HandPoint)
{
	if (!HasAuthority() || !NewHolder || !HandPoint)
	{
		return;
	}

	Holder = NewHolder;
	Mesh->SetSimulatePhysics(false);
	AttachToComponent(HandPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
}

void APPBasketBall::ThrowWithImpulse(const FVector& Impulse)
{
	if (!HasAuthority())
	{
		return;
	}

	Holder = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Mesh->SetSimulatePhysics(true);
	Mesh->AddImpulse(Impulse, NAME_None, /*bVelChange=*/true);
}

void APPBasketBall::ResetTo(const FTransform& Transform)
{
	if (!HasAuthority())
	{
		return;
	}

	Holder = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Mesh->SetSimulatePhysics(true);
	SetActorTransform(Transform, false, nullptr, ETeleportType::ResetPhysics);
	Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
}
