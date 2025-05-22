// Fill out your copyright notice in the Description page of Project Settings.

#include "PhysicsTossManager.h"
#include "Kismet/GameplayStatics.h"

UPhysicsTossManager::UPhysicsTossManager(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.bCanEverTick = true;
    DistanceToStartSlowing = 100.0f;
    MinimumSpeed = 100.0f; // Was orignally 200.0f, but this speed might be easier to grab from.
    CancelDistance = 20.0f;
}

void UPhysicsTossManager::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    RunThrowing();
}

void UPhysicsTossManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    CancelToss();
}

void UPhysicsTossManager::ServersideToss_Implementation(UPrimitiveComponent* TargetObject, UGripMotionControllerComponent* TargetMotionController)
{
    if (!IsValid(ObjectBeingThrown))
    {
        if (IsValid(TargetObject))
        {
            //IVRGripInterface* objectOwnerGripInterface = Cast<IVRGripInterface>(TargetObject->GetOwner());
            AActor* ownerActor = TargetObject->GetOwner();
            if (ownerActor && ownerActor->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
            {
                bool isHeld = false;
                TArray<FBPGripPair> holdingControllers;
                IVRGripInterface::Execute_IsHeld(ownerActor, holdingControllers, isHeld);
                if (!isHeld)
                {
                    OwningMotionController = TargetMotionController;
                    ObjectBeingThrown = TargetObject;
                    ObjectBeingThrown->OnComponentHit.AddDynamic(this, &UPhysicsTossManager::CancelThrow);
                    ObjectBeingThrown->SetSimulatePhysics(true);
                    RunToss(isHeld);
                    ToggleTick();
                }
            }
        }
    }
}

void UPhysicsTossManager::RunToss_Implementation(bool IsHeld)
{
    FVector throwVelocity = OwningMotionController->GetPivotLocation() - ObjectBeingThrown->GetComponentLocation();
    float currentDistanceSQ = throwVelocity.SizeSquared();
    FVector normalVector = throwVelocity.GetSafeNormal(0.0001f);

    if (currentDistanceSQ > FMath::Square(CancelDistance) && !IsHeld)
    {
        FVector outLaunchVelocity;
        if (UGameplayStatics::SuggestProjectileVelocity_CustomArc(GetWorld(), outLaunchVelocity, ObjectBeingThrown->GetComponentLocation(), OwningMotionController->GetPivotLocation(), 0.0f, 0.8f))
        {
            
            float vectorLengthSquared = FVector::VectorPlaneProject(outLaunchVelocity, normalVector).SizeSquared();

            float clampedSpeed = FMath::Clamp((FMath::Square(MinimumSpeed) - vectorLengthSquared), 0.0f, MinimumSpeed);

            float rangeSlowDownFloat = FMath::Clamp(1 - (FMath::Square(DistanceToStartSlowing) / currentDistanceSQ), 0.8f, 1.0f);

            FVector newVelocity = ((normalVector * clampedSpeed) + outLaunchVelocity) * rangeSlowDownFloat;

            ObjectBeingThrown->SetPhysicsLinearVelocity(newVelocity, false);
        }
        else
        {
            CancelThrowing(ObjectBeingThrown);
        }
    }
    else
    {
        CancelThrowing(ObjectBeingThrown);
    }
}

void UPhysicsTossManager::RunThrowing_Implementation()
{
    if (IsValid(ObjectBeingThrown))
    {
        bool isHeld = false;
        TArray<FBPGripPair> holdingControllers;
        // Use this if you want to check the owner instead but the blueprint implementation didn't do this.
        //if (ObjectBeingThrown->GetOwner()->Implements<UVRGripInterface>())
        if (ObjectBeingThrown->GetOwner()->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
        {
            //IVRGripInterface::Execute_IsHeld(ObjectBeingThrown->GetOwner(), holdingControllers, isHeld);
            IVRGripInterface::Execute_IsHeld(ObjectBeingThrown->GetOwner(), holdingControllers, isHeld);
        }
        RunToss(isHeld);
    }
    else
    {
        CancelThrowing(ObjectBeingThrown);
    }
}

void UPhysicsTossManager::CancelToss_Implementation()
{
    CancelThrowing(ObjectBeingThrown);
}

void UPhysicsTossManager::CancelThrowing_Implementation(UPrimitiveComponent* ObjectToCancel)
{
    if (IsValid(ObjectToCancel))
    {
        ObjectToCancel->OnComponentHit.RemoveDynamic(this, &UPhysicsTossManager::CancelThrow);
    }
    ObjectBeingThrown = nullptr;
    ToggleTick();
}

void UPhysicsTossManager::CancelThrow_Implementation(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    CancelThrowing(HitComponent);
}



void UPhysicsTossManager::IsThrowing_Implementation(bool &Throwing)
{
    Throwing = IsValid(ObjectBeingThrown);
}

void UPhysicsTossManager::ToggleTick_Implementation()
{
    SetComponentTickEnabled(IsValid(ObjectBeingThrown));
}
