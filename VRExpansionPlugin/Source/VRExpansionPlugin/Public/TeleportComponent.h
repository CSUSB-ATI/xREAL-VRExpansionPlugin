// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GraspingHandManny.h"
#include "TeleportComponent.generated.h"

class ATeleportController;
class AxREAL_VRCharacter;
class APlayerState;
class UGripMotionControllerComponent;

//Enums
enum class EVRMovementMode : uint8;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VREXPANSIONPLUGIN_API UTeleportComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UTeleportComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void SetTeleportControllers(ATeleportController* _TeleportControllerLeft, ATeleportController* _TeleportControllerRight);

    /** Called every frame by owning character to adjust thumb‐stick rotation on active teleporter(s) */
    void UpdateTeleportRotations(float ThumbLeftX, float ThumbLeftY,
                                 float ThumbRightX, float ThumbRightY,
                                 FRotator VRRotation);

    UFUNCTION(BlueprintCallable)
    void SetTeleporterActive(EControllerHand Hand, bool Active);
    UFUNCTION(Server, Reliable, Category="Teleport")
    void NotifyTeleportActive_Server(EControllerHand Hand, bool State);
    UFUNCTION(NetMulticast, Reliable, Category="Teleport")
    void TeleportActive_Multicast(EControllerHand Hand, bool State);
    UFUNCTION(BlueprintCallable)
    void ExecuteTeleportation(ATeleportController* MotionController, EVRMovementMode MovementMode, EControllerHand Hand);

    UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Teleport")
    TObjectPtr<ATeleportController> TeleportControllerLeft;

    UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Teleport")
    TObjectPtr<ATeleportController> TeleportControllerRight;

    UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Control Booleans", meta=(DisplayName="B Teleport Uses Thumb Rotation"))
    bool bTeleportUsesThumbRotation;

    UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Teleport")
    double TeleportThumbDeadzone;

    FTimerHandle TeleportFadeIn_TimerHandle;
    FTimerHandle NavigationFinishedTeleportFade_TimerHandle;

    void CalcPadRotationAndMagnitude(float YAxis, float XAxis, float OptMagnitudeScaler, float OptionalDeadzone, FRotator &Rotation, float &Magnitude, bool &WasValid);

    UFUNCTION(BlueprintCallable, Category="Teleport")
    void TeleportRight_Started();

    UFUNCTION(BlueprintCallable, Category="Teleport")
    void TeleportRight_Completed();

    UFUNCTION(BlueprintCallable, Category="Teleport")
    void TeleportLeft_Started();

    UFUNCTION(BlueprintCallable, Category="Teleport")
    void TeleportLeft_Completed();

    UFUNCTION(BlueprintCallable, Category="Teleport")
    void SetControllerRotationOffset(EControllerHand Hand, const FRotator& Rotation);

private:
    // Utility to fade camera, move the pawn, etc.

    // ---------- Member variables moved from VRCharacter: ----------
    bool bIsTeleporting;

    // Fading parameters:
    float FadeOutDuration;
    float FadeInDuration;
    FLinearColor TeleportFadeColor;

    // Cached reference to owning character's movement interface (if needed for immediate stop):
    class UVRBaseCharacterMovementComponent* VRMovementReference;

    class AxREAL_VRCharacter* OwningVRCharacter;
};
