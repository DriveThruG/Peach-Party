#include "Core/PPCharacter.h"
#include "Core/PPPlayerController.h"
#include "Core/PPPlayerState.h"
#include "Interaction/PPInteractable.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

APPCharacter::APPCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

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
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -88.f)); // base at capsule bottom
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
}

void APPCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Owner sets sprint locally for responsiveness; others get it replicated.
	DOREPLIFETIME_CONDITION(APPCharacter, bIsSprinting, COND_SkipOwner);
}

void APPCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Only the controlling client needs to discover what it's looking at.
	if (IsLocallyControlled())
	{
		UpdateFocus();
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

	// Movement actions.
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &APPCharacter::OnJumpPressed);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &APPCharacter::OnJumpReleased);
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &APPCharacter::ToggleSprint);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &APPCharacter::OnCrouchPressed);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &APPCharacter::OnCrouchReleased);

	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &APPCharacter::OnInteractPressed);
	PlayerInputComponent->BindAction("SpectateNext", IE_Pressed, this, &APPCharacter::OnSpectateNext);
	PlayerInputComponent->BindAction("SpectatePrev", IE_Pressed, this, &APPCharacter::OnSpectatePrev);

	// Minigame controls — forwarded to the active match (no-op in the hub).
	PlayerInputComponent->BindAction("MGPrimary", IE_Pressed, this, &APPCharacter::OnMG_PrimaryPressed);
	PlayerInputComponent->BindAction("MGPrimary", IE_Released, this, &APPCharacter::OnMG_PrimaryReleased);
	PlayerInputComponent->BindAction("MGLeft", IE_Pressed, this, &APPCharacter::OnMG_Left);
	PlayerInputComponent->BindAction("MGRight", IE_Pressed, this, &APPCharacter::OnMG_Right);
	PlayerInputComponent->BindAction("MGUp", IE_Pressed, this, &APPCharacter::OnMG_Up);
	PlayerInputComponent->BindAction("MGDown", IE_Pressed, this, &APPCharacter::OnMG_Down);
	PlayerInputComponent->BindAction("MGPowerUp", IE_Pressed, this, &APPCharacter::OnMG_PowerUp);
	PlayerInputComponent->BindAction("MGPowerDown", IE_Pressed, this, &APPCharacter::OnMG_PowerDown);
	PlayerInputComponent->BindAction("MGWeapon", IE_Pressed, this, &APPCharacter::OnMG_Weapon);
}

// ---------------------------------------------------------------- movement ----

void APPCharacter::MoveForward(float Value)
{
	if (IsInMinigame()) { return; } // hub pawn is parked while playing a minigame
	if (Controller && Value != 0.f)
	{
		const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Value);
	}
}

void APPCharacter::MoveRight(float Value)
{
	if (IsInMinigame()) { return; }
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
	if (IsInMinigame()) { return; } // Space is the minigame action while playing
	Jump();                          // ACharacter::Jump — networked via CharacterMovement
}

void APPCharacter::OnJumpReleased()
{
	StopJumping();
}

void APPCharacter::ToggleSprint()
{
	if (IsInMinigame()) { return; }
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
	if (IsInMinigame()) { return; }
	Crouch();   // ACharacter::Crouch — networked
}

void APPCharacter::OnCrouchReleased()
{
	UnCrouch();
}

// ------------------------------------------------------------- interaction ----

void APPCharacter::UpdateFocus()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector Start = CamLoc;
	const FVector End = Start + CamRot.Vector() * InteractTraceDistance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(InteractTraceRadius), Params);

	AActor* NewFocus = nullptr;
	if (bHit && Hit.GetActor() && Cast<IPPInteractable>(Hit.GetActor()))
	{
		NewFocus = Hit.GetActor();
	}

	if (FocusedActor.Get() != NewFocus)
	{
		if (IPPInteractable* Old = Cast<IPPInteractable>(FocusedActor.Get()))
		{
			Old->OnEndFocus();
		}
		if (IPPInteractable* New = Cast<IPPInteractable>(NewFocus))
		{
			New->OnBeginFocus();
		}
		FocusedActor = NewFocus;
	}
}

void APPCharacter::OnInteractPressed()
{
	APPPlayerController* PC = Cast<APPPlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// Already seated? One key to stand up and snap back to the 3D world.
	if (PC->IsSeated())
	{
		PC->ServerLeaveStation();
		return;
	}

	if (AActor* Target = FocusedActor.Get())
	{
		PC->ServerRequestInteract(Target);
	}
}

void APPCharacter::OnSpectateNext()
{
	if (APPPlayerController* PC = Cast<APPPlayerController>(GetController()))
	{
		PC->ServerCycleSpectate(+1);
	}
}

void APPCharacter::OnSpectatePrev()
{
	if (APPPlayerController* PC = Cast<APPPlayerController>(GetController()))
	{
		PC->ServerCycleSpectate(-1);
	}
}

// --------------------------------------------------------- minigame input ----

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
void APPCharacter::OnMG_Left()            { ForwardMinigameInput(TEXT("Left"), true); }
void APPCharacter::OnMG_Right()           { ForwardMinigameInput(TEXT("Right"), true); }
void APPCharacter::OnMG_Up()              { ForwardMinigameInput(TEXT("Up"), true); }
void APPCharacter::OnMG_Down()            { ForwardMinigameInput(TEXT("Down"), true); }
void APPCharacter::OnMG_PowerUp()         { ForwardMinigameInput(TEXT("Power+"), true); }
void APPCharacter::OnMG_PowerDown()       { ForwardMinigameInput(TEXT("Power-"), true); }
void APPCharacter::OnMG_Weapon()          { ForwardMinigameInput(TEXT("Weapon"), true); }
