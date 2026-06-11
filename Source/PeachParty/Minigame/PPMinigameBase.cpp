#include "Minigame/PPMinigameBase.h"
#include "Core/PPPlayerState.h"
#include "Core/PPGameMode.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

APPMinigameBase::APPMinigameBase()
{
	PrimaryActorTick.bCanEverTick = false; // event/timer driven; subclasses enable tick if needed
	bReplicates = true;
	// Arenas are far apart, so distance relevancy would cull these for players still in the hub.
	// Always-relevant keeps every match visible to every client (cheap at <=5 concurrent) and is
	// required for the spectator camera + view-target pointers to resolve. (Scale note in README.)
	bAlwaysRelevant = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GameCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("GameCamera"));
	GameCamera->SetupAttachment(SceneRoot);
	GameCamera->SetRelativeLocation(FVector(0.f, -900.f, 250.f));
	GameCamera->SetRelativeRotation(FRotator(-10.f, 90.f, 0.f));
	// Minigames are a flat 2D side-on look -> orthographic (no perspective foreshortening). The FP hub
	// camera stays perspective, so the view auto-switches projection when this becomes the view target.
	GameCamera->SetProjectionMode(ECameraProjectionMode::Orthographic);
	GameCamera->SetOrthoWidth(OrthoWidth);
}

void APPMinigameBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPMinigameBase, Player1Score);
	DOREPLIFETIME(APPMinigameBase, Player2Score);
	DOREPLIFETIME(APPMinigameBase, Result);
	DOREPLIFETIME(APPMinigameBase, Player1);
	DOREPLIFETIME(APPMinigameBase, Player2);
}

void APPMinigameBase::StartMinigame(APPPlayerState* InP1, APPPlayerState* InP2)
{
	if (!HasAuthority())
	{
		return;
	}

	Player1 = InP1;
	Player2 = InP2;
	Player1Score = 0;
	Player2Score = 0;
	Result = EMatchResult::Undecided;

	OnMinigameStarted();
	BP_OnMinigameStarted();

	if (Duration > 0.f)
	{
		GetWorldTimerManager().SetTimer(DurationTimer, this, &APPMinigameBase::OnDurationElapsed, Duration, false);
	}
}

void APPMinigameBase::OnDurationElapsed()
{
	FinishWithResult(ForceResolve());
}

EMatchResult APPMinigameBase::ForceResolve() const
{
	if (Player1Score > Player2Score) return EMatchResult::Player1;
	if (Player2Score > Player1Score) return EMatchResult::Player2;
	return EMatchResult::Draw;
}

void APPMinigameBase::ForceFinish()
{
	if (HasAuthority() && Result == EMatchResult::Undecided)
	{
		FinishWithResult(ForceResolve());
	}
}

void APPMinigameBase::AddScore(APPPlayerState* Scorer, int32 Delta)
{
	if (!HasAuthority() || Result != EMatchResult::Undecided || !Scorer)
	{
		return;
	}

	if (Scorer == Player1) Player1Score += Delta;
	else if (Scorer == Player2) Player2Score += Delta;
}

void APPMinigameBase::FinishWithResult(EMatchResult InResult)
{
	if (!HasAuthority() || Result != EMatchResult::Undecided)
	{
		return;
	}

	Result = InResult;
	GetWorldTimerManager().ClearTimer(DurationTimer);

	OnMinigameFinished();
	OnRep_Result(); // host mirror

	if (APPGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APPGameMode>() : nullptr)
	{
		GM->NotifyMinigameFinished(this, Result);
	}
}

void APPMinigameBase::OnRep_Result()
{
	BP_OnMinigameFinished(Result);
}

APPPlayerState* APPMinigameBase::GetOpponentOf(const APPPlayerState* PS) const
{
	if (PS == Player1) return Player2;
	if (PS == Player2) return Player1;
	return nullptr;
}

EPPTeam APPMinigameBase::GetWinningTeam() const
{
	if (Result == EMatchResult::Player1 && Player1) return Player1->GetTeam();
	if (Result == EMatchResult::Player2 && Player2) return Player2->GetTeam();
	return EPPTeam::None;
}
