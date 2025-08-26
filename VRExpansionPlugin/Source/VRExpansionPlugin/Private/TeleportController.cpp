// Fill out your copyright notice in the Description page of Project Settings.


#include "TeleportController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "VRExpansionFunctionLibrary.h"
#include "DrawDebugHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "xREAL_VRCharacter.h"
#include "NavigationSystem.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "EnhancedInputComponent.h"
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
    RotOffset = FRotator(-60.0f, 0.0f, 0.0f);
    
    EuroLowPassFilter = FBPEuroLowPassFilter(1.2f, 0.001f, 0.001f);

    LaserBeamRadius = 1.0f;
    bUseSmoothLaser = true;
    NumberOfLaserSplinePoints = 10;
    //DrawSmoothLaserTrace = true;


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
    static ConstructorHelpers::FObjectFinder<UStaticMesh> TeleportMeshAsset(TEXT("StaticMesh'/VRExpansionPlugin/VRE/Core/Character/Meshes/BeamMesh.BeamMesh'"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> TeleportMaterialAsset(TEXT("Material'/VRExpansionPlugin/VRE/Core/Character/Materials/M_SplineArcMat.M_SplineArcMat'"));
    if (TeleportMeshAsset.Succeeded() && TeleportMaterialAsset.Succeeded())
    {
        TeleportSplineMesh = TeleportMeshAsset.Object;
        TeleportSplineMaterial = TeleportMaterialAsset.Object;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load Teleport Spline Assets"));
    }

    //Load Laser Spline Assets
    static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserMeshAsset(TEXT("StaticMesh'/VRExpansionPlugin/VRE/Core/Character/Meshes/BeamMesh.BeamMesh'"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> LaserMaterialAsset(TEXT("Material'/VRExpansionPlugin/VRE/Core/Character/LaserBeamSplineMat.LaserBeamSplineMat'"));
    if (LaserMeshAsset.Succeeded() && LaserMaterialAsset.Succeeded())
    {
        LaserSplineMesh = LaserMeshAsset.Object;
        LaserSplineMaterial = LaserMaterialAsset.Object;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load Laser Spline Assets"));
    }
    
}

void ATeleportController::BeginPlay()
{
    Super::BeginPlay();

    TeleportCylinder->SetVisibility(false, true);

    GetWorld()->GetTimerManager().SetTimer(ControllerBind_TimerHandle, this, &ATeleportController::BindController, 0.1f, false);
}

void ATeleportController::BindController()
{
    APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (IsValid(OwningMotionController))
    {
        LaserSpline->AttachToComponent(OwningMotionController, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true));
        LaserBeam->AttachToComponent(OwningMotionController, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true));
        LaserBeamEndPoint->AttachToComponent(OwningMotionController, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true));
        LaserBeamEndPoint->SetRelativeScale3D(FVector(.02f, .02f, .02f));

        if (OwningMotionController->IsLocallyControlled())
        {
            EnableInput(playerController);
            if (UEnhancedInputComponent* playerInput = Cast<UEnhancedInputComponent>(playerController->InputComponent))
            {
                UInputAction* useHeldObjectRight = FindFirstObjectSafe<UInputAction>(TEXT("UseHeldObjectRight"));
                if (useHeldObjectRight)
                {
                    playerInput->BindAction(useHeldObjectRight, ETriggerEvent::Started, this, &ATeleportController::StartedUseHeldObjectRight);
                }
                
                UInputAction* useHeldObjectLeft = FindFirstObjectSafe<UInputAction>(TEXT("UseHeldObjectLeft"));
                if (useHeldObjectLeft)
                {
                    playerInput->BindAction(useHeldObjectLeft, ETriggerEvent::Started, this, &ATeleportController::StartedUseHeldObjectLeft);
                }

            }
            AVRCharacter* vrCharacter = Cast<AVRCharacter>(OwningMotionController->GetOwner());
            if (vrCharacter)
            {
                vrCharacter->OnCharacterTeleported_Bind.AddDynamic(this, &ATeleportController::CancelTracking);
                SetOwner(vrCharacter);
            }
        }
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

void ATeleportController::ServersideToss(UPrimitiveComponent* TargetObject)
{
    PhysicsTossManager->ServersideToss(TargetObject, OwningMotionController);

}

void ATeleportController::SetLaserBeamActive(bool LaserBeamActive)
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
        }

        ToggleTick();
    }
}

void ATeleportController::ActivateTeleporter()
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

void ATeleportController::DisableTeleporter()
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

void ATeleportController::TraceTeleportDestination(bool &Success, TArray<FVector> &TracePoints, FVector &NavMeshLocation, FVector &TraceLocation)
{
    FVector worldLocation;
    FVector forwardVector;
    GetTeleWorldLocAndForwardVector(worldLocation, forwardVector);

    // Setup Projectile Path Parameters
    FPredictProjectilePathParams Params;
    Params.StartLocation = worldLocation;
    Params.LaunchVelocity = forwardVector * TeleportLaunchVelocity;
    Params.bTraceWithCollision = true;
    Params.bTraceWithChannel = false;
    Params.ProjectileRadius = 0.0f; // Set this based on your needs
    Params.ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic)); // Adjust this based on your collision settings
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

void ATeleportController::ClearArc()
{
    for (USplineMeshComponent* mesh : SplineMeshes)
    {
        mesh->DestroyComponent();
    }
    SplineMeshes.Empty();
    ArcSpline->ClearSplinePoints();
}

void ATeleportController::UpdateArcSpline(bool FoundValidLocation, UPARAM(ref) TArray<FVector> &SplinePoints)
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

void ATeleportController::UpdateArcEndpoint(FVector NewLocation, bool ValidLocationFound)
{
    ArcEndPoint->SetVisibility(ValidLocationFound && IsTeleporterActive);
    ArcEndPoint->SetWorldLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
    Arrow->SetWorldRotation(TeleportRotation + TeleportBaseRotation);
}

void ATeleportController::GetTeleportDestination(bool RelativeToHMD, FVector &Location, FRotator &Rotation)
{
    FVector devicePosition = FVector();
    // Rotation is not being used
    FRotator deviceRotation = FRotator();

    UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(deviceRotation, devicePosition);
    
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

void ATeleportController::GetTeleWorldLocAndForwardVector(FVector &WorldLoc, FVector &ForwardVector)
{
    WorldLoc = OwningMotionController->GetComponentLocation();
    FRotator controllerRotation = OwningMotionController->GetComponentRotation();
    ForwardVector = UKismetMathLibrary::GetForwardVector(UKismetMathLibrary::ComposeRotators(RotOffset, controllerRotation));
}

bool ATeleportController::IfOverWidget_Use(bool bPressed)
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
            return true;
        }
    }
    return false;
}

void ATeleportController::InitController()
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

void ATeleportController::ToggleTick()
{
    SetActorTickEnabled(IsTeleporterActive || IsLaserBeamActive || IsValid(ActorBeingThrown));
}

void ATeleportController::ClearLaserBeam()
{
    for (USplineMeshComponent* mesh : LaserSplineMeshes)
    {
        mesh->DestroyComponent();
    }
    LaserSplineMeshes.Empty();
    LaserSpline->ClearSplinePoints(true);
}

void ATeleportController::CreateLaserSpline()
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
           smc->SetStartScale(FVector2D(2.0f, 2.0f));
           smc->SetEndScale(FVector2D(2.0f, 2.0f));
           smc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
           smc->SetGenerateOverlapEvents(false);
           smc->SetMobility(EComponentMobility::Movable);
           smc->AttachToComponent(LaserSpline, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true));
           smc->RegisterComponent();
           //AddInstanceComponent(smc);
           LaserSplineMeshes.Add(smc);
        }
    }
}

void ATeleportController::FilterGrabspline(UPARAM(ref) TArray<FVector> &Locations, UPARAM(ref) FVector &Target, int32 ClosestIndex, double ClosestDist)
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

void ATeleportController::UpdateLaserBeam(float Deltatime)
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
            FVector smoothLineTraceEnd = smoothedValue + ((smoothedValue - teleWorldLoc).GetSafeNormal(.0001f) * 100.0f);
            FCollisionQueryParams smoothCollisionParams;
            smoothCollisionParams.AddIgnoredActors(TArray<AActor*>({this, OwningMotionController->GetOwner()}));

            if (DrawSmoothLaserTrace)
            {
                DrawDebugLine(GetWorld(), teleWorldLoc, smoothLineTraceEnd, FColor::Green, false, .01f, 0, 1.0f);
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

void ATeleportController::DisableWidgetActivation()
{
    WidgetInteraction->SetCustomHitResult(FHitResult());
    // Fix: There was a delay here in the blueprint, could cause problems?
    WidgetInteraction->Deactivate();
}

void ATeleportController::RumbleController(float Intensity)
{
    if (OwningMotionController->IsValidLowLevel())
    {
        EControllerHand hand;
        OwningMotionController->GetHandType(hand);
        GetWorld()->GetFirstPlayerController()->PlayHapticEffect(TeleportHapticEffect, hand, Intensity);
    }
}

 void ATeleportController::StartedUseHeldObjectLeft()
 {
    EControllerHand hand;
    OwningMotionController->GetHandType(hand);
    if (hand == EControllerHand::Left)
    {
        TossToHand(hand);
    }
 }

 void ATeleportController::StartedUseHeldObjectRight()
 {
    EControllerHand hand;
    OwningMotionController->GetHandType(hand);
    if (!(hand == EControllerHand::Left))
    {
        TossToHand(hand);
    }
 }


void ATeleportController::TossToHand(EControllerHand Hand)
{
    if (IsLaserBeamActive)
    {

        bool isThrowing, isOverWidget;
        PhysicsTossManager->IsThrowing(isThrowing);
        isOverWidget = WidgetInteraction->IsOverInteractableWidget() || WidgetInteraction->IsOverFocusableWidget();

        if (!isThrowing && !isOverWidget && IsValid(LaserHighlightingObject))
        {
            AxREAL_VRCharacter* vrCharacter = Cast<AxREAL_VRCharacter>(OwningMotionController->GetOwner());
            if (IsValid(vrCharacter))
            {
                vrCharacter->NotifyServerOfTossRequest(Hand == EControllerHand::Left, LaserHighlightingObject);
            }
        }
    }
}

void ATeleportController::CancelTracking()
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
        TArray<TEnumAsByte<EObjectTypeQuery>> objectTypes;
        objectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));
        FVector downwardVector = navMeshLocation + FVector(0.0f, 0.0f, -200.0f);

        bool bHit = GetWorld()->LineTraceSingleByObjectType( outHitResult, navMeshLocation, downwardVector, FCollisionObjectQueryParams(objectTypes), collisionParams);
        
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