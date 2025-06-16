// Fill out your copyright notice in the Description page of Project Settings.


#include "VRMovementComponent.h"
#include "Enums/EVRMovementMode.h"


// Sets default values for this component's properties
UVRMovementComponent::UVRMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
    
    // Default settings (can expose to BP if needed)
    // TeleportThumbDeadzone = 0.4f;
    // SnapTurnAngle = 45.f;
    // SmoothTurnSpeed = 50.f;
    // bTurnModeIsSnap = true;
    //
    // CurrentMovementMode = EVRMovementMode::Teleport;

	// ...
}


// Called when the game starts
void UVRMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UVRMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UVRMovementComponent::BeginTeleport() 
{
    switch (CurrentMovementMode)
    {
    case EVRMovementMode::Teleport:
    case EVRMovementMode::Navigate:
    case EVRMovementMode::OutOfBodyNavigation:
        // if (!IsHandClimbing && !bIsOutOfBody && !DisableMovement)
        // {
        //     SetTeleporterActive(EControllerHand::Right, true);
        //     SetTeleporterActive(EControllerHand::Left, false);
        // }
        break; 

    default:
        break;
    }
}
void UVRMovementComponent::SnapTurn(float Direction)
{
}
void UVRMovementComponent::SmoothTurn(float AxisValue)
{
}
void UVRMovementComponent::SetMovementMode(EVRMovementMode NewMode)
{
}
EVRMovementMode UVRMovementComponent::GetCurrentMode() const
{
    return EVRMovementMode::Teleport;
}
