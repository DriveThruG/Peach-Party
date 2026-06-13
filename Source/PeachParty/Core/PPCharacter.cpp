#include "Core/PPCharacter.h"
#include "Core/PPPlayerController.h"
#include "Core/PPPlayerState.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"

APPCharacter::APPCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// Both players spawn into a (near-empty) map; never let capsule overlap at the origin block the
	// second pawn from spawning — that would leave a player with no pawn and no input.
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
}

void APPCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->IsLocalController())
		{
			// Clear any leftover viewport widgets, then make sure game input is active.
			if (UGameViewportClient* VP = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
			{
				VP->RemoveAllViewportWidgets();
			}
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;
		}
	}
}

void APPCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// The whole game is one button. (MGPrimary = SpaceBar in DefaultInput.ini.)
	PlayerInputComponent->BindAction("MGPrimary", IE_Pressed, this, &APPCharacter::OnMG_PrimaryPressed);
	PlayerInputComponent->BindAction("MGPrimary", IE_Released, this, &APPCharacter::OnMG_PrimaryReleased);
}

bool APPCharacter::IsInMinigame() const
{
	const APPPlayerState* PS = GetPlayerState<APPPlayerState>();
	return PS && PS->GetCurrentMinigame() != nullptr;
}

void APPCharacter::ForwardMinigameInput(FName Action, bool bPressed)
{
	if (!IsInMinigame())
	{
		return;
	}
	if (APPPlayerController* PC = Cast<APPPlayerController>(GetController()))
	{
		PC->ServerMinigameInput(Action, bPressed);
	}
}

void APPCharacter::OnMG_PrimaryPressed()  { ForwardMinigameInput(TEXT("Primary"), true); }
void APPCharacter::OnMG_PrimaryReleased() { ForwardMinigameInput(TEXT("Primary"), false); }
