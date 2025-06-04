// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LockOnTargetComponent.generated.h"

UCLASS(ClassGroup = Custom, meta = (BlueprintSpawnableComponent))
class KIN_API ULockOnTargetComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULockOnTargetComponent();

    /** Higher = preferred when cycling or auto-lock finds multiples */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn")
    float LockPriority = 1.0f;
};