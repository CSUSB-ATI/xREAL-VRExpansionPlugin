/*
Copyright (c) Meta Platforms, Inc. and affiliates.
All rights reserved.

This source code is licensed under the license found in the
LICENSE file in the root directory of this source tree.
*/

#include "OculusXRAnchorBPFunctionLibrary.h"
#include "OculusXRHMD.h"
#include "OculusXRSpatialAnchorComponent.h"
#include "OculusXRAnchorsPrivate.h"
#include "Kismet/BlueprintFunctionLibrary.h"

AActor* UOculusXRAnchorBPFunctionLibrary::SpawnActorWithAnchorHandle(UObject* WorldContextObject, FOculusXRUInt64 Handle, FOculusXRUUID UUID, EOculusXRSpaceStorageLocation Location, UClass* ActorClass,
	AActor* Owner, APawn* Instigator, ESpawnActorCollisionHandlingMethod CollisionHandlingMethod)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = Owner;
	SpawnInfo.Instigator = Instigator;
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.SpawnCollisionHandlingOverride = CollisionHandlingMethod;

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World == nullptr)
	{
		UE_LOG(LogOculusXRAnchors, Warning, TEXT("Invalid WorldContext Object for SpawnActorWithAnchorHandle."));
		return nullptr;
	}

	AActor* NewSpatialAnchorActor = World->SpawnActor(ActorClass, nullptr, nullptr, SpawnInfo);
	if (NewSpatialAnchorActor == nullptr)
	{
		UE_LOG(LogOculusXRAnchors, Warning, TEXT("Failed to spawn Actor in SpawnActorWithAnchorHandle"));
		return nullptr;
	}

	UOculusXRSpatialAnchorComponent* SpatialAnchorComponent = NewSpatialAnchorActor->FindComponentByClass<UOculusXRSpatialAnchorComponent>();
	if (SpatialAnchorComponent == nullptr)
	{
		SpatialAnchorComponent = Cast<UOculusXRSpatialAnchorComponent>(NewSpatialAnchorActor->AddComponentByClass(UOculusXRSpatialAnchorComponent::StaticClass(), false, FTransform::Identity, false));
	}

	if (!IsValid(SpatialAnchorComponent))
	{
		UE_LOG(LogOculusXRAnchors, Warning, TEXT("Failed to find or spawn Spatial Anchor component in SpawnActorWithAnchorHandle"));
		return nullptr;
	}

	SpatialAnchorComponent->SetHandle(Handle);
	SpatialAnchorComponent->SetUUID(UUID);
	SpatialAnchorComponent->SetStoredLocation(Location, true);
	return NewSpatialAnchorActor;
}

AActor* UOculusXRAnchorBPFunctionLibrary::SpawnActorWithAnchorQueryResults(UObject* WorldContextObject, const FOculusXRSpaceQueryResult& QueryResult, UClass* ActorClass, AActor* Owner, APawn* Instigator, ESpawnActorCollisionHandlingMethod CollisionHandlingMethod)
{
	return SpawnActorWithAnchorHandle(WorldContextObject, QueryResult.Space, QueryResult.UUID, QueryResult.Location, ActorClass, Owner, Instigator, CollisionHandlingMethod);
}

bool UOculusXRAnchorBPFunctionLibrary::GetAnchorComponentStatus(AActor* TargetActor, EOculusXRSpaceComponentType ComponentType, bool& bIsEnabled)
{
	UOculusXRAnchorComponent* AnchorComponent = Cast<UOculusXRAnchorComponent>(TargetActor->GetComponentByClass(UOculusXRAnchorComponent::StaticClass()));

	if (!IsValid(AnchorComponent))
	{
		UE_LOG(LogOculusXRAnchors, Warning, TEXT("Invalid Anchor Component provided to GetAnchorComponentStatus"));
		bIsEnabled = false;
		return false;
	}

	bool bOutIsEnabled = false;
	bool bIsChangePending = false;

	EOculusXRAnchorResult::Type AnchorResult;
	bool bDidCallStart = OculusXRAnchors::FOculusXRAnchors::GetAnchorComponentStatus(AnchorComponent, ComponentType, bOutIsEnabled, bIsChangePending, AnchorResult);
	if (!bDidCallStart)
	{
		UE_LOG(LogOculusXRAnchors, Warning, TEXT("Failed to start call to internal GetAnchorComponentStatus"));
		bIsEnabled = false;
		return false;
	}

	bIsEnabled = bOutIsEnabled;
	return bIsEnabled;
}

bool UOculusXRAnchorBPFunctionLibrary::GetAnchorTransformByHandle(const FOculusXRUInt64& Handle, FTransform& OutTransform)
{
	OculusXRHMD::FOculusXRHMD* OutHMD = OculusXRHMD::FOculusXRHMD::GetOculusXRHMD();
	if (!OutHMD)
	{
		UE_LOG(LogOculusXRAnchors, Warning, TEXT("Unable to retrieve OculusXRHMD, cannot calculate anchor transform."));
		return false;
	}

	ovrpTrackingOrigin ovrpOrigin = ovrpTrackingOrigin_EyeLevel;
	const bool bTrackingOriginSuccess = FOculusXRHMDModule::GetPluginWrapper().GetInitialized() && OVRP_SUCCESS(FOculusXRHMDModule::GetPluginWrapper().GetTrackingOriginType2(&ovrpOrigin));
	if (!bTrackingOriginSuccess)
	{
		UE_LOG(LogOculusXRAnchors, Warning, TEXT("Unable to get tracking origin, cannot calculate anchor transform."));
		return false;
	}

	const ovrpUInt64 ovrpSpace = Handle.GetValue();
	ovrpPosef ovrpPose;

	const bool bSuccess = FOculusXRHMDModule::GetPluginWrapper().GetInitialized() && OVRP_SUCCESS(FOculusXRHMDModule::GetPluginWrapper().LocateSpace(&ovrpPose, &ovrpSpace, ovrpOrigin));
	if (bSuccess)
	{
		OculusXRHMD::FPose Pose;
		OutHMD->ConvertPose(ovrpPose, Pose);
		const FTransform trackingToWorld = OutHMD->GetLastTrackingToWorld();

		OutTransform.SetLocation(trackingToWorld.TransformPosition(Pose.Position));
		OutTransform.SetRotation(FRotator(trackingToWorld.TransformRotation(FQuat(Pose.Orientation))).Quaternion());
	}

	return bSuccess;
}

FString UOculusXRAnchorBPFunctionLibrary::AnchorHandleToString(const FOculusXRUInt64 Value)
{
	return FString::Printf(TEXT("%llu"), Value.Value);
}

FString UOculusXRAnchorBPFunctionLibrary::AnchorUUIDToString(const FOculusXRUUID& Value)
{
	return Value.ToString();
}

FOculusXRUUID UOculusXRAnchorBPFunctionLibrary::StringToAnchorUUID(const FString& Value)
{
	// Static size for the max length of the string, two chars per hex digit, 16 digits.
	checkf(Value.Len() == 32, TEXT("'%s' is not a valid UUID"), *Value);

	ovrpUuid newID;
	HexToBytes(Value, newID.data);

	return FOculusXRUUID(newID.data);
}
bool UOculusXRAnchorBPFunctionLibrary::IsAnchorResultSuccess(EOculusXRAnchorResult::Type result)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	return OVRP_SUCCESS(result);
#endif
	return false;
}

const UOculusXRBaseAnchorComponent* UOculusXRAnchorBPFunctionLibrary::GetAnchorComponent(const FOculusXRSpaceQueryResult& QueryResult, EOculusXRSpaceComponentType ComponentType, UObject* Outer)
{
	switch (ComponentType)
	{
		case EOculusXRSpaceComponentType::Locatable:
			return UOculusXRBaseAnchorComponent::FromSpace<UOculusXRLocatableAnchorComponent>(QueryResult.Space.Value, Outer);
		case EOculusXRSpaceComponentType::ScenePlane:
			return UOculusXRBaseAnchorComponent::FromSpace<UOculusXRPlaneAnchorComponent>(QueryResult.Space.Value, Outer);
		case EOculusXRSpaceComponentType::SceneVolume:
			return UOculusXRBaseAnchorComponent::FromSpace<UOculusXRVolumeAnchorComponent>(QueryResult.Space.Value, Outer);
		case EOculusXRSpaceComponentType::SemanticClassification:
			return UOculusXRBaseAnchorComponent::FromSpace<UOculusXRSemanticClassificationAnchorComponent>(QueryResult.Space.Value, Outer);
		case EOculusXRSpaceComponentType::RoomLayout:
			return UOculusXRBaseAnchorComponent::FromSpace<UOculusXRRoomLayoutAnchorComponent>(QueryResult.Space.Value, Outer);
		case EOculusXRSpaceComponentType::SpaceContainer:
			return UOculusXRBaseAnchorComponent::FromSpace<UOculusXRSpaceContainerAnchorComponent>(QueryResult.Space.Value, Outer);
		case EOculusXRSpaceComponentType::Sharable:
			return UOculusXRBaseAnchorComponent::FromSpace<UOculusXRSharableAnchorComponent>(QueryResult.Space.Value, Outer);
		case EOculusXRSpaceComponentType::Storable:
			return UOculusXRBaseAnchorComponent::FromSpace<UOculusXRStorableAnchorComponent>(QueryResult.Space.Value, Outer);
		default:
			return nullptr;
	}
}
