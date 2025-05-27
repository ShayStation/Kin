// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThrowAimComponent.generated.h"


UCLASS(ClassGroup = Custom, meta = (BlueprintSpawnableComponent))
class KIN_API UThrowAimComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UThrowAimComponent();

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    bool ComputeThrow(FVector& OutStart, FVector& OutVelocity);

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDebugDraw = true;

    UPROPERTY(EditAnywhere, Category = "Throw")
    float MaxTraceDistance = 1500.0f;

    UPROPERTY(EditAnywhere, Category = "Throw")
    float SnapUpHeight = 300.0f;

    UPROPERTY(EditAnywhere, Category = "Throw")
    float LaunchSpeed = 1500.0f;

    UPROPERTY(EditAnywhere, Category = "Throw")
    float ProjectileGravityScale = 0.4f;

    UPROPERTY(EditAnywhere, Category = "Throw", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ArcParam = 1.0f;

    UPROPERTY(EditAnywhere, Category = "Throw")
    float MaxArcHeight = 500.0f;

    // New: how fast the projectile traverses the arc.
    // 1.0 = real-time, >1 = faster, <1 = slower.
    UPROPERTY(EditAnywhere, Category = "Throw")
    float TimeScale = 1.0f;
};