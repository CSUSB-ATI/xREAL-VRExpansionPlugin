// Fill out your copyright notice in the Description page of Project Settings.


#include "xREAL_FPS_VRCharacter.h"
#include "Camera/CameraActor.h"
#include "EnhancedInputComponent.h"
#include "VRRootComponent.h"
#include "WristMenuActor.h"
#include "PlayerMappableInputConfig.h"


AxREAL_FPS_VRCharacter::AxREAL_FPS_VRCharacter(const FObjectInitializer& ObjectInitializer) : Super()
{
	VRRootReference->VRCapsuleOffset = FVector(0,0,0);

	RightMotionController->SetWorldLocation(FVector(65.074234, 30.863922, 9.278572));
	RightMotionController->bUseWithoutTracking = true;

	LeftMotionController->SetWorldLocation(FVector(65.0, -43.256454, -11.514534));
	LeftMotionController->bUseWithoutTracking = true;

	VRReplicatedCamera->bUsePawnControlRotation = true;
	VRReplicatedCamera->bLockToHmd = false;
	VRReplicatedCamera->bFPSDebugMode = true;
	VRReplicatedCamera->SetWorldLocation(FVector(0,0,137.364502));

	VRMovementReference->bUseClientControlRotation = true;
	VRMovementReference->GetNavAgentPropertiesRef().bCanCrouch = true;

    InputConfig = FindFirstObjectSafe<UPlayerMappableInputConfig>(TEXT("/VRExpansionPlugin/VRE/Input/FPSInputConfig.FPSInputConfig"));
}

void AxREAL_FPS_VRCharacter::BeginPlay()
{
	Super::BeginPlay();
	LeftMotionController->AttachToComponent(VRReplicatedCamera, FAttachmentTransformRules(EAttachmentRule::KeepRelative, true));
	RightMotionController->AttachToComponent(VRReplicatedCamera, FAttachmentTransformRules(EAttachmentRule::KeepRelative, true));

	GripTraceLength = 10.0f;
	CurrentMovementMode = EVRMovementMode::Teleport;

	LeftMotionController->RemoveTickPrerequisiteComponent(VRReplicatedCamera);
	RightMotionController->RemoveTickPrerequisiteComponent(VRReplicatedCamera);
	
	// Timer needed here because takes a while for teleport controllers to be instantiated
	GetWorldTimerManager().SetTimer(MoveLaserSplineTimerHandle, FTimerDelegate::CreateLambda([this]() {
		if (!HasAuthority())
			MoveLaserSpline();
		}), 0.01f, false);
}

void AxREAL_FPS_VRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// This function is not complete, needs to be fixed.
void AxREAL_FPS_VRCharacter::SpawnWristMenu()
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("FPS"));
    if (!WristMenuActor)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = GetInstigator();

        if (!WristMenuActorClass)
        {
			UE_LOG(LogTemp, Warning, TEXT("WristMenuActor not spawned, WristMenuActorClass not set in VRCharacter!"));
            return;
        }
		WristMenuActor = GetWorld()->SpawnActor<AWristMenuActor>(WristMenuActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		WristMenuActor->AttachToComponent(VRReplicatedCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		WristMenuActor->SetActorRelativeLocation(FVector(0, 0, 100));
    }
}

void AxREAL_FPS_VRCharacter::MoveLaserSpline()
{
	TeleportControllerLeft->RotOffset = FRotator(-70.0, 0, 0);
	TeleportControllerRight->RotOffset = FRotator(-70.0, 0, 0);
}

void AxREAL_FPS_VRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* PlayerEnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
        Turn = FindFirstObjectSafe<UInputAction>(TEXT("Turn"));
        if (Turn)
        {
            PlayerEnhancedInputComponent->BindAction(Turn, ETriggerEvent::Triggered, this, &AxREAL_FPS_VRCharacter::TurnInput_Triggered);
        }

        LookUp = FindFirstObjectSafe<UInputAction>(TEXT("LookUp"));
        if (LookUp)
        {
            PlayerEnhancedInputComponent->BindAction(LookUp, ETriggerEvent::Triggered, this, &AxREAL_FPS_VRCharacter::LookUp_Triggered);
        }

        JumpAction = FindFirstObjectSafe<UInputAction>(TEXT("Jump"));
        if (JumpAction)
        {
            PlayerEnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AxREAL_FPS_VRCharacter::Jump_Started);
            PlayerEnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AxREAL_FPS_VRCharacter::Jump_Completed);
        }

		MoveForward = FindFirstObjectSafe<UInputAction>(TEXT("MoveForward"));
		if (MoveForward)
		{
			PlayerEnhancedInputComponent->BindAction(MoveForward, ETriggerEvent::Triggered, this, &AxREAL_FPS_VRCharacter::MoveForward_Triggered);
		}

		MoveRight = FindFirstObjectSafe<UInputAction>(TEXT("MoveRight"));
		if (MoveRight)
		{
			PlayerEnhancedInputComponent->BindAction(MoveRight, ETriggerEvent::Triggered, this, &AxREAL_FPS_VRCharacter::MoveRight_Triggered);
		}

		MouseWheelAxis = FindFirstObjectSafe<UInputAction>(TEXT("MouseWheel"));
		if (MouseWheelAxis)
		{
			PlayerEnhancedInputComponent->BindAction(MouseWheelAxis, ETriggerEvent::Triggered, this, &AxREAL_FPS_VRCharacter::MouseWheelAxis_Handler);
		}

	}

	//PlayerInputComponent->BindAxis("MouseWheel", this, &AxREAL_FPS_VRCharacter::MouseWheelAxis_Handler);

}

void AxREAL_FPS_VRCharacter::SwitchOutOfBodyCamera_Implementation(bool SwitchToOutOfBody)
{
	Super::SwitchOutOfBodyCamera(SwitchToOutOfBody);
	if (SwitchToOutOfBody && IsValid(OutOfBodyCamera))
	{
		OutOfBodyCamera->SetActorLocation(VRReplicatedCamera->GetComponentLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void AxREAL_FPS_VRCharacter::TurnInput_Triggered(const FInputActionValue& Value)
{
	AddControllerYawInput(Value.Get<float>());
}

void AxREAL_FPS_VRCharacter::LookUp_Triggered(const FInputActionValue& Value)
{
	AddControllerPitchInput(Value.Get<float>());
}

void AxREAL_FPS_VRCharacter::Jump_Started()
{
	ACharacter::Jump();
}

void AxREAL_FPS_VRCharacter::Jump_Completed()
{
	ACharacter::StopJumping();
}

void AxREAL_FPS_VRCharacter::MoveForward_Triggered(const FInputActionValue& Value)
{
	AddMovementInput(GetVRForwardVector(), Value.Get<float>());
}

void AxREAL_FPS_VRCharacter::MoveRight_Triggered(const FInputActionValue& Value)
{
	AddMovementInput(GetVRRightVector(), Value.Get<float>());
}

// Currently used to rotate the hands of the character
void AxREAL_FPS_VRCharacter::MouseWheelAxis_Handler(const FInputActionValue& Value)
{
	FRotator handRotation = FRotator(20.0f * Value.Get<float>(), 0.0f, 0.0f);
	LeftMotionController->AddLocalRotation(handRotation);
	RightMotionController->AddLocalRotation(handRotation);
}
