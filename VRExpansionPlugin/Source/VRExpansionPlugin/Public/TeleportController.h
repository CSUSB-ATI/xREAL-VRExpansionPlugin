// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhysicsTossManager.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Enums/EGripState.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "TeleportController.generated.h"

UCLASS(Blueprintable, BlueprintType)
class VREXPANSIONPLUGIN_API ATeleportController : public AActor
{
	GENERATED_BODY()
	public:

	ATeleportController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	void BindController();

	// Serverside Toss Custom Event
	UFUNCTION(BlueprintCallable, Category="Default")
	void ServersideToss(UPrimitiveComponent* TargetObject);

	UFUNCTION(BlueprintCallable, Category="Laser")
	void SetLaserBeamActive(bool LaserBeamActive);

	UFUNCTION(BlueprintCallable, Category="Teleportation")
	void ActivateTeleporter();

	UFUNCTION(BlueprintCallable, Category="Teleportation")
	void DisableTeleporter();

	UFUNCTION(BlueprintCallable, Category="Teleportation")
	void TraceTeleportDestination(bool& Success, TArray<FVector>& TracePoints, FVector& NavMeshLocation, FVector& TraceLocation);

	UFUNCTION(BlueprintCallable, Category="Teleportation")
	void ClearArc();

	UFUNCTION(BlueprintCallable, Category="Teleportation")
	void UpdateArcSpline(bool FoundValidLocation, UPARAM(ref) TArray<FVector>& SplinePoints);

	UFUNCTION(BlueprintCallable, Category="Teleportation")
	void UpdateArcEndpoint(FVector NewLocation, bool ValidLocationFound);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Teleportation")
	void GetTeleportDestination(bool RelativeToHMD, FVector& Location, FRotator& Rotation);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Teleportation")
	void GetTeleWorldLocAndForwardVector(FVector& WorldLoc, FVector& ForwardVector);

	UFUNCTION(BlueprintCallable)
	bool IfOverWidget_Use(bool bPressed);

	UFUNCTION(BlueprintCallable)
	void InitController();

	UFUNCTION(BlueprintCallable)
	void ToggleTick();

	UFUNCTION(BlueprintCallable, Category="Default")
	void ClearLaserBeam();

	UFUNCTION(BlueprintCallable, Category="Laser")
	void CreateLaserSpline();

	UFUNCTION(BlueprintCallable)
	void FilterGrabspline(UPARAM(ref) TArray<FVector>& Locations, UPARAM(ref) FVector& Target, int32 ClosestIndex, double ClosestDist);

	UFUNCTION(BlueprintCallable)
	void DisableWidgetActivation();

	UFUNCTION(BlueprintCallable)
	void RumbleController(float Intensity);

	UFUNCTION(BlueprintCallable)
	void StartedUseHeldObjectLeft();

	UFUNCTION(BlueprintCallable)
	void StartedUseHeldObjectRight();

	UFUNCTION(BlueprintCallable)
	void TossToHand(EControllerHand Hand);

	UFUNCTION(BlueprintCallable)
	void CancelTracking();

	void CreateTeleportationArc();

	UFUNCTION(BlueprintCallable, Category="Laser")
	void UpdateLaserBeam(float Deltatime);

protected:
public:
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	double TeleportLaunchVelocity;

	// Haptic feedback to play when teleportation is activated
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	TObjectPtr<UHapticFeedbackEffect_Base> TeleportHapticEffect;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Default")
	TObjectPtr<USplineComponent> LaserSpline;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Default")
	TObjectPtr<UStaticMeshComponent> LaserBeamEndPoint;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Default")
	TObjectPtr<UStaticMeshComponent> FinalFacingArrow;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Default")
	TObjectPtr<UWidgetInteractionComponent> WidgetInteraction;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Default")
	TObjectPtr<USplineMeshComponent> LaserBeam;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Default")
	TObjectPtr<UStaticMeshComponent> Arrow;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Default")
	TObjectPtr<USplineComponent> ArcSpline;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Default")
	TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Default")
	TObjectPtr<UStaticMeshComponent> TeleportCylinder;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Default")
	TObjectPtr<UStaticMeshComponent> ArcEndPoint;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Default")
	TObjectPtr<USceneComponent> Scene;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	TObjectPtr<AActor> AttachedActor;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	bool IsTeleporterActive;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	TArray<USplineMeshComponent*> SplineMeshes;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	bool bLastFrameValidDestination;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	bool IsRoomScale;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	bool IsValidTeleportDestination;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	EGripState GripState;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	bool bWantsToGrip;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	FRotator TeleportRotation;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<UPhysicsTossManager> PhysicsTossManager;

	/** Rotation of Motion Controller at the start of the Teleport press. */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	FRotator InitialControllerRotation;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default", meta=(ExposeOnSpawn="true"))
	TObjectPtr<UGripMotionControllerComponent> OwningMotionController;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	bool IsLaserBeamActive;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default")
	double LaserBeamMaxDistance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default", meta=(ExposeOnSpawn="true"))
	bool bIsLocal;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	bool bArcDirty;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	FRotator TeleportBaseRotation;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	FVector LastValidTeleportLocation;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Laser")
	FBPEuroLowPassFilter EuroLowPassFilter;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Laser")
	TArray<USplineMeshComponent*> LaserSplineMeshes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Laser")
	int32 NumberOfLaserSplinePoints;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Laser")
	double LaserBeamRadius;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Laser", meta=(DisplayName="B Use Smooth Laser"))
	bool bUseSmoothLaser;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Laser")
	bool DrawSmoothLaserTrace;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Laser")
	FHitResult LastLaserHitResult;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	bool bShowTrackingSensors;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default", meta=(MultiLine="true"))
	TObjectPtr<UPrimitiveComponent> LaserHighlightingObject;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default", meta=(MultiLine="true"))
	TObjectPtr<UPrimitiveComponent> ActorBeingThrown;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default", meta=(MultiLine="true"))
	FRotator RotOffset;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUseHeldObjectDispatch, UPrimitiveComponent*, HighlightedObject, bool, LeftHand);
	UPROPERTY(BlueprintAssignable, EditDefaultsOnly, Category="Default")
	FUseHeldObjectDispatch UseHeldObjectDispatch;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNotUseHeldObjectDispatch);
	UPROPERTY(BlueprintAssignable, EditDefaultsOnly, Category="Default")
	FNotUseHeldObjectDispatch NotUseHeldObjectDispatch;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	UStaticMesh* TeleportSplineMesh;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	UMaterialInterface* TeleportSplineMaterial;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	UStaticMesh* LaserSplineMesh;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	UMaterialInterface* LaserSplineMaterial;

	// Timer Handles

    FTimerHandle ControllerBind_TimerHandle;

private:



};
