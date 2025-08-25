// Fill out your copyright notice in the Description page of Project Settings.


#include "GraspingHandManny.h"
#include "Components/CapsuleComponent.h"
#include "Misc/VREPhysicsConstraintComponent.h"
#include "Misc/VREPhysicalAnimationComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Enums/ETriggerIndexes.h"
#include "GripMotionControllerComponent.h"
#include "Grippables/HandSocketComponent.h"
#include "xREAL_VRCharacter.h"
#include "OpenXRHandPoseComponent.h"
#include "Enums/EHandAnimState.h"
#include "UObject/ConstructorHelpers.h"
#include "GameplayTagAssetInterface.h"

AGraspingHandManny::AGraspingHandManny(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer)
{
    // Replication Settings
    bReplicates = true;
    bIgnoreAttachmentReplication = true;
    bNetLoadOnClient = false;

    // Defaults
    MaxConstraintDistance = 50.0f;
    PrimaryActorTick.bStartWithTickEnabled = true; //TODO: Was set to false in blueprint
    PrimaryActorTick.bCanEverTick = true;
    SetTickGroup(ETickingGroup::TG_PostPhysics);
    GraspEval_startDispl = -7.0f;
    GraspEval_endDisp = 5.0f;
    BoneName = FName("hand_r");
    Laterality = EControllerHand::Right;
    
    // Physics Hand
    HandAnimState = EHandAnimState::Hand_Animated;
    ConstraintCheckRate = 0.05f;
    
    // Physics and Collision Settings
    USkeletalMeshComponent* skelMesh = GetSkeletalMeshComponent();
    skelMesh->SetEnableGravity(false);
    skelMesh->SetGenerateOverlapEvents(true);
    skelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    skelMesh->SetCollisionObjectType(ECC_WorldDynamic);
    skelMesh->SetCollisionResponseToAllChannels(ECR_Ignore); // TODO: This is not what the blueprint was set to, but I believe collisions should be ignored until later. Check that this is the case.

    // Mesh Setup
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> SkeletalMeshAsset(TEXT("SkeletalMesh'/VRExpansionPlugin/VRE/Core/GraspingHands/VRHandMeshes/Meshes/SKM_MannyXR_right.SKM_MannyXR_right'"));
    if (SkeletalMeshAsset.Succeeded())
    {
        skelMesh->SetSkeletalMesh(SkeletalMeshAsset.Object);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load skeletal mesh for %s"), *GetName());
    }

    static ConstructorHelpers::FObjectFinder<UClass> AnimBP(TEXT("Class'/VRExpansionPlugin/VRE/Core/GraspingHands/GraspAnimBP.GraspAnimBP_C'"));
    if (AnimBP.Succeeded())
    {
        skelMesh->SetAnimClass(AnimBP.Object);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load animation blueprint for %s"), *GetName());
    }
    
    static ConstructorHelpers::FObjectFinder<UCurveFloat> gripSmoothCurveFloat(TEXT("CurveFloat'/VRExpansionPlugin/VRE/Core/GraspingHands/GripSmoothCurveFloat.GripSmoothCurveFloat'"));    
    if (gripSmoothCurveFloat.Succeeded())
    {
        GripSmoothCurveFloat = gripSmoothCurveFloat.Object;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load grip smooth curve float for %s"), *GetName());
    }
    
    static ConstructorHelpers::FObjectFinder<UCurveFloat> curlCurveFloat(TEXT("CurveFloat'/VRExpansionPlugin/VRE/Core/GraspingHands/CurlCurveFloat.CurlCurveFloat'"));
    if (curlCurveFloat.Succeeded())
    {
        CurlCurveFloat = curlCurveFloat.Object;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load curl curve float for %s"), *GetName());
    }
    
    static ConstructorHelpers::FObjectFinder<UCurveFloat> lerpBackCurveFloat(TEXT("CurveFloat'/VRExpansionPlugin/VRE/Core/GraspingHands/LerpBackCurveFloat.LerpBackCurveFloat'"));
    if (lerpBackCurveFloat.Succeeded())
    {
        LerpBackCurveFloat = lerpBackCurveFloat.Object;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load lerp back curve float for %s"), *GetName());
    }

    // Root Physics Component Setup
    RootPhysics = CreateDefaultSubobject<USphereComponent>(TEXT("RootPhysics"));
    RootPhysics->SetupAttachment(RootComponent);
    RootPhysics->SetSphereRadius(4.0f);
    RootPhysics->SetRelativeLocation(FVector(0.235685f, 7.717347f, -1.996791f));
    RootPhysics->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    RootPhysics->SetCollisionObjectType(ECC_WorldDynamic);
    RootPhysics->SetCollisionResponseToAllChannels(ECR_Ignore);

    SimulatingHandConstraint = CreateDefaultSubobject<UVREPhysicsConstraintComponent>(TEXT("SimulatingHandConstraint"));
    SimulatingHandConstraint->SetupAttachment(RootComponent);
    SimulatingHandConstraint->SetRelativeLocation(FVector(8.902467f, 0.000019f, 0.0f));
    SimulatingHandConstraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Free, 0.0f);
    SimulatingHandConstraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Free, 0.0f);
    SimulatingHandConstraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Free, 0.0f);
    SimulatingHandConstraint->SetLinearDriveParams(300000.0f, 30000.0f, 300000.0f);
    SimulatingHandConstraint->SetLinearPositionDrive(true, true, true);
    SimulatingHandConstraint->SetLinearVelocityDrive(true, true, true);
    SimulatingHandConstraint->SetAngularDriveParams(700000.0f, 60000.0f, 700000.0f);
    SimulatingHandConstraint->SetAngularDriveMode(EAngularDriveMode::SLERP);
    SimulatingHandConstraint->SetOrientationDriveSLERP(true);
    SimulatingHandConstraint->SetAngularVelocityDriveSLERP(true);
    

    // Finger Capsule Setup
    thumb_03 = SetupFingerCapsule(FName("Thumb_03"), FName("thumb_03_r"), 0.3f, 1.2f, FVector(0.0f, 0.209252f, -1.398646f), FVector(2.0f, 3.352294f, 2.0f));
    thumb_02 = SetupFingerCapsule(FName("Thumb_02"), FName("thumb_02_r"), 0.4f, 1.8f, FVector(0.0f, 0.309198f, -0.482607f), FVector(2.777476f, 2.777476f, 1.322263f));
    index_03 = SetupFingerCapsule(FName("Index_03"), FName("index_03_r"), 0.3f, 1.5f, FVector(0.0f, 0.110991f, -1.107199f), FVector(2.777476f, 2.777476f, 1.24098f));
    index_02 = SetupFingerCapsule(FName("Index_02"), FName("index_02_r"), 0.5f, 1.5f, FVector(0.0f, 0.307028f, 0.0f), FVector(2.50543f, 2.381881f, 1.322263f));
    middle_03 = SetupFingerCapsule(FName("Middle_03"), FName("middle_03_r"), 0.4f, 1.4f, FVector(0.0f, 0.0f, -0.994521f), FVector(2.206929f, 2.206929f, 1.322263f));
    middle_02 = SetupFingerCapsule(FName("Middle_02"), FName("middle_02_r"), 0.4f, 1.4f, FVector(0.0f, 0.088712f, 0.0f), FVector(2.777476f, 2.777476f, 1.322263f));
    ring_03 = SetupFingerCapsule(FName("Ring_03"), FName("ring_03_r"), 0.35f, 1.2f, FVector(0.0f, 0.141571f, -1.365767f), FVector(2.777476f, 2.777476f, 1.322263f));
    ring_02 = SetupFingerCapsule(FName("Ring_02"), FName("ring_02_r"), 0.5f, 1.2f, FVector(0.0f, 0.315437f, 0.0f), FVector(2.203802f, 2.203802f, 1.830621f));
    pinky_03 = SetupFingerCapsule(FName("Pinky_03"), FName("pinky_03_r"), 0.3f, 1.0f, FVector(0.0f, 0.225811f, -1.116105f), FVector(2.546638f, 2.546638f, 1.322263f));
    pinky_02 = SetupFingerCapsule(FName("Pinky_02"), FName("pinky_02_r"), 0.4f, 1.0f, FVector(0.0f, 0.255944f, 0.0f), FVector(2.777476f, 2.777476f, 1.650782f));

    // Open XR Hand Pose Component Setup
    OpenXRHandPose = CreateDefaultSubobject<UOpenXRHandPoseComponent>(TEXT("OpenXRHandPose"));
    OpenXRHandPose->PrimaryComponentTick.bStartWithTickEnabled = false;
    OpenXRHandPose->SetDetectGestures(false);
    static ConstructorHelpers::FObjectFinder<UOpenXRGestureDatabase> GesturesDBAsset(TEXT("OpenXRGestureDatabase'/VRExpansionPlugin/VRE/Core/GraspingHands/Gestures.Gestures'"));
    if (GesturesDBAsset.Succeeded())
    {
        OpenXRHandPose->GesturesDB = GesturesDBAsset.Object;
    }
    FBPOpenXRActionSkeletalData handSkeletalAction;
    handSkeletalAction.TargetHand = EVRSkeletalHandIndex::EActionHandIndex_Left;
    OpenXRHandPose->HandSkeletalActions.Add(handSkeletalAction);
    
    VREPhysicalAnimation = CreateDefaultSubobject<UVREPhysicalAnimationComponent>(TEXT("VREPhysicalAnimation"));

}

void AGraspingHandManny::BeginPlay()
{
    Super::BeginPlay();

    // Setup Timeline Functions
    if (GripSmoothCurveFloat)
    {
        FOnTimelineFloat GripSmoothTimelineProgress;
        FOnTimelineEvent GripSmoothTimelineFinishedEvent;
        GripSmoothTimelineProgress.BindUFunction(this, FName("OnTimeline_GripSmooth_Update"));
        GripSmoothTimelineFinishedEvent.BindUFunction(this, FName("OnTimeline_GripSmooth_Finished"));

        GripSmoothTimeline.AddInterpFloat(GripSmoothCurveFloat, GripSmoothTimelineProgress);
        GripSmoothTimeline.SetTimelineFinishedFunc(GripSmoothTimelineFinishedEvent);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No grip smooth curve float set for %s"), *GetName());
    }

    if (CurlCurveFloat)
    {
        FOnTimelineFloat CurlTimelineProgress;
        FOnTimelineEvent CurlTimelineFinishedEvent;
        CurlTimelineProgress.BindUFunction(this, FName("OnTimeline_Curl_Update"));
        CurlTimelineFinishedEvent.BindUFunction(this, FName("OnTimeline_Curl_Finished"));

        CurlTimeline.AddInterpFloat(CurlCurveFloat, CurlTimelineProgress);
        CurlTimeline.SetTimelineFinishedFunc(CurlTimelineFinishedEvent);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No curl curve float set for %s"), *GetName());
    }
    
    if (LerpBackCurveFloat)
    {
        FOnTimelineFloat LerpBackTimelineProgress;
        FOnTimelineEvent LerpBackTimelineFinishedEvent;
        LerpBackTimelineProgress.BindUFunction(this, FName("OnTimeline_LerpBack_Update"));
        LerpBackTimelineFinishedEvent.BindUFunction(this, FName("OnTimeline_LerpBack_Finished"));
        
        LerpBackTimeline.AddInterpFloat(LerpBackCurveFloat, LerpBackTimelineProgress);
        LerpBackTimeline.SetTimelineFinishedFunc(LerpBackTimelineFinishedEvent);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No lerp back curve float set for %s"), *GetName());
    }


    GetWorldTimerManager().SetTimer(ValidityCheckTimerHandle, [this]()
    {
        if (IsValid(OwningController) && IsValid(OwningController->CustomPivotComponent))
        {
            GetWorldTimerManager().ClearTimer(ValidityCheckTimerHandle);
            
            // Proceed with begin play after components are valid
            SetupAfterComponentsAreValid();
        }
    }, .1f, true); 
}

void AGraspingHandManny::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (GripSmoothTimeline.IsPlaying())
    {
        GripSmoothTimeline.TickTimeline(DeltaTime);
    }
    if (CurlTimeline.IsPlaying())
    {
        CurlTimeline.TickTimeline(DeltaTime);
    }
    if (LerpBackTimeline.IsPlaying())
    {
        LerpBackTimeline.TickTimeline(DeltaTime);
    }

}

void AGraspingHandManny::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    SetupFingerOverlapBindings();
    RootPhysics->SetMassOverrideInKg(NAME_None, 0.001f);
}

void AGraspingHandManny::SetupFingerOverlapBindings()
{
    thumb_03->OnComponentBeginOverlap.AddDynamic(this, &AGraspingHandManny::OnThumb3BeginOverlap);
    thumb_03->OnComponentEndOverlap.AddDynamic(this, &AGraspingHandManny::OnThumb3EndOverlap);
    thumb_02->OnComponentBeginOverlap.AddDynamic(this, &AGraspingHandManny::OnThumb2BeginOverlap);
    thumb_02->OnComponentEndOverlap.AddDynamic(this, &AGraspingHandManny::OnThumb2EndOverlap);
    index_03->OnComponentBeginOverlap.AddDynamic(this, &AGraspingHandManny::OnIndex3BeginOverlap);
    index_03->OnComponentEndOverlap.AddDynamic(this, &AGraspingHandManny::OnIndex3EndOverlap);
    index_02->OnComponentBeginOverlap.AddDynamic(this, &AGraspingHandManny::OnIndex2BeginOverlap);
    index_02->OnComponentEndOverlap.AddDynamic(this, &AGraspingHandManny::OnIndex2EndOverlap);
    middle_03->OnComponentBeginOverlap.AddDynamic(this, &AGraspingHandManny::OnMiddle3BeginOverlap);
    middle_03->OnComponentEndOverlap.AddDynamic(this, &AGraspingHandManny::OnMiddle3EndOverlap);
    middle_02->OnComponentBeginOverlap.AddDynamic(this, &AGraspingHandManny::OnMiddle2BeginOverlap);
    middle_02->OnComponentEndOverlap.AddDynamic(this, &AGraspingHandManny::OnMiddle2EndOverlap);
    ring_03->OnComponentBeginOverlap.AddDynamic(this, &AGraspingHandManny::OnRing3BeginOverlap);
    ring_03->OnComponentEndOverlap.AddDynamic(this, &AGraspingHandManny::OnRing3EndOverlap);
    ring_02->OnComponentBeginOverlap.AddDynamic(this, &AGraspingHandManny::OnRing2BeginOverlap);
    ring_02->OnComponentEndOverlap.AddDynamic(this, &AGraspingHandManny::OnRing2EndOverlap);
    pinky_03->OnComponentBeginOverlap.AddDynamic(this, &AGraspingHandManny::OnPinky3BeginOverlap);
    pinky_03->OnComponentEndOverlap.AddDynamic(this, &AGraspingHandManny::OnPinky3EndOverlap);
    pinky_02->OnComponentBeginOverlap.AddDynamic(this, &AGraspingHandManny::OnPinky2BeginOverlap);
    pinky_02->OnComponentEndOverlap.AddDynamic(this, &AGraspingHandManny::OnPinky2EndOverlap);
}

void AGraspingHandManny::SetupAfterComponentsAreValid()
{
    SetupFingerAnimations();
    OwningController->GetHandType(Laterality);
    GetOrSpawnAttachmentProxy(); // Temp until engine bug fixes
    BoneName = FName("hand_r"); // TODO: Make this a conditional check, depending on which HandType is being used. The blueprint states the conditional check doesn't work with the new manny skeleton.

    USkeletalMeshComponent* skelMesh = GetSkeletalMeshComponent();
    if (HasAuthority())
    {
        // Repped to end clients
        BaseRelativeTransform = UKismetMathLibrary::MakeRelativeTransform(skelMesh->GetComponentTransform(), OwningController->GetComponentTransform());
        UE_LOG(LogTemp, Warning, TEXT("BaseRelativeTransform: %s"), *BaseRelativeTransform.ToString());
    }

    if (IsValid(OwningController->CustomPivotComponent))
    {
        OwningController->CustomPivotComponent->SetWorldLocation(RootPhysics->GetComponentLocation(), false, nullptr, ETeleportType::TeleportPhysics);
    }

    SetFingerCapsules(); 
    //AddTickPrerequisiteComponent(this); Blueprint had this but nothing was passed in
    skelMesh->SetTickGroup(ETickingGroup::TG_PrePhysics);

    if (AxREAL_VRCharacter* owningChar = Cast<AxREAL_VRCharacter>(OwningController->GetOwner()))
    {
        BindToEvents(owningChar); // Bind to the grip and climb events
    }

    if (HasAuthority())
    {
        RootPhysics->SetWorldTransform(OwningController->CustomPivotComponent->GetComponentTransform());
        OriginalGripTrans = RootPhysics->GetRelativeTransform();
        OriginalPivotTrans = OwningController->CustomPivotComponent->GetRelativeTransform();
    }
    else
    {
        // Not all transforms were replicated yet so had to add delay
        GetWorldTimerManager().SetTimer(ClientTransformTimerHandle, this, &AGraspingHandManny::SetupClientTransforms, .01f, false);
    }

    if (!OwningController->HasGrippedObjects())
    {
        SetupPhysicsIfNeeded(true, false);
    }

    InitPhysicsSetup();
}

UCapsuleComponent* AGraspingHandManny::SetupFingerCapsule(FName CapsuleName, FName SocketName, float CapsuleRadius, float CapsuleHalfHeight, FVector RelativeLocation, FVector RelativeScale, FRotator RelativeRotation)
{
    UCapsuleComponent* Capsule = CreateDefaultSubobject<UCapsuleComponent>(CapsuleName);
    Capsule->SetupAttachment(RootComponent, SocketName);
    Capsule->SetCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
    Capsule->SetRelativeLocation(RelativeLocation);
    Capsule->SetRelativeScale3D(RelativeScale);
    Capsule->SetRelativeRotation(RelativeRotation);
    Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Capsule->SetCollisionObjectType(ECC_WorldDynamic);

    return Capsule;
    
}

void AGraspingHandManny::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    // Clear all timers related to this actor to ensure proper cleanup
    GetWorldTimerManager().ClearAllTimersForObject(this);
}

void AGraspingHandManny::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AGraspingHandManny, OwningController);
    DOREPLIFETIME(AGraspingHandManny, BaseRelativeTransform);
    DOREPLIFETIME(AGraspingHandManny, PhysicsRoot);
    DOREPLIFETIME(AGraspingHandManny, IsPhysicalHand);
    DOREPLIFETIME(AGraspingHandManny, OriginalGripTrans);
    DOREPLIFETIME(AGraspingHandManny, OtherController);
    DOREPLIFETIME(AGraspingHandManny, UseCurls);
    DOREPLIFETIME(AGraspingHandManny, OriginalPivotTrans);
    DOREPLIFETIME(AGraspingHandManny, AttachmentProxy);
}

void AGraspingHandManny::InitPhysicsSetup()
{
    if (IsValid(PhysicsRoot))
    {
        if (AxREAL_VRCharacter* owningChar = Cast<AxREAL_VRCharacter>(OwningController->GetOwner()))
        {
            VREPhysicalAnimation->AddTickPrerequisiteComponent(GetSkeletalMeshComponent());
            VREPhysicalAnimation->AddTickPrerequisiteComponent(owningChar->VRMovementReference);
            GetSkeletalMeshComponent()->AddTickPrerequisiteComponent(owningChar->VRMovementReference);
            EControllerHand handType;
            OwningController->GetHandType(handType);

            if (handType == EControllerHand::Right)
            {
                owningChar->RightHandGripComponent = RootPhysics;
            }
            else
            {
                owningChar->LeftHandGripComponent = RootPhysics;
            }

            // Set our grippping components and pivot to our custom sphere
            OwningController->SetCustomPivotComponent(RootPhysics);
            CheckForFallbackGrip(0, false);
        }
    }
    else
    {
        CheckForFallbackGrip(0, false);
    }
}

void AGraspingHandManny::SetFingerCapsules()
{
    FingerCollisionZones = {
        {ETriggerIndexes::Thumb_3, thumb_03},
        {ETriggerIndexes::Thumb_2, thumb_02},
        {ETriggerIndexes::Index_3, index_03},
        {ETriggerIndexes::Index_2, index_02},
        {ETriggerIndexes::Middle_3, middle_03},
        {ETriggerIndexes::Middle_2, middle_02},
        {ETriggerIndexes::Ring_3, ring_03},
        {ETriggerIndexes::Ring_2, ring_02},
        {ETriggerIndexes::Pinky_3, pinky_03},
        {ETriggerIndexes::Pinky_2, pinky_02}
    }; 
    for (ETriggerIndexes finger : TEnumRange<ETriggerIndexes>())
    {
        FingersBlocked.Add(finger, false);
        FingerFlex.Add(finger, 0.0f);
        FingersOverlapping.Add(finger, false);
    }
}

void AGraspingHandManny::SetFingerOverlapping(bool IsOverlapping, ETriggerIndexes Finger, AActor* Actor, UPrimitiveComponent* Component)
{
    if (Actor == GrippedObject || Component == GrippedObject)
    {
        FingersOverlapping.Add(Finger, IsOverlapping);
        if (IsOverlapping)
        {
            // Print which finger is overlapping
            FString fingerName = UEnum::GetValueAsString(Finger);
            FingersBlocked.Add(Finger, true);
        }
    }
}

void AGraspingHandManny::HandleCurls(float GripCurl, ETimelineDirection::Type Direction)
{
    TArray<ETriggerIndexes> fingersBlocked;
    FingersBlocked.GetKeys(fingersBlocked);
    bool isForward = (Direction == ETimelineDirection::Type::Forward);
    for (ETriggerIndexes finger : fingersBlocked)
    {
        bool shouldMoveFinger = (isForward || GripCurl <= FingerFlex[finger]);
        if (!shouldMoveFinger)
        {
            continue;
        }

        if (FingersBlocked[finger])
        {
            bool canUnblock = (GripCurl < FingerFlex[finger] || !FingersOverlapping[finger]);
            if (canUnblock)
            {
                FingersBlocked.Add(finger, false);
                FingerMovement(finger, 1.0f, GripCurl);
            }
        }
        else
        {
            FingerMovement(finger, 1.0f, GripCurl);
        }
    }
}

void AGraspingHandManny::FingerMovement(ETriggerIndexes FingerIndex, float CurlSpeed, float AxisInput)
{
    if (FMath::Abs(AxisInput - FingerFlex[FingerIndex]) > 0.05f)
    {
        FingerFlex.Add(FingerIndex, AxisInput);
    }
}

void AGraspingHandManny::EvaluateGrasping()
{
    if (bAlreadyGrasped)
    {
        FBPActorGripInformation gripInfo;
        EBPVRResultSwitch result;
        OwningController->GetGripByID(gripInfo, GraspID, result);
        if (result == EBPVRResultSwitch::OnSucceeded)
        {
            TArray<ETriggerIndexes> fingerCollisions;
            FingerCollisionZones.GetKeys(fingerCollisions);
            for (ETriggerIndexes finger : fingerCollisions)
            {
                bool initialOverlap = false;
                if (FingersBlocked[finger])
                {
                    continue;
                }
                switch (gripInfo.GripTargetType)
                {
                    
                    case EGripTargetType::ActorGrip:
                        if (AActor* grippedObject = Cast<AActor>(gripInfo.GrippedObject))
                        {
                            initialOverlap = FingerCollisionZones[finger]->IsOverlappingActor(grippedObject);
                        }
                    case EGripTargetType::ComponentGrip:
                        if (UPrimitiveComponent* grippedComponent = Cast<UPrimitiveComponent>(gripInfo.GrippedObject))
                        {
                            initialOverlap = FingerCollisionZones[finger]->IsOverlappingComponent(grippedComponent);
                        }
                }
                if (initialOverlap)
                {
                    FingersBlocked.Add(finger, true);
                    FingersOverlapping.Add(finger, true);
                }
            }
        }
    }
}

void AGraspingHandManny::SetOverlaps(bool EnableOverlaps)
{
    thumb_02->SetCollisionEnabled(EnableOverlaps ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    thumb_03->SetCollisionEnabled(EnableOverlaps ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    index_02->SetCollisionEnabled(EnableOverlaps ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    index_03->SetCollisionEnabled(EnableOverlaps ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    middle_02->SetCollisionEnabled(EnableOverlaps ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    middle_03->SetCollisionEnabled(EnableOverlaps ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    ring_02->SetCollisionEnabled(EnableOverlaps ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    ring_03->SetCollisionEnabled(EnableOverlaps ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    pinky_02->SetCollisionEnabled(EnableOverlaps ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    pinky_03->SetCollisionEnabled(EnableOverlaps ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void AGraspingHandManny::SetupPhysicsIfNeeded(bool SimulationOn, bool isSetRelativeTransform)
{
    if (SimulationOn)
    {
        if (IsValid(PhysicsRoot))
        {
            ConstraintCheck_Handle = UKismetSystemLibrary::K2_SetTimer(this, "CheckConstraintDistance", ConstraintCheckRate, true);
            USkeletalMeshComponent* skelMesh = GetSkeletalMeshComponent();
            skelMesh->SetAllMassScale(1.0f);
            skelMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            skelMesh->SetSimulatePhysics(true);
            SimulatingHandConstraint->SetWorldLocation(skelMesh->GetCenterOfMass());
            SimulatingHandConstraint->SetConstrainedComponents(PhysicsRoot, NAME_None, skelMesh, BoneName);

            if (HasInitializedConstraint)
            {
                SimulatingHandConstraint->SetConstraintReferenceFrame(EConstraintFrame::Frame1, Frame1);
                SimulatingHandConstraint->SetConstraintReferenceFrame(EConstraintFrame::Frame2, Frame2);
            }

            else
            {
                SimulatingHandConstraint->GetConstraintReferenceFrame(EConstraintFrame::Frame1, Frame1);
                SimulatingHandConstraint->GetConstraintReferenceFrame(EConstraintFrame::Frame2, Frame2);
                HasInitializedConstraint = true;
            }

            SimulatingHandConstraint->SetConstraintToForceBased(true);
            OwningController->bDisableLowLatencyUpdate = true;

            if (IsValid(PhysicsRoot))
            {
                if (bSetupWeldedDriver)
                {
                    VREPhysicalAnimation->RefreshWeldedBoneDriver();
                }

                else
                {
                   VREPhysicalAnimation->SetSkeletalMeshComponent(skelMesh); 
                   VREPhysicalAnimation->SetupWeldedBoneDriver(TArray<FName>{BoneName});
                   bSetupWeldedDriver = true;

                    if (HasAuthority())
                    {
                        IsPhysicalHand = true;
                    }
                }
            }
        }
    }
    else
    {
        SimulatingHandConstraint->BreakConstraint();
        if (ConstraintCheck_Handle.IsValid())
        {
            GetWorld()->GetTimerManager().ClearTimer(ConstraintCheck_Handle);
        }
        GetSkeletalMeshComponent()->SetSimulatePhysics(false);
        
        if (IsPhysicalHand)
        {
            VREPhysicalAnimation->RefreshWeldedBoneDriver();
        }
        else
        {
            GetSkeletalMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }

        GetSkeletalMeshComponent()->SetAllMassScale(0.0f);
        return;
    }
    
}

void AGraspingHandManny::ClearFingers()
{
    TArray<ETriggerIndexes> fingersBlocked;
    FingersBlocked.GetKeys(fingersBlocked);
    for (ETriggerIndexes finger : fingersBlocked)
    {
        FingersBlocked.Add(finger, false);
        FingersOverlapping.Add(finger, false);
        FingerFlex.Add(finger, 0.0f);
    }
}

void AGraspingHandManny::SetPhysicalRelativeTrans()
{
    USkeletalMeshComponent* skelMesh = GetSkeletalMeshComponent();
    skelMesh->SetRelativeTransform(BaseRelativeTransform);

    // This was not connected in the blueprint
    //if (skelMesh->GetComponentScale().GetMin() < 0.0f)
    //{
        //skelMesh->SetRelativeTransform(FTransform(FRotator(-180.0f, 0.0f, 0.0f), FVector::Zero(), FVector::One());
    //}
    //else
    //{
        //skelMesh->SetRelativeTransform(BaseRelativeTransform);
    //}
}

void AGraspingHandManny::SetMesh()
{
    // No implementation in blueprint
}

void AGraspingHandManny::SetDynamicFingerCurls(float Thumb, float Index, float Middle, float Ring, float Pinky)
{
    FingerFlex.Add(ETriggerIndexes::Thumb_3, Thumb);
    FingerFlex.Add(ETriggerIndexes::Thumb_2, Thumb);
    FingerFlex.Add(ETriggerIndexes::Index_3, Index);
    FingerFlex.Add(ETriggerIndexes::Index_2, Index);
    FingerFlex.Add(ETriggerIndexes::Middle_3, Middle);
    FingerFlex.Add(ETriggerIndexes::Middle_2, Middle);
    FingerFlex.Add(ETriggerIndexes::Ring_3, Ring);
    FingerFlex.Add(ETriggerIndexes::Ring_2, Ring);
    FingerFlex.Add(ETriggerIndexes::Pinky_3, Pinky);
    FingerFlex.Add(ETriggerIndexes::Pinky_2, Pinky);
}

bool AGraspingHandManny::InitializeAndAttach(FBPActorGripInformation GripInformation, bool SecondaryGrip, bool SkipEvaluation)
{
    FTransform rootTransform;
    SetOverlaps(!SkipEvaluation);
    GrippedObject = GripInformation.GrippedObject;
    if (SecondaryGrip)
    {
        SecondaryGraspID = GripInformation.GripID;
        GraspID = 0;
    }
    else
    {
        SecondaryGraspID = 0;
        GraspID = GripInformation.GripID;
    }
    if (IsValid(PhysicsRoot))
    {
        rootTransform = PhysicsRoot->GetComponentTransform();
        RootPhysics->AttachToComponent(PhysicsRoot, FAttachmentTransformRules(EAttachmentRule::KeepWorld, true), NAME_None);
    }
    if (!SkipEvaluation)
    {
        EvaluateGrasping();
    }

    switch (GripInformation.GripTargetType)
    {
        case EGripTargetType::ActorGrip:
            if (AActor* grippedActor = Cast<AActor>(GripInformation.GrippedObject))
            {
                // Apparently attachment proxy is needed because of engine bug, should test attaching the object to this actor in future
                AttachmentProxy->AttachToComponent(grippedActor->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepWorld, true), GripInformation.GrippedBoneName);
            }
        case EGripTargetType::ComponentGrip:
            if (UPrimitiveComponent* grippedComponent = Cast<UPrimitiveComponent>(GripInformation.GrippedObject))
            {
                AttachmentProxy->AttachToComponent(grippedComponent, FAttachmentTransformRules(EAttachmentRule::KeepWorld, true), GripInformation.GrippedBoneName);
            }
    }

    FTransform newTransform;
    if (bUseTargetMeshTransform)
    {
        newTransform = TargetMeshTransform;
    }
    else
    {
        // If the physics root is valid then we need to use it as the offset
        FTransform owningControllerOrRootTransform = (IsPhysicalHand ? rootTransform : OwningController->GetPivotTransform());
        FTransform secondaryOrPrimaryTransform = (SecondaryGrip ? GripInformation.SecondaryGripInfo.SecondaryRelativeTransform.Inverse() : GripInformation.RelativeTransform);
        FTransform worldObjectTransform = secondaryOrPrimaryTransform * owningControllerOrRootTransform;
        FTransform worldHandTransform = FTransform(BaseRelativeTransform.GetRotation(), BaseRelativeTransform.GetLocation(), FVector::One()) * OwningController->GetComponentTransform();

        // Get our hands position relative to the object
        FTransform handToObject = worldHandTransform.GetRelativeTransform(worldObjectTransform);
        newTransform = handToObject;
    }

    AttachmentProxy->SetRelativeLocationAndRotation(newTransform.GetLocation(), newTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);

    AttachToComponent(AttachmentProxy, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true), NAME_None);

    SetupPhysicsIfNeeded(false, false);

    return true;
}

bool AGraspingHandManny::RemoveAndLerpBack(uint8 GripID, bool ForceDetach)
{
    if (ForceDetach || GripID == GraspID || GripID == SecondaryGraspID)
    {
        if (!IsPhysicalHand)
        {
            AttachToComponent(OwningController, FAttachmentTransformRules(EAttachmentRule::KeepWorld, true), NAME_None);
        }
        bAlreadyGrasped = false;
        GraspID = 0;
        SecondaryGraspID = 0;
        SetOverlaps(false);
        
        USkeletalMeshComponent* skelMesh = GetSkeletalMeshComponent();
        BeginLerpTransform = skelMesh->GetRelativeTransform();

        if (IsValid(PhysicsRoot))
        {
            RootPhysics->AttachToComponent(skelMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), NAME_None);
            RootPhysics->SetRelativeTransform(OriginalGripTrans);
        }
        return true;
    }
    return false;
}

void AGraspingHandManny::ClimbingStarted(UGripMotionControllerComponent* ClimbingHand, UPrimitiveComponent* ObjectGripped, FTransform RelativeTransform)
{
    IsClimbing = (ClimbingHand == OwningController);

    if (IsClimbing)
    {
        InitializeAndAttachClimbing(ObjectGripped, RelativeTransform);
        FinalizeAttach(true);
        bAlreadyGrasped = true;
        bIsSecondaryGrip = false;
    }
}

void AGraspingHandManny::ClimbingEnded()
{
    if (IsClimbing)
    {
        if (RemoveAndLerpBack(0, true))
        {
            FinalizeDetach();
            bAlreadyGrasped = false;
        }
    }
}

bool AGraspingHandManny::InitializeAndAttachClimbing(UPrimitiveComponent* Target, FTransform RelativeTransform)
{
    SetOverlaps(true);
    GrippedObject = Target;
    GraspID = 0;

    // Get our hands position relative to the object
    FTransform transformedPivot = BaseRelativeTransform * OriginalPivotTrans.Inverse();
    FTransform transformedTarget = RelativeTransform * Target->GetComponentTransform();
    FTransform handToObject = transformedPivot * transformedTarget;

    AttachmentProxy->SetWorldLocationAndRotation(handToObject.GetLocation(), handToObject.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
    AttachmentProxy->AttachToComponent(Target, FAttachmentTransformRules(EAttachmentRule::KeepWorld, true), NAME_None);
    if (IsValid(PhysicsRoot))
    {
        RootPhysics->AttachToComponent(PhysicsRoot, FAttachmentTransformRules(EAttachmentRule::KeepWorld, true));
    }

    EvaluateGrasping();
    AttachToComponent(AttachmentProxy, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true), NAME_None);
    SetupPhysicsIfNeeded(false, false);
    return true;
}

void AGraspingHandManny::UpdateToNewGripTransform(bool ReCurlFingers)
{
    // No blueprint implementation
}

void AGraspingHandManny::RetrievePoses(FBPActorGripInformation GripInfo)
{
    bool shouldRetrieve = (bIsSecondaryGrip ? GripInfo.SecondaryGripInfo.bIsSlotGrip : GripInfo.bIsSlotGrip);

    bUseTargetMeshTransform = false;
    HasCustomAnimation = false;
    CustomAnimIsSnapShot = false;
    CustomSnapShot = FPoseSnapshot();

    if (shouldRetrieve)
    {
        FName socketName = (bIsSecondaryGrip ? GripInfo.SecondaryGripInfo.SecondarySlotName : GripInfo.SlotName);
        UHandSocketComponent* handSocket = UHandSocketComponent::GetHandSocketComponentFromObject(GripInfo.GrippedObject, socketName);

        if (IsValid(handSocket))
        {
            EControllerHand handType;
            OwningController->GetHandType(handType);

            bool isRightHand = (handType == EControllerHand::Right);

            TargetMeshTransform = handSocket->GetMeshRelativeTransform(isRightHand);
            bUseTargetMeshTransform = true;

            FPoseSnapshot poseSnapShot;
            if (handSocket->GetBlendedPoseSnapShot(poseSnapShot, GetSkeletalMeshComponent()))
            {
                CustomSnapShot = poseSnapShot;
                HasCustomAnimation = true;
                CustomAnimIsSnapShot = true;
            }
        }
    }
}

void AGraspingHandManny::GetOrSpawnAttachmentProxy()
{
    if (HasAuthority())
    {
        TArray<UNoRepSphereComponent*> sphereComponents;
        OwningController->GetOwner()->GetComponents<UNoRepSphereComponent>(sphereComponents);


        EControllerHand handType;
        OwningController->GetHandType(handType);
        bool isRightHand = (handType == EControllerHand::Right);
        
        for (UActorComponent* sphereComponent : sphereComponents)
        {
            if (sphereComponent->ComponentHasTag(isRightHand ? "ATTACHPROXYRIGHT" : "ATTACHPROXYLEFT"))
            {
                AttachmentProxy = Cast<UNoRepSphereComponent>(sphereComponent);
            }
        }

        if (!IsValid(AttachmentProxy))
        {
            UActorComponent* newComponent = OwningController->GetOwner()->AddComponentByClass(UNoRepSphereComponent::StaticClass(), true, FTransform::Identity, true);
            AttachmentProxy = Cast<UNoRepSphereComponent>(newComponent);

            AttachmentProxy->SetSphereRadius(4.0f, false);
            AttachmentProxy->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
            AttachmentProxy->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
            AttachmentProxy->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
            AttachmentProxy->SetAllMassScale(0.0f);
            UE_LOG(LogTemp, Warning, TEXT("Attachment Proxy Attach to component running"));
            AttachmentProxy->AttachToComponent(RootPhysics, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepRelative, false), NAME_None);
        }

        TArray<FName> rightHandTag = {TEXT("ATTACHPROXYRIGHT")};
        TArray<FName> leftHandTag = {TEXT("ATTACHPROXYLEFT")};

        AttachmentProxy->ComponentTags = isRightHand ? rightHandTag : leftHandTag;

    }
}

void AGraspingHandManny::ResetAttachmentProxy()
{
    if (IsValid(AttachmentProxy))
    {
        AttachmentProxy->AttachToComponent(OwningController, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), NAME_None);
    }
}

void AGraspingHandManny::BindToEvents(AxREAL_VRCharacter* OwningChar)
{
    if (IsValid(OwningController))
    {
        OwningController->OnGrippedObject.AddDynamic(this, &AGraspingHandManny::OnGrippedObject);
        OwningController->OnDroppedObject.AddDynamic(this, &AGraspingHandManny::OnDroppedObject);
        OwningController->OnSocketingObject.AddDynamic(this, &AGraspingHandManny::OnSocketingObject);
        OtherController->OnSecondaryGripAdded.AddDynamic(this, &AGraspingHandManny::SecondaryAddedOnOther);
        OtherController->OnSecondaryGripRemoved.AddDynamic(this, &AGraspingHandManny::SecondaryRemovedOnOther);
        OwningController->OnGripTransformChanged.AddDynamic(this, &AGraspingHandManny::OnGripTransformChanged);
        OwningController->OnLerpToHandFinished.AddDynamic(this, &AGraspingHandManny::OnLerpToHandFinished);
        OwningChar->OnClimbingInitiated.AddDynamic(this, &AGraspingHandManny::ClimbingStarted);
        OwningChar->OnClimbingEnded.AddDynamic(this, &AGraspingHandManny::ClimbingEnded);
        OwningChar->OnCharacterTeleported_Bind.AddDynamic(this, &AGraspingHandManny::OnOwnerTeleported);
        OwningChar->OnCharacterNetworkCorrected_Bind.AddDynamic(this, &AGraspingHandManny::OnCharacterCorrected);
    }
}

void AGraspingHandManny::SetupFingerAnimations()
{
    FBPOpenXRActionSkeletalData handAction = OpenXRHandPose->HandSkeletalActions[0];
    EControllerHand handType;
    OwningController->GetHandType(handType);
    EVRSkeletalHandIndex handIndex = handType == EControllerHand::Right ? EVRSkeletalHandIndex::EActionHandIndex_Right : EVRSkeletalHandIndex::EActionHandIndex_Left;
    handAction.TargetHand = handIndex;
    handAction.bMirrorLeftRight = (handType == EControllerHand::Left);

    OpenXRHandPose->SetComponentTickEnabled(UseCurls);
}

void AGraspingHandManny::OnGrippedObject(UPARAM(ref) const FBPActorGripInformation &GripInfo)
{
    InitGrip(GripInfo);
}

void AGraspingHandManny::OnGripTransformChanged(UPARAM(ref) const FBPActorGripInformation& GripInfo){
    if (GripInfo.GripID == GraspID)
    {
        InitializeAndAttach(GripInfo, bIsSecondaryGrip, true);
    }
}

void AGraspingHandManny::OnDroppedObject(UPARAM(ref) const FBPActorGripInformation& GripInfo, bool bWasSocketed)
{
    HasCustomAnimation = false;
    if (RemoveAndLerpBack(GripInfo.GripID, false))
    {
        FinalizeDetach();
        CheckForFallbackGrip(GripInfo.GripID, false);
    }
}

void AGraspingHandManny::OnSocketingObject(UPARAM(ref) const FBPActorGripInformation& GripInfo, const USceneComponent* NewParentComp, FName OptionalSocketName, FTransform RelativeTransformToParent, bool WeldingBodies)
{
    HasCustomAnimation = false;
    if (RemoveAndLerpBack(GripInfo.GripID, false))
    {
        FinalizeDetach();
        CheckForFallbackGrip(GripInfo.GripID, false);
    }
}

void AGraspingHandManny::OnLerpToHandFinished(UPARAM(ref) const FBPActorGripInformation &GripInfo)
{
    // Made into a function so we can check bIsLerping
    InitGrip(GripInfo);
}

void AGraspingHandManny::SecondaryAddedOnOther(UPARAM(ref) const FBPActorGripInformation &GripInfo)
{
    if ((GripInfo.SecondaryGripInfo.SecondaryAttachment == OwningController) || 
    (GripInfo.SecondaryGripInfo.SecondaryAttachment->GetAttachParent() == OwningController))
    {
        if (IGameplayTagAssetInterface* grippedObjectInterface = Cast<IGameplayTagAssetInterface>(GripInfo.GrippedObject))
        {
            bool hasMatchingTag = grippedObjectInterface->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("GripSockets.DontAttachHand"));
            if (!hasMatchingTag && !bAlreadyGrasped)
            {
                bAlreadyGrasped = true;
                bIsSecondaryGrip = true;
                RetrievePoses(GripInfo);
                InitializeAndAttach(GripInfo, true, HasCustomAnimation);
                FinalizeAttach(false);
            }
        }
    }
}

void AGraspingHandManny::SecondaryRemovedOnOther(UPARAM(ref) const FBPActorGripInformation &GripInfo)
{
    if (RemoveAndLerpBack(GripInfo.GripID, false))
    {
        HasCustomAnimation = false;
        bIsSecondaryGrip = false;
        FinalizeDetach();
        CheckForFallbackGrip(GripInfo.GripID, true);
    }
}

void AGraspingHandManny::FinalizeAttach(bool isClimbing)
{
    if (IsValid(PhysicsRoot))
    {
        if (!isClimbing || bAllowLerpingPhysicalHandsWhenClimbing)
        {
            GripSmoothTimeline.PlayFromStart();
        }
    }

    if (HasCustomAnimation)
    {
        HandAnimState = EHandAnimState::Custom; 
        CurlTimeline.Stop();
    }
    else
    {
        HandAnimState = EHandAnimState::Hand_Dynamic;
        CurlTimeline.Play();
    }

    LerpBackTimeline.Stop();
}

void AGraspingHandManny::FinalizeDetach()
{
    if (IsPhysicalHand)
    {
        USkeletalMeshComponent* skelMesh = GetSkeletalMeshComponent();
        // Refresh the physical body
        skelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        skelMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        SetupPhysicsIfNeeded(true, true);
    }
    else
    {
        LerpBackTimeline.PlayFromStart();
    }

    if (IsPhysicalHand)
    {
        HandAnimState = EHandAnimState::Hand_Animated;
        HadCurled = false;
        ClearFingers();
        CurlTimeline.Stop();
        CurlTimeline.SetNewTime(0.0f);
    }
    else
    {
        HandAnimState = EHandAnimState::Hand_Dynamic;
        CurlTimeline.Reverse();
    }

    GripSmoothTimeline.Stop();
    ResetAttachmentProxy();
}

void AGraspingHandManny::InitGrip(FBPActorGripInformation GripInfo)
{
    if (IGameplayTagAssetInterface* grippedObjectInterface = Cast<IGameplayTagAssetInterface>(GripInfo.GrippedObject))
    {
        bool hasMatchingTag = grippedObjectInterface->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("GripSockets.DontAttachHand"));
        bool alreadyGraspedAndNotSecondary = !bIsSecondaryGrip && bAlreadyGrasped;
        if (!hasMatchingTag && !alreadyGraspedAndNotSecondary && !GripInfo.bIsLerping)
        {
            bAlreadyGrasped = true;
            bIsSecondaryGrip = false;
            RetrievePoses(GripInfo);
            InitializeAndAttach(GripInfo, false, HasCustomAnimation);
            FinalizeAttach(false);
        }
    }
}

void AGraspingHandManny::CheckConstraintDistance()
{
    float linearDistance = SimulatingHandConstraint->GetCurrentLinearDistance(EConstraintFrame::Frame1).SquaredLength();
    if (linearDistance > FMath::Square(MaxConstraintDistance))
    {
        USkeletalMeshComponent* skelMesh = GetSkeletalMeshComponent();
        // Taking the origin of the skeletal mesh bounding box and converting it to the local space of the skel mesh component.
        FVector localBoundsOrigin = skelMesh->GetComponentTransform().InverseTransformPosition(skelMesh->Bounds.Origin);
        FVector spherePos = (BaseRelativeTransform * OwningController->GetComponentTransform()).TransformPosition(localBoundsOrigin);
        float sphereRadius = skelMesh->Bounds.SphereRadius;
        TArray<TEnumAsByte<EObjectTypeQuery>> queryParams = {EObjectTypeQuery::ObjectTypeQuery1, EObjectTypeQuery::ObjectTypeQuery2};
        TArray<AActor*> actorsToIgnore = {this, GetOwner()};
        TArray<UPrimitiveComponent*> overlappingComponents;
        bool hasOverlap = UKismetSystemLibrary::SphereOverlapComponents(GetWorld(), spherePos, sphereRadius, queryParams, nullptr, actorsToIgnore, overlappingComponents);
        // Don't teleport if it's going to put us into collision anyway.
        if (!hasOverlap)
        {
            OnOwnerTeleported();
        }
    }
}

void AGraspingHandManny::OnOwnerTeleported()
{
    OnTeleportIfPhysicalHand();
}

void AGraspingHandManny::OnCharacterCorrected()
{
    OnTeleportIfPhysicalHand();
}

void AGraspingHandManny::OnTeleportIfPhysicalHand()
{
    if (IsPhysicalHand && !OwningController->HasGrippedObjects() && !bIsSecondaryGrip)
    {
        //TODO: There was a delay here of 0 seconds to wait for post physics, see if it is necessary in C++
        USkeletalMeshComponent* skelMesh = GetSkeletalMeshComponent();
        skelMesh->SetSimulatePhysics(false);
        SetActorTransform(BaseRelativeTransform * OwningController->GetComponentTransform(), false, nullptr, ETeleportType::TeleportPhysics);
        skelMesh->AttachToComponent(OwningController, FAttachmentTransformRules::KeepWorldTransform, NAME_None);
        // There was also a 0 delay here
        skelMesh->SetSimulatePhysics(true);
        VREPhysicalAnimation->RefreshWeldedBoneDriver();
    }
}

bool AGraspingHandManny::CheckForFallbackGrip(uint8 OriginalGripID, bool WasSecondary)
{
    if (OwningController->IsBeingDestroyed())
    {
       return false; 
    }

    if (OwningController->HasGrippedObjects())
    {
        TArray<FBPActorGripInformation> grips;
        OwningController->GetAllGrips(grips);
        for (FBPActorGripInformation grip : grips)
        {
            if (!grip.bIsPaused && !grip.bIsPendingKill)
            {
                if (WasSecondary || (!WasSecondary && grip.GripID != OriginalGripID))
                {
                    OnGrippedObject(grip);
                    return true;
                }
            }
        }
    }
    FBPActorGripInformation gripInfo;
    if (OtherController->GetIsSecondaryAttachment(OwningController, gripInfo))
    {
        bool isNotSecondaryOrDifferent = (WasSecondary && gripInfo.GripID != OriginalGripID) || (!WasSecondary);
        if (isNotSecondaryOrDifferent && !gripInfo.bIsPaused && !gripInfo.bIsPendingKill)
        {
            SecondaryAddedOnOther(gripInfo);
            return true;
        }
    }
    return false;
}

void AGraspingHandManny::SetupClientTransforms()
{
    if (!OwningController->HasGrippedObjects())
    {
        GetSkeletalMeshComponent()->SetWorldTransform(BaseRelativeTransform * OwningController->GetComponentTransform());
    }

    OwningController->CustomPivotComponent->SetRelativeTransform(OriginalPivotTrans);
    // Use repped value
    RootPhysics->SetRelativeTransform(OriginalGripTrans);
}

void AGraspingHandManny::OnTimeline_GripSmooth_Update(float Value)
{
    RootPhysics->SetRelativeTransform(UKismetMathLibrary::TLerp(RootPhysics->GetRelativeTransform(), FTransform::Identity, Value));
}

void AGraspingHandManny::OnTimeline_GripSmooth_Finished()
{
    RootPhysics->SetRelativeTransform(FTransform::Identity);
}

void AGraspingHandManny::OnTimeline_Curl_Update(float Value)
{
    EvaluateGrasping();
    HandleCurls(Value, CurlTimeline.IsReversing() ? ETimelineDirection::Backward : ETimelineDirection::Forward);
}

void AGraspingHandManny::OnTimeline_Curl_Finished()
{
    HadCurled = false;
    HandAnimState = CurlTimeline.IsReversing() ? EHandAnimState::Hand_Animated : EHandAnimState::Hand_Frozen;
    SetOverlaps(false);
}

void AGraspingHandManny::OnTimeline_LerpBack_Update(float Value)
{
    GetSkeletalMeshComponent()->SetRelativeTransform(UKismetMathLibrary::TLerp(BeginLerpTransform, BaseRelativeTransform, Value));

}

void AGraspingHandManny::OnTimeline_LerpBack_Finished()
{
    if (IsPhysicalHand)
    {
        SetPhysicalRelativeTrans();
        // TODO: There was a 0 second delay here
        SetupPhysicsIfNeeded(true, true);
    }
}

void AGraspingHandManny::OnThumb3BeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult)
{
    SetFingerOverlapping(true, ETriggerIndexes::Thumb_3, OtherActor, OtherComp);
    SetFingerOverlapping(true, ETriggerIndexes::Thumb_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnThumb3EndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex)
{
    SetFingerOverlapping(false, ETriggerIndexes::Thumb_3, OtherActor, OtherComp);
    SetFingerOverlapping(false, ETriggerIndexes::Thumb_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnThumb2BeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult)
{
    SetFingerOverlapping(true, ETriggerIndexes::Thumb_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnThumb2EndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex)
{
    SetFingerOverlapping(false, ETriggerIndexes::Thumb_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnIndex3BeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult)
{
    SetFingerOverlapping(true, ETriggerIndexes::Index_3, OtherActor, OtherComp);
    SetFingerOverlapping(true, ETriggerIndexes::Index_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnIndex3EndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex)
{
    SetFingerOverlapping(false, ETriggerIndexes::Index_3, OtherActor, OtherComp);
    SetFingerOverlapping(false, ETriggerIndexes::Index_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnIndex2BeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult)
{
    SetFingerOverlapping(true, ETriggerIndexes::Index_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnIndex2EndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex)
{
    SetFingerOverlapping(false, ETriggerIndexes::Index_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnMiddle3BeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult)
{
    SetFingerOverlapping(true, ETriggerIndexes::Middle_3, OtherActor, OtherComp);
    SetFingerOverlapping(true, ETriggerIndexes::Middle_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnMiddle3EndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex)
{
    SetFingerOverlapping(false, ETriggerIndexes::Middle_3, OtherActor, OtherComp);
    SetFingerOverlapping(false, ETriggerIndexes::Middle_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnMiddle2BeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult)
{
    SetFingerOverlapping(true, ETriggerIndexes::Middle_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnMiddle2EndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex)
{
    SetFingerOverlapping(false, ETriggerIndexes::Middle_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnRing3BeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult)
{
    SetFingerOverlapping(true, ETriggerIndexes::Ring_3, OtherActor, OtherComp);
    SetFingerOverlapping(true, ETriggerIndexes::Ring_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnRing3EndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex)
{
    SetFingerOverlapping(false, ETriggerIndexes::Ring_3, OtherActor, OtherComp);
    SetFingerOverlapping(false, ETriggerIndexes::Ring_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnRing2BeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult)
{
    SetFingerOverlapping(true, ETriggerIndexes::Ring_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnRing2EndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex)
{
    SetFingerOverlapping(false, ETriggerIndexes::Ring_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnPinky3BeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult)
{
    SetFingerOverlapping(true, ETriggerIndexes::Pinky_3, OtherActor, OtherComp);
    SetFingerOverlapping(true, ETriggerIndexes::Pinky_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnPinky3EndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex)
{
    SetFingerOverlapping(false, ETriggerIndexes::Pinky_3, OtherActor, OtherComp);
    SetFingerOverlapping(false, ETriggerIndexes::Pinky_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnPinky2BeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult)
{
    SetFingerOverlapping(true, ETriggerIndexes::Pinky_2, OtherActor, OtherComp);
}

void AGraspingHandManny::OnPinky2EndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex)
{
    SetFingerOverlapping(false, ETriggerIndexes::Pinky_2, OtherActor, OtherComp);
}