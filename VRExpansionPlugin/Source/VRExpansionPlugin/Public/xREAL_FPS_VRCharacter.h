// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "xREAL_VRCharacter.h"
#include "xREAL_FPS_VRCharacter.generated.h"

/**
 * 
 */
UCLASS()
class VREXPANSIONPLUGIN_API AxREAL_FPS_VRCharacter : public AxREAL_VRCharacter
{
	GENERATED_BODY()

	AxREAL_FPS_VRCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void SpawnWristMenu() override;

	UFUNCTION()
	void MoveLaserSpline();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void SwitchOutOfBodyCamera_Implementation(bool SwitchToOutOfBody) override;

	UPROPERTY()
	FTimerHandle MoveLaserSplineTimerHandle;

	UFUNCTION()
	void TurnInput_Triggered(const FInputActionValue& Value);

	UFUNCTION()
	void LookUp_Triggered(const FInputActionValue& Value);

	UFUNCTION()
	void Jump_Started();

	UFUNCTION()
	void Jump_Completed();

	UFUNCTION()
	void MoveForward_Triggered(const FInputActionValue& Value);

	UFUNCTION()
	void MoveRight_Triggered(const FInputActionValue& Value);

	UFUNCTION()
	void MouseWheelAxis_Handler(const FInputActionValue& Value);

private:

    UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> Turn;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookUp;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveForward;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveRight;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MouseWheelAxis;
};
