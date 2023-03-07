/*
Copyright (c) Meta Platforms, Inc. and affiliates.
All rights reserved.

This source code is licensed under the license found in the
LICENSE file in the root directory of this source tree.
*/

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "OculusXRAnchorTypes.h"
#include "OculusXRAnchorBPFunctionLibrary.generated.h"

//Helper
UCLASS()
class OCULUSXRANCHORS_API UOculusXRAnchorBPFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Spawn Oculus Anchor Actor", WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"), Category = "OculusXR|SpatialAnchor")
	static AActor* SpawnActorWithAnchorHandle(UObject* WorldContextObject, FOculusXRUInt64 Handle, FOculusXRUUID UUID, EOculusXRSpaceStorageLocation AnchorLocation, UClass* ActorClass, AActor* Owner, APawn* Instigator, ESpawnActorCollisionHandlingMethod CollisionHandlingMethod);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Spawn Oculus Anchor Actor From Query", WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"), Category = "OculusXR|SpatialAnchor")
	static AActor* SpawnActorWithAnchorQueryResults(UObject* WorldContextObject, const FOculusXRSpaceQueryResult& QueryResult, UClass* ActorClass, AActor* Owner, APawn* Instigator, ESpawnActorCollisionHandlingMethod CollisionHandlingMethod);

	UFUNCTION(BlueprintCallable, Category = "OculusXR|SpatialAnchor")
	static bool GetAnchorComponentStatus(AActor* TargetActor, EOculusXRSpaceComponentType ComponentType, bool& bIsEnabled);

	UFUNCTION(BlueprintCallable, Category = "OculusXR|SpatialAnchor")
	static bool GetAnchorTransformByHandle(const FOculusXRUInt64& Handle, FTransform& OutTransform);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "FOculusXRUInt64 To String", CompactNodeTitle = "->", BlueprintAutocast), Category = "OculusXR|SpatialAnchor")
	static FString AnchorHandleToString(const FOculusXRUInt64 Value);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "FOculusXRUUID To String", CompactNodeTitle = "->", BlueprintAutocast), Category = "OculusXR|SpatialAnchor")
	static FString AnchorUUIDToString(const FOculusXRUUID& Value);

	UFUNCTION(BlueprintCallable, Category = "OculusXR|SpatialAnchor")
	static FOculusXRUUID StringToAnchorUUID(const FString& Value);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "FOculusXRUInt64 equal", CompactNodeTitle = "==", Keywords = "equal", BlueprintAutocast), Category = "OculusXR|SpatialAnchor")
	static bool IsEqual_FOculusXRUInt64(const FOculusXRUInt64 Left, const FOculusXRUInt64 Right) { return Left == Right; };

	UFUNCTION(BlueprintPure, meta = (DisplayName = "FOculusXRUUID equal", CompactNodeTitle = "==", Keywords = "equal", BlueprintAutocast), Category = "OculusXR|SpatialAnchor")
	static bool IsEqual_FOculusXRUUID(const FOculusXRUUID& Left, const FOculusXRUUID& Right) { return Left.IsEqual(Right); };
};