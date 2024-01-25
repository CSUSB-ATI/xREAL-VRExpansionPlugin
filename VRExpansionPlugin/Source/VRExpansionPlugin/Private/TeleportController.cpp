// Fill out your copyright notice in the Description page of Project Settings.


#include "TeleportController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "VRExpansionFunctionLibrary.h"
#include "DrawDebugHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "xREAL_VRCharacter.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"


ATeleportController::ATeleportController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.bCanEverTick = true;
    bNetLoadOnClient = false;
    SetTickGroup(ETickingGroup::TG_PostPhysics); //Temporary fix for the lagging of the teleport arc

    TeleportLaunchVelocity = 1200.0f;
    LaserBeamMaxDistance = 5000.0f;
    RotOffset = FRotator(0.0f, -60.0f, 0.0f);

    Scene = CreateDefaultSubobject<USceneComponent>(TEXT("Scene"));
    RootComponent = Scene;

    // Setting up ArcSpline and LaserSpline
    ArcSpline = CreateDefaultSubobject<USplineComponent>(TEXT("ArcSpline"));
    ArcSpline->SetupAttachment(Scene);
    LaserSpline = CreateDefaultSubobject<USplineComponent>(TEXT("LaserSpline"));
    LaserSpline->SetupAttachment(Scene);

    // Setting up ArcEndPoint
    ArcEndPoint = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArcEndPoint"));
    ArcEndPoint->SetupAttachment(Scene);
    ArcEndPoint->SetRelativeScale3D(FVector(.15f, .15f, .15f));
    ArcEndPoint->SetStaticMesh(Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("StaticMesh'/Engine/BasicShapes/Sphere.Sphere'"))));
    ArcEndPoint->SetMaterial(0, Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("Material'/VRExpansionPlugin/VRE/Core/Character/Materials/M_ArcEndpoint.M_ArcEndpoint'"))));
    ArcEndPoint->SetVisibility(false);
    ArcEndPoint->SetGenerateOverlapEvents(false);
    ArcEndPoint->SetCollisionProfileName(FName("NoCollision"));

    // Setting up LaserBeamEndPoint
    LaserBeamEndPoint = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserBeamEndPoint"));
    LaserBeamEndPoint->SetupAttachment(Scene);
    LaserBeamEndPoint->SetRelativeScale3D(FVector(.02f, .02f, .02f));
    LaserBeamEndPoint->SetStaticMesh(Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("StaticMesh'/Engine/BasicShapes/Sphere.Sphere'"))));
    LaserBeamEndPoint->SetMaterial(0, Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("Material'/VRExpansionPlugin/VRE/Core/Character/LaserBeamSplineMat.LaserBeamSplineMat'"))));
    LaserBeamEndPoint->SetGenerateOverlapEvents(false);
    LaserBeamEndPoint->SetCollisionProfileName(FName("NoCollision"));
    LaserBeamEndPoint->SetHiddenInGame(true);

    // Setting up TeleportCylinder
    TeleportCylinder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TeleportCylinder"));
    TeleportCylinder->SetupAttachment(Scene);
    TeleportCylinder->SetRelativeScale3D(FVector(.75f, .75f, 1.0f));
    TeleportCylinder->SetStaticMesh(Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("StaticMesh'/Engine/BasicShapes/Cylinder.Cylinder'")))); 
    TeleportCylinder->SetMaterial(0, Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("Material'/VRExpansionPlugin/VRE/Core/Character/Materials/MI_TeleportCylinderPreview.MI_TeleportCylinderPreview'"))));
    TeleportCylinder->SetGenerateOverlapEvents(false);
    TeleportCylinder->SetCanEverAffectNavigation(false);
    TeleportCylinder->SetCollisionProfileName(FName("NoCollision"));

    Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
    Ring->SetupAttachment(TeleportCylinder);
    Ring->SetRelativeScale3D(FVector(.5f, .5f, .15f));
    Ring->SetStaticMesh(Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("StaticMesh'/VRExpansionPlugin/VRE/Core/Character/Meshes/SM_FatCylinder.SM_FatCylinder'"))));
    Ring->SetMaterial(0, Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("Material'/VRExpansionPlugin/VRE/Core/Character/Materials/M_ArcEndPoint.M_ArcEndPoint'"))));
    Ring->SetGenerateOverlapEvents(false);
    Ring->SetCollisionProfileName(FName("NoCollision"));

    Arrow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Arrow"));
    Arrow->SetupAttachment(TeleportCylinder);
    Arrow->SetStaticMesh(Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("StaticMesh'/VRExpansionPlugin/VRE/Core/Character/Meshes/BeaconDirection.BeaconDirection'"))));
    Arrow->SetMaterial(0, Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("Material'/VRExpansionPlugin/VRE/Core/Character/Materials/M_ArcEndPoint.M_ArcEndPoint'"))));
    Arrow->SetGenerateOverlapEvents(false);
    Arrow->SetCollisionProfileName(FName("NoCollision"));

    FinalFacingArrow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FinalFacingArrow"));
    FinalFacingArrow->SetupAttachment(Arrow);
    FinalFacingArrow->SetStaticMesh(Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("StaticMesh'/VRExpansionPlugin/VRE/Core/Character/Meshes/BeaconDirection.BeaconDirection'"))));
    FinalFacingArrow->SetMaterial(0, Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("Material'/VRExpansionPlugin/VRE/Core/Character/LaserBeamSplineMat.LaserBeamSplineMat'"))));
    FinalFacingArrow->SetGenerateOverlapEvents(false);
    FinalFacingArrow->SetCollisionProfileName(FName("NoCollision"));
    FinalFacingArrow->SetHiddenInGame(true);

    // Setting up LaserBeam
    LaserBeam = CreateDefaultSubobject<USplineMeshComponent>(TEXT("LaserBeam"));
    LaserBeam->SetupAttachment(Scene);
    LaserBeam->SetMobility(EComponentMobility::Movable);
    LaserBeam->SetStaticMesh(Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("StaticMesh'/VRExpansionPlugin/VRE/Core/Character/Meshes/BeamMesh.BeamMesh'"))));
    LaserBeam->SetMaterial(0, Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("Material'/VRExpansionPlugin/VRE/Core/Character/LaserBeamSplineMat.LaserBeamSplineMat'"))));
    // Set Start pos tangent end pos and end tangent of the spline mesh
    LaserBeam->SetStartAndEnd(FVector(0.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), true);
    LaserBeam->SetGenerateOverlapEvents(false);
    LaserBeam->SetCollisionProfileName(FName("NoCollision"));
    LaserBeam->SetHiddenInGame(true);

    // Setting up WidgetInteraction
    WidgetInteraction = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteraction"));
    WidgetInteraction->SetupAttachment(LaserBeam);
    WidgetInteraction->InteractionSource = EWidgetInteractionSource::Custom;
    WidgetInteraction->SetAutoActivate(false);

    // Setting up PhysicsTossManager
    PhysicsTossManager = CreateDefaultSubobject<UPhysicsTossManager>(TEXT("PhysicsTossManager"));

    //Load Teleport Spline Assets
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("StaticMesh'/VRExpansionPlugin/VRE/Core/Character/Meshes/BeamMesh.BeamMesh'"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialAsset(TEXT("Material'/VRExpansionPlugin/VRE/Core/Character/Materials/M_SplineArcMat.M_SplineArcMat'"));
    if (MeshAsset.Succeeded() && MaterialAsset.Succeeded())
    {
        TeleportSplineMesh = MeshAsset.Object;
        TeleportSplineMaterial = MaterialAsset.Object;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load Teleport Spline Assets"));
    }

    //Load Laser Spline Assets
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("StaticMesh'/VRExpansionPlugin/VRE/Core/Character/Meshes/BeamMesh.BeamMesh'"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialAsset(TEXT("Material'/VRExpansionPlugin/VRE/Core/Character/LaserBeamSplineMat.LaserBeamSplineMat'"));
    if (MeshAsset.Succeeded() && MaterialAsset.Succeeded())
    {
        TeleportSplineMesh = MeshAsset.Object;
        TeleportSplineMaterial = MaterialAsset.Object;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load Laser Spline Assets"));
    }

}

void ATeleportController::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

    TeleportCylinder->SetVisibility(false, true);

    if (OwningMotionController->IsValidLowLevel())
    {
        LaserSpline->AttachToComponent(OwningMotionController, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true));
        LaserBeam->AttachToComponent(OwningMotionController, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true));
        LaserBeamEndPoint->AttachToComponent(OwningMotionController, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true));
        LaserBeamEndPoint->SetRelativeScale3D(FVector(.02f, .02f, .02f));

        if (OwningMotionController->IsLocallyControlled())
        {
            EnableInput(playerController);
            AVRCharacter* vrCharacter = Cast<AVRCharacter>(OwningMotionController->GetOwner());
            if (vrCharacter)
            {
                vrCharacter->OnCharacterTeleported_Bind.AddDynamic(this, &ATeleportController::CancelTracking);
                SetOwner(vrCharacter);
            }
        }
    }

    if (playerController->IsValidLowLevel())
    {
        playerController->InputComponent->BindAction("UseHeldObjectLeft", IE_Pressed, this, &ATeleportController::StartedUseHeldObjectLeft);
        playerController->InputComponent->BindAction("UseHeldObjectRight", IE_Pressed, this, &ATeleportController::StartedUseHeldObjectRight);
    }

    
}

void ATeleportController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateLaserBeam(DeltaTime);
    if (IsTeleporterActive) 
    {
        CreateTeleportationArc();
    }
}

void ATeleportController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    ClearArc();
    ClearLaserBeam();
    DisableWidgetActivation();
}

void ATeleportController::ServersideToss_Implementation(UPrimitiveComponent* TargetObject)
{
    PhysicsTossManager->ServersideToss(TargetObject, OwningMotionController);

}

void ATeleportController::SetLaserBeamActive_Implementation(bool LaserBeamActive)
{
    if (LaserBeamActive != IsLaserBeamActive)
    {
        IsLaserBeamActive = LaserBeamActive;
        if (IsLaserBeamActive)
        {
            LaserBeam->SetHiddenInGame(bUseSmoothLaser);
            LaserBeamEndPoint->SetHiddenInGame(bUseSmoothLaser);
            EuroLowPassFilter.ResetSmoothingFilter();
            CreateLaserSpline();
            if (bIsLocal)
            {
                WidgetInteraction->Activate();
            }
            ToggleTick();
        }
        else
        {
            LaserBeam->SetHiddenInGame(true);
            LaserBeamEndPoint->SetHiddenInGame(true);
            ClearLaserBeam();

            // Clearing the HitResult, might not be the right way to do it
            WidgetInteraction->SetCustomHitResult(FHitResult());
            if (bIsLocal)
            {
                DisableWidgetActivation();
            }
            ToggleTick();
        }
    }
}

void ATeleportController::ActivateTeleporter_Implementation()
{
    // Set the flag, rest of the teleportation is handled in the tick function
    IsTeleporterActive = true;

    TeleportCylinder->SetVisibility(true, true);

    // Store rotation to later compare Roll value to support Wrist-based orientation of the teleporter
    if (OwningMotionController->IsValidLowLevel())
    {
        InitialControllerRotation = OwningMotionController->GetComponentRotation();
        ToggleTick();
    }
}

void ATeleportController::DisableTeleporter_Implementation()
{
    if (IsTeleporterActive)
    {
        IsTeleporterActive = false;
        TeleportCylinder->SetVisibility(false, true);
        ArcEndPoint->SetVisibility(false);
        ToggleTick();
        ClearArc();
    }
}

void ATeleportController::TraceTeleportDestination_Implementation(bool &Success, TArray<FVector> &TracePoints, FVector &NavMeshLocation, FVector &TraceLocation)
{
    FVector worldLocation;
    FVector forwardVector;
    GetTeleWorldLocAndForwardVector(worldLocation, forwardVector);

    // Setup Projectile Path Parameters
    FPredictProjectilePathParams Params;
    Params.StartLocation = worldLocation;
    Params.LaunchVelocity = forwardVector * TeleportLaunchVelocity;
    Params.bTraceWithCollision = true;
    Params.ProjectileRadius = 0.0f; // Set this based on your needs
    Params.ObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery1); // Adjust this based on your collision settings
    Params.SimFrequency = 30.0f; // Simulation frequency
    Params.MaxSimTime = 2.0f; // Adjust simulation time as needed

    // Perform the projectile path prediction
    FPredictProjectilePathResult result;
    bool isPathColliding = UGameplayStatics::PredictProjectilePath(this, Params, result);

    // Extract Trace Points and Last Hit Location
    for (const FPredictProjectilePathPointData& PointData : result.PathData)
    {
        TracePoints.Add(PointData.Location);
    }
    FVector hitLocation = result.HitResult.Location;

    // Project Point to Navigation
    FVector projectedLocation;
    float projectNavExtends = 500.0f;
    UNavigationSystemV1::K2_ProjectPointToNavigation(this, hitLocation, projectedLocation, nullptr, nullptr, FVector(projectNavExtends));

    // Set Output Parameters
    NavMeshLocation = projectedLocation;
    TraceLocation = hitLocation;
    Success = ((hitLocation != projectedLocation) && (projectedLocation != FVector::ZeroVector) && isPathColliding);
}

void ATeleportController::ClearArc_Implementation()
{
    for (USplineMeshComponent* mesh : SplineMeshes)
    {
        mesh->DestroyComponent();
    }
    SplineMeshes.Empty();
    ArcSpline->ClearSplinePoints();
}

void ATeleportController::UpdateArcSpline_Implementation(bool FoundValidLocation, UPARAM(ref) TArray<FVector> &SplinePoints)
{
    FVector worldLocation;
    FVector forwardVector;
    int pointDiffNum = 0;
    ArcSpline->ClearSplinePoints(true);
    if (!FoundValidLocation)
    {
        // Create Small Stub line when we failed to find a teleport location
        SplinePoints.Empty();
        GetTeleWorldLocAndForwardVector(worldLocation, forwardVector);
        SplinePoints.Add(worldLocation);
        SplinePoints.Add(worldLocation + (forwardVector * 20.0f));
    }

    for (FVector splinePoint : SplinePoints)
    {
        ArcSpline->AddSplinePoint(splinePoint, ESplineCoordinateSpace::Local, true);
    }
    ArcSpline->SetSplinePointType(SplinePoints.Num() - 1, ESplinePointType::CurveClamped, true);

    int splinePointsLastIndex = ArcSpline->GetNumberOfSplinePoints() - 1;
    if (SplineMeshes.Num() < ArcSpline->GetNumberOfSplinePoints())
    {
        pointDiffNum = splinePointsLastIndex - SplineMeshes.Num();
        for (int i = 0; i <= pointDiffNum; i++)
        {
           USplineMeshComponent* smc = NewObject<USplineMeshComponent>(this, USplineMeshComponent::StaticClass()); 
           
           if (TeleportSplineMesh && TeleportSplineMaterial)
           {
                smc->SetStaticMesh(TeleportSplineMesh);
                smc->SetMaterial(0, TeleportSplineMaterial);
           }
           smc->SetStartScale(FVector2D(4.0f, 4.0f));
           smc->SetEndScale(FVector2D(4.0f, 4.0f));
           smc->SplineBoundaryMax = 1.0f;
           smc->SetMobility(EComponentMobility::Movable);
           smc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
           smc->RegisterComponent();
           SplineMeshes.Add(smc);
        }
    }
    for (int i = 0; i < SplineMeshes.Num(); i++)
    {
        if (i < splinePointsLastIndex)
        {
            SplineMeshes[i]->SetVisibility(true);
            FVector startTangent, endTangent;
            startTangent = ArcSpline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
            endTangent = ArcSpline->GetTangentAtSplinePoint(i+1, ESplineCoordinateSpace::Local);
            SplineMeshes[i]->SetStartAndEnd(SplinePoints[i], startTangent, SplinePoints[i+1], endTangent, true);
        }
        else
        {
            SplineMeshes[i]->SetVisibility(false);
        }
    }
}

void ATeleportController::UpdateArcEndpoint_Implementation(FVector NewLocation, bool ValidLocationFound)
{
    ArcEndPoint->SetVisibility(ValidLocationFound && IsTeleporterActive);
    ArcEndPoint->SetWorldLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
    Arrow->SetWorldRotation(TeleportRotation + TeleportBaseRotation);
}

void ATeleportController::GetTeleportDestination_Implementation(bool RelativeToHMD, FVector &Location, FRotator &Rotation)
{
    FVector devicePosition;
    FQuat deviceRotation;
    GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, deviceRotation, devicePosition);
    if (RelativeToHMD)
    {
        FVector heightAgnosticPosition = FVector(devicePosition.X, devicePosition.Y, 0.0f);
        Location = LastValidTeleportLocation - TeleportRotation.RotateVector(heightAgnosticPosition);
        Rotation = TeleportRotation;
    }
    else
    {
        Location = LastValidTeleportLocation;
        Rotation = TeleportRotation;
    }
}

void ATeleportController::GetTeleWorldLocAndForwardVector_Implementation(FVector &WorldLoc, FVector &ForwardVector)
{
    WorldLoc = OwningMotionController->GetComponentLocation();
    FRotator controllerRotation = OwningMotionController->GetComponentRotation();
    ForwardVector = UKismetMathLibrary::GetForwardVector(UKismetMathLibrary::ComposeRotators(RotOffset, controllerRotation));
}

void ATeleportController::IfOverWidget_Use_Implementation(bool bPressed, bool &WasOverWidget)
{
    if (IsLaserBeamActive)
    {
        if (WidgetInteraction->IsOverInteractableWidget() || WidgetInteraction->IsOverFocusableWidget())
        {
            if (bPressed)
            {
                WidgetInteraction->PressPointerKey(EKeys::LeftMouseButton);
            }
            else
            {
                WidgetInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
            }
            WasOverWidget = true;
            return;
        }
    }
    WasOverWidget = false;
}

void ATeleportController::InitController_Implementation()
{
    if (bIsLocal)
    {
        EControllerHand hand;
        OwningMotionController->GetHandType(hand);
        switch (hand)
        {
        case EControllerHand::Left:
            WidgetInteraction->VirtualUserIndex = 0;
            WidgetInteraction->PointerIndex = 0;
            break;
        
        case EControllerHand::Right:
            WidgetInteraction->VirtualUserIndex = 0;
            WidgetInteraction->PointerIndex = 1;
            break;
        
        default:
            break;
        }
    }
}

void ATeleportController::ToggleTick_Implementation()
{
    SetActorTickEnabled(IsTeleporterActive || IsLaserBeamActive || ActorBeingThrown->IsValidLowLevel());
}

void ATeleportController::ClearLaserBeam_Implementation()
{
    for (USplineMeshComponent* mesh : LaserSplineMeshes)
    {
        mesh->DestroyComponent();
    }
    LaserSplineMeshes.Empty();
    LaserSpline->ClearSplinePoints(true);
}

void ATeleportController::CreateLaserSpline_Implementation()
{
    if (bUseSmoothLaser)
    {
        for (int i = 0; i < NumberOfLaserSplinePoints; i++)
        {
           USplineMeshComponent* smc = NewObject<USplineMeshComponent>(this, USplineMeshComponent::StaticClass()); 
           if (LaserSplineMesh && LaserSplineMaterial)
           {
                smc->SetStaticMesh(LaserSplineMesh);
                smc->SetMaterial(0, LaserSplineMaterial);
           }
           smc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
           smc->SetGenerateOverlapEvents(false);
           smc->AttachToComponent(LaserSpline, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true));
           LaserSplineMeshes.Add(smc);
        }
    }
}

void ATeleportController::FilterGrabspline_Implementation(UPARAM(ref) TArray<FVector> &Locations, UPARAM(ref) FVector &Target, int32 ClosestIndex, double ClosestDist)
{
    if (Locations.Num() > 1)
    {
        for (int i = 0; i < Locations.Num(); i++)
        {
            float distance = (Locations[i] - Target).SquaredLength();
            if (distance < ClosestDist || ClosestDist == 0.0f)
            {
                ClosestDist = distance;
                ClosestIndex = i;
            }
        }

        for (int i = Locations.Num() - 1; i >= 0; i--)
        {
            if (i > ClosestIndex)
            {
                Locations.RemoveAt(i);
            }
        }

        Locations[ClosestIndex] = Target;

    }
}

void ATeleportController::UpdateLaserBeam_Implementation(float Deltatime)
{
    if (IsLaserBeamActive)
    {
        LaserHighlightingObject = nullptr;
        WidgetInteraction->InteractionDistance = LaserBeamMaxDistance;
        FVector teleWorldLoc;
        FVector teleForwardVector;
        GetTeleWorldLocAndForwardVector(teleWorldLoc, teleForwardVector);
        FVector laserStart = teleWorldLoc;
        FVector laserEnd = teleWorldLoc + (teleForwardVector * LaserBeamMaxDistance);
        FCollisionQueryParams collisionParams;
        collisionParams.AddIgnoredActors(TArray<AActor*>({this, OwningMotionController->GetOwner()}));
        bool successfulCollision = GetWorld()->LineTraceSingleByChannel(LastLaserHitResult, laserStart, laserEnd, ECC_Visibility, collisionParams, FCollisionResponseParams::DefaultResponseParam);

        // Smooth Laser Beam
        if (bUseSmoothLaser)
        {
            // Might need to recalculate the TeleWorldLocAndForwardVector here
            FVector smoothedValue = EuroLowPassFilter.RunFilterSmoothing(teleWorldLoc + (LastLaserHitResult.Time * LaserBeamMaxDistance * teleForwardVector), Deltatime);
            UVRExpansionFunctionLibrary::SmoothUpdateLaserSpline(LaserSpline, LaserSplineMeshes, teleWorldLoc, smoothedValue, teleForwardVector, LaserBeamRadius);
            FHitResult hitResult;
            FVector smoothLineTraceEnd = smoothedValue + ((smoothedValue - teleWorldLoc).Normalize() * 100.0f);
            FCollisionQueryParams smoothCollisionParams;
            smoothCollisionParams.AddIgnoredActors(TArray<AActor*>({this, OwningMotionController->GetOwner()}));

            if (DrawSmoothLaserTrace)
            {
                DrawDebugLine(GetWorld(), teleWorldLoc, smoothLineTraceEnd, FColor::Red, false, .01f, 0, 1.0f);
            }

            if (GetWorld()->LineTraceSingleByChannel(hitResult, teleWorldLoc, smoothLineTraceEnd, ECollisionChannel::ECC_Visibility, smoothCollisionParams, FCollisionResponseParams::DefaultResponseParam))
            {
                LastLaserHitResult = hitResult;
                WidgetInteraction->SetCustomHitResult(LastLaserHitResult);
                LaserHighlightingObject = hitResult.GetComponent();
            }

            else
            {
                WidgetInteraction->SetCustomHitResult(hitResult);
            }

        }

        // Normal Laser Beam
        else
        {
            FVector scale = FVector(LastLaserHitResult.Time * LaserBeamMaxDistance, 1.0f, 1.0f);
            LaserBeam->SetWorldScale3D(scale);
            if (successfulCollision)
            {
                WidgetInteraction->SetCustomHitResult(LastLaserHitResult);
                LaserBeamEndPoint->SetRelativeLocation(FVector(LastLaserHitResult.Time * LaserBeamMaxDistance, 0.0f, 0.0f));
                LaserBeamEndPoint->SetHiddenInGame(false);
            }
            else
            {
                LaserBeamEndPoint->SetHiddenInGame(true);
            }
        }

    }
}

void ATeleportController::DisableWidgetActivation_Implementation()
{
    WidgetInteraction->SetCustomHitResult(FHitResult());
    // Fix: There was a delay here in the blueprint, could cause problems?
    WidgetInteraction->Deactivate();
}

void ATeleportController::RumbleController_Implementation(float Intensity)
{
    if (OwningMotionController->IsValidLowLevel())
    {
        EControllerHand hand;
        OwningMotionController->GetHandType(hand);
        GetWorld()->GetFirstPlayerController()->PlayHapticEffect(TeleportHapticEffect, hand, Intensity);
    }
}

 void ATeleportController::StartedUseHeldObjectLeft_Implementation()
 {
    EControllerHand hand;
    OwningMotionController->GetHandType(hand);
    if (hand == EControllerHand::Left)
    {
        TossToHand();
    }
 }

 void ATeleportController::StartedUseHeldObjectRight_Implementation()
 {
    EControllerHand hand;
    OwningMotionController->GetHandType(hand);
    if (!(hand == EControllerHand::Left))
    {
        TossToHand();
    }
 }


void ATeleportController::TossToHand_Implementation()
{
    if (IsLaserBeamActive)
    {
        //UseHeldObjectDispatch.Broadcast(LaserHighlightingObject, );

        bool isThrowing, isOverWidget;
        PhysicsTossManager->IsThrowing(isThrowing);
        isOverWidget = WidgetInteraction->IsOverInteractableWidget() || WidgetInteraction->IsOverFocusableWidget();

        if (!isThrowing && !isOverWidget && LaserHighlightingObject->IsValidLowLevel())
        {
            AxREAL_VRCharacter* vrCharacter = Cast<AxREAL_VRCharacter>(OwningMotionController->GetOwner());
            if (vrCharacter->IsValidLowLevel())
            {
                EControllerHand hand;
                OwningMotionController->GetHandType(hand);
                //TODO: Implement the following function in the AxREAL_VRCharacter class
                //vrCharacter->NotifyServerOfTossRequest(hand == EControllerHand::Left, LaserHighlightingObject);
            }
        }
    }
}

void ATeleportController::CancelTracking_Implementation()
{
    PhysicsTossManager->CancelToss();
}

void ATeleportController::CreateTeleportationArc()
{
    TArray<FVector> tracePoints;
    FVector navMeshLocation;
    FVector traceLocation;

    TraceTeleportDestination(IsValidTeleportDestination, tracePoints, navMeshLocation, traceLocation);
    TeleportCylinder->SetVisibility(IsValidTeleportDestination, true);

    if (IsValidTeleportDestination)
    {

        //Line Trace for Objects
        FHitResult outHitResult;
        FCollisionQueryParams collisionParams;
        collisionParams.AddIgnoredActor(this);
        FVector downwardVector = navMeshLocation + FVector(0.0f, 0.0f, -200.0f);

        bool bHit = GetWorld()->LineTraceSingleByObjectType( outHitResult, navMeshLocation, downwardVector, FCollisionObjectQueryParams::AllStaticObjects, collisionParams);

        if (bHit)
        {
            FVector newCylinderLocation = navMeshLocation;

            if (outHitResult.bBlockingHit)
            {
                newCylinderLocation = outHitResult.ImpactPoint;
            }

            TeleportCylinder->SetWorldLocation(newCylinderLocation, false, nullptr, ETeleportType::TeleportPhysics);
            LastValidTeleportLocation = newCylinderLocation;
            
        }


    }

    // Rumble Controller when a valid teleport location is found
    if ( (IsValidTeleportDestination && !bLastFrameValidDestination) || (!IsValidTeleportDestination && bLastFrameValidDestination))
    {
        RumbleController(0.3f);
    }

    bLastFrameValidDestination = IsValidTeleportDestination;

    UpdateArcSpline(IsValidTeleportDestination, tracePoints);

    UpdateArcEndpoint(traceLocation, IsValidTeleportDestination);

}