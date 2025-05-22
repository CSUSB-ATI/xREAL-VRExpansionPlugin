// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VRCharacter.h"
#include "Enums/EVRMovementMode.h"
#include "Enums/EGripState.h"
#include "TeleportController.h"
#include "GameplayTagContainer.h"
#include "GraspingHandManny.h"
#include "OpenXRExpansionFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "xREAL_VRCharacter.generated.h"

class USphereComponent;
class UNoRepSphereComponent;
class UTextRenderComponent;
class UVOIPTalker;
class UInputAction;
class AWristMenuActor;

UCLASS(Blueprintable, BlueprintType)
class VREXPANSIONPLUGIN_API AxREAL_VRCharacter : public AVRCharacter
{
	GENERATED_BODY()
public:

	AxREAL_VRCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	void InitializeDefaults();
    
    void InitializeHeadMesh();

    void InitializeBody();

    void InitializeMotionControllers();

	// Function for replicating variables
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

    void QuitGame();

	virtual void Destroyed() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Called when the pawn is possessed. Only called on the server (or in standalone).
	virtual void PossessedBy(AController* NewController) override;

	// Executes on owning client
	UFUNCTION(Client, Reliable)
	void OnPossessed();

	void GetControllerTypeLoop();

    void SetControllerProfile(EBPOpenXRControllerDeviceType ControllerType);

	UFUNCTION(BlueprintNativeEvent)
	void CharacterSetup();
	virtual void CharacterSetup_Implementation();

	UFUNCTION(BlueprintNativeEvent)
    void SetupMotionControllers();
	virtual void SetupMotionControllers_Implementation();


	bool IsALocalGrip(EGripMovementReplicationSettings GripRepType);

	void WriteToLog(bool Left, FString& Text);

	void TryToGrabObject(UObject* ObjectToTryToGrab, FTransform WorldTransform, UGripMotionControllerComponent* Hand, UGripMotionControllerComponent* OtherHand, bool IsSlotGrip, FGameplayTag GripSecondaryTag, FName GripBoneName, FName SlotName, bool IsSecondaryGrip, bool& Gripped);

	// Server drop function
	UFUNCTION(Server, Reliable, Category="Gripping")
	void TryDropSingle_Server(UGripMotionControllerComponent* Hand, FVector_NetQuantize100 AngleVel, FVector_NetQuantize100 LinearVel, uint8 GripHash);

	// Client drop function
	UFUNCTION(BlueprintNativeEvent, Category="Gripping")
	void TryDropSingle_Client(UGripMotionControllerComponent* Hand, FBPActorGripInformation& GripToDrop, FVector AngleVel, FVector LinearVel);

	void GetNearestOverlappingObject(UPrimitiveComponent* OverlapComponent, UGripMotionControllerComponent* Hand, FGameplayTagContainer RelevantGameplayTags, UObject*& NearestObject, bool& ImplementsInterface, FTransform& ObjectTransform, bool& CanBeClimbed, FName& BoneName, FVector& ImpactLoc);

	bool PerformTraceForObjects(UPrimitiveComponent* OverlapComponent, UGripMotionControllerComponent* Hand, TArray<FHitResult>& OutHits);

	bool IsClimbingModeEnabled(UGripMotionControllerComponent *Hand);

	void GetDPadMovementFacing(EVRMovementMode MovementMode, UGripMotionControllerComponent* Hand, UGripMotionControllerComponent* OtherHand, FVector& ForwardVector, FVector& RightVector);

	/// @param RelevantGameplayTags Relevant GameplayTags are tags to search for and trigger operations depending on the key using this function
	/// @return Whether or not an action was performed
	bool GripOrDropObject(UGripMotionControllerComponent* CallingMotionController, UGripMotionControllerComponent* OtherController, bool CanCheckClimb, UPrimitiveComponent* GrabSphere, FGameplayTag GripTag, FGameplayTag DropTag, FGameplayTag UseTag, FGameplayTag EndUseTag, FGameplayTag GripSecondaryTag, FGameplayTag DropSecondaryTag);

	virtual void OnClimbingSteppedUp_Implementation() override;

	void CheckAndHandleClimbingMovement(double DeltaTime);

	void ClearClimbing(bool BecauseOfStepUp);

	void InitClimbing(UGripMotionControllerComponent* Hand, UObject* Object, bool _IsObjectRelative);

	virtual void UpdateClimbingMovement_Implementation(float DeltaTime) override;

	void CheckAndHandleGripAnimations();

	UObject* GetNearestOverlapOfHand(UGripMotionControllerComponent *Hand, UPrimitiveComponent *OverlapSphere);

	void CalculateRelativeVelocities();

	void HandleRunInPlace(FVector ForwardVector, bool IncludeHands);

	void GetSmoothedVelocityOfObject(FVector CurRelLocation, UPARAM(ref) FVector& LastRelLocation, UPARAM(ref) FVector& RelativeVelocityOut, UPARAM(ref) FVector& LowEndRelativeVelocityOut, bool bRollingAverage);

	float GetRelativeVelocityForLocomotion(bool IsHMD, bool IsMotionZVelBased, FVector VeloctyVector);

	void CallCorrectGrabEvent(UObject* ObjectToGrip, EControllerHand Hand, bool IsSlotGrip, FTransform GripTransform, FGameplayTag GripSecondaryTag, FName OptionalBoneName, FName SlotName, bool IsSecondaryGrip);

	void TryGrabClient(UObject* ObjectToGrab, bool IsSlotGrip, FTransform_NetQuantize GripTransform, EControllerHand Hand, FGameplayTag GripSecondaryTag, FName GripBoneName, FName SlotName, bool IsSecondaryGrip);
	
	UFUNCTION(Server, Reliable, Category="Gripping")
	void TryGrabServer(UObject* ObjectToGrab, bool IsSlotGrip, FTransform_NetQuantize GripTransform, EControllerHand Hand, FGameplayTag GripSecondaryTag, FName GripBoneName, FName SlotName, bool IsSecondaryGrip);

	void CallCorrectDropSingleEvent(UGripMotionControllerComponent* Hand, FBPActorGripInformation Grip);

	bool IfOverWidgetUse(UGripMotionControllerComponent* CallingHand, bool Pressed);

	void TryRemoveSecondaryAttachment(UGripMotionControllerComponent* CallingMotionController, UGripMotionControllerComponent* OtherController, FGameplayTagContainer GameplayTags, bool& DroppedSecondary, bool& HadSecondary);

	UFUNCTION(Server, Reliable, Category="Gripping")
	void RemoveSecondaryGrip_Server(UGripMotionControllerComponent* Hand, UObject* GrippedActorToRemoveAttachment);

	UFUNCTION(BlueprintNativeEvent)
	void SwitchOutOfBodyCamera(bool SwitchToOutOfBody);

	UFUNCTION(BlueprintCallable)
	void ExecuteTeleportation(ATeleportController* MotionController, EVRMovementMode MovementMode, EControllerHand Hand);
	
	UFUNCTION(BlueprintCallable)
	void SetTeleporterActive(EControllerHand Hand, bool Active);

protected:
	virtual void SwitchOutOfBodyCamera_Implementation(bool SwitchToOutOfBody);

	virtual void SpawnWristMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetWristMenuEnabled(bool bEnabled);


	UFUNCTION(Server, Reliable, Category="Teleport")
	void NotifyTeleportActive_Server(EControllerHand Hand, bool State);

	UFUNCTION(NetMulticast, Reliable, Category="Teleport")
	void TeleportActive_Multicast(EControllerHand Hand, bool State);

	virtual void NavigationMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	virtual void OnSeatedModeChanged_Implementation(bool bNewSeatedMode, bool bWasAlreadySeated) override;

	void HandleSlidingMovement(EVRMovementMode MovementMode, UGripMotionControllerComponent* CallingHand, bool bThumbPadInfluencesDirection);

	/**
	 * Calulates yaw rotation and magnitude from 0-1 of a -1 to 1 pad axis pair
	 * 
	 * YAxis is automatically inverted
	 */
	void CalcPadRotationAndMagnitude(float YAxis, float XAxis, float OptMagnitudeScaler, float OptionalDeadzone, FRotator& Rotation, float& Magnitude, bool& WasValid);

	void UpdateTeleportRotations();

	/** Pivot is in world space */
	void GetCharacterRotatedPosition(FVector OriginalLocation, FRotator DeltaRotation, FVector PivotPoint, FRotator& OutRotation, FVector& OutNewPosition);

	/// @return If the tag is matched or is the default tag
	bool ValidateGameplayTag(FGameplayTag BaseTag, FGameplayTag GameplayTag, UObject* Object, FGameplayTag DefaultTag);

	void CycleMovementModes(bool IsLeft);

	UFUNCTION(BlueprintCallable)
	void DropItems(UGripMotionControllerComponent* Hand, FGameplayTagContainer GameplayTags);

	void DropItem(UGripMotionControllerComponent* Hand, FBPActorGripInformation GripInfo, FGameplayTagContainer GameplayTags);

	void OnDestroy();

	/**
	 * Returns socket name if is a Touch controller or object has hand specific left/right gameplay tags
	 * 
	 * Touch = VRGripTouchP
	 * 
	 * LeftTouch = VRGripTouchLP
	 * 
	 * LeftNormal = VRGripLP
	 */
	void GetCorrectPrimarySlotPrefix(UObject* ObjectToCheckForTag, EControllerHand Hand, FName NearestBoneName, FName& SocketPrefix);

	bool CanObjectBeClimbed(UPrimitiveComponent* ObjectToCheck);

	bool HasValidGripCollision(UPrimitiveComponent* Component);

	UFUNCTION(BlueprintNativeEvent)
	void SetVehicleMode(bool IsInVehicleMode, bool& IsVR);

protected:
	virtual void SetVehicleMode_Implementation(bool IsInVehicleMode, bool& IsVR);

public:
	void SetVehicleMode(bool IsInVehicleMode);

	bool ShouldGripComponent(UPrimitiveComponent* ComponentToCheck, uint8 GripPrioToCheckAgainst, bool bCheckAgainstPrior, FName BoneName, FGameplayTagContainer RelevantGameplayTags, UGripMotionControllerComponent* CallingController, UObject*& ObjectToGrip, bool& ObjectImplementsInterface, FTransform& ObjectsWorldTransform, uint8& GripPrio);

	void TryToSecondaryGripObject(UGripMotionControllerComponent* Hand, UGripMotionControllerComponent* OtherHand, UObject* ObjectToTryToGrab, FGameplayTag GripSecondaryTag, bool ObjectImplementsInterface, FTransform RelativeSecondaryTransform, FName SlotName, bool bHadSlot, bool& SecondaryGripped);

	void ClearMovementVelocities();

	
	/// @param RelevantGameplayTags Relevant GameplayTags are tags to search for and trigger operations depending on the key using this function
	/// @return Whether or not an action was performed
	bool GripOrDropObjectClean(UGripMotionControllerComponent* CallingMotionController, UGripMotionControllerComponent* OtherController, bool CanCheckClimb, UPrimitiveComponent* GrabSphere, FGameplayTagContainer RelevantGameplayTags);

	/// @return If the tag is matched or is the default tag
	bool ValidateGameplayTagContainer(FGameplayTag BaseTag, UObject* Object, FGameplayTag DefaultTag, FGameplayTagContainer GameplayTags);

	// Crazy Style
	// Load bearing code ^
	UFUNCTION(BlueprintCallable)
	void DropSecondaryAttachment(UGripMotionControllerComponent* CallingMotionController, UGripMotionControllerComponent* OtherController, FGameplayTagContainer GameplayTags, bool& DroppedSecondary, bool& HadSecondary);

	void SelectObjectFromHitArray(UPARAM(ref) TArray<FHitResult>& Hits, FGameplayTagContainer RelevantGameplayTags, UGripMotionControllerComponent* Hand, bool& bShouldGrip, bool& ObjectImplementsInterface, UObject*& ObjectToGrip, FTransform& WorldTransform, UPrimitiveComponent*& FirstPrimitiveHit, FName& BoneName, FVector& ImpactPoint);

	void CheckGripPriority(UObject* ObjectToCheck, uint8 PrioToCheckAgainst, bool CheckAgainstPrior, bool& HadHigherPriority, uint8& NewGripPrio);

	void GetBoneTransform(UObject* Object, FName BoneName, FTransform& BoneTransform);

	bool CanSecondaryGripObject(UGripMotionControllerComponent* Hand, UGripMotionControllerComponent* OtherHand, UObject* ObjectToTryToGrab, FGameplayTag GripSecondaryTag, bool HadSlot, ESecondaryGripType SecGripType);

	bool CanAttemptGrabOnObject(UObject* ObjectToCheck);

	bool CanAttemptSecondaryGrabOnObject(UObject* ObjectToCheck, ESecondaryGripType* OutSecondaryGripType = nullptr);

	FRotator GetCorrectRotation();

	/** Sets which components are used for gripping collision */
	void SetGripComponents(UPrimitiveComponent* LeftHand, UPrimitiveComponent* RightHand);

	void GetThrowingVelocity(UGripMotionControllerComponent* ThrowingController, UPARAM(ref) FBPActorGripInformation& Grip, FVector AngularVel, FVector ObjectsLinearVel, FVector& angVel, FVector& LinearVelocity);

	void CheckSpawnGraspingHands();

	void ClearGraspingHands();

	void RepositionHandElements(bool IsRightHand, FTransform NewTransformForProcComps);

	void ShouldSocketGrip(UPARAM(ref) FBPActorGripInformation& Grip, bool& ShouldSocket, USceneComponent*& SocketParent, FTransform_NetQuantize& RelativeTransform, FName& OptionalSocketName);

	UFUNCTION(BlueprintNativeEvent)
	void InitTeleportControllers_Event();
	   
	UFUNCTION(BlueprintNativeEvent)
	void OnPlayerStateReplicated(const APlayerState* NewPlayerState);

protected:
	virtual void OnPlayerStateReplicated_Implementation(const APlayerState* NewPlayerState);

public:

	/** Initialize controllers and setup voice */
	UFUNCTION(BlueprintNativeEvent)
	void InitTeleportControllers(const APlayerState* ValidPlayerState);

	UFUNCTION()
	void RepositionRightControllerProceduralMeshes(const FTransform& NewRelTransformForProcComps, const FTransform& NewProfileTransform);

	UFUNCTION()
	void RepositionLeftControllerProceduralMeshes(const FTransform& NewRelTransformForProcComps, const FTransform& NewProfileTransform);

	UFUNCTION(Server, Reliable)
	void SendControllerProfileToServer(bool IsRightHand, FTransform_NetQuantize NewTransform);

	UFUNCTION(Server, Reliable)
	void NotifyServerOfTossRequest(bool LeftHand, UPrimitiveComponent* TargetPrimitive);

	bool CheckIsValidForGripping(UObject* Object, FGameplayTagContainer RelevantGameplayTags);

	/// @brief 
	/// @param SocketTransform 
	/// @param GrippingController 
	/// @return The final transform after removing the controller scale
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Gripping")
	FTransform RemoveControllerScale(FTransform SocketTransform, UGripMotionControllerComponent* GrippingController);

	UFUNCTION(BlueprintNativeEvent, Category="Gripping")
	void CheckUseHeldItems(UGripMotionControllerComponent* Hand, bool ButtonState);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Movement")
	USceneComponent* GetCorrectAimComp(UGripMotionControllerComponent* Hand);

	UFUNCTION(BlueprintCallable, Category="Input")
	void MapInput();

	UFUNCTION(BlueprintNativeEvent, Category="Gripping|Velocity")
	void SampleGripVelocities();

	UFUNCTION(BlueprintNativeEvent, Category="Gripping")
	void CheckUseSecondaryAttachment(UGripMotionControllerComponent* CallingMotionController, UGripMotionControllerComponent* OtherController, bool ButtonPressed, bool& DroppedOrUsedSecondary, bool& HadSecondary);

	UFUNCTION(BlueprintNativeEvent, Category="Movement")
	void HandleCurrentMovementInput(float MovementInput, UGripMotionControllerComponent* MovingHand, UGripMotionControllerComponent* OtherHand);

	UFUNCTION(BlueprintNativeEvent, Category="Movement")
	void HandleTurnInput(float InputAxis);

	UFUNCTION(BlueprintNativeEvent, Category="Movement")
	void SetMovementHands(bool RightHandForMovement);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Locomotion")
	FVector MapThumbToWorld(FRotator PadRotation, UGripMotionControllerComponent* CallingHand);

	UFUNCTION(BlueprintNativeEvent)
	void OnRep_RightControllerOffset();

	UFUNCTION(BlueprintNativeEvent)
	void OnRep_LeftControllerOffset();

	// Function to add multiple tags from string values to a tag container
	UFUNCTION()
	void AddTagsToContainer(FGameplayTagContainer& BaseContainer, const TArray<FString>& TagsToAdd);

	// Toggle LaserBeams for the controllers
	UFUNCTION(Server, Reliable)
	void ActivateBeam_Server(bool LeftController, bool Active);

	UFUNCTION(NetMulticast, Reliable)
	void ActivateBeam_Multicast(bool LeftController, bool Active);

	// Trigger Grip/Drop/Use Logic
	UFUNCTION()
	void TriggerGripOrDrop(UGripMotionControllerComponent* CallingHand, UGripMotionControllerComponent* OtherHand, bool isGrip, UPrimitiveComponent* GrabSphere);

protected:

	// Input Handler Functions

	UFUNCTION()
	void ToggleWristMenu_Started();

	UFUNCTION()
	void TeleportRight_Started();
	UFUNCTION()
	void TeleportRight_Completed();

	UFUNCTION()
	void TeleportLeft_Started();
	UFUNCTION()
	void TeleportLeft_Completed();

	UFUNCTION()
	void LaserBeamRight_Started();

	UFUNCTION()
	void LaserBeamLeft_Started();

	UFUNCTION()
	void AlternateGripRight_Started();
	UFUNCTION()
	void AlternateGripRight_Completed();

	UFUNCTION()
	void AlternateGripLeft_Started();
	UFUNCTION()
	void AlternateGripLeft_Completed();

	// Trigger "Grab" L/R
	UFUNCTION()
	void PrimaryGripRight_Started();
	UFUNCTION()
	void PrimaryGripRight_Completed();

	UFUNCTION()
	void PrimaryGripLeft_Started();
	UFUNCTION()
	void PrimaryGripLeft_Completed();

	// Use Held Items L/R
	UFUNCTION()
	void UseHeldObjectRight_Started();
	UFUNCTION()
	void UseHeldObjectRight_Completed();

	UFUNCTION()
	void UseHeldObjectLeft_Started();
	UFUNCTION()
	void UseHeldObjectLeft_Completed();



	UFUNCTION()
	void ControllerMovementRight_Triggered(const FInputActionValue& Value);
	UFUNCTION()
	void ControllerMovementRight_Completed(const FInputActionValue& Value);

	UFUNCTION()
	void ControllerMovementLeft_Triggered(const FInputActionValue& Value);
	UFUNCTION()
	void ControllerMovementLeft_Completed(const FInputActionValue& Value);

	UFUNCTION()
	void MotionControllerThumbLeft_X_Handler(const FInputActionValue& Value);

	UFUNCTION()
	void MotionControllerThumbLeft_Y_Handler(const FInputActionValue& Value);

	UFUNCTION()
	void MotionControllerThumbRight_X_Handler(const FInputActionValue& Value);

	UFUNCTION()
	void MotionControllerThumbRight_Y_Handler(const FInputActionValue& Value);

	// Component Handler Functions

	UFUNCTION()
	void OnRightMotionControllerGripped(const FBPActorGripInformation& GripInfo);

	UFUNCTION()
	void OnLeftMotionControllerGripped(const FBPActorGripInformation& GripInfo);

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	bool bUseWristMenu;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	bool bWristMenuOnRightHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "UI")
	bool bIsWristMenuEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	FVector WristMenuRelativeLocation = FVector(0,0,0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	FRotator WristMenuRelativeRotation = FRotator(0,0,0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<AWristMenuActor> WristMenuActorClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	AWristMenuActor* WristMenuActor;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement")
	double MinimumLowEndRipVelocity;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<USphereComponent> GrabSphereRight;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<UNoRepSphereComponent> AttachmentProxyLeft;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<UNoRepSphereComponent> AttachmentProxyRight;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<USceneComponent> AimRight;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<USceneComponent> AimLeft;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<UTextRenderComponent> TextL;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<UVOIPTalker> VOIPTalker;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<UTextRenderComponent> TextR;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<USphereComponent> GrabSphereLeft;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<USkeletalMeshComponent> HandMesh_Left;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<UTextRenderComponent> PlayerNameDisplay;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Misc")
	FTimerHandle ControllerTimerHandle;

	FTimerHandle GetControllerType_TimerHandle;

	FTimerHandle InitTeleportControllers_TimerHandle;

	FTimerHandle TeleportFade_TimerHandle;

	FTimerHandle NavigationFinishedTeleportFade_TimerHandle;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Teleport")
	bool IsTeleporting;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Teleport")
	float FadeinDuration;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Teleport")
	FLinearColor TeleportFadeColor;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Teleport")
	float FadeOutDuration;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Teleport", Transient)
	TObjectPtr<ATeleportController> TeleportControllerLeft;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Teleport", Transient)
	TObjectPtr<ATeleportController> TeleportControllerRight;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement")
	EVRMovementMode CurrentMovementMode;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement")
	EVRMovementMode MovementModeRight;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement")
	double DPadVelocityScaler;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping")
	double GripTraceLength;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Climbing")
	FTransform ClimbGripLocation;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Climbing")
	bool IsHandClimbing;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Climbing")
	TObjectPtr<UGripMotionControllerComponent> ClimbingHand;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Climbing")
	TObjectPtr<UPrimitiveComponent> GrippedObject;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Climbing")
	bool IsObjectRelative;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Control Booleans")
	bool AlwaysAllowClimbing;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping")
	EGripState HandStateLeft;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping")
	EGripState HandStateRight;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement")
	double SwingAndRunMagnitude;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement|Vel")
	FVector LeftControllerRelativeVel;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement|Vel")
	FVector RightControllerRelativeVel;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement|Vel")
	FVector HeadRelativeVel;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement")
	double RunningInPlaceScaler;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement")
	int32 RIPMotionSmoothingSteps;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement|Vel")
	FVector LastLContPos;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement|Vel")
	FVector LastRContPos;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement|Vel")
	FVector LastHMDPos;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Control Booleans")
	bool bIsArmSwingZBased;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement")
	double MinimumRIPVelocity;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Misc")
	FTimerHandle TrackedDeviceTimerHandle;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement")
	int32 RipMotionLowPassSmoothingSteps;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement|Vel")
	FVector LowEndLContRelativeVel;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement|Vel")
	FVector LowEndRContRelativeVel;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement|Vel")
	FVector LowEndHeadRelativeVel;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Default")
	TObjectPtr<USkeletalMeshComponent> HandMesh_Right;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Control Booleans")
	bool bIncludeHandsForRIP;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement")
	bool bIsOutOfBody;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Control Booleans", meta=(DisplayName="B Thumb Pad Effects Sliding Direction"))
	bool bThumbPadEffectsSlidingDirection;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Control Booleans", meta=(DisplayName="B Teleport Uses Thumb Rotation"))
	bool bTeleportUsesThumbRotation;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Teleport")
	double TeleportThumbDeadzone;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping", meta=(MultiLine="true"))
	FGameplayTag DefaultGripTag;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping")
	FGameplayTag DefaultDropTag;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping")
	FGameplayTag DefaultSecondaryDropTag;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping")
	FGameplayTag DefaultSecondaryGripTag;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	EBPHMDDeviceType HeadsetType;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	bool bWasAlreadyPossessed;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Vehicle")
	bool bIsInVehicle;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement")
	bool DisableMovement;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default")
	TObjectPtr<ACameraActor> OutOfBodyCamera;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping")
	TArray<TEnumAsByte<EObjectTypeQuery>> CollisionToCheckDuringGrip;

	/** If true, will skip trace step of checking for components to grip */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping")
	bool bForceOverlapOnlyGripChecks;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="VOIP")
	TObjectPtr<USoundAttenuation> AttenuationSettingsForVOIP;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default")
	TObjectPtr<UPrimitiveComponent> LeftHandGripComponent;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default")
	TObjectPtr<UPrimitiveComponent> RightHandGripComponent;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement")
	double SlidingMovementDeadZone;

	static_assert(true, "You will need to add DOREPLIFETIME_WITH_PARAMS(ABP_VRCharacter, LeftControllerOffset, COND_SkipOwner) to GetLifetimeReplicatedProps");
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default", ReplicatedUsing="OnRep_LeftControllerOffset")
	FTransform_NetQuantize LeftControllerOffset;

	static_assert(true, "You will need to add DOREPLIFETIME_WITH_PARAMS(ABP_VRCharacter, RightControllerOffset, COND_SkipOwner) to GetLifetimeReplicatedProps");
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default", ReplicatedUsing="OnRep_RightControllerOffset")
	FTransform_NetQuantize RightControllerOffset;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default", meta=(MultiLine="true"))
	bool SpawnGraspingHands;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default", meta=(MultiLine="true"))
	bool UsePhysicalGraspingHands;

	static_assert(true, "You will need to add DOREPLIFETIME(ABP_VRCharacter, GraspingHandRight) to GetLifetimeReplicatedProps");
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default", Replicated, meta=(MultiLine="true"))
	TObjectPtr<AGraspingHandManny> GraspingHandRight;

	static_assert(true, "You will need to add DOREPLIFETIME(ABP_VRCharacter, GraspingHandLeft) to GetLifetimeReplicatedProps");
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default", Replicated, meta=(MultiLine="true"))
	TObjectPtr<AGraspingHandManny> GraspingHandLeft;

	/** If true then we  will sample the controller velocity on release if we aren't sampling the objects velocity */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping|Velocity", meta=(MultiLine="true"))
	bool UseControllerVelocityOnRelease;

	/** what mass we should consider full strength, anything more than this starts to scale throw speed down */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping", meta=(MultiLine="true"))
	double ThrowingMassScaleFactor;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping|Velocity", meta=(MultiLine="true"))
	bool bSampleGripVelocity;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping|Velocity", meta=(MultiLine="true"))
	FBPLowPassPeakFilter PeakVelocityLeft;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping|Velocity", meta=(MultiLine="true"))
	FBPLowPassPeakFilter PeakVelocityRight;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping|Velocity", meta=(MultiLine="true"))
	bool ScaleVelocityByMass;

	/** Maximum mass at which we have full velocity, it will scale down from here up */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping|Velocity", meta=(MultiLine="true"))
	double ThrowingMassMaximum;

	/** Minimum value that mass scaling can go to */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping|Velocity", meta=(MultiLine="true"))
	double MassScalerMin;

	/** Maximum size of the throwing velocity, we will clamp to this */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping|Velocity", meta=(MultiLine="true"))
	double MaximumThrowingVelocity;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Gripping|Velocity", meta=(MultiLine="true"))
	bool bLimitMaxThrowVelocity;

	/** Use OpenXR Hand Tracking - Used to be SteamVR Curls */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Default", meta=(MultiLine="true"))
	bool GraspingHandsUseFingerCurls;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	bool bDisableLaserBeams;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnClimbingInitiated, UGripMotionControllerComponent*, ClimbingHand, UPrimitiveComponent*, GrippedObject, FTransform, RelativeTransform);
	UPROPERTY(BlueprintAssignable, EditDefaultsOnly, Category="Default", meta=(MultiLine="true"))
	FOnClimbingInitiated OnClimbingInitiated;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnClimbingEnded);
	UPROPERTY(BlueprintAssignable, EditDefaultsOnly, Category="Default", meta=(MultiLine="true"))
	FOnClimbingEnded OnClimbingEnded;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Movement", meta=(MultiLine="true"))
	bool bRightHandMovement;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Movement|Turning", meta=(MultiLine="true"))
	bool bTurnModeIsSnap;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement|Turning", meta=(MultiLine="true"))
	double SnapTurnAngle;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement|Turning", meta=(MultiLine="true"))
	double SmoothTurnSpeed;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement|Turning", meta=(MultiLine="true", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1"))
	float TurningActivationThreshold;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Movement|Turning", meta=(MultiLine="true"))
	bool bTurningFlag;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="ControllerProfile", meta=(MultiLine="true"))
	EBPOpenXRControllerDeviceType CurrentControllerTypeXR;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Input")
	TObjectPtr<UPlayerMappableInputConfig> InputConfig;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Movement")
	bool bTwoHandMovement;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Movement")
	bool bAllowCycleMovementMode;

private:

	UPROPERTY()
	float MotionControllerThumbLeft_X_Value;

	UPROPERTY()
	float MotionControllerThumbLeft_Y_Value;

	UPROPERTY()
	float MotionControllerThumbRight_X_Value;

	UPROPERTY()
	float MotionControllerThumbRight_Y_Value;

	//Blueprint Input Actions
    UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MotionControllerThumbLeft_X;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MotionControllerThumbLeft_Y;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MotionControllerThumbRight_X;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MotionControllerThumbRight_Y;

};
