// Fill out your copyright notice in the Description page of Project Settings.


#include "WristMenuActor.h"
#include "Components/WidgetComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Blueprint/UserWidget.h"
#include "WristMenuUserWidget.h"

// Sets default values
AWristMenuActor::AWristMenuActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("WristMenuWidget"));
	WidgetComponent->SetupAttachment(RootComponent);

	WidgetComponent->SetDrawSize(FVector2D(50, 50));
	WidgetComponent->SetRelativeLocation(FVector(0, 0, 0));

}

// Called when the game starts or when spawned
void AWristMenuActor::BeginPlay()
{
	Super::BeginPlay();
	
}

void AWristMenuActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (WristWidgetClass)
	{
		WidgetComponent->SetWidgetClass(WristWidgetClass);
	}
}

// Called every frame
void AWristMenuActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AWristMenuActor::SetWristMenuEnabled(bool bEnabled)
{
	WidgetComponent->SetVisibility(bEnabled);
	WidgetComponent->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

