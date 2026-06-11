#include "Minigame/PPBasket.h"
#include "Minigame/PPVisual.h"
#include "Components/StaticMeshComponent.h"
#include "PaperSpriteComponent.h"
#include "PaperSprite.h"
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

	Sprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Mouth);
	Sprite->SetRelativeRotation(FRotator(0.f, 90.f, 0.f)); // face the camera; tune if edge-on
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UPaperSprite> HoopSpr(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Hoop_Sprite.Hoop_Sprite"));
	if (HoopSpr.Succeeded())
	{
		Sprite->SetSprite(HoopSpr.Object);
		Ring->SetVisibility(false); // hide the placeholder ring
	}
}

void APPBasket::BeginPlay()
{
	Super::BeginPlay();
	PPVisual::Tint(Ring, FLinearColor(0.95f, 0.15f, 0.10f)); // red hoop
}

FVector APPBasket::GetMouthLocation() const
{
	return Mouth ? Mouth->GetComponentLocation() : GetActorLocation();
}
