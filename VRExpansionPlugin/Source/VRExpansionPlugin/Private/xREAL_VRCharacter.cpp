#include "xREAL_VRCharacter.h"
#include "Components/TextRenderComponent.h"
#include "EnhancedInputComponent.h"
#include "GameplayTagAssetInterface.h"
#include "VRExpansionFunctionLibrary.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "PlayerMappableInputConfig.h"
#include "Engine/LocalPlayer.h"
#include "Net/VoiceConfig.h"
#include "VRGlobalSettings.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerState.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "WristMenuActor.h"
#include "ParentRelativeAttachmentComponent.h"
#include "TeleportComponent.h"

#pragma region Initialization

AxREAL_VRCharacter::AxREAL_VRCharacter(const FObjectInitializer& ObjectInitializer): Super() 
{
    bReplicates = true;
    
    InitializeDefaults();
    InitializeHeadMesh();
    InitializeBody();
    InitializeMotionControllers();
    

    // Set up player name text component
    PlayerNameDisplay = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PlayerNameDisplay"));
    if (PlayerNameDisplay)
    {
        PlayerNameDisplay->SetupAttachment(VRReplicatedCamera);
        PlayerNameDisplay->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
        PlayerNameDisplay->SetTextMaterial(FindFirstObjectSafe<UMaterialInterface>(TEXT("DefaultTextMaterialOpaque.DefaultTextMaterialOpaque")));
        PlayerNameDisplay->SetText(FText::FromString("PlayerName"));
        PlayerNameDisplay->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
        PlayerNameDisplay->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextBottom);
        PlayerNameDisplay->SetWorldSize(26.0f);
    }

    TeleportComponent = CreateDefaultSubobject<UTeleportComponent>(TEXT("TeleportComponent"));

}

void AxREAL_VRCharacter::InitializeDefaults()
{
    AlwaysAllowClimbing = true;
    bThumbPadEffectsSlidingDirection = true;

    FadeinDuration = 0.25f;
    FadeOutDuration = 0.25f;

    TeleportThumbDeadzone = 0.4f;

    bTurnModeIsSnap = true;
    SnapTurnAngle = 45.0f;
    SmoothTurnSpeed = 50.0f;
    TurningActivationThreshold = 0.7f;
    CurrentMovementMode = EVRMovementMode::Teleport;
    MovementModeRight = EVRMovementMode::Teleport;
    DPadVelocityScaler = 1.25f;
    SwingAndRunMagnitude = 2.0f;
    RunningInPlaceScaler = 2.0f;

    RIPMotionSmoothingSteps = 15;
    MinimumRIPVelocity = 0.3f;
    RipMotionLowPassSmoothingSteps = 1;
    MinimumLowEndRipVelocity = 0.1f;

    bTwoHandMovement = true;

    PeakVelocityLeft.VelocitySamples = 30;
    PeakVelocityRight.VelocitySamples = 30;

    ThrowingMassMaximum = 50.0f;
    MassScalerMin = 0.3f;
    MaximumThrowingVelocity = 1000.0f;

    bLimitMaxThrowVelocity = true;
    GripTraceLength = 0.1f;
    HandStateRight = EGripState::Open;
    HandStateLeft = EGripState::Open;

    DefaultGripTag = FGameplayTag::RequestGameplayTag(FName("GripType.OnPrimaryGrip"));
    DefaultDropTag = FGameplayTag::RequestGameplayTag(FName("DropType.OnPrimaryGripRelease"));
    DefaultSecondaryDropTag = FGameplayTag::RequestGameplayTag(FName("DropType.Secondary.OnPrimaryGripRelease"));
    DefaultSecondaryGripTag = FGameplayTag::RequestGameplayTag(FName("GripType.Secondary.OnPrimaryGrip"));

    // World Static, World Dynamic, Pawn, Physics Body, Vehicle, Destructible
    CollisionToCheckDuringGrip = TArray<TEnumAsByte<EObjectTypeQuery>>({
        UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic),
        UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic),
        UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn),
        UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_PhysicsBody),
        UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Vehicle),
        UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Destructible)});

    ThrowingMassScaleFactor = 10.0f;

    IsObjectRelative = true;

    CurrentControllerTypeXR = EBPOpenXRControllerDeviceType::DT_SimpleController;

    InputConfig = FindFirstObjectSafe<UPlayerMappableInputConfig>(TEXT("/VRExpansionPlugin/VRE/Input/VREInputConfig.VREInputConfig"));

    HeadsetType = EBPHMDDeviceType::DT_OculusHMD;

    SpawnGraspingHands = true;
    UsePhysicalGraspingHands = false;

    // Wrist Menu Defaults
    bUseWristMenu = false;
    bWristMenuOnRightHand = false;
}

void AxREAL_VRCharacter::InitializeHeadMesh()
{
    HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
    if (HeadMesh)
    {
        HeadMesh->SetupAttachment(VRReplicatedCamera);
        HeadMesh->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.25f));
		static ConstructorHelpers::FObjectFinder<UStaticMesh>MeshAsset(TEXT("StaticMesh'/VRExpansionPlugin/VRE/Misc/Meshes/1M_Cube_Chamfer.1M_Cube_Chamfer'"));
        if (MeshAsset.Object)
        {
            HeadMesh->SetStaticMesh(MeshAsset.Object);
        }

        HeadMesh->SetCollisionProfileName(FName(TEXT("NoCollision")));
    }
}

void AxREAL_VRCharacter::InitializeBody()
{
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    if (Body)
    {
        Body->SetupAttachment(ParentRelativeAttachment);
        Body->SetRelativeLocation(FVector(-15.0f, 0.0f, -61.59807f));
        Body->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.1f));
    }
}

void AxREAL_VRCharacter::InitializeMotionControllers()
{
    // Does not affect the final position of the controllers, only set to make the controllers more visible in editor viewport.
	RightMotionController->SetWorldLocation(FVector(65.074234, 30.863922, 9.278572));
	LeftMotionController->SetWorldLocation(FVector(65.0, -43.256454, -11.514534));

    // Set up text elements on motion controllers
    TextR = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TextR"));
    if (TextR)
    {
        TextR->SetupAttachment(RightMotionController);
        TextR->SetRelativeLocation(FVector(10.0f, -0.001106f, 2.106884f));
        TextR->SetRelativeRotation(FRotator(82.999741f, -179.9993f, 0.0f));
        TextR->SetTextMaterial(FindFirstObjectSafe<UMaterialInterface>(TEXT("DefaultTextMaterialOpaque.DefaultTextMaterialOpaque")));
        TextR->SetText(FText::FromString("R"));
        TextR->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
        TextR->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
        TextR->SetWorldSize(3.0f);
    }
    TextL = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TextL"));
    if (TextL)
    {
        TextL->SetupAttachment(LeftMotionController);
        TextL->SetRelativeLocation(FVector(10.0f, -0.001764f, 2.106884f));
        TextL->SetRelativeRotation(FRotator(82.999741f, -179.9993f, 0.0f));
        TextL->SetTextMaterial(FindFirstObjectSafe<UMaterialInterface>(TEXT("DefaultTextMaterialOpaque.DefaultTextMaterialOpaque")));
        TextL->SetText(FText::FromString("L"));
        TextL->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
        TextL->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
        TextL->SetWorldSize(3.0f);
    }

    // Set up attachment proxy components
    AttachmentProxyRight = CreateDefaultSubobject<UNoRepSphereComponent>(TEXT("AttachmentProxyRight"));
    if (AttachmentProxyRight)
    {
        AttachmentProxyRight->SetupAttachment(RightMotionController);
        AttachmentProxyRight->SetRelativeLocation(FVector(2.0f, 0.0f, 0.0f));
        AttachmentProxyRight->SetSphereRadius(4.0f);
        AttachmentProxyRight->SetCollisionProfileName(FName(TEXT("NoCollision")));
        AttachmentProxyRight->ComponentTags.Add(FName(TEXT("ATTACHPROXYRIGHT")));
    }
    AttachmentProxyLeft = CreateDefaultSubobject<UNoRepSphereComponent>(TEXT("AttachmentProxyLeft"));
    if (AttachmentProxyLeft)
    {
        AttachmentProxyLeft->SetupAttachment(LeftMotionController);
        AttachmentProxyLeft->SetRelativeLocation(FVector(2.0f, 0.0f, 0.0f));
        AttachmentProxyLeft->SetSphereRadius(4.0f);
        AttachmentProxyLeft->SetCollisionProfileName(FName(TEXT("NoCollision")));
        AttachmentProxyLeft->ComponentTags.Add(FName(TEXT("ATTACHPROXYLEFT")));
    }

    // Set up aim components
    AimRight = CreateDefaultSubobject<USceneComponent>(TEXT("AimRight"));
    if (AimRight)
    {
        AimRight->SetupAttachment(RightMotionController);
        AimRight->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
    }
    AimLeft = CreateDefaultSubobject<USceneComponent>(TEXT("AimLeft"));
    if (AimLeft)
    {
        AimLeft->SetupAttachment(LeftMotionController);
        AimLeft->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
    }


    // Set up hand mesh components
    HandMesh_Right = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandMeshRight"));
    if (HandMesh_Right)
    {
        HandMesh_Right->SetupAttachment(RightMotionController);
        HandMesh_Right->SetRelativeLocation(FVector(-1.120374f, 2.826352f, 7.41304f));
        HandMesh_Right->SetRelativeRotation(FRotator(20.0f, 0.0f, 80.0f));
        HandMesh_Right->SetSkeletalMesh(FindFirstObjectSafe<USkeletalMesh>(TEXT("SkeletalMesh'/VRExpansionPlugin/VRE/Core/GraspingHands/VRHandMeshes/Meshes/SKM_MannyXR_right.SKM_MannyXR_right'")));
        HandMesh_Right->SetCollisionProfileName(FName(TEXT("NoCollision")));
    }
    HandMesh_Left = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandMeshLeft"));
    if (HandMesh_Left)
    {
        HandMesh_Left->SetupAttachment(LeftMotionController);
        HandMesh_Left->SetRelativeLocation(FVector(-1.120374f, -2.826352f, 7.41304f));
        HandMesh_Left->SetRelativeRotation(FRotator(20.0f, 0.0f, 100.0f));
        HandMesh_Left->SetRelativeScale3D(FVector(1.0f, 1.0f, -1.0f));
        HandMesh_Left->SetSkeletalMesh(FindFirstObjectSafe<USkeletalMesh>(TEXT("SkeletalMesh'/VRExpansionPlugin/VRE/Core/GraspingHands/VRHandMeshes/Meshes/SKM_MannyXR_right.SKM_MannyXR_right'")));
        HandMesh_Left->SetCollisionProfileName(FName(TEXT("NoCollision")));
    }

    // Set up grab sphere components
    GrabSphereRight = CreateDefaultSubobject<USphereComponent>(TEXT("GrabSphereRight"));
    if (GrabSphereRight)
    {
        GrabSphereRight->SetupAttachment(RightMotionController);
        GrabSphereRight->SetRelativeLocation(FVector(2.0f, 0.0f, 0.0f));
        GrabSphereRight->SetSphereRadius(4.0f);
        GrabSphereRight->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        GrabSphereRight->SetCollisionObjectType(ECollisionChannel::ECC_Pawn);
        GrabSphereRight->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
    }
    GrabSphereLeft = CreateDefaultSubobject<USphereComponent>(TEXT("GrabSphereLeft"));
    if (GrabSphereLeft)
    {
        GrabSphereLeft->SetupAttachment(LeftMotionController);
        GrabSphereLeft->SetRelativeLocation(FVector(2.0f, 0.0f, 0.0f));
        GrabSphereLeft->SetSphereRadius(4.0f);
        GrabSphereLeft->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        GrabSphereLeft->SetCollisionObjectType(ECollisionChannel::ECC_Pawn);
        GrabSphereLeft->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
    }

}

void AxREAL_VRCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority())
    {
        CharacterSetup(); // Runs in PossessedBy function on the server since BeginPlay runs before possession on server.
    }

}

void AxREAL_VRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) 
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* PlayerEnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        MotionControllerThumbLeft_X = FindFirstObjectSafe<UInputAction>(TEXT("MotionControllerThumbLeft_X"));
        if (MotionControllerThumbLeft_X)
        {
            PlayerEnhancedInputComponent->BindAction(MotionControllerThumbLeft_X, ETriggerEvent::Triggered, this, &AxREAL_VRCharacter::MotionControllerThumbLeft_X_Handler);
        }
        MotionControllerThumbLeft_Y = FindFirstObjectSafe<UInputAction>(TEXT("MotionControllerThumbLeft_Y"));
        if (MotionControllerThumbLeft_Y)
        {
            PlayerEnhancedInputComponent->BindAction(MotionControllerThumbLeft_Y, ETriggerEvent::Triggered, this, &AxREAL_VRCharacter::MotionControllerThumbLeft_Y_Handler);
        }
        MotionControllerThumbRight_X = FindFirstObjectSafe<UInputAction>(TEXT("MotionControllerThumbRight_X"));
        if (MotionControllerThumbRight_X)
        {
            PlayerEnhancedInputComponent->BindAction(MotionControllerThumbRight_X, ETriggerEvent::Triggered, this, &AxREAL_VRCharacter::MotionControllerThumbRight_X_Handler);
        }
        MotionControllerThumbRight_Y = FindFirstObjectSafe<UInputAction>(TEXT("MotionControllerThumbRight_Y"));
        if (MotionControllerThumbRight_Y)
        {
            PlayerEnhancedInputComponent->BindAction(MotionControllerThumbRight_Y, ETriggerEvent::Triggered, this, &AxREAL_VRCharacter::MotionControllerThumbRight_Y_Handler);
        }

        if (UInputAction* ControllerMovementRight = FindFirstObjectSafe<UInputAction>(TEXT("ControllerMovementRight")))
        {
            PlayerEnhancedInputComponent->BindAction(ControllerMovementRight, ETriggerEvent::Triggered, this, &AxREAL_VRCharacter::ControllerMovementRight_Triggered);
            PlayerEnhancedInputComponent->BindAction(ControllerMovementRight, ETriggerEvent::Completed, this, &AxREAL_VRCharacter::ControllerMovementRight_Completed);
        }

        if (UInputAction* ControllerMovementLeft = FindFirstObjectSafe<UInputAction>(TEXT("ControllerMovementLeft")))
        {
            PlayerEnhancedInputComponent->BindAction(ControllerMovementLeft, ETriggerEvent::Triggered, this, &AxREAL_VRCharacter::ControllerMovementLeft_Triggered);
            PlayerEnhancedInputComponent->BindAction(ControllerMovementLeft, ETriggerEvent::Completed, this, &AxREAL_VRCharacter::ControllerMovementLeft_Completed);
        }

        if (UInputAction* AlternateGripRight = FindFirstObjectSafe<UInputAction>(TEXT("AlternateGripRight")))
        {
            PlayerEnhancedInputComponent->BindAction(AlternateGripRight, ETriggerEvent::Started, this, &AxREAL_VRCharacter::AlternateGripRight_Started);
            PlayerEnhancedInputComponent->BindAction(AlternateGripRight, ETriggerEvent::Completed, this, &AxREAL_VRCharacter::AlternateGripRight_Completed);
        }

        if (UInputAction* AlternateGripLeft = FindFirstObjectSafe<UInputAction>(TEXT("AlternateGripLeft")))
        {
            PlayerEnhancedInputComponent->BindAction(AlternateGripLeft, ETriggerEvent::Started, this, &AxREAL_VRCharacter::AlternateGripLeft_Started);
            PlayerEnhancedInputComponent->BindAction(AlternateGripLeft, ETriggerEvent::Completed, this, &AxREAL_VRCharacter::AlternateGripLeft_Completed);
        }

        if (UInputAction* LaserBeamRight = FindFirstObjectSafe<UInputAction>(TEXT("LaserBeamRight")))
        {
            PlayerEnhancedInputComponent->BindAction(LaserBeamRight, ETriggerEvent::Started, this, &AxREAL_VRCharacter::LaserBeamRight_Started);
        }

        if (UInputAction* LaserBeamLeft = FindFirstObjectSafe<UInputAction>(TEXT("LaserBeamLeft")))
        {
            PlayerEnhancedInputComponent->BindAction(LaserBeamLeft, ETriggerEvent::Started, this, &AxREAL_VRCharacter::LaserBeamLeft_Started);
        }

        if (UInputAction* PrimaryGripRight = FindFirstObjectSafe<UInputAction>(TEXT("PrimaryGripRight")))
        {
            PlayerEnhancedInputComponent->BindAction(PrimaryGripRight, ETriggerEvent::Started, this, &AxREAL_VRCharacter::PrimaryGripRight_Started);
            PlayerEnhancedInputComponent->BindAction(PrimaryGripRight, ETriggerEvent::Completed, this, &AxREAL_VRCharacter::PrimaryGripRight_Completed);
        }
        
        if (UInputAction* PrimaryGripLeft = FindFirstObjectSafe<UInputAction>(TEXT("PrimaryGripLeft")))
        {
            PlayerEnhancedInputComponent->BindAction(PrimaryGripLeft, ETriggerEvent::Started, this, &AxREAL_VRCharacter::PrimaryGripLeft_Started);
            PlayerEnhancedInputComponent->BindAction(PrimaryGripLeft, ETriggerEvent::Completed, this, &AxREAL_VRCharacter::PrimaryGripLeft_Completed);
        }

        if (UInputAction* UseHeldObjectRight = FindFirstObjectSafe<UInputAction>(TEXT("UseHeldObjectRight")))
        {
            PlayerEnhancedInputComponent->BindAction(UseHeldObjectRight, ETriggerEvent::Started, this, &AxREAL_VRCharacter::UseHeldObjectRight_Started);
            PlayerEnhancedInputComponent->BindAction(UseHeldObjectRight, ETriggerEvent::Completed, this, &AxREAL_VRCharacter::UseHeldObjectRight_Completed);
        }

        if (UInputAction* UseHeldObjectLeft = FindFirstObjectSafe<UInputAction>(TEXT("UseHeldObjectLeft")))
        {
            PlayerEnhancedInputComponent->BindAction(UseHeldObjectLeft, ETriggerEvent::Started, this, &AxREAL_VRCharacter::UseHeldObjectLeft_Started);
            PlayerEnhancedInputComponent->BindAction(UseHeldObjectLeft, ETriggerEvent::Completed, this, &AxREAL_VRCharacter::UseHeldObjectLeft_Completed);
        }

        if (UInputAction* TeleportRight = FindFirstObjectSafe<UInputAction>(TEXT("TeleportRight")))
        {
            PlayerEnhancedInputComponent->BindAction(TeleportRight, ETriggerEvent::Started, this, &AxREAL_VRCharacter::TeleportRight_Started);
            PlayerEnhancedInputComponent->BindAction(TeleportRight, ETriggerEvent::Completed, this, &AxREAL_VRCharacter::TeleportRight_Completed);
        }

        if (UInputAction* TeleportLeft = FindFirstObjectSafe<UInputAction>(TEXT("TeleportLeft")))
        {
            PlayerEnhancedInputComponent->BindAction(TeleportLeft, ETriggerEvent::Started, this, &AxREAL_VRCharacter::TeleportLeft_Started);
            PlayerEnhancedInputComponent->BindAction(TeleportLeft, ETriggerEvent::Completed, this, &AxREAL_VRCharacter::TeleportLeft_Completed);
        }
        
        if (bUseWristMenu)
        {
			if (UInputAction* ToggleWristMenu = FindFirstObjectSafe<UInputAction>(TEXT("ToggleWristMenu")))
			{
				PlayerEnhancedInputComponent->BindAction(ToggleWristMenu, ETriggerEvent::Started, this, &AxREAL_VRCharacter::ToggleWristMenu_Started);
			}
        }
    }

    PlayerInputComponent->BindKey(EKeys::Escape, EInputEvent::IE_Pressed, this, &AxREAL_VRCharacter::QuitGame);
}

void AxREAL_VRCharacter::SetupTeleportControllers()
{
    
}

void AxREAL_VRCharacter::OnPlayerStateReplicated_Implementation(const APlayerState* NewPlayerState)
{
    OnPlayerStateReplicated_Bind.RemoveDynamic(this, &AxREAL_VRCharacter::OnPlayerStateReplicated);
    InitTeleportControllers(NewPlayerState);
}

void AxREAL_VRCharacter::Destroyed()
{
    Super::Destroyed();
    OnDestroy();
}

#pragma endregion

#pragma region Gameplay

void AxREAL_VRCharacter::Tick(float DeltaTime)
{
    if (IsLocallyControlled())
    {
        // Use for movement modes that use relative velocities
        CalculateRelativeVelocities();
        // CheckAndHandleClimbingMovement(DeltaTime); This was disconnected in the original blueprint
        CheckAndHandleGripAnimations();

        if (IsValid(TeleportComponent))
            TeleportComponent->UpdateTeleportRotations(MotionControllerThumbLeft_X_Value, MotionControllerThumbLeft_Y_Value, MotionControllerThumbRight_X_Value, MotionControllerThumbRight_Y_Value, GetVRRotation());

        SampleGripVelocities();
    }
}

#pragma endregion

#pragma region Replication and Networking

void AxREAL_VRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const 
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AxREAL_VRCharacter, LeftControllerOffset);
    DOREPLIFETIME(AxREAL_VRCharacter, RightControllerOffset);
    DOREPLIFETIME(AxREAL_VRCharacter, GraspingHandLeft);
    DOREPLIFETIME(AxREAL_VRCharacter, GraspingHandRight);
}

void AxREAL_VRCharacter::PossessedBy(AController *NewController)
{
    Super::PossessedBy(NewController);
    if (bWasAlreadyPossessed)
    {
        UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Floor);
    }
    else
    {

        // Called on client side
        OnPossessed();

        bWasAlreadyPossessed = true;
        // Servers don't use begin play for setup on the character, because it can be called before possession is done.
        CharacterSetup();
    }
}

void AxREAL_VRCharacter::OnPossessed_Implementation()
{
    
    // Delete if statement if not using a child FPS pawn, this is just here so that this logic isn't ran if we aren't in VR or a packaged build.
    if (UVRExpansionFunctionLibrary::IsInVREditorPreviewOrGame())
    {
        GetWorldTimerManager().SetTimer(GetControllerType_TimerHandle, this, &AxREAL_VRCharacter::GetControllerTypeLoop, 0.2f, true);
    }
}

void AxREAL_VRCharacter::NotifyServerOfTossRequest_Implementation(bool LeftHand, UPrimitiveComponent *TargetPrimitive)
{
    LeftHand?TeleportControllerLeft->ServersideToss(TargetPrimitive):TeleportControllerRight->ServersideToss(TargetPrimitive);
}

void AxREAL_VRCharacter::SendControllerProfileToServer_Implementation(bool IsRightHand, FTransform_NetQuantize NewTransform)
{
    if (IsRightHand)
    {
        RightControllerOffset = NewTransform;
    }
    else
    {
        LeftControllerOffset = NewTransform;
    }
}

#pragma endregion

#pragma region Input Handling

void AxREAL_VRCharacter::HandleTurnInput_Implementation(float InputAxis)
{
    if (InputAxis > 0.0f)
    {
        float InputValue = bRightHandMovement ? MotionControllerThumbLeft_X_Value : MotionControllerThumbRight_X_Value;

        bool isTurningInputGreaterThanThreshold = FMath::Abs(InputValue) > TurningActivationThreshold;
        
        //Snap Turn
        if (bTurnModeIsSnap)
        {
            if (bTurningFlag)
            {
                if (!isTurningInputGreaterThanThreshold)
                {
                    bTurningFlag = false;
                }
            }
            else
            {
                if (isTurningInputGreaterThanThreshold)
                {
                    bTurningFlag = true;
                    VRMovementReference->PerformMoveAction_SnapTurn(FMath::Sign(InputValue) * SnapTurnAngle, EVRMoveActionVelocityRetention::VRMOVEACTION_Velocity_Turn, true, true, true);
                }
            }
        }

        //Smooth Turn
        else
        {
            if (isTurningInputGreaterThanThreshold)
            {
                VRMovementReference->PerformMoveAction_SnapTurn(FMath::Sign(InputValue) * SmoothTurnSpeed * GetWorld()->GetDeltaSeconds(), EVRMoveActionVelocityRetention::VRMOVEACTION_Velocity_Turn, false, false, true);
            }
        }
    }
    else
    {
        if (CurrentControllerTypeXR == EBPOpenXRControllerDeviceType::DT_ViveController || CurrentControllerTypeXR == EBPOpenXRControllerDeviceType::DT_ViveProController)
        {
            bTurningFlag = false;
        }
    }
}

void AxREAL_VRCharacter::HandleCurrentMovementInput_Implementation(float MovementInput, UGripMotionControllerComponent *MovingHand, UGripMotionControllerComponent *OtherHand)
{
    bool isNotSeatedOrClimbing = VRMovementReference->GetReplicatedMovementMode() != EVRConjoinedMovementModes::C_VRMOVE_Seated && VRMovementReference->GetReplicatedMovementMode() != EVRConjoinedMovementModes::C_VRMOVE_Climbing;
    if (MovementInput > 0.0f && isNotSeatedOrClimbing && !DisableMovement)
    {
        switch (CurrentMovementMode)
        {
            case EVRMovementMode::DPadPress_ControllerOrient:
            case EVRMovementMode::DPadPress_HMDOrient:
            
                HandleSlidingMovement(CurrentMovementMode, MovingHand, bThumbPadEffectsSlidingDirection); //TODO

        }
    }
}

//Input Handlers

void AxREAL_VRCharacter::ToggleWristMenu_Started()
{
    SetWristMenuEnabled(!bIsWristMenuEnabled);
}

void AxREAL_VRCharacter::TeleportRight_Started()
{
    if (bRightHandMovement || bTwoHandMovement)
    {
        switch (CurrentMovementMode)
        {
        case EVRMovementMode::Teleport:
        case EVRMovementMode::Navigate:
        case EVRMovementMode::OutOfBodyNavigation:
            if (!IsHandClimbing && !bIsOutOfBody && !DisableMovement)
            {
                if (TeleportComponent)
                    TeleportComponent->TeleportRight_Started();
            }
            break; 
        
        default:
            break;
        }
    }
}

void AxREAL_VRCharacter::TeleportRight_Completed()
{
    if (bRightHandMovement || bTwoHandMovement)
    {
        
        switch (CurrentMovementMode)
        {
        case EVRMovementMode::Teleport:
        case EVRMovementMode::Navigate:
        case EVRMovementMode::OutOfBodyNavigation:
            if (!IsHandClimbing && !bIsOutOfBody && !DisableMovement)
            {

                if (TeleportComponent)
                    TeleportComponent->TeleportRight_Completed();
            }
            break; 
        
        default:
            break;
        }
    }
}

void AxREAL_VRCharacter::TeleportLeft_Started()
{
    if (!bRightHandMovement || bTwoHandMovement)
    {
        switch (CurrentMovementMode)
        {
        case EVRMovementMode::Teleport:
        case EVRMovementMode::Navigate:
        case EVRMovementMode::OutOfBodyNavigation:
            if (!IsHandClimbing && !bIsOutOfBody && !DisableMovement)
            {
                if (TeleportComponent)
                    TeleportComponent->TeleportLeft_Started();
            }
            break; 
        
        default:
            break;
        }
    }

}

void AxREAL_VRCharacter::TeleportLeft_Completed()
{
    if (!bRightHandMovement || bTwoHandMovement)
    {
        switch (CurrentMovementMode)
        {
        case EVRMovementMode::Teleport:
        case EVRMovementMode::Navigate:
        case EVRMovementMode::OutOfBodyNavigation:
            if (!IsHandClimbing && !bIsOutOfBody && !DisableMovement)
            {
                if (TeleportComponent)
                    TeleportComponent->TeleportLeft_Completed();
            }
            break; 
        
        default:
            break;
        }
    }
}

void AxREAL_VRCharacter::LaserBeamRight_Started()
{
    if (!bDisableLaserBeams)
    {
        ActivateBeam_Server(false, !TeleportControllerRight->IsLaserBeamActive);
    }
}

void AxREAL_VRCharacter::LaserBeamLeft_Started()
{
    if (!bDisableLaserBeams)
    {
        ActivateBeam_Server(true, !TeleportControllerLeft->IsLaserBeamActive);
    }
}

void AxREAL_VRCharacter::AlternateGripRight_Started()
{
    FGameplayTagContainer relevantGameplayTags;
    AddTagsToContainer(relevantGameplayTags, {"DropType.OnAltGrip", "DropType.Secondary.OnAltGrip", "GripType.OnAltGrip", "GripType.Secondary.OnAltGrip"});

    bool performedAction = GripOrDropObjectClean(RightMotionController, LeftMotionController, false, RightHandGripComponent, relevantGameplayTags);
    if  (!performedAction && bAllowCycleMovementMode)
    {
        CycleMovementModes(false);
    }
}

void AxREAL_VRCharacter::AlternateGripRight_Completed()
{
    FGameplayTagContainer relevantGameplayTags;
    AddTagsToContainer(relevantGameplayTags, {"DropType.OnAltGripRelease", "DropType.Secondary.OnAltGripRelease"});

    GripOrDropObjectClean(RightMotionController, LeftMotionController, false, RightHandGripComponent, relevantGameplayTags);
}

void AxREAL_VRCharacter::AlternateGripLeft_Started()
{
    FGameplayTagContainer relevantGameplayTags;
    AddTagsToContainer(relevantGameplayTags, {"DropType.OnAltGrip", "DropType.Secondary.OnAltGrip", "GripType.OnAltGrip", "GripType.Secondary.OnAltGrip"});

    bool performedAction = GripOrDropObjectClean(LeftMotionController, RightMotionController, false, LeftHandGripComponent, relevantGameplayTags);
    if (!performedAction && bAllowCycleMovementMode)
    {
        CycleMovementModes(true);
    }
}

void AxREAL_VRCharacter::AlternateGripLeft_Completed()
{
    FGameplayTagContainer relevantGameplayTags;
    AddTagsToContainer(relevantGameplayTags, {"DropType.OnAltGripRelease", "DropType.Secondary.OnAltGripRelease"});

    GripOrDropObjectClean(LeftMotionController, RightMotionController, false, LeftHandGripComponent, relevantGameplayTags);
}

void AxREAL_VRCharacter::PrimaryGripRight_Started()
{
    TriggerGripOrDrop(RightMotionController, LeftMotionController, true, RightHandGripComponent);
}

void AxREAL_VRCharacter::PrimaryGripRight_Completed()
{
    TriggerGripOrDrop(RightMotionController, LeftMotionController, false, RightHandGripComponent);
}

void AxREAL_VRCharacter::PrimaryGripLeft_Started()
{
    TriggerGripOrDrop(LeftMotionController, RightMotionController, true, LeftHandGripComponent);
}

void AxREAL_VRCharacter::PrimaryGripLeft_Completed()
{
    TriggerGripOrDrop(LeftMotionController, RightMotionController, false, LeftHandGripComponent);
}

void AxREAL_VRCharacter::UseHeldObjectRight_Started()
{
    if (!IfOverWidgetUse(RightMotionController, true))
    {
        CheckUseHeldItems(RightMotionController, true);
        bool droppedOrUsedSecondary, hadSecondary;
        CheckUseSecondaryAttachment(RightMotionController, LeftMotionController, true, droppedOrUsedSecondary, hadSecondary);
    }
}

void AxREAL_VRCharacter::UseHeldObjectRight_Completed()
{
    if (!IfOverWidgetUse(RightMotionController, false))
    {
        CheckUseHeldItems(RightMotionController, false);
        bool droppedOrUsedSecondary, hadSecondary;
        CheckUseSecondaryAttachment(RightMotionController, LeftMotionController, false, droppedOrUsedSecondary, hadSecondary);
    }
}

void AxREAL_VRCharacter::UseHeldObjectLeft_Started()
{
    if (!IfOverWidgetUse(LeftMotionController, true))
    {
        CheckUseHeldItems(LeftMotionController, true);
        bool droppedOrUsedSecondary, hadSecondary;
        CheckUseSecondaryAttachment(LeftMotionController, RightMotionController, true, droppedOrUsedSecondary, hadSecondary);
    }
}

void AxREAL_VRCharacter::UseHeldObjectLeft_Completed()
{
    if (!IfOverWidgetUse(LeftMotionController, false))
    {
        CheckUseHeldItems(LeftMotionController, false);
        bool droppedOrUsedSecondary, hadSecondary;
        CheckUseSecondaryAttachment(LeftMotionController, RightMotionController, false, droppedOrUsedSecondary, hadSecondary);
    }
}

void AxREAL_VRCharacter::ControllerMovementRight_Triggered(const FInputActionValue& Value)
{
    if  (bRightHandMovement || bTwoHandMovement)
    {
        HandleCurrentMovementInput(Value.Get<float>(), RightMotionController, LeftMotionController);
    }
    else
    {
        HandleTurnInput(Value.Get<float>());
    }
}

// TODO: This is the same as triggered event, can we remove it?
void AxREAL_VRCharacter::ControllerMovementRight_Completed(const FInputActionValue& Value)
{
    if (bRightHandMovement || bTwoHandMovement)
    {
        HandleCurrentMovementInput(Value.Get<float>(), RightMotionController, LeftMotionController);
    }
    else
    {
        HandleTurnInput(Value.Get<float>());
    }
}

void AxREAL_VRCharacter::ControllerMovementLeft_Triggered(const FInputActionValue& Value)
{
    if (!bRightHandMovement || bTwoHandMovement)
    {
        HandleCurrentMovementInput(Value.Get<float>(), LeftMotionController, RightMotionController);
    }
    else
    {
        HandleTurnInput(Value.Get<float>());
    }
}

void AxREAL_VRCharacter::ControllerMovementLeft_Completed(const FInputActionValue& Value)
{
    if (!bRightHandMovement || bTwoHandMovement)
    {
        HandleCurrentMovementInput(Value.Get<float>(), LeftMotionController, RightMotionController);
    }
    else
    {
        HandleTurnInput(Value.Get<float>());
    }
}

void AxREAL_VRCharacter::MotionControllerThumbLeft_X_Handler(const FInputActionValue& Value)
{
    MotionControllerThumbLeft_X_Value = Value.Get<float>();
}

void AxREAL_VRCharacter::MotionControllerThumbRight_X_Handler(const FInputActionValue& Value)
{
    MotionControllerThumbRight_X_Value = Value.Get<float>();
}

void AxREAL_VRCharacter::MotionControllerThumbLeft_Y_Handler(const FInputActionValue& Value)
{
    MotionControllerThumbLeft_Y_Value = Value.Get<float>();
}

void AxREAL_VRCharacter::MotionControllerThumbRight_Y_Handler(const FInputActionValue& Value)
{
    MotionControllerThumbRight_Y_Value = Value.Get<float>();
}

#pragma endregion

#pragma region Utility and Helper Functions

void AxREAL_VRCharacter::SetControllerText(bool Left, FString &Text)
{
    if (Left && IsValid(TextL))
    {
        TextL->SetText(FText::FromString(Text));
    }
    else if (!Left && IsValid(TextR))
    {
        TextR->SetText(FText::FromString(Text));
    }
}

void AxREAL_VRCharacter::MapInput()
{
    // Get Player Controller
    APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = playerController->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    FModifyContextOptions inputOptions = FModifyContextOptions();
    inputOptions.bForceImmediately = true;
    inputOptions.bIgnoreAllPressedKeysUntilRelease = true;
    InputSubsystem->AddPlayerMappableConfig(InputConfig, inputOptions);
}

void AxREAL_VRCharacter::CalculateRelativeVelocities()
{
    if (CurrentMovementMode >= EVRMovementMode::RunInPlace)
    {
        //Smooths over VelCalculationsSmoothingSteps
        GetSmoothedVelocityOfObject(LeftMotionController->GetRelativeLocation(), LastLContPos, LeftControllerRelativeVel, LowEndLContRelativeVel, true);
        GetSmoothedVelocityOfObject(RightMotionController->GetRelativeLocation(), LastRContPos, RightControllerRelativeVel, LowEndRContRelativeVel, true);
        GetSmoothedVelocityOfObject(VRReplicatedCamera->GetRelativeLocation(), LastHMDPos, HeadRelativeVel, LowEndHeadRelativeVel, true);
    }
}

void AxREAL_VRCharacter::GetSmoothedVelocityOfObject(FVector CurRelLocation, UPARAM(ref) FVector &LastRelLocation, UPARAM(ref) FVector &RelativeVelocityOut, UPARAM(ref) FVector &LowEndRelativeVelocityOut, bool bRollingAverage)
{
    FVector tempVel;
    FVector relativeLocationDifference = CurRelLocation - LastRelLocation;
    FVector ABSVector = FVector(FMath::Abs(relativeLocationDifference.X), FMath::Abs(relativeLocationDifference.Y), FMath::Abs(relativeLocationDifference.Z));
    if (bRollingAverage)
    {
        // New rolling average low pass
        UVRExpansionFunctionLibrary::LowPassFilter_RollingAverage(RelativeVelocityOut, ABSVector, RelativeVelocityOut, RIPMotionSmoothingSteps);
        // Low end out
        UVRExpansionFunctionLibrary::LowPassFilter_RollingAverage(RelativeVelocityOut, ABSVector, LowEndRelativeVelocityOut, RipMotionLowPassSmoothingSteps);
    }
    else
    {
        // New exponential low pass
        UVRExpansionFunctionLibrary::LowPassFilter_Exponential(RelativeVelocityOut, ABSVector, RelativeVelocityOut, 0.3f);
    }
    // Set Last rel loc
    LastRelLocation = CurRelLocation;
}

#pragma endregion


void AxREAL_VRCharacter::QuitGame()
{
    if (APlayerController* pc = Cast<APlayerController>(GetController()))
    {
        // TODO: Destroy online session, should be done in game instance utilizing the AdvancedSessions plugin.

        UKismetSystemLibrary::QuitGame(GetWorld(), pc, EQuitPreference::Quit, false);
    }
}

void AxREAL_VRCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    GetWorld()->GetTimerManager().ClearAllTimersForObject(this);

    RightMotionController->OnGrippedObject.RemoveDynamic(this, &AxREAL_VRCharacter::OnRightMotionControllerGripped);
    LeftMotionController->OnGrippedObject.RemoveDynamic(this, &AxREAL_VRCharacter::OnLeftMotionControllerGripped);
    OnPlayerStateReplicated_Bind.RemoveDynamic(this, &AxREAL_VRCharacter::OnPlayerStateReplicated);
}

void AxREAL_VRCharacter::GetControllerTypeLoop()
{
    EBPOpenXRControllerDeviceType deviceType;
    FString deviceTypeString;
    EBPXRResultSwitch result;
    UOpenXRExpansionFunctionLibrary::GetXRMotionControllerType(deviceTypeString, deviceType, result);

    if (result == EBPXRResultSwitch::OnSucceeded)
    {
        CurrentControllerTypeXR = deviceType;
        SetControllerProfile(deviceType);
        GetWorldTimerManager().ClearTimer(GetControllerType_TimerHandle);
    }
    else
    {
        // Print out no profile found
        UE_LOG(LogTemp, Warning, TEXT("No controller profile found, checking again..."));
    }
}

void AxREAL_VRCharacter::SetControllerProfile(EBPOpenXRControllerDeviceType ControllerType)
{
    // Controller profiles are no longer mandatory with OpenXR, however they still allow you to let users change their hand poses in custom ways, so I left them in.
    // But I emptied the values.
    switch (ControllerType)
    {
        case EBPOpenXRControllerDeviceType::DT_OculusTouchController:
            UVRGlobalSettings::LoadControllerProfileByName("Oculus_Touch", true);
            break;
        case EBPOpenXRControllerDeviceType::DT_ValveIndexController:
            UVRGlobalSettings::LoadControllerProfileByName("KnucklesEV2", true);
            break;
        case EBPOpenXRControllerDeviceType::DT_ViveController:
            UVRGlobalSettings::LoadControllerProfileByName("Vive_Wands", true);
            break;
        case EBPOpenXRControllerDeviceType::DT_MicrosoftMotionController:
            UVRGlobalSettings::LoadControllerProfileByName("Windows_MR", true);
            break;
        default:
            break;
    }
}

void AxREAL_VRCharacter::CharacterSetup_Implementation()
{
    SetupMotionControllers();

	if (IsLocallyControlled())
	{
		if (bUseWristMenu)
		{
			SpawnWristMenu();
		}
    }

    RightMotionController->OnGrippedObject.AddDynamic(this, &AxREAL_VRCharacter::OnRightMotionControllerGripped);
    LeftMotionController->OnGrippedObject.AddDynamic(this, &AxREAL_VRCharacter::OnLeftMotionControllerGripped);

}

void AxREAL_VRCharacter::SetupMotionControllers_Implementation()
{
    SetGripComponents(GrabSphereLeft, GrabSphereRight);

    if (IsLocallyControlled())
    {

        // Don't think this works on VR headset -> might need to use a different fade method
        UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0)->StartCameraFade(1.0f, 0.0f, 1.0f, FLinearColor::Black, false, false);

        MapInput();
        UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Floor);
        UHeadMountedDisplayFunctionLibrary::SetSpectatorScreenMode(ESpectatorScreenMode::SingleEyeCroppedToFill);

        // Set text above controllers to show movement mode
        SetMovementHands(bRightHandMovement);
    }

    // Spawn and attach both teleport controllers to the capsule

    FActorSpawnParameters spawnParams = FActorSpawnParameters();
    spawnParams.Owner = this;
    spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    spawnParams.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
    TeleportControllerLeft = GetWorld()->SpawnActor<ATeleportController>(ATeleportController::StaticClass(), FTransform::Identity, spawnParams);
    TeleportControllerLeft->bIsLocal = IsLocallyControlled();
    TeleportControllerLeft->OwningMotionController = LeftMotionController;
    TeleportControllerLeft->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);

    TeleportControllerRight = GetWorld()->SpawnActor<ATeleportController>(ATeleportController::StaticClass(), FTransform::Identity, spawnParams);
    TeleportControllerRight->bIsLocal = IsLocallyControlled();
    TeleportControllerRight->OwningMotionController = RightMotionController;
    TeleportControllerRight->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);

    // Pass the teleport controllers to the teleport component
    if (TeleportComponent)
        TeleportComponent->SetTeleportControllers(TeleportControllerLeft, TeleportControllerRight);

    LoopTryInitTeleportControllers();

    if (SpawnGraspingHands)
        CheckSpawnGraspingHands();
    else
        ClearGraspingHands();
}

bool AxREAL_VRCharacter::IsALocalGrip(EGripMovementReplicationSettings GripRepType)
{
    if (GripRepType == EGripMovementReplicationSettings::ClientSide_Authoritive || GripRepType == EGripMovementReplicationSettings::ClientSide_Authoritive_NoRep)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void AxREAL_VRCharacter::TryToGrabObject(UObject *ObjectToTryToGrab, FTransform WorldTransform, UGripMotionControllerComponent *Hand, UGripMotionControllerComponent *OtherHand, bool IsSlotGrip, FGameplayTag GripSecondaryTag, FName GripBoneName, FName SlotName, bool IsSecondaryGrip, bool &Gripped)
{
    // Set this variable for reference later
    bool implementsInterface = ObjectToTryToGrab->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass());

    // Is the calling hand already holding this object?
    if (Hand->GetIsObjectHeld(ObjectToTryToGrab))
    {
        Gripped = false;
        return;
    }
    else
    {
        // Does the other hand hold this already?
        if (OtherHand->GetIsObjectHeld(ObjectToTryToGrab))
        {
            // If we do not allow multiple grips, check for hand swap or secondary grip
            if (!implementsInterface || !IVRGripInterface::Execute_AllowsMultipleGrips(ObjectToTryToGrab))
            {
                if (IsSecondaryGrip || !IsSlotGrip)
                {
                    // Check for secondary attachment
                    bool isSecondaryGripped;
                    TryToSecondaryGripObject(Hand, OtherHand, ObjectToTryToGrab, GripSecondaryTag, implementsInterface, WorldTransform, SlotName, IsSlotGrip, isSecondaryGripped);
                    if (isSecondaryGripped)
                    {
                        Gripped = true;
                        return;
                    }
                }
                // Drop if other hand holds, then continue below to pick up the object
                OtherHand->DropObject(ObjectToTryToGrab, 0);
            }

        }
        else
        {
            // If client is trying to secondary grip, don't allow trying to full grip
            if (IsSecondaryGrip)
            {
                Gripped = false;
                return;
            }
            else
            {
                if (ObjectToTryToGrab->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
                {
                    TArray<FBPGripPair> holdingControllers;
                    bool isHeld;
                    // See if anyone else is holding it, already check our specific hands
                    IVRGripInterface::Execute_IsHeld(ObjectToTryToGrab, holdingControllers, isHeld);
                    // If held and already failed previous two held checks, we are not the ones holding it, don't grip it
                    if (isHeld)
                    {
                        if (!IVRGripInterface::Execute_AllowsMultipleGrips(ObjectToTryToGrab))
                        {
                            Gripped = false;
                            return;
                        }
                    }
                }
                
            }
        }

        // Pick up the object

        if (implementsInterface)
        {
            Gripped = Hand->GripObjectByInterface(ObjectToTryToGrab, WorldTransform, true, GripBoneName, SlotName, IsSlotGrip);
        }
        else
        {
            Gripped = Hand->GripObject(ObjectToTryToGrab, WorldTransform, true, SlotName, GripBoneName, EGripCollisionType::InteractiveCollisionWithPhysics,
            EGripLateUpdateSettings::NotWhenCollidingOrDoubleGripping, EGripMovementReplicationSettings::ForceClientSideMovement, 2250.0f, 140.0f, IsSlotGrip);
        }
    }
}

void AxREAL_VRCharacter::GetNearestOverlappingObject(UPrimitiveComponent *OverlapComponent, UGripMotionControllerComponent *Hand, FGameplayTagContainer RelevantGameplayTags, UObject *&NearestObject, bool &ImplementsInterface, FTransform &ObjectTransform, bool &CanBeClimbed, FName &BoneName, FVector &ImpactLoc)
{
    float nearestOverlap = 10000000.0f;
    UObject* nearestOverlappingObject = nullptr;
    bool implementsVRGrip = false;
    FTransform worldTransform;
    UPrimitiveComponent* hitComponent = nullptr;
    uint8 lastGripPrio = 0;
    FName nearestBoneName;
    FVector impactPoint;
    bool fallBackOnOverlap = false;

    if (!IsValid(OverlapComponent))
    {
        NearestObject = nullptr;
        return;
    }
    // Draw sphere for debug
    //DrawDebugSphere(GetWorld(), OverlapComponent->GetComponentLocation(), OverlapComponent->Bounds.SphereRadius, 32, FColor::Green, false, 1.0f);

    if (!bForceOverlapOnlyGripChecks)
    {
        TArray<FHitResult> outHits;
        bool bHasHits = PerformTraceForObjects(OverlapComponent, Hand, outHits);
        if (bHasHits)
        {
            // Can we grip one of the components, and filter by priority
            bool shouldGrip, objectImplementsInterface;
            UObject* objectToGrip = nullptr;
            FTransform objectToGripWorldTransform;
            UPrimitiveComponent* firstPrimitiveHit = nullptr;
            FName objectToGripBoneName;
            FVector objectToGripImpactPoint;
            SelectObjectFromHitArray(outHits, RelevantGameplayTags, Hand, shouldGrip, objectImplementsInterface, objectToGrip, objectToGripWorldTransform, firstPrimitiveHit, objectToGripBoneName, objectToGripImpactPoint);
            if (shouldGrip)
            {
                nearestOverlappingObject = objectToGrip;
                implementsVRGrip = objectImplementsInterface;
                worldTransform = objectToGripWorldTransform;
                impactPoint = objectToGripImpactPoint;
                //TODO: Check if this works
                IGameplayTagAssetInterface* gameplayTagAsset = Cast<IGameplayTagAssetInterface>(nearestOverlappingObject);
                if (gameplayTagAsset)
                {
                    bool hasTag = gameplayTagAsset->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("Interactible.PerBoneGripping")));
                    if (hasTag)
                    {
                        nearestBoneName = objectToGripBoneName;
                    }
                    else
                    {
                        nearestBoneName = NAME_None;
                    }
                }
            }
            else
            {
                hitComponent = firstPrimitiveHit;
            }
        }
        else
        {
            fallBackOnOverlap = true;
        }
    }
    else
    {
        fallBackOnOverlap = true;
    }
    if (fallBackOnOverlap) 
    {
        FTransform overlapTransform = OverlapComponent->GetComponentTransform();
        TArray<AActor*> actorsToIgnore = {this};
        TArray<UPrimitiveComponent*> overlappingComponents;
        // Fall back on overlap, being inside of an object can cause this.
        bool bHasOverlaps = UKismetSystemLibrary::ComponentOverlapComponents(OverlapComponent, overlapTransform, CollisionToCheckDuringGrip, nullptr, actorsToIgnore, overlappingComponents);
        if (bHasOverlaps)
        {
            for (int i = 0; i < overlappingComponents.Num(); i++)
            {
                if (i == 0)
                {
                    hitComponent = overlappingComponents[i];
                }
                if (HasValidGripCollision(overlappingComponents[i]))
                {
                    UObject* objectToGrip;
                    bool objectImplementsInterface;
                    FTransform objectsWorldTransform;
                    uint8 gripPrio;
                    if (ShouldGripComponent(overlappingComponents[i], lastGripPrio, i > 0, NAME_None, RelevantGameplayTags, Hand, objectToGrip, objectImplementsInterface, objectsWorldTransform, gripPrio))
                    {
                        nearestOverlappingObject = objectToGrip;
                        implementsVRGrip = objectImplementsInterface;
                        worldTransform = objectsWorldTransform;
                        impactPoint = OverlapComponent->GetComponentLocation();
                        lastGripPrio = gripPrio;
                    }
                }
            }
        }
    }
    if (!IsValid(nearestOverlappingObject))
    {
        // Return climbable if not simulating and not grippable
        
        if (IsClimbingModeEnabled(Hand) && CanObjectBeClimbed(hitComponent))
        {
            CanBeClimbed = true;
            NearestObject = hitComponent;
            return;
        }
    }

    NearestObject = nearestOverlappingObject;
    ImplementsInterface = implementsVRGrip;
    ObjectTransform = worldTransform;
    CanBeClimbed = false;
    BoneName = nearestBoneName;
    ImpactLoc = impactPoint;
}

bool AxREAL_VRCharacter::IsClimbingModeEnabled(UGripMotionControllerComponent *Hand)
{
    EControllerHand handType;
    Hand->GetHandType(handType);
    return ((handType == EControllerHand::Left && (CurrentMovementMode == EVRMovementMode::ClimbingMode || AlwaysAllowClimbing)) ||
            (handType == EControllerHand::Right && (MovementModeRight == EVRMovementMode::ClimbingMode || AlwaysAllowClimbing)));
}

bool AxREAL_VRCharacter::PerformTraceForObjects(UPrimitiveComponent* OverlapComponent, UGripMotionControllerComponent* Hand, TArray<FHitResult>& OutHits)
{
    // Try to Trace for the object first - Preferred
    FBoxSphereBounds overlapComponentBounds = OverlapComponent->Bounds;
    FVector overlapComponentRotationVector = OverlapComponent->GetComponentRotation().Vector();
    FVector gripVector = overlapComponentRotationVector * GripTraceLength;
    FVector startTracePoint = overlapComponentBounds.Origin - gripVector;
    FVector endTracePoint = overlapComponentBounds.Origin + gripVector;

    float radius = overlapComponentBounds.SphereRadius;
    TArray<AActor*> actorsToIgnore;
    Hand->GetGrippedActors(actorsToIgnore);

    FCollisionQueryParams queryParams = FCollisionQueryParams(FName(TEXT("GripOverlap")), true);
    queryParams.AddIgnoredActors(actorsToIgnore);
    ECollisionChannel vrTraceChannel = ECollisionChannel::ECC_GameTraceChannel1; //VR Trace Channel
    return GetWorld()->SweepMultiByChannel(OutHits, startTracePoint, endTracePoint, FQuat::Identity, vrTraceChannel, \
    FCollisionShape::MakeSphere(radius), queryParams);
}

void AxREAL_VRCharacter::GetDPadMovementFacing(EVRMovementMode MovementMode, UGripMotionControllerComponent *Hand, UGripMotionControllerComponent *OtherHand, FVector &ForwardVector, FVector &RightVector)
{
    USceneComponent* handAimComp;
    USceneComponent* otherHandAimComp;
    switch (MovementMode)
    {
        case EVRMovementMode::Teleport:
            ForwardVector = FVector::ZeroVector;
            RightVector = FVector::ZeroVector;
            break;

        case EVRMovementMode::Navigate:
            ForwardVector = FVector::ZeroVector;
            RightVector = FVector::ZeroVector;
            break;

        case EVRMovementMode::DPadPress_ControllerOrient:
            handAimComp = GetCorrectAimComp(Hand);
            if (handAimComp)
            {
                ForwardVector = FVector::VectorPlaneProject(handAimComp->GetForwardVector(), GetVRUpVector()).GetSafeNormal(0.0001f);
                RightVector = FVector::VectorPlaneProject(handAimComp->GetRightVector(), GetVRUpVector()).GetSafeNormal(0.0001f);
            }
            break;
        
        case EVRMovementMode::DPadPress_HMDOrient:
            ForwardVector = GetVRForwardVector();
            RightVector = GetVRRightVector();
            break;

        case EVRMovementMode::RunInPlace:
            otherHandAimComp = GetCorrectAimComp(OtherHand);
            handAimComp = GetCorrectAimComp(Hand);
            if (otherHandAimComp && handAimComp)
            {
                FVector otherHandForwardVector = FVector::VectorPlaneProject(otherHandAimComp->GetForwardVector(), GetVRUpVector()).GetSafeNormal(0.0001f);
                FVector handForwardVector = FVector::VectorPlaneProject(handAimComp->GetForwardVector(), GetVRUpVector()).GetSafeNormal(0.0001f);

                ForwardVector = (otherHandForwardVector + handForwardVector) / 2.0f;

                FVector otherHandRightVector = FVector::VectorPlaneProject(otherHandAimComp->GetRightVector(), GetVRUpVector()).GetSafeNormal(0.0001f);
                FVector handRightVector = FVector::VectorPlaneProject(handAimComp->GetRightVector(), GetVRUpVector()).GetSafeNormal(0.0001f);

                RightVector = (otherHandRightVector + handRightVector) / 2.0f;
            }
            break;

        case EVRMovementMode::RunInPlaceHeadForward:
            ForwardVector = GetVRForwardVector();
            RightVector = GetVRRightVector();
            break;
        
        case EVRMovementMode::ArmSwing:
            otherHandAimComp = GetCorrectAimComp(OtherHand);
            handAimComp = GetCorrectAimComp(Hand);
            if (otherHandAimComp && handAimComp)
            {
                FVector otherHandForwardVector = FVector::VectorPlaneProject(otherHandAimComp->GetForwardVector(), GetVRUpVector()).GetSafeNormal(0.0001f);
                FVector handForwardVector = FVector::VectorPlaneProject(handAimComp->GetForwardVector(), GetVRUpVector()).GetSafeNormal(0.0001f);

                ForwardVector = (otherHandForwardVector + handForwardVector) / 2.0f;

                FVector otherHandRightVector = FVector::VectorPlaneProject(otherHandAimComp->GetRightVector(), GetVRUpVector()).GetSafeNormal(0.0001f);
                FVector handRightVector = FVector::VectorPlaneProject(handAimComp->GetRightVector(), GetVRUpVector()).GetSafeNormal(0.0001f);

                RightVector = (otherHandRightVector + handRightVector) / 2.0f;
            }
            break;

        case EVRMovementMode::ClimbingMode:
            ForwardVector = FVector::ZeroVector;
            RightVector = FVector::ZeroVector;
            break;
        
        default:
            break;
    }
}

bool AxREAL_VRCharacter::GripOrDropObject(UGripMotionControllerComponent *CallingMotionController, UGripMotionControllerComponent *OtherController, bool CanCheckClimb, UPrimitiveComponent *GrabSphere, FGameplayTag GripTag, FGameplayTag DropTag, FGameplayTag UseTag, FGameplayTag EndUseTag, FGameplayTag GripSecondaryTag, FGameplayTag DropSecondaryTag)
{
    TArray<FGameplayTag> relevantTags = {GripTag, DropTag, UseTag, EndUseTag, GripSecondaryTag, DropSecondaryTag};
    FGameplayTagContainer relevantGameplayTagContainer = FGameplayTagContainer::CreateFromArray(relevantTags);
    return GripOrDropObjectClean(CallingMotionController, OtherController, CanCheckClimb, GrabSphere, relevantGameplayTagContainer);
}

void AxREAL_VRCharacter::OnClimbingSteppedUp_Implementation()
{
    if (IsLocallyControlled())
    {
        ClearClimbing(true);
        if (TeleportComponent)
        {
            APlayerCameraManager* cameraManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
            if (cameraManager)
            {
                cameraManager->StartCameraFade(1.0f, 0.0f, 0.75f, FLinearColor::Black);
            }
        }
    }
}

void AxREAL_VRCharacter::CheckAndHandleClimbingMovement(double DeltaTime)
{
    if (IsLocallyControlled() && IsHandClimbing && IsValid(ClimbingHand))
    {
        // Object Relative Climbing
        if (IsObjectRelative)
        {
            if (IsValid(GrippedObject))
            {
                FVector movement = UKismetMathLibrary::NegateVector(ClimbingHand->GetPivotLocation() - UKismetMathLibrary::TransformLocation(GrippedObject->GetComponentTransform(), ClimbGripLocation.GetLocation()));
                VRMovementReference->AddCustomReplicatedMovement(movement);
            }
        }
        // World Position Climbing
        else
        {
            FVector movement = UKismetMathLibrary::NegateVector(ClimbingHand->GetPivotLocation() - ClimbGripLocation.GetLocation());
            VRMovementReference->AddCustomReplicatedMovement(movement);
        }
    }
}

void AxREAL_VRCharacter::ClearClimbing(bool BecauseOfStepUp)
{
    if (IsHandClimbing && IsLocallyControlled())
    {
        if (!BecauseOfStepUp)
        {
            VRMovementReference->SetClimbingMode(false);
        }

        IsHandClimbing = false;
        ClimbingHand = nullptr;
        ClimbGripLocation = FTransform::Identity;
        IsObjectRelative = false;
        GrippedObject = nullptr;
        OnClimbingEnded.Broadcast();
    }
}

void AxREAL_VRCharacter::InitClimbing(UGripMotionControllerComponent *Hand, UObject *Object, bool _IsObjectRelative)
{
    if (IsHandClimbing && ClimbingHand != Hand)
    {
        OnClimbingEnded.Broadcast();
    }
    UPrimitiveComponent* objectAsPrimitive = Cast<UPrimitiveComponent>(Object);
    if (objectAsPrimitive)
    {
        FTransform objectTransform = objectAsPrimitive->GetComponentTransform();
        ClimbGripLocation = UKismetMathLibrary::MakeRelativeTransform(Hand->GetPivotTransform(), objectTransform);
        GrippedObject = objectAsPrimitive;
        IsObjectRelative = _IsObjectRelative;
    }
    else
    {
        ClimbGripLocation = Hand->GetPivotTransform();
        IsObjectRelative = false;
    }
    IsHandClimbing = true;
    ClimbingHand = Hand;
    VRMovementReference->SetClimbingMode(true);
    OnClimbingInitiated.Broadcast(ClimbingHand, GrippedObject, ClimbGripLocation);
}

void AxREAL_VRCharacter::UpdateClimbingMovement_Implementation(float DeltaTime)
{
    Super::UpdateClimbingMovement_Implementation(DeltaTime);
    CheckAndHandleClimbingMovement(DeltaTime);
}

void AxREAL_VRCharacter::CheckAndHandleGripAnimations()
{
    // Left Hand
    if (IsValid(LeftHandGripComponent) && !HandMesh_Left->bOwnerNoSee)
    {
        if (LeftMotionController->HasGrippedObjects())
        {
            HandStateLeft = EGripState::Grab;
        }
        else
        {
            // Ensure something was found
            if (GetNearestOverlapOfHand(LeftMotionController, LeftHandGripComponent) != nullptr)
            {
                HandStateLeft = EGripState::CanGrab;
            }
            else
            {
                HandStateLeft = EGripState::Open;
            }
        }
        //HandMesh_Left->GetAnimInstance()
    }
    if (IsValid(RightHandGripComponent) && !HandMesh_Right->bOwnerNoSee)
    {
        if (RightMotionController->HasGrippedObjects())
        {
            HandStateRight = EGripState::Grab;
        }
        else
        {
            // Ensure something was found
            if (GetNearestOverlapOfHand(RightMotionController, RightHandGripComponent) != nullptr)
            {
                HandStateRight = EGripState::CanGrab;
            }
            else
            {
                HandStateRight = EGripState::Open;
            }
        }
    }
}

UObject* AxREAL_VRCharacter::GetNearestOverlapOfHand(UGripMotionControllerComponent *Hand, UPrimitiveComponent *OverlapSphere)
{
    TArray<UPrimitiveComponent*> overlappingComponents;
    OverlapSphere->GetOverlappingComponents(overlappingComponents);

    float nearestOverlap = 1000000.0f;
    UObject* nearestOverlapObject = nullptr;

    for (UPrimitiveComponent* component : overlappingComponents)
    {
        float distanceSquared = (component->GetComponentLocation() - OverlapSphere->GetComponentLocation()).SizeSquared();
        if (distanceSquared < nearestOverlap)
        {
            nearestOverlapObject = component;
            nearestOverlap = distanceSquared; // For some reason this was set to distance not squared in the Blueprint
            //nearestOverlap = (component->GetComponentLocation() - OverlapSphere->GetComponentLocation()).Size();
        }
        /*
        else
        {
            return nearestOverlapObject;
        }*/
    }
    return nearestOverlapObject;
}

void AxREAL_VRCharacter::HandleRunInPlace(FVector ForwardVector, bool IncludeHands)
{
    float headRelativeVelocity = GetRelativeVelocityForLocomotion(true, bIsArmSwingZBased, HeadRelativeVel);
    float leftControllerRelativeVelocity = GetRelativeVelocityForLocomotion(true, bIsArmSwingZBased, LeftControllerRelativeVel);
    float rightControllerRelativeVelocity = GetRelativeVelocityForLocomotion(true, bIsArmSwingZBased, RightControllerRelativeVel);
    float averageVelocity = (headRelativeVelocity + leftControllerRelativeVelocity + rightControllerRelativeVelocity) / 3.0f;
    // Used to set a min scaler to cutoff earlier on stop
    float lowEndHeadRelativeVelocity = GetRelativeVelocityForLocomotion(true, bIsArmSwingZBased, LowEndHeadRelativeVel);
    if (IncludeHands)
    {
        if (averageVelocity >= MinimumRIPVelocity && lowEndHeadRelativeVelocity >= MinimumLowEndRipVelocity)
        {
            AddMovementInput(ForwardVector, UKismetMathLibrary::Clamp(RunningInPlaceScaler * averageVelocity, 0.0f, 1.0f), false);
        }
    }
    else
    {
        // Not including hands
        if (headRelativeVelocity >= MinimumRIPVelocity && lowEndHeadRelativeVelocity >= MinimumLowEndRipVelocity)
        {
            AddMovementInput(ForwardVector, UKismetMathLibrary::Clamp(RunningInPlaceScaler * headRelativeVelocity, 0.0f, 1.0f), false);
        }
    }
}

float AxREAL_VRCharacter::GetRelativeVelocityForLocomotion(bool IsHMD, bool IsMotionZVelBased, FVector VeloctyVector)
{
    // HMD is always Z based
    if (IsHMD || IsMotionZVelBased)
    {
        return VeloctyVector.Z;
    }
    else
    {
        return VeloctyVector.Size();
    }
}

void AxREAL_VRCharacter::CallCorrectGrabEvent(UObject *ObjectToGrip, EControllerHand Hand, bool IsSlotGrip, FTransform GripTransform, FGameplayTag GripSecondaryTag, FName OptionalBoneName, FName SlotName, bool IsSecondaryGrip)
{
    if (ObjectToGrip->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
    {
        if (IsALocalGrip(IVRGripInterface::Execute_GripMovementReplicationType(ObjectToGrip)))
        {
            TryGrabClient(ObjectToGrip, IsSlotGrip, GripTransform, Hand, GripSecondaryTag, OptionalBoneName, SlotName, IsSecondaryGrip);
        }
        else
        {
            TryGrabServer(ObjectToGrip, IsSlotGrip, GripTransform, Hand, GripSecondaryTag, OptionalBoneName, SlotName, IsSecondaryGrip);
        }
    }
}

void AxREAL_VRCharacter::TryGrabClient(UObject* ObjectToGrab, bool IsSlotGrip, FTransform_NetQuantize GripTransform, EControllerHand Hand, FGameplayTag GripSecondaryTag, FName GripBoneName, FName SlotName, bool IsSecondaryGrip)
{
    bool isGrabbed;
    switch (Hand)
    {
        case EControllerHand::Left:
            TryToGrabObject(ObjectToGrab, GripTransform, LeftMotionController, RightMotionController, IsSlotGrip, GripSecondaryTag, GripBoneName, SlotName, IsSecondaryGrip, isGrabbed);
            break;

        case EControllerHand::Right:
            TryToGrabObject(ObjectToGrab, GripTransform, RightMotionController, LeftMotionController, IsSlotGrip, GripSecondaryTag, GripBoneName, SlotName, IsSecondaryGrip, isGrabbed);
            break;
    }
}

void AxREAL_VRCharacter::TryGrabServer_Implementation(UObject *ObjectToGrab, bool IsSlotGrip, FTransform_NetQuantize GripTransform, EControllerHand Hand, FGameplayTag GripSecondaryTag, FName GripBoneName, FName SlotName, bool IsSecondaryGrip)
{
    bool isGrabbed;
    switch (Hand)
    {
        case EControllerHand::Left:
            TryToGrabObject(ObjectToGrab, GripTransform, LeftMotionController, RightMotionController, IsSlotGrip, GripSecondaryTag, GripBoneName, SlotName, IsSecondaryGrip, isGrabbed);
            break;

        case EControllerHand::Right:
            TryToGrabObject(ObjectToGrab, GripTransform, RightMotionController, LeftMotionController, IsSlotGrip, GripSecondaryTag, GripBoneName, SlotName, IsSecondaryGrip, isGrabbed);
            break;
    }
}

void AxREAL_VRCharacter::CallCorrectDropSingleEvent(UGripMotionControllerComponent *Hand, FBPActorGripInformation Grip)
{
    FVector angularVelocity, objectsLinearVelocity;
    Hand->GetPhysicsVelocities(Grip, angularVelocity, objectsLinearVelocity);
    FVector throwingAngularVelocity, throwingLinearVelocity;
    GetThrowingVelocity(Hand, Grip, angularVelocity, objectsLinearVelocity, throwingAngularVelocity, throwingLinearVelocity);

    if (Grip.GrippedObject->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
    {
        if (IsALocalGrip(IVRGripInterface::Execute_GripMovementReplicationType(Grip.GrippedObject)))
        {
            // Try Drop Single Client
            TryDropSingle_Client(Hand, Grip, throwingAngularVelocity, throwingLinearVelocity);
            return;
        }
    }

    // Try Drop Single Server
    TryDropSingle_Server(Hand, throwingAngularVelocity, throwingLinearVelocity, Grip.GripID);
}

void AxREAL_VRCharacter::TryDropSingle_Server_Implementation(UGripMotionControllerComponent* Hand, FVector_NetQuantize100 AngleVel, FVector_NetQuantize100 LinearVel, uint8 GripHash)
{
    FBPActorGripInformation gripInfo;
    EBPVRResultSwitch gripResult;
    Hand->GetGripByID(gripInfo, GripHash, gripResult);
    if (gripResult == EBPVRResultSwitch::OnSucceeded)
    {
        TryDropSingle_Client(Hand, gripInfo, AngleVel, LinearVel);
    }
}

void AxREAL_VRCharacter::TryDropSingle_Client_Implementation(UGripMotionControllerComponent* Hand, UPARAM(ref)FBPActorGripInformation& GripToDrop, FVector AngleVel, FVector LinearVel)
{
    bool shouldSocket;
    USceneComponent* socketParent;
    FTransform_NetQuantize relativeTransform;
    FName optSocketName;
    ShouldSocketGrip(GripToDrop, shouldSocket, socketParent, relativeTransform, optSocketName);
    
    if (shouldSocket)
    {
        Hand->DropAndSocketGrip(GripToDrop, socketParent, optSocketName, relativeTransform);
    }
    else
    {
        if (GripToDrop.GrippedObject && GripToDrop.GrippedObject->Implements<UVRGripInterface>())
        {
            Hand->DropObjectByInterface(nullptr, GripToDrop.GripID, AngleVel, LinearVel);
        }
        else
        {
            Hand->DropGrip(GripToDrop, true, AngleVel, LinearVel);
        }
    }
}

bool AxREAL_VRCharacter::IfOverWidgetUse(UGripMotionControllerComponent *CallingHand, bool Pressed)
{
    EControllerHand handType;
    CallingHand->GetHandType(handType);
    switch (handType)
    {
        case EControllerHand::Left:
            if (IsValid(TeleportControllerLeft))
            {
                return TeleportControllerLeft->IfOverWidget_Use(Pressed);
            } 
            break;

        case EControllerHand::Right:
            if (IsValid(TeleportControllerRight))
            {
                return TeleportControllerRight->IfOverWidget_Use(Pressed);
            }
            break;

        default:
            break;
    }
    return false;
}

void AxREAL_VRCharacter::TryRemoveSecondaryAttachment(UGripMotionControllerComponent *CallingMotionController, UGripMotionControllerComponent *OtherController, FGameplayTagContainer GameplayTags, bool &DroppedSecondary, bool &HadSecondary)
{
    FBPActorGripInformation gripInfo;
    if (OtherController->GetIsSecondaryAttachment(CallingMotionController, gripInfo))
    {
        bool isMatchedOrDefault = ValidateGameplayTagContainer(FGameplayTag::RequestGameplayTag(TEXT("DropType.SecondaryGrip")), gripInfo.GrippedObject, DefaultSecondaryDropTag, GameplayTags);
        if (isMatchedOrDefault)
        {
            if (IsALocalGrip(gripInfo.GripMovementReplicationSetting))
            {
                OtherController->RemoveSecondaryAttachmentPoint(gripInfo.GrippedObject, 0.25f);
            }
            else
            {
                RemoveSecondaryGrip_Server(OtherController, gripInfo.GrippedObject);
            }
            HadSecondary = true;
            DroppedSecondary = true;
        }
        else
        {
            HadSecondary = true;
            DroppedSecondary = false;
        }
    }
    else
    {
        HadSecondary = false;
        DroppedSecondary = false;
    }
}

void AxREAL_VRCharacter::RemoveSecondaryGrip_Server_Implementation(UGripMotionControllerComponent* Hand, UObject* GrippedActorToRemoveAttachment)
{
    Hand->RemoveSecondaryAttachmentPoint(GrippedActorToRemoveAttachment, 0.25f);
}

void AxREAL_VRCharacter::SwitchOutOfBodyCamera_Implementation(bool SwitchToOutOfBody)
{
    if (SwitchToOutOfBody)
    {
        // Go out of body
        if (!IsValid(OutOfBodyCamera))
        {
            OutOfBodyCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass());
            OutOfBodyCamera->AttachToActor(this, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, true));
            OutOfBodyCamera->SetActorTickEnabled(false);
            OutOfBodyCamera->GetCameraComponent()->SetComponentTickEnabled(false);
        }
        OutOfBodyCamera->GetCameraComponent()->SetActive(true);
        bIsOutOfBody = true;
        OutOfBodyCamera->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        UGameplayStatics::GetPlayerController(GetWorld(), 0)->SetViewTargetWithBlend(OutOfBodyCamera);
        VRReplicatedCamera->bSetPositionDuringTick = true;
    }

    else
    {
        // Switch back into body
        if (bIsOutOfBody && IsValid(OutOfBodyCamera))
        {
            OutOfBodyCamera->GetCameraComponent()->SetActive(false);
            bIsOutOfBody = false;
            OutOfBodyCamera->AttachToActor(this, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, true));
            UGameplayStatics::GetPlayerController(GetWorld(), 0)->SetViewTargetWithBlend(this);
            VRReplicatedCamera->bSetPositionDuringTick = false;
        }
    }
}

void AxREAL_VRCharacter::SpawnWristMenu()
{
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
		if (bWristMenuOnRightHand && GraspingHandRight)
		{
			WristMenuActor->AttachToActor(GraspingHandRight, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("wrist_outer_r"));
			
		}
		else if (!bWristMenuOnRightHand && GraspingHandLeft)
		{
			WristMenuActor->AttachToActor(GraspingHandLeft, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("wrist_outer_r"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("WristMenuActor not spawned, no valid motion controller to attach to!"));
            return;
		}

		WristMenuActor->SetActorRelativeLocation(WristMenuRelativeLocation);
		WristMenuActor->SetActorRelativeRotation(WristMenuRelativeRotation);
        SetWristMenuEnabled(false);
    }
}

void AxREAL_VRCharacter::SetWristMenuEnabled(bool bEnabled)
{
    if (WristMenuActor)
    {
        WristMenuActor->SetWristMenuEnabled(bEnabled);
        bIsWristMenuEnabled = bEnabled;
    }
}

void AxREAL_VRCharacter::SetTeleporterActive(EControllerHand Hand, bool Active)
{
    if (!TeleportComponent)
        return;

    TeleportComponent->SetTeleporterActive(Hand, Active);
}

void AxREAL_VRCharacter::NavigationMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult &Result)
{
    Super::NavigationMoveCompleted(RequestID, Result);
    if (bIsOutOfBody && TeleportComponent)
    {
        APlayerCameraManager* cameraManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
        if (cameraManager)
        {
            cameraManager->StartCameraFade(0.0f, 1.0f, FadeOutDuration, FLinearColor::Black, false, true);
            GetWorldTimerManager().SetTimer(NavigationFinishedTeleportFade_TimerHandle, FTimerDelegate::CreateLambda([this, cameraManager]() {
                if (cameraManager)
                {
                    cameraManager->StartCameraFade(1.0f, 0.0f, FadeinDuration, FLinearColor::Black, false, false);
                }
                SwitchOutOfBodyCamera(false);
            }), FadeOutDuration, false);
        }
    }
}

void AxREAL_VRCharacter::OnSeatedModeChanged_Implementation(bool bNewSeatedMode, bool bWasAlreadySeated)
{
    Super::OnSeatedModeChanged_Implementation(bNewSeatedMode, bWasAlreadySeated);
    VRMovementReference->NavAgentProps.bCanSwim = !bNewSeatedMode;
    if (IsLocallyControlled())
    {
        SetVehicleMode(bNewSeatedMode);
    }
}

void AxREAL_VRCharacter::HandleSlidingMovement(EVRMovementMode MovementMode, UGripMotionControllerComponent *CallingHand, bool bThumbPadInfluencesDirection)
{
    EControllerHand handType;
    CallingHand->GetHandType(handType);

    float thumbY = 0.0f;
    float thumbX = 0.0f;

    FVector direction;

    switch (handType)
    {
        case EControllerHand::Left:
            thumbY = MotionControllerThumbLeft_Y_Value; 
            thumbX = MotionControllerThumbLeft_X_Value;
            break;
        case EControllerHand::Right:
            thumbY = MotionControllerThumbRight_Y_Value;
            thumbX = MotionControllerThumbRight_X_Value;
            break;
        default:
            return;
    }

    bool  isPadCalcValid;
    float padMagnitude;
    FRotator padRotation;
    CalcPadRotationAndMagnitude(thumbY, thumbX, DPadVelocityScaler, SlidingMovementDeadZone, padRotation, padMagnitude, isPadCalcValid);

    FVector padForwardVector, padRightVector;
    GetDPadMovementFacing(MovementMode, CallingHand, nullptr, padForwardVector, padRightVector);
    if (isPadCalcValid && bThumbPadInfluencesDirection)
    {
        if (MovementMode == EVRMovementMode::DPadPress_ControllerOrient)
        {
            direction = MapThumbToWorld(padRotation, CallingHand);
        }
        else
        {
            direction = padForwardVector;
        }
    }
    else
    {
        direction = padForwardVector;
    }
    AddMovementInput(direction, padMagnitude, false);
}

void AxREAL_VRCharacter::CalcPadRotationAndMagnitude(float YAxis, float XAxis, float OptMagnitudeScaler, float OptionalDeadzone, FRotator &Rotation, float &Magnitude, bool &WasValid)
{
    WasValid = FMath::Abs(YAxis) + FMath::Abs(XAxis) > OptionalDeadzone;
    Rotation = UKismetMathLibrary::MakeRotFromX(FVector(YAxis, XAxis, 0.0f));
    Magnitude = FMath::Clamp(FMath::Max(FMath::Abs(YAxis*OptMagnitudeScaler), FMath::Abs(XAxis*OptMagnitudeScaler)), 0.0f, 1.0f);
}

void AxREAL_VRCharacter::GetCharacterRotatedPosition(FVector OriginalLocation, FRotator DeltaRotation, FVector PivotPoint, FRotator &OutRotation, FVector &OutNewPosition)
{
    UVRExpansionFunctionLibrary::RotateAroundPivot(DeltaRotation, OriginalLocation, GetCorrectRotation(), UKismetMathLibrary::InverseTransformLocation(GetActorTransform(), PivotPoint), OutNewPosition, OutRotation, true);
}

bool AxREAL_VRCharacter::ValidateGameplayTag(FGameplayTag BaseTag, FGameplayTag GameplayTag, UObject *Object, FGameplayTag DefaultTag)
{
    if (GameplayTag.IsValid())
    {
        IGameplayTagAssetInterface* TagAssetInterface = Cast<IGameplayTagAssetInterface>(Object);
        if (TagAssetInterface && TagAssetInterface->HasMatchingGameplayTag(BaseTag))
        {
            return TagAssetInterface->HasMatchingGameplayTag(GameplayTag);
        }
    }
    return (GameplayTag == DefaultTag);
}

void AxREAL_VRCharacter::CycleMovementModes(bool IsLeft)
{
    FString mode = "";
    if ((bRightHandMovement && IsLeft) || (!bRightHandMovement && !IsLeft))
    {
        bTurnModeIsSnap = !bTurnModeIsSnap;
        mode = bTurnModeIsSnap?TEXT("Snap Turn"):TEXT("Smooth Turn");
        // Show movement mode above controller
        SetControllerText(IsLeft, mode);
    }
    else
    {
        // Continue only if not currently climbing
        if (!IsHandClimbing)
        {
            switch (CurrentMovementMode)
            {
            case EVRMovementMode::ClimbingMode:
                CurrentMovementMode = EVRMovementMode::Teleport;
                mode = TEXT("Teleport");
                SetControllerText(IsLeft, mode);
                break;
            
            default:
                CurrentMovementMode = static_cast<EVRMovementMode>(static_cast<int>(CurrentMovementMode)+1);
                mode = UEnum::GetValueAsString(CurrentMovementMode);
                // Show movement mode above controller
                SetControllerText(IsLeft, mode);
                break;
            }

            // This prevents conflicts
            SetTeleporterActive(IsLeft?EControllerHand::Left:EControllerHand::Right, false);
            ClearMovementVelocities();
        }
    }
}

void AxREAL_VRCharacter::DropItems(UGripMotionControllerComponent *Hand, FGameplayTagContainer GameplayTags)
{
    TArray<FBPActorGripInformation> grips;
    Hand->GetAllGrips(grips);

    // Reverse for each loop
    for (int i = grips.Num()-1; i >= 0; i--)
    {
        DropItem(Hand, grips[i], GameplayTags);
    }
}

void AxREAL_VRCharacter::DropItem(UGripMotionControllerComponent *Hand, FBPActorGripInformation GripInfo, FGameplayTagContainer GameplayTags)
{
    UObject* grippedObject = GripInfo.GrippedObject;
    bool isMatchedOrDefault = ValidateGameplayTagContainer(FGameplayTag::RequestGameplayTag(TEXT("DropType")), grippedObject, DefaultDropTag, GameplayTags);

    if ((IsValid(grippedObject) && isMatchedOrDefault) || !IsValid(grippedObject))
    {
        CallCorrectDropSingleEvent(Hand, GripInfo);
    }
}

void AxREAL_VRCharacter::OnDestroy()
{
    if (IsValid(TeleportControllerLeft))
        TeleportControllerLeft->Destroy();

    if (IsValid(TeleportControllerRight))
        TeleportControllerRight->Destroy();

    if (IsValid(OutOfBodyCamera))
        OutOfBodyCamera->Destroy();

    if (HasAuthority())
    {
        if (IsValid(GraspingHandRight))
        {
            GraspingHandRight->Destroy();
        }
        if (IsValid(GraspingHandLeft))
        {
            GraspingHandLeft->Destroy();
        }
    }
}

void AxREAL_VRCharacter::GetCorrectPrimarySlotPrefix(UObject* ObjectToCheckForTag, EControllerHand Hand, FName NearestBoneName, FName& SocketPrefix)
{

    FString localBasePrefix = (NearestBoneName == NAME_None) ? TEXT("") : NearestBoneName.ToString();

    IGameplayTagAssetInterface* TagAssetInterface = Cast<IGameplayTagAssetInterface>(ObjectToCheckForTag);
    if (TagAssetInterface)
    {
        bool hasPerHandSockets = TagAssetInterface->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("GripSockets.SeperateHandSockets")));

        if (hasPerHandSockets)
        {
            switch (Hand)
            {
                // Left Hand Specific
                case EControllerHand::Left:
                    SocketPrefix = FName(localBasePrefix + TEXT("VRGripLP"));
                    return;
                // Right Hand Specific
                case EControllerHand::Right:
                    SocketPrefix = FName(localBasePrefix + TEXT("VRGripRP"));
                    return;
                default:
                    break;
            }
        }
    }
    // Bone specific or default
    SocketPrefix = (NearestBoneName != NAME_None) ? FName(localBasePrefix + TEXT("VRGripP")) : NAME_None;
}

bool AxREAL_VRCharacter::CanObjectBeClimbed(UPrimitiveComponent* ObjectToCheck)
{
    // If Object has VRGripInterface then it can't be climbed
    if (ObjectToCheck)
    {
        if (DisableMovement || ObjectToCheck->Implements<UVRGripInterface>() || ObjectToCheck->GetOwner()->Implements<UVRGripInterface>())
        {
            return false;
        }
        else
        {
            // VR Trace Channel
            ECollisionResponse collisionResponse = ObjectToCheck->GetCollisionResponseToChannel(ECC_GameTraceChannel1);
            switch (collisionResponse)
            {
            case ECR_Ignore:
                return false;
            case ECR_Overlap: case ECR_Block:
                return true;
            default:
                return false;
            }
        }
    }
    return false;
}

bool AxREAL_VRCharacter::HasValidGripCollision(UPrimitiveComponent* Component)
{
    // VR Trace Channel
    ECollisionResponse collisionResponse = Component->GetCollisionResponseToChannel(ECC_GameTraceChannel1);
    switch (collisionResponse)
    {
        case ECR_Ignore:
            return false;
        case ECR_Overlap: case ECR_Block:
            return true;
        default:
            return false;
    }
}

void AxREAL_VRCharacter::SetVehicleMode_Implementation(bool IsInVehicleMode, bool &IsVR)
{
    if (IsLocallyControlled())
    {
        bIsInVehicle = IsInVehicleMode;
        LeftMotionController->bDisableLowLatencyUpdate = IsInVehicleMode;
        RightMotionController->bDisableLowLatencyUpdate = IsInVehicleMode;
        DisableMovement = IsInVehicleMode;
        SetTeleporterActive(EControllerHand::Left, false);
        SetTeleporterActive(EControllerHand::Right, false);
        ClearClimbing(false);
        IsVR = true;
    }
}

void AxREAL_VRCharacter::SetVehicleMode(bool IsInVehicleMode)
{
    if (IsLocallyControlled())
    {
        bIsInVehicle = IsInVehicleMode;
        LeftMotionController->bDisableLowLatencyUpdate = IsInVehicleMode;
        RightMotionController->bDisableLowLatencyUpdate = IsInVehicleMode;
        DisableMovement = IsInVehicleMode;
        SetTeleporterActive(EControllerHand::Left, false);
        SetTeleporterActive(EControllerHand::Right, false);
        ClearClimbing(false);
    }
}

// TODO: Refactor this function, and make sure it works
bool AxREAL_VRCharacter::ShouldGripComponent(UPrimitiveComponent *ComponentToCheck, uint8 GripPrioToCheckAgainst, bool bCheckAgainstPrior, FName BoneName, FGameplayTagContainer RelevantGameplayTags, UGripMotionControllerComponent *CallingController, UObject *&ObjectToGrip, bool &ObjectImplementsInterface, FTransform &ObjectsWorldTransform, uint8 &GripPrio)
{
    if (HasValidGripCollision(ComponentToCheck))
    {
        // If implements interface, allow gripping
        if (ComponentToCheck->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
        {
            bool denyGripping = IVRGripInterface::Execute_DenyGripping(ComponentToCheck, CallingController);
            bool noSecondaryGrip = IVRGripInterface::Execute_SecondaryGripType(ComponentToCheck) == ESecondaryGripType::SG_None;
            if (!denyGripping || !noSecondaryGrip)
            {
                bool hadHigherPriority;
                uint8 newGripPriority;
                CheckGripPriority(ComponentToCheck, GripPrioToCheckAgainst, bCheckAgainstPrior, hadHigherPriority, newGripPriority);
                if (hadHigherPriority)
                {
                    // If gameplay tags allow for it
                    if (CheckIsValidForGripping(ComponentToCheck, RelevantGameplayTags))
                    {
                        ObjectToGrip = ComponentToCheck;
                        ObjectImplementsInterface = true;
                        ObjectsWorldTransform = ComponentToCheck->GetComponentTransform();
                        GripPrio = newGripPriority;
                        return true;
                    }
                }

            }
        }
        // If not check owning actor instead
        else
        {
            AActor* owningActor = ComponentToCheck->GetOwner();
            if (IsValid(owningActor) && owningActor != this)
            {
                // Actor implements interface
                if (owningActor->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
                {
                    bool denyGripping = IVRGripInterface::Execute_DenyGripping(owningActor, CallingController);
                    bool noSecondaryGrip = IVRGripInterface::Execute_SecondaryGripType(owningActor) == ESecondaryGripType::SG_None;
                    if (!denyGripping || !noSecondaryGrip)
                    {
                        bool hadHigherPriority;
                        uint8 newGripPriority;
                        CheckGripPriority(owningActor, GripPrioToCheckAgainst, bCheckAgainstPrior, hadHigherPriority, newGripPriority);
                        if (hadHigherPriority)
                        {
                            // If gameplay tags allow for it
                            if (CheckIsValidForGripping(owningActor, RelevantGameplayTags))
                            {
                                ObjectToGrip = owningActor;
                                ObjectImplementsInterface = true;
                                ObjectsWorldTransform = owningActor->GetActorTransform();
                                GripPrio = newGripPriority;
                                return true;
                            }
                        }
                    }
                }
                // If it is just a simulating component
                else
                {
                    if (!bCheckAgainstPrior && ComponentToCheck->IsSimulatingPhysics(BoneName))
                    {
                        ObjectToGrip = ComponentToCheck;
                        ObjectImplementsInterface = false;
                        ObjectsWorldTransform = ComponentToCheck->GetComponentTransform();
                        return true;
                    }
                }

            }
        }
    }
    ObjectImplementsInterface = false;
    return false;
}

void AxREAL_VRCharacter::TryToSecondaryGripObject(UGripMotionControllerComponent *Hand, UGripMotionControllerComponent *OtherHand, UObject *ObjectToTryToGrab, FGameplayTag GripSecondaryTag, bool ObjectImplementsInterface, FTransform RelativeSecondaryTransform, FName SlotName, bool bHadSlot, bool &SecondaryGripped)
{
    if (ObjectImplementsInterface)
    {
        if (ObjectToTryToGrab->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
        {
            if (CanSecondaryGripObject(Hand, OtherHand, ObjectToTryToGrab, GripSecondaryTag, bHadSlot, IVRGripInterface::Execute_SecondaryGripType(ObjectToTryToGrab)))
            {
                SecondaryGripped = OtherHand->AddSecondaryAttachmentPoint(ObjectToTryToGrab, Hand, RelativeSecondaryTransform, true, 0.25f, bHadSlot, SlotName);
                return;
            }
        }
    }
    SecondaryGripped = false;
    return;
}

void AxREAL_VRCharacter::ClearMovementVelocities()
{
    LastLContPos = FVector::ZeroVector;
    LeftControllerRelativeVel = FVector::ZeroVector;
    LowEndLContRelativeVel = FVector::ZeroVector;
    LastRContPos = FVector::ZeroVector;
    RightControllerRelativeVel = FVector::ZeroVector;
    LowEndRContRelativeVel = FVector::ZeroVector;
    LastHMDPos = FVector::ZeroVector;
    HeadRelativeVel = FVector::ZeroVector;
    LowEndHeadRelativeVel = FVector::ZeroVector;
}

// TODO: Refactor this hot garbage
bool AxREAL_VRCharacter::GripOrDropObjectClean(UGripMotionControllerComponent *CallingMotionController, UGripMotionControllerComponent *OtherController, bool CanCheckClimb, UPrimitiveComponent *GrabSphere, FGameplayTagContainer RelevantGameplayTags)
{
    // Objects in calling controllers hand?
    if (CallingMotionController->HasGrippedObjects())
    {
        // This controller already has an object gripped, drop or use the object
        DropItems(CallingMotionController, RelevantGameplayTags);
        return true;
    }

    bool droppedSecondary, hadSecondary;
    DropSecondaryAttachment(CallingMotionController, OtherController, RelevantGameplayTags, droppedSecondary, hadSecondary);
    // If secondary attachment was found and dropped
    if (hadSecondary)
        return true;

    UObject* nearestObject;
    bool implementsInterface, canBeClimbed;
    FTransform objectTransform;
    FName nearestBoneName;
    FVector impactPoint;
    GetNearestOverlappingObject(GrabSphere, CallingMotionController, RelevantGameplayTags, nearestObject, implementsInterface, objectTransform, canBeClimbed, nearestBoneName, impactPoint);

    // Return if there is no valid object
    if (!IsValid(nearestObject))
    {
        return false;
    }

    if (canBeClimbed)
    {
        // Attempt climbing if it's not grippable
        if (CanCheckClimb)
        {
            InitClimbing(CallingMotionController, nearestObject, IsValid(nearestObject));
            return true;
        }

        return false;
    }
    

    if (implementsInterface)
    {
        // Make sure we hold it, or it isn't held...server checks itself as well
        if (!CanAttemptGrabOnObject(nearestObject))
            return false;

        if (nearestObject->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
        {
            ESecondaryGripType secondaryType;
            // If set to allow secondary gripping
            if (CanAttemptSecondaryGrabOnObject(nearestObject, &secondaryType))
            {
                // Try to second grip first
                
                // If the button is correct
                if (ValidateGameplayTagContainer(FGameplayTag::RequestGameplayTag("GripType.Secondary"), nearestObject, DefaultSecondaryGripTag, RelevantGameplayTags))
                {
                    bool hadSlot;
                    FTransform slotWorldTransform;
                    FName slotName;
                    // If has snap point in range, use it
                    IVRGripInterface::Execute_ClosestGripSlotInRange(nearestObject, impactPoint, true, hadSlot, slotWorldTransform, slotName, CallingMotionController, NAME_None);

                    FGameplayTag gripSecondaryTag;
                    UVRExpansionFunctionLibrary::GetFirstGameplayTagWithExactParent(FGameplayTag::RequestGameplayTag("GripType.Secondary"), RelevantGameplayTags, gripSecondaryTag);

                    if (CanSecondaryGripObject(CallingMotionController, OtherController, nearestObject, gripSecondaryTag, hadSlot, secondaryType))
                    {
                        EControllerHand handType;
                        CallingMotionController->GetHandType(handType);
                        FTransform gripTransform;
                        if (hadSlot)
                        {
                            gripTransform = UGripMotionControllerComponent::ConvertToGripRelativeTransform(objectTransform, slotWorldTransform);
                        }
                        else
                        {
                            gripTransform = UGripMotionControllerComponent::ConvertToGripRelativeTransform(objectTransform, CallingMotionController->GetPivotTransform());
                        }
                        CallCorrectGrabEvent(nearestObject, handType, hadSlot, gripTransform, gripSecondaryTag, NAME_None, slotName, true);
                        return true;
                    }
                }
            }

            // Try to primary grip next
            if (!IVRGripInterface::Execute_DenyGripping(nearestObject, CallingMotionController))
            {
                if (!ValidateGameplayTagContainer(FGameplayTag::RequestGameplayTag("GripType"), nearestObject, DefaultGripTag, RelevantGameplayTags))
                    return false;
                
                bool hadSlot;
                FTransform slotWorldTransform;
                FName slotName;

                EControllerHand handType;
                CallingMotionController->GetHandType(handType);

                FName overridePrefix;
                GetCorrectPrimarySlotPrefix(nearestObject, handType, nearestBoneName, overridePrefix);

                // If has snap point in range, use it
                IVRGripInterface::Execute_ClosestGripSlotInRange(nearestObject, impactPoint, false, hadSlot, slotWorldTransform, slotName, CallingMotionController, overridePrefix);
                
                if (!hadSlot)
                {
                    if (IGameplayTagAssetInterface* nearestObjectGameplayTagInterface = Cast<IGameplayTagAssetInterface>(nearestObject))
                    {
                        if (nearestObjectGameplayTagInterface->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.DenyFreeGripping")))
                        {
                            // Don't grip, we are denying free gripping
                            return false;
                        }
                    }
                }

                FTransform plainOrBoneTransform, gripTransform;

                if (nearestBoneName != NAME_None)
                {
                    GetBoneTransform(nearestObject, nearestBoneName, plainOrBoneTransform);
                }
                else
                {
                    plainOrBoneTransform = objectTransform;
                }

                // Slot transform
                if (hadSlot)
                {
                    gripTransform = RemoveControllerScale(UKismetMathLibrary::MakeRelativeTransform(plainOrBoneTransform, slotWorldTransform), CallingMotionController);
                }
                // Normal Transform
                else
                {
                    gripTransform = CallingMotionController->ConvertToControllerRelativeTransform(plainOrBoneTransform);
                }

                CallCorrectGrabEvent(nearestObject, handType, hadSlot, gripTransform, FGameplayTag(), nearestBoneName, slotName, false);
                return true;
                
            }
        }

    }

    if (!RelevantGameplayTags.HasTagExact(DefaultGripTag))
        return false;

    FTransform plainOrBoneTransform;
    if (nearestBoneName != NAME_None)
    {
        GetBoneTransform(nearestObject, nearestBoneName, plainOrBoneTransform);
    }
    else
    {
        plainOrBoneTransform = objectTransform;
    }
    FTransform gripTransform = CallingMotionController->ConvertToControllerRelativeTransform(plainOrBoneTransform);
    EControllerHand handType;
    CallingMotionController->GetHandType(handType);
    // Default grip for non VR Interface simulating objects
    TryGrabServer(nearestObject, false, gripTransform, handType, FGameplayTag(), nearestBoneName, NAME_None, false);

    return true;
}

bool AxREAL_VRCharacter::ValidateGameplayTagContainer(FGameplayTag BaseTag, UObject* Object, FGameplayTag DefaultTag, FGameplayTagContainer GameplayTags)
{
    if (IGameplayTagAssetInterface* TagAssetInterface = Cast<IGameplayTagAssetInterface>(Object))
    {
        if (TagAssetInterface->HasMatchingGameplayTag(BaseTag))
        {
            FGameplayTagContainer objectTags;
            TagAssetInterface->GetOwnedGameplayTags(objectTags);
            return UVRExpansionFunctionLibrary::MatchesAnyTagsWithDirectParentTag(BaseTag, GameplayTags, objectTags);
        }
    }

    // Falling back to default tag test
    return GameplayTags.HasTagExact(DefaultTag);
}

void AxREAL_VRCharacter::DropSecondaryAttachment(UGripMotionControllerComponent* CallingMotionController, UGripMotionControllerComponent* OtherController, FGameplayTagContainer GameplayTags, bool& DroppedSecondary, bool& HadSecondary)
{
    FBPActorGripInformation gripInfo;

    // Return if no secondary attachment
    if (!OtherController->GetIsSecondaryAttachment(CallingMotionController, gripInfo))
    {
        HadSecondary = false;
        DroppedSecondary = false;
        return;
    }

    bool isMatchedOrDefault = ValidateGameplayTagContainer(FGameplayTag::RequestGameplayTag(TEXT("DropType.Secondary")), gripInfo.GrippedObject, DefaultSecondaryDropTag, GameplayTags);

    // Return if no valid tag
    if (!isMatchedOrDefault)
    {
        HadSecondary = true;
        DroppedSecondary = false;
        return;
    }

    // Remove secondary attachment

    if (IsALocalGrip(gripInfo.GripMovementReplicationSetting))
    {
        OtherController->RemoveSecondaryAttachmentPoint(gripInfo.GrippedObject, 0.25f);
    }

    else
    {
        RemoveSecondaryGrip_Server(OtherController, gripInfo.GrippedObject);
    }

    DroppedSecondary = true;
    HadSecondary = true;
}

void AxREAL_VRCharacter::SelectObjectFromHitArray(UPARAM(ref) TArray<FHitResult> &Hits, FGameplayTagContainer RelevantGameplayTags, UGripMotionControllerComponent *Hand, bool &bShouldGrip, bool &ObjectImplementsInterface, UObject *&ObjectToGrip, FTransform &WorldTransform, UPrimitiveComponent *&FirstPrimitiveHit, FName &BoneName, FVector &ImpactPoint)
{
    UPrimitiveComponent* firstHitPrimitive = nullptr;

    for (int i = 0; i < Hits.Num(); i++)
    {
        UPrimitiveComponent* hitComponent = Hits[i].GetComponent();
        if (IsValid(hitComponent))
        {
            if (i == 0)
            {
                firstHitPrimitive = hitComponent;
            }

            uint8 bestGripPrio = 0;
            UObject* lOutObject = nullptr;
            FTransform lOutTransform;
            bool lObjectImplementsInterface, lShouldGrip;
            FName lOutBoneName = Hits[i].BoneName;
            FVector lImpactPoint;
            lShouldGrip = ShouldGripComponent(hitComponent, bestGripPrio, i > 0, lOutBoneName, RelevantGameplayTags, Hand, lOutObject, lObjectImplementsInterface, lOutTransform, bestGripPrio);
            if (lShouldGrip)
            {
                bShouldGrip = lShouldGrip;
                ObjectImplementsInterface = lObjectImplementsInterface;
                ObjectToGrip = lOutObject;
                WorldTransform = lOutTransform;
                FirstPrimitiveHit = firstHitPrimitive;
                BoneName = lOutBoneName;
                ImpactPoint = Hits[i].ImpactPoint;
            }
        }
    }

}

void AxREAL_VRCharacter::CheckGripPriority(UObject *ObjectToCheck, uint8 PrioToCheckAgainst, bool CheckAgainstPrior, bool &HadHigherPriority, uint8 &NewGripPrio)
{
    if (ObjectToCheck->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
    {
        NewGripPrio = IVRGripInterface::Execute_AdvancedGripSettings(ObjectToCheck).GripPriority;
        if (CheckAgainstPrior)
        {
            HadHigherPriority = NewGripPrio > PrioToCheckAgainst;
        }
        else
        {
            HadHigherPriority = true;
        }
    }
}

void AxREAL_VRCharacter::GetBoneTransform(UObject *Object, FName BoneName, FTransform &BoneTransform)
{
    if (BoneName != NAME_None)
    {
        if (USceneComponent* sceneComp = Cast<USceneComponent>(Object))
        {
            BoneTransform = sceneComp->GetSocketTransform(BoneName);
        }
        else if (ASkeletalMeshActor* skelActor = Cast<ASkeletalMeshActor>(Object))
        {
            BoneTransform = skelActor->GetSkeletalMeshComponent()->GetSocketTransform(BoneName);
        }
    }
}

bool AxREAL_VRCharacter::CanSecondaryGripObject(UGripMotionControllerComponent *Hand, UGripMotionControllerComponent *OtherHand, UObject *ObjectToTryToGrab, FGameplayTag GripSecondaryTag, bool HadSlot, ESecondaryGripType SecGripType)
{
    // If double grip is not allowed
    if (SecGripType == ESecondaryGripType::SG_None)
    {
        return false;
    }

    bool isMatchedOrDefault = ValidateGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("GripType.Secondary")), GripSecondaryTag, ObjectToTryToGrab, DefaultSecondaryGripTag);
    // If wrong button or doesn't implement interface
    if (!isMatchedOrDefault || !ObjectToTryToGrab->Implements<UVRGripInterface>()) 
    {
        return false;
    }

    if (!HadSlot)
    {
        switch (SecGripType)
        {
            // Can't grip slot only types without a slot
            case ESecondaryGripType::SG_None: case ESecondaryGripType::SG_SlotOnly: case ESecondaryGripType::SG_SlotOnly_Retain: case ESecondaryGripType::SG_SlotOnlyWithScaling_Retain:
                return false;
        }
    }

    return true;
}

bool AxREAL_VRCharacter::CanAttemptGrabOnObject(UObject *ObjectToCheck)
{
    if (ObjectToCheck->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
    {
        TArray<FBPGripPair> gripPairs;
        bool isHeld;
        IVRGripInterface::Execute_IsHeld(ObjectToCheck, gripPairs, isHeld);
        bool allowsMultipleGrips = IVRGripInterface::Execute_AllowsMultipleGrips(ObjectToCheck);
        // Because controller could be invalid
        if (isHeld && !allowsMultipleGrips)
        {
            return gripPairs[0].HoldingController->GetOwner() == this;
        }

    }
    return true;
}

bool AxREAL_VRCharacter::CanAttemptSecondaryGrabOnObject(UObject* ObjectToCheck, ESecondaryGripType* OutSecondaryGripType)
{
    if (ObjectToCheck->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
    {
        TArray<FBPGripPair> gripPairs;
        bool isHeld;
        IVRGripInterface::Execute_IsHeld(ObjectToCheck, gripPairs, isHeld);
        // Because controller could be invalid
        if (isHeld)
        {
            ESecondaryGripType secGripType = IVRGripInterface::Execute_SecondaryGripType(ObjectToCheck);
            if (OutSecondaryGripType != nullptr)
            {
                *OutSecondaryGripType = secGripType;
            }
            return (secGripType != ESecondaryGripType::SG_None) && (gripPairs[0].HoldingController->GetOwner() == this);
        }
    }
    return false;
}

FRotator AxREAL_VRCharacter::GetCorrectRotation()
{
    if (bUseControllerRotationYaw)
    {
        return GetControlRotation();
    }
    return GetActorRotation();
}

void AxREAL_VRCharacter::SetGripComponents(UPrimitiveComponent *LeftHand, UPrimitiveComponent *RightHand)
{
    if (IsValid(LeftHand))
    {
        LeftHandGripComponent = LeftHand;
        LeftMotionController->SetCustomPivotComponent(LeftHandGripComponent);
    }
    else
    {
        FString errorText = TEXT("Invalid Grip Component Set for Left Hand");
        SetControllerText(true, errorText);
    }
    if (IsValid(RightHand))
    {
        RightHandGripComponent = RightHand;
        RightMotionController->SetCustomPivotComponent(RightHandGripComponent);
    }
    else
    {
        FString errorText = TEXT("Invalid Grip Component Set for Right Hand");
        SetControllerText(true, errorText);
    }
}

void AxREAL_VRCharacter::GetThrowingVelocity(UGripMotionControllerComponent *ThrowingController, UPARAM(ref) FBPActorGripInformation &Grip, FVector AngularVel, FVector ObjectsLinearVel, FVector &outAngularVel, FVector &outLinearVel)
{
    FVector localVelocity;
    if (Grip.GrippedObject->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
    {
        bool isHeld;
        TArray<FBPGripPair> gripPairs;
        IVRGripInterface::Execute_IsHeld(Grip.GrippedObject, gripPairs, isHeld);
        // If more than one hand is holding, don't set velocity
        if (gripPairs.Num() > 1)
        {
            // Returning 0 here keeps current velocity
            outAngularVel = FVector::ZeroVector;
            outLinearVel = FVector::ZeroVector;
            return;
        }
    }

    if (bSampleGripVelocity)
    {
        EControllerHand handType;
        ThrowingController->GetHandType(handType);
        if (handType == EControllerHand::Left)
        {
            localVelocity = UVRExpansionFunctionLibrary::GetPeak_PeakLowPassFilter(PeakVelocityLeft);
        }
        else
        {
            localVelocity = UVRExpansionFunctionLibrary::GetPeak_PeakLowPassFilter(PeakVelocityRight);
        }
    }

    else
    {
        UseControllerVelocityOnRelease?localVelocity = ThrowingController->GetComponentVelocity():localVelocity = ObjectsLinearVel;
    }

    if (ScaleVelocityByMass)
    {
        float gripMass = 0.0f;
        ThrowingController->GetGripMass(Grip, gripMass);
        float clampedMass = FMath::Clamp((ThrowingMassMaximum / gripMass), MassScalerMin, 1.0f);
        localVelocity = localVelocity * clampedMass;
    }

    if (bLimitMaxThrowVelocity)
    {
        // Normalized local velocity
        FVector normalizedVelocity = localVelocity.GetSafeNormal();
        if (localVelocity.SizeSquared() > FMath::Square(MaximumThrowingVelocity))
        {
            outLinearVel = normalizedVelocity * MaximumThrowingVelocity;
        }
        else
        {
            outLinearVel = localVelocity;
        }
        outAngularVel = AngularVel;
        return;
    }

    outAngularVel = AngularVel;
    outLinearVel = localVelocity;
}

void AxREAL_VRCharacter::CheckSpawnGraspingHands()
{
    if (HasAuthority())
    {
        // Clear the grasping hands so we can re-init them
        ClearGraspingHands();

        FActorSpawnParameters spawnParams = FActorSpawnParameters();
        spawnParams.Owner = this;
        spawnParams.Instigator = this;
        spawnParams.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;

        GraspingHandRight = GetWorld()->SpawnActor<AGraspingHandManny>(AGraspingHandManny::StaticClass(), HandMesh_Right->GetComponentTransform(), spawnParams);
        GraspingHandRight->OwningController = RightMotionController;
        GraspingHandRight->PhysicsRoot = UsePhysicalGraspingHands?GrabSphereRight:nullptr;
        GraspingHandRight->OtherController = LeftMotionController;
        GraspingHandRight->UseCurls = GraspingHandsUseFingerCurls;
        GraspingHandRight->AttachToComponent(RightMotionController, FAttachmentTransformRules::KeepWorldTransform);
        HandMesh_Right->SetHiddenInGame(true);
        GrabSphereRight->SetHiddenInGame(true);

        GraspingHandLeft = GetWorld()->SpawnActor<AGraspingHandManny>(AGraspingHandManny::StaticClass(), HandMesh_Left->GetComponentTransform(), spawnParams);
        GraspingHandLeft->OwningController = LeftMotionController;
        GraspingHandLeft->PhysicsRoot = UsePhysicalGraspingHands?GrabSphereLeft:nullptr;
        GraspingHandLeft->OtherController = RightMotionController;
        GraspingHandLeft->UseCurls = GraspingHandsUseFingerCurls;
        GraspingHandLeft->AttachToComponent(LeftMotionController, FAttachmentTransformRules::KeepWorldTransform);
        HandMesh_Left->SetHiddenInGame(true);
        GrabSphereLeft->SetHiddenInGame(true);

    }
    else
    {
        GrabSphereRight->SetHiddenInGame(true);
        HandMesh_Right->SetHiddenInGame(true);
        GrabSphereLeft->SetHiddenInGame(true);
        HandMesh_Left->SetHiddenInGame(true);
        return;
    }
}

void AxREAL_VRCharacter::ClearGraspingHands()
{
    if (IsValid(GraspingHandRight))
    {
        GraspingHandRight->Destroy();
        if (HasAuthority())
        {
            GraspingHandRight = nullptr;
        }
        HandMesh_Right->SetHiddenInGame(false);
    }
    
    if (IsValid(GraspingHandLeft))
    {
        GraspingHandLeft->Destroy();
        if (HasAuthority())
        {
            GraspingHandLeft = nullptr;
        }
        HandMesh_Left->SetHiddenInGame(false);
    }

    SetGripComponents(GrabSphereLeft, GrabSphereRight);
}

// The display component and the procedural mesh component are both created out of the actual controller mesh.
// They should already be placed and positioned perfectly for the controller. So we need to offset them in the inverse of the controller profile offset to account for it.
void AxREAL_VRCharacter::RepositionHandElements(bool IsRightHand, FTransform NewTransformForProcComps)
{
    if (IsRightHand)
    {
        if (IsValid(RightMotionController->DisplayComponent))
        {
            RightMotionController->DisplayComponent->SetRelativeTransform(NewTransformForProcComps);
        }
    }
    else
    {
        if (IsValid(LeftMotionController->DisplayComponent))
        {
            LeftMotionController->DisplayComponent->SetRelativeTransform(NewTransformForProcComps);
        }
    }
}

void AxREAL_VRCharacter::ShouldSocketGrip(UPARAM(ref) FBPActorGripInformation& Grip, bool& ShouldSocket, USceneComponent*& SocketParent, FTransform_NetQuantize& RelativeTransform, FName& OptionalSocketName)
{
    if (Grip.GrippedObject->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
    {
        ShouldSocket = IVRGripInterface::Execute_RequestsSocketing(Grip.GrippedObject, SocketParent, OptionalSocketName, RelativeTransform);
    }
}

void AxREAL_VRCharacter::LoopTryInitTeleportControllers_Implementation()
{
    if (HasAuthority())
    {
        if (IsValid(GetPlayerState()))
        {
            InitTeleportControllers(GetPlayerState());
        }
        else
        {
            GetWorldTimerManager().SetTimer(InitTeleportControllers_TimerHandle, FTimerDelegate::CreateLambda([this]() {
                if (IsValid(GetPlayerState()))
                {
                    InitTeleportControllers(GetPlayerState());
                    GetWorldTimerManager().ClearTimer(InitTeleportControllers_TimerHandle);
                }
            }), 0.1f, true);
        }
    }
    else
    {
        if (IsValid(GetPlayerState()))
        {
            InitTeleportControllers(GetPlayerState());
        }
        else
        {
            // Bind event to on player state replicated
            OnPlayerStateReplicated_Bind.AddDynamic(this, &AxREAL_VRCharacter::OnPlayerStateReplicated);
        }
    }
}

void AxREAL_VRCharacter::InitTeleportControllers_Implementation(const APlayerState* ValidPlayerState)
{
    if (ValidPlayerState != nullptr)
    {
        TeleportControllerRight->InitController();
        TeleportControllerLeft->InitController();

        if (!IsLocallyControlled())
        {
            // TODO: Move VOIP init out of this function
            if (AttenuationSettingsForVOIP)
            {
                FVoiceSettings VOIPTalkerSettings = FVoiceSettings();
                VOIPTalkerSettings.ComponentToAttachTo = VRReplicatedCamera;
                VOIPTalkerSettings.AttenuationSettings = AttenuationSettingsForVOIP;
                VOIPTalker->Settings = VOIPTalkerSettings;
                VOIPTalker->RegisterWithPlayerState(GetPlayerState());
            }
        }
    }
}

void AxREAL_VRCharacter::RepositionRightControllerProceduralMeshes(const FTransform &NewRelTransformForProcComps, const FTransform &NewProfileTransform)
{
    RepositionHandElements(true, NewRelTransformForProcComps);
    SendControllerProfileToServer(true, NewRelTransformForProcComps);
}

void AxREAL_VRCharacter::RepositionLeftControllerProceduralMeshes(const FTransform &NewRelTransformForProcComps, const FTransform &NewProfileTransform)
{
    RepositionHandElements(false, NewRelTransformForProcComps);
    SendControllerProfileToServer(false, NewRelTransformForProcComps);
}

bool AxREAL_VRCharacter::CheckIsValidForGripping(UObject *Object, FGameplayTagContainer RelevantGameplayTags)
{
    if (CanAttemptSecondaryGrabOnObject(Object))
    {
        bool secondaryValidToGrip = ValidateGameplayTagContainer(FGameplayTag::RequestGameplayTag(TEXT("GripType.Secondary")), Object, DefaultSecondaryGripTag, RelevantGameplayTags);
        if (secondaryValidToGrip)
        {
            return true;
        }
    }
    bool primaryValidToGrip = ValidateGameplayTagContainer(FGameplayTag::RequestGameplayTag(TEXT("GripType")), Object, DefaultGripTag, RelevantGameplayTags);
    return primaryValidToGrip;
}

FTransform AxREAL_VRCharacter::RemoveControllerScale_Implementation(FTransform SocketTransform, UGripMotionControllerComponent *GrippingController)
{
    FVector finalScale = FVector::OneVector / GrippingController->GetPivotTransform().GetScale3D();
    return FTransform(SocketTransform.GetRotation(), SocketTransform.GetLocation(), finalScale);
}

void AxREAL_VRCharacter::CheckUseHeldItems_Implementation(UGripMotionControllerComponent *Hand, bool ButtonState)
{
    TArray<FBPActorGripInformation> grips;
    Hand->GetAllGrips(grips);
    // Reverse for each loop
    for (int i = grips.Num()-1; i >= 0; i--)
    {
        // Ensure we have a gripped object in hand
        if (IsValid(grips[i].GrippedObject))
        {
            if (grips[i].GrippedObject->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
            {
                // Use Object (trigger press)
                if (ButtonState)
                {
                    IVRGripInterface::Execute_OnUsed(grips[i].GrippedObject);
                }
                // End Use Object (trigger release)
                else
                {
                    IVRGripInterface::Execute_OnEndUsed(grips[i].GrippedObject);
                }
            }
        }
    }
}

USceneComponent* AxREAL_VRCharacter::GetCorrectAimComp_Implementation(UGripMotionControllerComponent *Hand)
{
    EControllerHand handType; 
    Hand->GetHandType(handType);
    if (handType == EControllerHand::Right)
    {
        return AimRight;
    }
    else
    {
        return AimLeft;
    }
}

void AxREAL_VRCharacter::SampleGripVelocities_Implementation()
{
    if (!bSampleGripVelocity)
    {
        return;
    }

    // Left Grip
    TArray<FBPActorGripInformation> leftGrips;
    LeftMotionController->GetAllGrips(leftGrips);
    if (leftGrips.Num() > 0)
    {
        FVector leftAngularVel, leftLinearVel;
        LeftMotionController->GetPhysicsVelocities(leftGrips[0], leftAngularVel, leftLinearVel);
        UVRExpansionFunctionLibrary::UpdatePeakLowPassFilter(PeakVelocityLeft, leftLinearVel);
    }

    // Right Grip
    TArray<FBPActorGripInformation> rightGrips;
    RightMotionController->GetAllGrips(rightGrips);
    if (rightGrips.Num() > 0)
    {
        FVector rightAngularVel, rightLinearVel;
        RightMotionController->GetPhysicsVelocities(rightGrips[0], rightAngularVel, rightLinearVel);
        UVRExpansionFunctionLibrary::UpdatePeakLowPassFilter(PeakVelocityRight, rightLinearVel);
    }
}

void AxREAL_VRCharacter::CheckUseSecondaryAttachment_Implementation(UGripMotionControllerComponent *CallingMotionController, UGripMotionControllerComponent *OtherController, bool ButtonPressed, bool &DroppedOrUsedSecondary, bool &HadSecondary)
{
    FBPActorGripInformation gripInfo;
    if (OtherController->GetIsSecondaryAttachment(CallingMotionController, gripInfo))
    {
        if (gripInfo.GrippedObject->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
        {
            if (ButtonPressed)
            {
                IVRGripInterface::Execute_OnSecondaryUsed(gripInfo.GrippedObject);
            }
            else
            {
                IVRGripInterface::Execute_OnEndSecondaryUsed(gripInfo.GrippedObject);
            }
            DroppedOrUsedSecondary = true;
        }
        else
        {
            DroppedOrUsedSecondary = false;
        }
        HadSecondary = true;
    }
    else
    {
        DroppedOrUsedSecondary = false;
        HadSecondary = false;
    }
}

void AxREAL_VRCharacter::SetMovementHands_Implementation(bool RightHandForMovement)
{
    bRightHandMovement = RightHandForMovement;
    const UEnum* EnumPtr = StaticEnum<EMovementMode>();
    if (EnumPtr)
    {
        FString enumValueName = EnumPtr->GetNameStringByValue(static_cast<int32>(CurrentMovementMode));
        //TODO: Check to make sure this works
        SetControllerText(!bRightHandMovement, enumValueName);
    }
    FString text = bTurnModeIsSnap ? "Snap Turn" : "Smooth Turn";
    SetControllerText(bRightHandMovement, text);
}

FVector AxREAL_VRCharacter::MapThumbToWorld_Implementation(FRotator PadRotation, UGripMotionControllerComponent *CallingHand)
{
    FRotator combinedRotators = UKismetMathLibrary::ComposeRotators(PadRotation, CallingHand->GetComponentRotation());
    FVector projectedVector = UKismetMathLibrary::ProjectVectorOnToPlane(combinedRotators.Vector(), GetVRUpVector());
    return projectedVector.GetSafeNormal();
}

void AxREAL_VRCharacter::OnRep_RightControllerOffset_Implementation()
{
    if (!IsLocallyControlled())
    {
        RepositionHandElements(true, RightControllerOffset);
    }
}

void AxREAL_VRCharacter::OnRep_LeftControllerOffset_Implementation()
{
    if (!IsLocallyControlled())
    {
        RepositionHandElements(false, LeftControllerOffset);
    }
}

void AxREAL_VRCharacter::AddTagsToContainer(FGameplayTagContainer& BaseContainer, const TArray<FString>& TagsToAdd)
{
    for (const FString& tagString : TagsToAdd)
    {
        FGameplayTag newTag = FGameplayTag::RequestGameplayTag(FName(*tagString), false);

        if (newTag.IsValid())
        {
            BaseContainer.AddTag(newTag);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Invalid Gameplay Tag: %s"), *tagString);
        }
    }
}

void AxREAL_VRCharacter::ActivateBeam_Server_Implementation(bool LeftController, bool Active)
{
    ActivateBeam_Multicast(LeftController, Active);
}

void AxREAL_VRCharacter::ActivateBeam_Multicast_Implementation(bool LeftController, bool Active)
{
    if (LeftController && IsValid(TeleportControllerLeft))
    {
        TeleportControllerLeft->SetLaserBeamActive(Active);
    }
    else if (!LeftController && IsValid(TeleportControllerRight))
    {
        TeleportControllerRight->SetLaserBeamActive(Active);
    }
}

void AxREAL_VRCharacter::TriggerGripOrDrop(UGripMotionControllerComponent *CallingHand, UGripMotionControllerComponent *OtherHand, bool isGrip, UPrimitiveComponent *GrabSphere)
{
    // True = trigger pressed
    if (isGrip)
    {
        FGameplayTagContainer relevantGameplayTags;
        AddTagsToContainer(relevantGameplayTags, {"DropType.OnPrimaryGrip", "DropType.Secondary.OnPrimaryGrip", "GripType.OnPrimaryGrip", "GripType.Secondary.OnPrimaryGrip"});
        GripOrDropObjectClean(CallingHand, OtherHand, true, GrabSphere, relevantGameplayTags);
    }
    // False = trigger released
    else
    {
        if ((ClimbingHand == CallingHand) && IsHandClimbing)
        {
            ClearClimbing(false);
        }
        else
        {
            FGameplayTagContainer relevantGameplayTags;
            AddTagsToContainer(relevantGameplayTags, {"DropType.OnPrimaryGripRelease", "DropType.Secondary.OnPrimaryGripRelease"});
            GripOrDropObjectClean(CallingHand, OtherHand, false, GrabSphere, relevantGameplayTags);
        }

    }
}

// Component Event Handlers

void AxREAL_VRCharacter::OnRightMotionControllerGripped(const FBPActorGripInformation& GripInfo)
{
    if (bSampleGripVelocity)
    {
        UVRExpansionFunctionLibrary::ResetPeakLowPassFilter(PeakVelocityRight);
    }
}

void AxREAL_VRCharacter::OnLeftMotionControllerGripped(const FBPActorGripInformation& GripInfo)
{
    if (bSampleGripVelocity)
    {
        UVRExpansionFunctionLibrary::ResetPeakLowPassFilter(PeakVelocityLeft);
    }
}
