#include "Minigame/PPBasketBall.h"
#include "Minigame/PPVisual.h"
#include "Components/StaticMeshComponent.h"
#include "PaperSpriteComponent.h"
#include "PaperSprite.h"
#include "PhysicsEngine/BodyInstance.h"
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
	Mesh->BodyInstance.DOFMode = EDOFMode::XZPlane; // keep the ball in the 2D plane

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
		Mesh->SetWorldScale3D(FVector(0.4f));
	}

	Sprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Mesh);
	Sprite->SetRelativeRotation(FRotator(0.f, 90.f, 0.f)); // face the camera; tune if edge-on
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UTexture2D> BallTex(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Ball.Ball"));
	BallTexture = BallTex.Object;
}

void APPBasketBall::BeginPlay()
{
	Super::BeginPlay();
	if (UPaperSprite* S = PPVisual::SpriteFromTexture(this, BallTexture))
	{
		Sprite->SetSprite(S);
		Mesh->SetVisibility(false); // hide the placeholder sphere; keep its collision
	}
	else
	{
		PPVisual::Tint(Mesh, FLinearColor(1.0f, 0.55f, 0.05f)); // fallback orange sphere
	}
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
