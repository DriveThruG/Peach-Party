#include "Core/PPPlayerController.h"
#include "Core/PPPlayerState.h"
#include "Minigame/PPMinigameBase.h"
#include "Minigame/PPPeachBasketUMGGame.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void APPPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Camera state only matters to the local owner.
	DOREPLIFETIME_CONDITION(APPPlayerController, ServerViewTarget, COND_OwnerOnly);
}

void APPPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (IsLocalController())
	{
		UpdateMinigameHud();
	}
}

AActor* APPPlayerController::GetViewedMinigame() const
{
	if (Cast<APPMinigameBase>(ServerViewTarget))
	{
		return ServerViewTarget;
	}
	if (const APPPlayerState* PS = GetPlayerState<APPPlayerState>())
	{
		return PS->GetCurrentMinigame();
	}
	return nullptr;
}

void APPPlayerController::UpdateMinigameHud()
{
	// Show the widget only while viewing a UMG-type minigame; remove it otherwise.
	const bool bWantHud = Cast<APPPeachBasketUMGGame>(GetViewedMinigame()) != nullptr;

	if (bWantHud)
	{
		if (!MinigameHud)
		{
			if (UClass* WClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/PeachParty/UI/WBP_BasketGame.WBP_BasketGame_C")))
			{
				MinigameHud = CreateWidget<UUserWidget>(this, WClass);
			}
		}
		// Self-heal: (re)add to the viewport if something removed it (e.g. a pawn restart).
		if (MinigameHud && !MinigameHud->IsInViewport())
		{
			MinigameHud->AddToViewport(50);
		}
	}
	else if (MinigameHud)
	{
		MinigameHud->RemoveFromParent();
		MinigameHud = nullptr;
	}
}

void APPPlayerController::SetServerViewTarget(AActor* NewTarget)
{
	if (!HasAuthority() || ServerViewTarget == NewTarget)
	{
		return;
	}
	ServerViewTarget = NewTarget;
	OnRep_View(); // host mirror
}

void APPPlayerController::ServerMinigameInput_Implementation(FName Action, bool bPressed)
{
	// SERVER. Forward to the match this player is actually in; reject otherwise.
	APPPlayerState* PS = GetPlayerState<APPPlayerState>();
	APPMinigameBase* Match = PS ? PS->GetCurrentMinigame() : nullptr;
	if (Match && !Match->IsFinished())
	{
		Match->HandleInput(PS, Action, bPressed);
	}
}

void APPPlayerController::OnRep_View()
{
	RefreshViewTarget();
	if (IsLocalController())
	{
		PlayTransitionFade(0.25f);
	}
}

void APPPlayerController::PlayTransitionFade(float HalfSeconds)
{
	if (!PlayerCameraManager)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(FadeTimer);
	PlayerCameraManager->StartCameraFade(0.f, 1.f, HalfSeconds, FLinearColor::Black, false, /*bHoldWhenFinished=*/true);
	GetWorldTimerManager().SetTimer(FadeTimer,
		FTimerDelegate::CreateUObject(this, &APPPlayerController::FadeBackIn, HalfSeconds), HalfSeconds, false);
}

void APPPlayerController::FadeBackIn(float Seconds)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.f, 0.f, Seconds, FLinearColor::Black, false, false);
	}
}

void APPPlayerController::RefreshViewTarget(float BlendTime)
{
	// Priority: active minigame  >  own pawn.
	AActor* Target = ServerViewTarget ? ServerViewTarget : Cast<AActor>(GetPawn());
	if (Target)
	{
		SetViewTargetWithBlend(Target, BlendTime, EViewTargetBlendFunction::VTBlend_Cubic);
	}
}
