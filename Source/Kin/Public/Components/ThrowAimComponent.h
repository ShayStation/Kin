// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SplineComponent.h"
#include "ThrowAimComponent.generated.h"


UCLASS(ClassGroup = Custom, meta = (BlueprintSpawnableComponent))
class KIN_API UThrowAimComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UThrowAimComponent();

    /** Raw 2D stick input (–1..+1 in X/Y) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
    FVector2D AimInput = FVector2D::ZeroVector;

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    bool ComputeThrow(FVector& OutStart, FVector& OutVelocity, FVector& OutAimPoint);

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDebugDraw = true;

    UPROPERTY(EditAnywhere, Category = "Throw")
    float MaxTraceDistance = 1500.0f;

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

    /** How far past a blocking wall the throw can still reach (so you always clear the lip) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw|Range")
    float ClearanceBuffer = 100.0f;

    /** How quickly the reticle’s range eases in/out (higher = snappier) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw|Range")
    float RangeInterpSpeed = 10.0f;

    /** How many points to sample along the trajectory for ground projection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw|Reticle")
    int32 ReticleSampleCount = 16;

    /** How far up above the trajectory point to start the downward trace */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw|Reticle")
    float ReticleTraceHeight = 200.0f;



protected:


private:
    /** Smoothed throw range we actually use each frame */
    float CurrentEffectiveRange;

    /** Samples the physics arc, projects each point to the ground, updates ReticleSpline */
    void UpdateGroundReticle(
        const FVector& SpawnStart,
        const FVector& AimPoint
    );

};