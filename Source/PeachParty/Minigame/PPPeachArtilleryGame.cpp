#include "Minigame/PPPeachArtilleryGame.h"
#include "Core/PPPlayerState.h"
#include "Camera/CameraComponent.h"
#include "Net/UnrealNetwork.h"

APPPeachArtilleryGame::APPPeachArtilleryGame()
{
	MinigameType = EMinigameType::PeachArtillery;
	Duration = 60.f; // turn-based, give it room

	GameCamera->SetRelativeLocation(FVector(0.f, -1500.f, 400.f));
	GameCamera->SetRelativeRotation(FRotator(-8.f, 90.f, 0.f));
}

void APPPeachArtilleryGame::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPPeachArtilleryGame, Player1Health);
	DOREPLIFETIME(APPPeachArtilleryGame, Player2Health);
}

void APPPeachArtilleryGame::OnMinigameStarted()
{
	Player1Health = MaxHealth;
	Player2Health = MaxHealth;
	// SERVER. TODO: spawn two tanks + destructible terrain relative to GetActorLocation();
	// start the turn loop. Each landed shot calls ApplyDamage(<hit player>, dmg).
	UE_LOG(LogTemp, Log, TEXT("[PeachArtillery] started at %s"), *GetActorLocation().ToString());
}

void APPPeachArtilleryGame::OnMinigameFinished()
{
	// SERVER. TODO: destroy tanks/terrain.
}

void APPPeachArtilleryGame::ApplyDamage(APPPlayerState* Target, int32 Amount)
{
	if (!HasAuthority() || IsFinished() || !Target || Amount <= 0)
	{
		return;
	}

	if (Target == Player1)
	{
		Player1Health = FMath::Max(0, Player1Health - Amount);
		if (Player1Health == 0) { FinishWithResult(EMatchResult::Player2); }
	}
	else if (Target == Player2)
	{
		Player2Health = FMath::Max(0, Player2Health - Amount);
		if (Player2Health == 0) { FinishWithResult(EMatchResult::Player1); }
	}
}

EMatchResult APPPeachArtilleryGame::ForceResolve() const
{
	if (Player1Health > Player2Health) return EMatchResult::Player1;
	if (Player2Health > Player1Health) return EMatchResult::Player2;
	return EMatchResult::Draw;
}
