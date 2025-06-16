// Fill out your copyright notice in the Description page of Project Settings.


#include "TeleportComponent.h"
#include "GameFramework/Character.h"
#include "GraspingHandManny.h"
#include "TeleportController.h"
#include "GripMotionControllerComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"
#include "VRBaseCharacterMovementComponent.h"
#include "VRExpansionFunctionLibrary.h"
#include "Enums/EVRMovementMode.h"
#include "Kismet/KismetMathLibrary.h"
#include "xREAL_VRCharacter.h"
#include "GameFramework/PlayerState.h"


// Sets default values for this component's properties
UTeleportComponent::UTeleportComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

    //Default values (copying from VRCharacter::InitializeDefaults()):
    FadeOutDuration      = 0.25f;
    FadeInDuration       = 0.25f;
    TeleportFadeColor    = FLinearColor::Black;
    TeleportThumbDeadzone = 0.4f;
    bTeleportUsesThumbRotation = true;

    TeleportControllerLeft  = nullptr;
    TeleportControllerRight = nullptr;
    bIsTeleporting          = false;
}


// Called when the game starts
void UTeleportComponent::BeginPlay()
{
	Super::BeginPlay();

    // If OwningCharacter is nullptr, this will log a warning (once)
    // but won’t crash the game. Execution continues.
    OwningVRCharacter = Cast<AxREAL_VRCharacter>(GetOwner());
    ensure(OwningVRCharacter);
    VRMovementReference = OwningVRCharacter->VRMovementReference;
    ensure(VRMovementReference);
}

void UTeleportComponent::SetTeleportControllers(ATeleportController* _TeleportControllerLeft, ATeleportController* _TeleportControllerRight)
{
    TeleportControllerLeft = _TeleportControllerLeft;
    TeleportControllerRight = _TeleportControllerRight;
}


// Called every frame
void UTeleportComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UTeleportComponent::SetTeleporterActive(EControllerHand Hand, bool Active)
{
    switch (Hand)
    {
    case EControllerHand::Left:
        if (IsValid(TeleportControllerLeft))
        {
            Active ? TeleportControllerLeft->ActivateTeleporter() : TeleportControllerLeft->DisableTeleporter();
            NotifyTeleportActive_Server(Hand, Active);
        }
        break;
    case EControllerHand::Right:
        if (IsValid(TeleportControllerRight))
        {
            Active ? TeleportControllerRight->ActivateTeleporter() : TeleportControllerRight->DisableTeleporter();
            NotifyTeleportActive_Server(Hand, Active);
        }
        break;
    default:
        break;
    }
}

void UTeleportComponent::NotifyTeleportActive_Server_Implementation(EControllerHand Hand, bool State)
{
    // Place your server-side logic here, or leave empty if not needed
    TeleportActive_Multicast(Hand, State);
}

void UTeleportComponent::TeleportActive_Multicast_Implementation(EControllerHand Hand, bool State)
{
    switch (Hand)
    {
    case EControllerHand::Left:
        if (IsValid(TeleportControllerLeft))
        {
            State ? TeleportControllerLeft->ActivateTeleporter() : TeleportControllerLeft->DisableTeleporter();
        }
        break;
    case EControllerHand::Right:
        if (IsValid(TeleportControllerRight))
        {
            State ? TeleportControllerRight->ActivateTeleporter() : TeleportControllerRight->DisableTeleporter();
        }
        break;
    default:
        break;
    }
}

void UTeleportComponent::ExecuteTeleportation(ATeleportController* MotionController, EVRMovementMode MovementMode, EControllerHand Hand)
{
    //Early return if already teleporting
    if (bIsTeleporting)
        return;

    switch (MovementMode)
    {
    case EVRMovementMode::Teleport:
        if (VRMovementReference)
            VRMovementReference->StopMovementImmediately();
        if (MotionController && MotionController->IsValidTeleportDestination)
        {
            bIsTeleporting = true;
            APlayerCameraManager* cameraManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
            cameraManager->StartCameraFade(0.0f, 1.0f, FadeOutDuration, TeleportFadeColor, false, true);
            FVector teleportLocation, finalTeleportLocation;
            FRotator teleportRotation, finalTeleportRotation;
            MotionController->GetTeleportDestination(false, teleportLocation, teleportRotation);
            teleportLocation = OwningVRCharacter->GetTeleportLocation(teleportLocation); // Includes the neck offset
            OwningVRCharacter->GetCharacterRotatedPosition(teleportLocation, teleportRotation, OwningVRCharacter->GetVRLocation(), finalTeleportRotation, finalTeleportLocation);

            // Timer that waits for fade out to finsh before fading back in
            GetWorld()->GetTimerManager().SetTimer(
                TeleportFadeIn_TimerHandle,
                FTimerDelegate::CreateLambda([this, cameraManager, Hand, finalTeleportLocation, finalTeleportRotation]() {
                    SetTeleporterActive(Hand, false);
                    if (VRMovementReference)
                        VRMovementReference->PerformMoveAction_Teleport(finalTeleportLocation, finalTeleportRotation);
                    cameraManager->StartCameraFade(1.0f, 0.0f, FadeInDuration, TeleportFadeColor, false, false);
                    bIsTeleporting = false;
                }),
                FadeOutDuration, false);
        }
        else
        {
            SetTeleporterActive(Hand, false);
        }
        break;
    case EVRMovementMode::Navigate:
    case EVRMovementMode::OutOfBodyNavigation:
        if (!OwningVRCharacter->IsHandClimbing)
            {
                if (MovementMode == EVRMovementMode::OutOfBodyNavigation)
                {
                    OwningVRCharacter->SwitchOutOfBodyCamera(true);
                }
                
                FVector teleportLocation;
                FRotator teleportRotation;
                MotionController->GetTeleportDestination(false, teleportLocation, teleportRotation);
                OwningVRCharacter->ExtendedSimpleMoveToLocation(teleportLocation);
                SetTeleporterActive(Hand, false);
            }
            break;
    default:
        break;
    }
}

void UTeleportComponent::UpdateTeleportRotations(float ThumbLeftX, float ThumbLeftY, float ThumbRightX, float ThumbRightY, FRotator VRRotation)
{
    // Right Controller
    if (IsValid(TeleportControllerRight) && TeleportControllerRight->IsTeleporterActive)
    {
        if (bTeleportUsesThumbRotation)
        {
            FRotator teleportRotation;
            bool isValid;
            teleportRotation = UKismetMathLibrary::MakeRotFromX(FVector(ThumbRightY, ThumbRightX, 0.0f));
            isValid = (FMath::Abs(ThumbRightY) + FMath::Abs(ThumbRightX)) > TeleportThumbDeadzone;
            if (isValid)
            {
                TeleportControllerRight->TeleportRotation = teleportRotation;
            }
        }
        else
        {
            TeleportControllerRight->TeleportRotation = FRotator(0.0f, 0.0f, 0.0f);
        }
        TeleportControllerRight->TeleportBaseRotation = OwningVRCharacter->GetVRRotation();
    }
    // Left Controller
    if (IsValid(TeleportControllerLeft) && TeleportControllerLeft->IsTeleporterActive)
    {
        if (bTeleportUsesThumbRotation)
        {
            FRotator teleportRotation;
            bool isValid;
            teleportRotation = UKismetMathLibrary::MakeRotFromX(FVector(ThumbLeftY, ThumbLeftX, 0.0f));
            isValid = (FMath::Abs(ThumbLeftY) + FMath::Abs(ThumbLeftX)) > TeleportThumbDeadzone;
            if (isValid)
            {
                TeleportControllerLeft->TeleportRotation = teleportRotation;
            }
        }
        else
        {
            TeleportControllerLeft->TeleportRotation = FRotator(0.0f, 0.0f, 0.0f);
        }
        TeleportControllerLeft->TeleportBaseRotation = OwningVRCharacter->GetVRRotation();
    }
}

void UTeleportComponent::TeleportRight_Started()
{
    SetTeleporterActive(EControllerHand::Right, true);
    SetTeleporterActive(EControllerHand::Left, false);
}

void UTeleportComponent::TeleportRight_Completed()
{
    ExecuteTeleportation(TeleportControllerRight, OwningVRCharacter->CurrentMovementMode, EControllerHand::Right);
}

void UTeleportComponent::TeleportLeft_Started()
{
    SetTeleporterActive(EControllerHand::Left, true);
    SetTeleporterActive(EControllerHand::Right, false);
}

void UTeleportComponent::TeleportLeft_Completed()
{
    ExecuteTeleportation(TeleportControllerLeft, OwningVRCharacter->CurrentMovementMode, EControllerHand::Left);
}

void UTeleportComponent::SetControllerRotationOffset(EControllerHand Hand, const FRotator& Rotation)
{
    if (Hand == EControllerHand::Left && IsValid(TeleportControllerLeft))
    {
        TeleportControllerLeft->RotOffset = Rotation;
    }
    else if (Hand == EControllerHand::Right && IsValid(TeleportControllerRight))
    {
        TeleportControllerRight->RotOffset = Rotation;
    }
}
