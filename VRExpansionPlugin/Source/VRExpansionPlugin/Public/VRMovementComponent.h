// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Enums/EVRMovementMode.h"
#include "VRMovementComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VREXPANSIONPLUGIN_API UVRMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UVRMovementComponent();

    void BeginTeleport();
    void SnapTurn(float Direction);
    void SmoothTurn(float AxisValue);

    void SetMovementMode(EVRMovementMode NewMode);
    EVRMovementMode GetCurrentMode() const;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

    EVRMovementMode CurrentMovementMode;

    // Configurable movement values
    float TeleportThumbDeadzone;
    float SnapTurnAngle;
    float SmoothTurnSpeed;
    bool bTurnModeIsSnap;
};
