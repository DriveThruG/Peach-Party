#include "Core/PPPlayerController.h"
#include "Core/PPPlayerState.h"
#include "Core/PPGameState.h"
#include "Core/PPGameMode.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void APPPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void APPPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (IsLocalController())
	{
		UpdateClassSelectHud();
	}
}

void APPPlayerController::ServerSelectClass_Implementation(EPPClass NewClass)
{
	APPPlayerState* PS = GetPlayerState<APPPlayerState>();
	if (!PS)
	{
		return;
	}
	if (PS->SetSelectedClass(NewClass))
	{
		// A pick during ClassSelect may complete the gate that starts the fight.
		if (APPGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APPGameMode>() : nullptr)
		{
			GM->NotifyClassChosen();
		}
	}
}

void APPPlayerController::UpdateClassSelectHud()
{
	const APPGameState* GS = GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
	const APPPlayerState* PS = GetPlayerState<APPPlayerState>();

	// Show the class menu only while choosing and before this player has confirmed a class.
	const bool bWantHud = GS && PS
		&& GS->GetCurrentPhase() == EMatchPhase::ClassSelect
		&& !PS->HasChosenClass();

	if (bWantHud && !ClassSelectHud)
	{
		if (UClass* WClass = LoadClass<UUserWidget>(nullptr,
			TEXT("/Game/PeachParty/UI/WBP_ClassSelect.WBP_ClassSelect_C"), nullptr, LOAD_Quiet | LOAD_NoWarn))
		{
			ClassSelectHud = CreateWidget<UUserWidget>(this, WClass);
			if (ClassSelectHud)
			{
				ClassSelectHud->AddToViewport(50);
				// Free the cursor ONLY when a real menu is on screen. With no widget yet, keep normal
				// gameplay input (mouse turns the camera) and pick a class with the 1-4 keyboard fallback.
				SetInputMode(FInputModeGameAndUI());
				bShowMouseCursor = true;
				bClassSelectInputActive = true;
			}
		}
	}
	else if (!bWantHud && (ClassSelectHud || bClassSelectInputActive))
	{
		if (ClassSelectHud)
		{
			ClassSelectHud->RemoveFromParent();
			ClassSelectHud = nullptr;
		}
		// Back to gameplay input for the fight.
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		bClassSelectInputActive = false;
	}
}

void APPPlayerController::PlayTransitionFade(float HalfSeconds, bool bHold)
{
	if (!PlayerCameraManager)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(FadeTimer);
	PlayerCameraManager->StartCameraFade(0.f, 1.f, HalfSeconds, FLinearColor::Black, false, /*bHoldWhenFinished=*/true);

	if (!bHold)
	{
		GetWorldTimerManager().SetTimer(FadeTimer,
			FTimerDelegate::CreateUObject(this, &APPPlayerController::FadeBackIn, HalfSeconds), HalfSeconds, false);
	}
}

void APPPlayerController::FadeBackIn(float Seconds)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.f, 0.f, Seconds, FLinearColor::Black, false, false);
	}
}
