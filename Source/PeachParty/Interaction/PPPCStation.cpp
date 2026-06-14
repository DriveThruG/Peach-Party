#include "Interaction/PPPCStation.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APPPCStation::APPPCStation()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Empty optional slot — assign your OWN model per placed instance (Details panel) and set
	// bHidePlaceholderBlocks=true to hide the cubes.
	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
	StationMesh->SetupAttachment(SceneRoot);
	StationMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Desk (a wide low block).
	DeskMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeskMesh"));
	DeskMesh->SetupAttachment(SceneRoot);
	DeskMesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	DeskMesh->SetRelativeScale3D(FVector(1.2f, 1.8f, 0.8f));
	if (CubeMesh.Succeeded()) { DeskMesh->SetStaticMesh(CubeMesh.Object); }

	// Screen (a thin upright block).
	ScreenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenMesh"));
	ScreenMesh->SetupAttachment(SceneRoot);
	ScreenMesh->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	ScreenMesh->SetRelativeScale3D(FVector(0.12f, 1.4f, 1.0f));
	ScreenMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh.Succeeded()) { ScreenMesh->SetStaticMesh(CubeMesh.Object); }
}

void APPPCStation::BeginPlay()
{
	Super::BeginPlay();
	ApplyPlaceholderVisibility();
}

void APPPCStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPlaceholderVisibility();
}

void APPPCStation::ApplyPlaceholderVisibility()
{
	// Only hide the cubes if a real model is actually present — otherwise you'd see NOTHING.
	const bool bHasModel = (StationMesh && StationMesh->GetStaticMesh() != nullptr);
	const bool bHide = bHidePlaceholderBlocks && bHasModel;
	if (DeskMesh)   { DeskMesh->SetVisibility(!bHide); }
	if (ScreenMesh) { ScreenMesh->SetVisibility(!bHide); }
}
