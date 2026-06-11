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
	Sprite->SetRelativeRotation(FRotator(0.f, 0.f, 0.f)); // Paper2D default faces -Y = the side camera
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UTexture2D> HoopTex(TEXT("/Game/PeachParty/Minigames/BasketPeach/Graphics/Hoop.Hoop"));
	HoopTexture = HoopTex.Object;
}

void APPBasket::BeginPlay()
{
	Super::BeginPlay();
	if (UPaperSprite* S = PPVisual::SpriteFromTexture(this, HoopTexture))
	{
		Sprite->SetSprite(S);
		Ring->SetVisibility(false); // hide the placeholder ring
	}
	else
	{
		PPVisual::Tint(Ring, FLinearColor(0.95f, 0.15f, 0.10f)); // fallback red hoop
	}
}

FVector APPBasket::GetMouthLocation() const
{
	return Mouth ? Mouth->GetComponentLocation() : GetActorLocation();
}
