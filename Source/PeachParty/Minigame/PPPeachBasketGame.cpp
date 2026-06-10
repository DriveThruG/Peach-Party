#include "Minigame/PPPeachBasketGame.h"
#include "Camera/CameraComponent.h"

APPPeachBasketGame::APPPeachBasketGame()
{
	MinigameType = EMinigameType::PeachBasket;
	Duration = 30.f; // short and chaotic

	// Physics chaos benefits from a wider, slightly pulled-back framing.
	GameCamera->SetRelativeLocation(FVector(0.f, -1200.f, 350.f));
	GameCamera->SetRelativeRotation(FRotator(-12.f, 90.f, 0.f));
}

void APPPeachBasketGame::OnMinigameStarted()
{
	// SERVER. TODO: spawn baskets + peach spawner + arena bounds relative to GetActorLocation().
	// Hook the basket overlap to AddScore(<owning player>, 1). The base Duration timer ends it.
	UE_LOG(LogTemp, Log, TEXT("[PeachBasket] started at %s"), *GetActorLocation().ToString());
}

void APPPeachBasketGame::OnMinigameFinished()
{
	// SERVER. TODO: destroy spawned peaches/baskets so the arena is clean for the next game.
}
