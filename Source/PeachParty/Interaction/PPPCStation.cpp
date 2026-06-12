#include "Interaction/PPPCStation.h"
#include "Core/PPPlayerController.h"
#include "Core/PPPlayerState.h"
#include "Core/PPGameState.h"
#include "Core/PPGameMode.h"
#include "Core/PPTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

APPPCStation::APPPCStation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> StationModel(TEXT("/Game/PeachParty/Interactables/PP_PC_Station.PP_PC_Station"));

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// The user's imported station model. Resolves by path on the user's machine; placeholder cubes are
	// hidden by default now that a real mesh exists (see bHidePlaceholderBlocks + OnConstruction).
	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
	StationMesh->SetupAttachment(SceneRoot);
	StationMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (StationModel.Succeeded()) { StationMesh->SetStaticMesh(StationModel.Object); }

	// Desk (a wide low block). Components are siblings under SceneRoot so scales don't cascade.
	DeskMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeskMesh"));
	DeskMesh->SetupAttachment(SceneRoot);
	DeskMesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	DeskMesh->SetRelativeScale3D(FVector(1.2f, 1.8f, 0.8f));
	if (CubeMesh.Succeeded()) { DeskMesh->SetStaticMesh(CubeMesh.Object); }

	// Screen (a thin upright block) — the "filler screen" you look at when seated.
	ScreenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenMesh"));
	ScreenMesh->SetupAttachment(SceneRoot);
	ScreenMesh->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	ScreenMesh->SetRelativeScale3D(FVector(0.12f, 1.4f, 1.0f));
	ScreenMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh.Succeeded()) { ScreenMesh->SetStaticMesh(CubeMesh.Object); }

	// Seat-side camera looking at the screen: this is the player's view while seated.
	MinigameCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("MinigameCamera"));
	MinigameCamera->SetupAttachment(SceneRoot);
	MinigameCamera->SetRelativeLocation(FVector(-70.f, 0.f, 150.f));
	MinigameCamera->SetRelativeRotation(FRotator(0.f, 0.f, 0.f)); // faces +X toward the screen
	// Seated = a flat "looking at the monitor" view -> orthographic too (matches the minigame look).
	// OrthoWidth frames the screen mesh (~140 wide); raise it to show more around the monitor.
	MinigameCamera->SetProjectionMode(ECameraProjectionMode::Orthographic);
	MinigameCamera->SetOrthoWidth(SeatedOrthoWidth);
	MinigameCamera->SetConstraintAspectRatio(false); // fill the whole viewport, no letterbox bars

	SeatPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint"));
	SeatPoint->SetupAttachment(SceneRoot);
	SeatPoint->SetRelativeLocation(FVector(-90.f, 0.f, 0.f));
}

void APPPCStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPPCStation, OccupantPlayerState);
}

void APPPCStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Hide the placeholder cubes when you've assigned your own StationMesh. Runs live in the editor.
	if (DeskMesh)   { DeskMesh->SetVisibility(!bHidePlaceholderBlocks); }
	if (ScreenMesh) { ScreenMesh->SetVisibility(!bHidePlaceholderBlocks); }
}

bool APPPCStation::CanInteract(APPPlayerController* InteractingController) const
{
	if (!InteractingController)
	{
		return false;
	}

	// Free seat -> anyone may sit. Occupied -> only the occupant may toggle (stand up).
	if (!IsOccupied())
	{
		return true;
	}
	return OccupantPlayerState == InteractingController->PlayerState;
}

void APPPCStation::ServerInteract(APPPlayerController* InteractingController)
{
	if (!HasAuthority() || !InteractingController)
	{
		return;
	}

	// Toggle: interacting with the seat you're already in stands you up.
	if (OccupantPlayerState == InteractingController->PlayerState)
	{
		ServerReleaseOccupant();
		return;
	}

	if (IsOccupied())
	{
		return; // taken by someone else
	}

	SeatController(InteractingController);
}

void APPPCStation::SeatController(APPPlayerController* Controller)
{
	APPPlayerState* PS = Controller ? Cast<APPPlayerState>(Controller->PlayerState) : nullptr;
	if (!PS)
	{
		return;
	}

	OccupantPlayerState = PS;
	OnRep_Occupant(); // host mirror

	Controller->SetSeatedStation(this);

	// In the Lobby, sitting at a PC means "ready". Other phases use the seat for the minigame.
	const APPGameState* GS = GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
	if (GS && GS->GetCurrentPhase() == EMatchPhase::Lobby)
	{
		PS->SetReady(true);

		if (APPGameMode* GM = GetWorld()->GetAuthGameMode<APPGameMode>())
		{
			GM->NotifyReadyStateChanged();
		}
	}
}

void APPPCStation::ServerReleaseOccupant()
{
	if (!HasAuthority() || !OccupantPlayerState)
	{
		return;
	}

	APPPlayerState* PS = OccupantPlayerState;

	// Clear the controller's seat (blends its camera back to the pawn => instant return to 3D).
	if (APPPlayerController* PC = PS->GetOwningController() ? Cast<APPPlayerController>(PS->GetOwningController()) : nullptr)
	{
		PC->SetSeatedStation(nullptr);
	}

	OccupantPlayerState = nullptr;
	OnRep_Occupant(); // host mirror

	const APPGameState* GS = GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
	if (GS && GS->GetCurrentPhase() == EMatchPhase::Lobby)
	{
		PS->SetReady(false);

		if (APPGameMode* GM = GetWorld()->GetAuthGameMode<APPGameMode>())
		{
			GM->NotifyReadyStateChanged();
		}
	}
}

void APPPCStation::OnRep_Occupant()
{
	BP_OnOccupantChanged(OccupantPlayerState);
}

void APPPCStation::OnBeginFocus()
{
	BP_OnFocusChanged(true);
}

void APPPCStation::OnEndFocus()
{
	BP_OnFocusChanged(false);
}
