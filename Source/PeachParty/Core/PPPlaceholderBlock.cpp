#include "Core/PPPlaceholderBlock.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

APPPlaceholderBlock::APPPlaceholderBlock()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true; // a handful of static blocks; keep it simple

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetSimulatePhysics(false);
	Mesh->SetMobility(EComponentMobility::Movable); // allow the spawn transform's scale

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
}
