#include "Minigame/PPPeachArtilleryGame.h"
#include "Minigame/PPTank.h"
#include "Minigame/PPProjectile.h"
#include "Minigame/PPVisual.h"
#include "Core/PPPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

APPPeachArtilleryGame::APPPeachArtilleryGame()
{
	PrimaryActorTick.bCanEverTick = false; // discrete turn-based; nothing to tick
	MinigameType = EMinigameType::PeachArtillery;
	Duration = 90.f; // generous fallback; usually ends on a KO

	GameCamera->SetRelativeLocation(FVector(0.f, -1700.f, 500.f));
	GameCamera->SetRelativeRotation(FRotator(-6.f, 90.f, 0.f));

	TankClass = APPTank::StaticClass();
	ProjectileClass = APPProjectile::StaticClass();

	// Small, safe weapon set. (Multi-shot / guided / terrain destruction are future work.)
	Weapons.Add({ TEXT("Shell"), 45, 220.f, 1700.f });
	Weapons.Add({ TEXT("Heavy"), 65, 320.f, 1450.f });

	// Ground for the shells to hit and the tanks to sit on.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	Floor->SetupAttachment(SceneRoot);
	Floor->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	Floor->SetRelativeScale3D(FVector(30.f, 8.f, 0.4f));
	Floor->SetCollisionProfileName(TEXT("BlockAll"));
	Floor->SetSimulatePhysics(false);
	if (CubeMesh.Succeeded()) { Floor->SetStaticMesh(CubeMesh.Object); }
}

void APPPeachArtilleryGame::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPPeachArtilleryGame, ActiveSlot);
	DOREPLIFETIME(APPPeachArtilleryGame, bTurnInProgress);
	DOREPLIFETIME(APPPeachArtilleryGame, Tank1);
	DOREPLIFETIME(APPPeachArtilleryGame, Tank2);
}

void APPPeachArtilleryGame::BeginPlay()
{
	Super::BeginPlay();
	PPVisual::Tint(Floor, FLinearColor(0.45f, 0.22f, 0.10f)); // brown terrain (runs on all machines)
}

void APPPeachArtilleryGame::OnMinigameStarted()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector O = GetActorLocation();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.Owner = this;

	Tank1 = World->SpawnActor<APPTank>(TankClass, O + FVector(-700.f, 0.f, 40.f), FRotator::ZeroRotator, P);
	Tank2 = World->SpawnActor<APPTank>(TankClass, O + FVector( 700.f, 0.f, 40.f), FRotator::ZeroRotator, P);
	if (Tank1) { Tank1->InitTank(GetPlayer1(), EPPTeam::TeamA, +1.f); } // faces +X (toward Tank2)
	if (Tank2) { Tank2->InitTank(GetPlayer2(), EPPTeam::TeamB, -1.f); } // faces -X

	ActiveSlot = 1;
	bTurnInProgress = false;
	OnRep_Turn(); // host mirror
}

// ----------------------------------------------------------------- input ----

void APPPeachArtilleryGame::HandleInput(APPPlayerState* Player, FName Action, bool bPressed)
{
	if (!bPressed)
	{
		return; // artillery uses discrete presses only
	}

	// Only the active player may act, and only while no shell is mid-flight.
	if (bTurnInProgress || Player != ActivePlayer())
	{
		return;
	}

	APPTank* Tank = ActiveTank();
	if (!Tank)
	{
		return;
	}

	if      (Action == FName(TEXT("Left")))   { Tank->MoveStep(-1.f); }
	else if (Action == FName(TEXT("Right")))  { Tank->MoveStep(+1.f); }
	else if (Action == FName(TEXT("Up")))     { Tank->AimStep(+Tank->AimStepDeg); }
	else if (Action == FName(TEXT("Down")))   { Tank->AimStep(-Tank->AimStepDeg); }
	else if (Action == FName(TEXT("Power+"))) { Tank->PowerStep(+Tank->PowerStepPct); }
	else if (Action == FName(TEXT("Power-"))) { Tank->PowerStep(-Tank->PowerStepPct); }
	else if (Action == FName(TEXT("Weapon"))) { Tank->NextWeapon(Weapons.Num()); }
	else if (Action == FName(TEXT("Primary"))){ FireActiveTank(); }
}

void APPPeachArtilleryGame::FireActiveTank()
{
	APPTank* Tank = ActiveTank();
	UWorld* World = GetWorld();
	if (!Tank || !World || Weapons.Num() == 0)
	{
		return;
	}

	const FPPWeaponData& W = Weapons[FMath::Clamp(Tank->GetWeaponIndex(), 0, Weapons.Num() - 1)];
	const float Speed = W.MaxSpeed * (Tank->GetPowerPercent() / 100.f);
	const FVector Velocity = Tank->GetLaunchDirection() * Speed;

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.Owner = this;

	APPProjectile* Shell = World->SpawnActor<APPProjectile>(
		ProjectileClass, Tank->GetMuzzleLocation(), Velocity.Rotation(), P);
	if (!Shell)
	{
		return;
	}

	bTurnInProgress = true; // lock input until impact resolves
	Shell->Launch(Velocity, this, Tank->GetWeaponIndex(), Tank);
}

// --------------------------------------------------------------- impact ----

void APPPeachArtilleryGame::OnProjectileImpact(const FVector& ImpactLocation, int32 WeaponIndex)
{
	if (!HasAuthority() || IsFinished())
	{
		return;
	}

	ApplyExplosion(ImpactLocation, WeaponIndex);

	// Resolve: a dead tank ends the match; otherwise hand over the turn.
	const bool bT1Dead = !Tank1 || !Tank1->IsAlive();
	const bool bT2Dead = !Tank2 || !Tank2->IsAlive();

	if (bT1Dead || bT2Dead)
	{
		EMatchResult Outcome = EMatchResult::Draw;
		if (bT1Dead && !bT2Dead)      { Outcome = EMatchResult::Player2; }
		else if (bT2Dead && !bT1Dead) { Outcome = EMatchResult::Player1; }
		FinishWithResult(Outcome);
		return;
	}

	SwitchTurn();
}

void APPPeachArtilleryGame::ApplyExplosion(const FVector& Center, int32 WeaponIndex)
{
	if (!Weapons.IsValidIndex(WeaponIndex))
	{
		return;
	}
	const FPPWeaponData& W = Weapons[WeaponIndex];

	auto DamageTank = [&](APPTank* Tank)
	{
		if (!Tank) return;
		const float Dist = FVector::Dist(Tank->GetActorLocation(), Center);
		if (Dist <= W.ExplosionRadius)
		{
			// Linear falloff: full damage at the centre, 0 at the rim.
			const float Frac = 1.f - (Dist / W.ExplosionRadius);
			const int32 Dmg = FMath::RoundToInt(W.Damage * Frac);
			Tank->ApplyDamage(Dmg);
		}
	};

	DamageTank(Tank1);
	DamageTank(Tank2);
}

void APPPeachArtilleryGame::SwitchTurn()
{
	ActiveSlot = (ActiveSlot == 1) ? 2 : 1;
	bTurnInProgress = false;
	OnRep_Turn(); // host mirror
}

// --------------------------------------------------------------- resolve ----

EMatchResult APPPeachArtilleryGame::ForceResolve() const
{
	const int32 H1 = Tank1 ? Tank1->GetHealth() : 0;
	const int32 H2 = Tank2 ? Tank2->GetHealth() : 0;
	if (H1 > H2) return EMatchResult::Player1;
	if (H2 > H1) return EMatchResult::Player2;
	return EMatchResult::Draw;
}

// -------------------------------------------------------------- teardown ----

void APPPeachArtilleryGame::OnMinigameFinished()
{
	if (Tank1) { Tank1->Destroy(); Tank1 = nullptr; }
	if (Tank2) { Tank2->Destroy(); Tank2 = nullptr; }
}

// ----------------------------------------------------------------- utils ----

APPTank* APPPeachArtilleryGame::ActiveTank() const
{
	return (ActiveSlot == 1) ? Tank1 : Tank2;
}

APPPlayerState* APPPeachArtilleryGame::ActivePlayer() const
{
	return (ActiveSlot == 1) ? GetPlayer1() : GetPlayer2();
}

void APPPeachArtilleryGame::OnRep_Turn()
{
	BP_OnTurnChanged(ActiveSlot);
}
