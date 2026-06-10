#include "Core/PPCharacter.h"
#include "Core/PPPlayerController.h"
#include "Interaction/PPInteractable.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

APPCharacter::APPCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
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

	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &APPCharacter::OnInteractPressed);
	PlayerInputComponent->BindAction("SpectateNext", IE_Pressed, this, &APPCharacter::OnSpectateNext);
	PlayerInputComponent->BindAction("SpectatePrev", IE_Pressed, this, &APPCharacter::OnSpectatePrev);
}

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
