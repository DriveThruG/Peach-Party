#include "Minigame/PPBasket.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

APPBasket::APPBasket()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;        // cosmetic, but visible to everyone watching the arena
	bAlwaysRelevant = true;

	Mouth = CreateDefaultSubobject<USceneComponent>(TEXT("Mouth"));
	SetRootComponent(Mouth);

	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(Mouth);
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision); // proximity scoring, no physics needed
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RingMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (RingMesh.Succeeded())
	{
		Ring->SetStaticMesh(RingMesh.Object);
		Ring->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.1f)); // flat disc ~ hoop
	}
}

FVector APPBasket::GetMouthLocation() const
{
	return Mouth ? Mouth->GetComponentLocation() : GetActorLocation();
}
