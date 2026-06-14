#include "Core/PPCharacter.h"
#include "Core/PPPlayerController.h"
#include "Core/PPPlayerState.h"
#include "Core/PPGameState.h"
#include "Core/PPGameMode.h"
#include "Final/PPWaterProjectile.h"
#include "Final/PPGrabbableObject.h"
#include "Core/PPDebug.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

APPCharacter::APPCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// First-person: the body turns with the controller's yaw; no orient-to-movement.
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.4f;
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true; // enable crouching

	// The capsule blocks world geometry, so players can't walk through walls/objects.
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f)); // ~eye height
	FirstPersonCamera->bUsePawnControlRotation = true;

	// Placeholder body (cylinder) so OTHER players can see you; hidden for your own first-person view.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // the capsule handles collision
	BodyMesh->SetOwnerNoSee(true);
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	BodyMesh->SetRelativeScale3D(FVector(0.9f, 0.9f, 1.76f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylMesh.Object);
	}

	// Don't draw the (unused) skeletal mesh to ourselves either.
	if (GetMesh())
	{
		GetMesh()->SetOwnerNoSee(true);
	}

	WaterProjectileClass = APPWaterProjectile::StaticClass(); // works with no BP setup

	HoldPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HoldPoint"));
	HoldPoint->SetupAttachment(FirstPersonCamera);
	HoldPoint->SetRelativeLocation(FVector(160.f, 0.f, -20.f)); // floats in front of the view
}

void APPCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Owner sets sprint locally for responsiveness; others get it replicated.
	DOREPLIFETIME_CONDITION(APPCharacter, bIsSprinting, COND_SkipOwner);
	DOREPLIFETIME(APPCharacter, bIsHolding);
}

void APPCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyClassStats(); // movement speed + ammo from the player's class (server + owning client)
}

void APPCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->IsLocalController())
		{
			// Clear leftover viewport widgets (e.g. the class menu once the fight starts).
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

	// Legacy bindings keep the project bootable with no InputAction assets to author first.
	PlayerInputComponent->BindAxis("MoveForward", this, &APPCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APPCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &APPCharacter::TurnYaw);
	PlayerInputComponent->BindAxis("LookUp", this, &APPCharacter::LookPitch);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &APPCharacter::OnJumpPressed);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &APPCharacter::OnJumpReleased);
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &APPCharacter::ToggleSprint);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &APPCharacter::OnCrouchPressed);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &APPCharacter::OnCrouchReleased);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &APPCharacter::OnFirePressed);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &APPCharacter::OnFireReleased);
	PlayerInputComponent->BindAction("Grab", IE_Pressed, this, &APPCharacter::OnGrabPressed);

	// Keyboard fallback for class pick (the WBP_ClassSelect buttons call the same ServerSelectClass).
	PlayerInputComponent->BindAction("Class1", IE_Pressed, this, &APPCharacter::OnSelectClass1);
	PlayerInputComponent->BindAction("Class2", IE_Pressed, this, &APPCharacter::OnSelectClass2);
	PlayerInputComponent->BindAction("Class3", IE_Pressed, this, &APPCharacter::OnSelectClass3);
	PlayerInputComponent->BindAction("Class4", IE_Pressed, this, &APPCharacter::OnSelectClass4);
}

void APPCharacter::OnSelectClass1() { SelectClass(EPPClass::Sprayer); }
void APPCharacter::OnSelectClass2() { SelectClass(EPPClass::Punisher); }
void APPCharacter::OnSelectClass3() { SelectClass(EPPClass::Engineer); }
void APPCharacter::OnSelectClass4() { SelectClass(EPPClass::Runner); }

void APPCharacter::SelectClass(EPPClass NewClass)
{
	if (APPPlayerController* PC = Cast<APPPlayerController>(GetController()))
	{
		PC->ServerSelectClass(NewClass);
	}
}

// ---------------------------------------------------------------- movement ----

void APPCharacter::MoveForward(float Value)
{
	if (Controller && Value != 0.f)
	{
		const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Value);
	}
}

void APPCharacter::MoveRight(float Value)
{
	if (Controller && Value != 0.f)
	{
		const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Value);
	}
}

void APPCharacter::TurnYaw(float Value)
{
	AddControllerYawInput(Value);
}

void APPCharacter::LookPitch(float Value)
{
	AddControllerPitchInput(Value);
}

void APPCharacter::OnJumpPressed()
{
	Jump(); // ACharacter::Jump — networked via CharacterMovement
}

void APPCharacter::OnJumpReleased()
{
	StopJumping();
}

void APPCharacter::ToggleSprint()
{
	bIsSprinting = !bIsSprinting;
	ApplyMovementSpeed();          // responsive on the owning client
	ServerSetSprint(bIsSprinting); // authoritative on the server
}

void APPCharacter::ServerSetSprint_Implementation(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;
	ApplyMovementSpeed();
}

void APPCharacter::ApplyMovementSpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
}

void APPCharacter::OnCrouchPressed()
{
	Crouch(); // ACharacter::Crouch — networked
}

void APPCharacter::OnCrouchReleased()
{
	UnCrouch();
}

// --------------------------------------------------------- final combat ----

bool APPCharacter::IsFightLive() const
{
	const APPGameState* GS = GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
	return GS && GS->GetCurrentPhase() == EMatchPhase::Final;
}

FPPClassStats APPCharacter::GetEffectiveStats() const
{
	const APPPlayerState* PS = GetPlayerState<APPPlayerState>();
	FPPClassStats St = PS ? PS->GetClassStats() : FPPClassStats();
	if (!PS)
	{
		return St;
	}
	const APPGameState* GS = GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
	switch (GS ? GS->GetTeamReward(PS->GetTeam()) : EPPReward::None)
	{
	case EPPReward::Speed:   St.MoveSpeed     *= PPReward::Bonus; break;
	case EPPReward::Ammo:    St.AmmoCapacity   = FMath::RoundToInt(St.AmmoCapacity * PPReward::Bonus); break;
	case EPPReward::Wetness: St.WetnessPerHit *= PPReward::Bonus; break;
	default: break; // Health is handled by the wetness slip threshold in PlayerState
	}
	return St;
}

void APPCharacter::ApplyClassStats()
{
	const FPPClassStats St = GetEffectiveStats();
	WalkSpeed = St.MoveSpeed;           // class (+reward) sets base walk speed...
	SprintSpeed = St.MoveSpeed * 1.25f; // ...sprint scales from it
	ApplyMovementSpeed();
	CurrentAmmo = St.AmmoCapacity;
}

void APPCharacter::OnFirePressed()
{
	if (!IsFightLive())
	{
		return;
	}
	if (bIsHolding)
	{
		ServerThrow(); // LMB throws while holding an object (single)
		return;
	}

	// Hold LMB = autofire: fire once now, then repeat at the class fire interval until released.
	TryFire();
	const float Interval = FMath::Max(0.03f, GetEffectiveStats().FireInterval);
	GetWorldTimerManager().SetTimer(AutoFireTimer, this, &APPCharacter::TryFire, Interval, /*bLoop=*/true);
}

void APPCharacter::OnFireReleased()
{
	GetWorldTimerManager().ClearTimer(AutoFireTimer);
}

void APPCharacter::TryFire()
{
	// Stop autofire if the situation no longer allows it (phase changed / picked up an object).
	if (!IsFightLive() || bIsHolding)
	{
		GetWorldTimerManager().ClearTimer(AutoFireTimer);
		return;
	}
	ServerFire(); // server still rate-gates + spends ammo
}

void APPCharacter::OnGrabPressed()
{
	if (!IsFightLive())
	{
		return;
	}
	ServerGrabOrDrop();
}

void APPCharacter::ServerFire_Implementation()
{
	APPPlayerState* PS = GetPlayerState<APPPlayerState>();
	UWorld* World = GetWorld();
	const APPGameState* GS = World ? World->GetGameState<APPGameState>() : nullptr;
	if (!PS || PS->IsSlipping() || bIsHolding || !World || !WaterProjectileClass
		|| !GS || GS->GetCurrentPhase() != EMatchPhase::Final)
	{
		return;
	}

	const FPPClassStats St = GetEffectiveStats();
	const float Now = World->GetTimeSeconds();
	if (Now - LastFireServerTime < St.FireInterval * 0.9f || CurrentAmmo <= 0)
	{
		return; // fire-rate gate (0.9 margin so client autofire isn't starved by timing) + ammo
	}
	LastFireServerTime = Now;
	--CurrentAmmo;

	// Server-authoritative aim from the replicated view rotation, plus class spread.
	FRotator Aim = GetBaseAimRotation();
	Aim.Yaw   += FMath::FRandRange(-St.SpreadDeg, St.SpreadDeg);
	Aim.Pitch += FMath::FRandRange(-St.SpreadDeg, St.SpreadDeg);
	const FVector Dir = Aim.Vector();
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 50.f) + Dir * 70.f;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	if (APPWaterProjectile* Shot = World->SpawnActor<APPWaterProjectile>(WaterProjectileClass, Start, Dir.Rotation(), Params))
	{
		Shot->Launch(Dir * St.ProjectileSpeed, PS->GetTeam(), St.WetnessPerHit, this);
	}

	PPDebug::Print(FString::Printf(TEXT("FIRE  %s (Team %d)  ammo %d/%d"),
		*PS->GetPlayerName(), (int32)PS->GetTeam(), CurrentAmmo, St.AmmoCapacity), FColor::Cyan, 2.f);
}

void APPCharacter::ApplyWetness(float Amount, EPPTeam InstigatorTeam)
{
	if (!HasAuthority())
	{
		return;
	}
	APPPlayerState* PS = GetPlayerState<APPPlayerState>();
	if (!PS)
	{
		return;
	}

	// DEBUG: fixed 10 wetness per enemy hit so EVERY player dies after exactly 10 shots, regardless of
	// class (threshold is 100). The shooter's class value (Amount) is logged but not applied for now.
	const float DebugWetnessPerHit = 10.f;
	const bool bDied = PS->AddWetness(DebugWetnessPerHit); // true when it just hit 100

	const int32 Hits = FMath::RoundToInt(PS->GetWetness() / DebugWetnessPerHit);
	PPDebug::Print(FString::Printf(TEXT("HIT   %s  wetness %.0f/100  (hit %d/10, by Team %d, raw %.0f)"),
		*PS->GetPlayerName(), PS->GetWetness(), Hits, (int32)InstigatorTeam, Amount), FColor::Yellow, 2.f);

	if (bDied)
	{
		// Slip impulse (server-authoritative, replicates via movement).
		LaunchCharacter(FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 500.f), true, true);
		MulticastSlip();
		PPDebug::Print(FString::Printf(TEXT("DIED  %s  (10 hits) -> respawn soon"), *PS->GetPlayerName()),
			FColor::Red, 4.f);
		if (APPGameMode* GM = GetWorld()->GetAuthGameMode<APPGameMode>())
		{
			GM->ScheduleRespawn(GetController());
		}
	}
}

void APPCharacter::MulticastSlip_Implementation()
{
	// Lock movement + input; the fresh pawn after respawn restores both.
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->DisableMovement();
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
	BP_OnSlip();
}

void APPCharacter::AddAmmo(int32 Amount)
{
	if (!HasAuthority() || Amount <= 0)
	{
		return;
	}
	const int32 Cap = GetEffectiveStats().AmmoCapacity;
	const int32 Before = CurrentAmmo;
	CurrentAmmo = FMath::Min(CurrentAmmo + Amount, Cap);
	if (CurrentAmmo != Before)
	{
		const APPPlayerState* PS = GetPlayerState<APPPlayerState>();
		PPDebug::Print(FString::Printf(TEXT("REFILL %s  +%d  ammo %d/%d"),
			PS ? *PS->GetPlayerName() : TEXT("?"), CurrentAmmo - Before, CurrentAmmo, Cap), FColor::Green, 2.f);
	}
}

void APPCharacter::ServerGrabOrDrop_Implementation()
{
	const APPPlayerState* PS = GetPlayerState<APPPlayerState>();
	if (PS && PS->IsSlipping())
	{
		return;
	}

	if (bIsHolding)
	{
		if (HeldObject) { HeldObject->Drop(); }
		HeldObject = nullptr;
		bIsHolding = false;
		PPDebug::Print(TEXT("DROP  object"), FColor::Orange, 2.f);
		return;
	}

	// Trace forward for a grabbable object.
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector End = Start + GetBaseAimRotation().Vector() * GrabDistance;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (APPGrabbableObject* Obj = Cast<APPGrabbableObject>(Hit.GetActor()))
		{
			if (!Obj->IsHeld())
			{
				Obj->Grab(this, HoldPoint);
				HeldObject = Obj;
				bIsHolding = true;
				PPDebug::Print(FString::Printf(TEXT("GRAB  %s"), *Obj->GetName()), FColor::Orange, 2.f);
			}
		}
	}
}

void APPCharacter::ServerThrow_Implementation()
{
	if (!bIsHolding || !HeldObject)
	{
		return;
	}
	const APPPlayerState* PS = GetPlayerState<APPPlayerState>();
	const FVector Dir = GetBaseAimRotation().Vector();
	HeldObject->ThrowWithImpulse(Dir * ThrowImpulse, PS ? PS->GetTeam() : EPPTeam::None);
	PPDebug::Print(FString::Printf(TEXT("THROW %s"), *HeldObject->GetName()), FColor::Orange, 2.f);
	HeldObject = nullptr;
	bIsHolding = false;
}
