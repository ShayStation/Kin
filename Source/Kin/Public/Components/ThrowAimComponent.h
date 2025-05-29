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

    /** Enable debug drawing of throw arc and reticle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bDebugDraw = true;

    /** Maximum throw trace distance (world units) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw")
    float MaxTraceDistance = 1500.0f;

    /** Initial launch speed (unused if arc solver); kept for legacy compatibility */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw")
    float LaunchSpeed = 1500.0f;

    /** Gravity scale applied to projectile gravity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw")
    float ProjectileGravityScale = 0.4f;

    /** Controls curve of arc: 1 = max height, 0 = flat */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ArcParam = 1.0f;

    /** Maximum apex height for arc solver */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw")
    float MaxArcHeight = 500.0f;

    /** Speed multiplier for debug trajectory sampling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw")
    float TimeScale = 1.0f;

    /** How far past a blocking wall the throw can still reach */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw|Range")
    float ClearanceBuffer = 100.0f;

    /** How quickly the reticle’s range interpolates to target (higher = snappier) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw|Range")
    float RangeInterpSpeed = 10.0f;

    /** Stick deflection [0..1] where reticle hits full range */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MovementThreshold = 0.65f;

    /** How quickly the reticle’s direction interpolates to stick direction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (ClampMin = "0.1"))
    float DirectionInterpSpeed = 10.0f;

    /** Number of points to sample along the trajectory for ground-projected reticle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw|Reticle")
    int32 ReticleSampleCount = 16;

    /** Height above trajectory points to start downward ground trace */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw|Reticle")
    float ReticleTraceHeight = 200.0f;

    /** Smoothed, world-space aim direction (unit) */
    UPROPERTY(VisibleAnywhere, Category = "Aim")
    FVector SmoothedAimDirection = FVector::ForwardVector;

    virtual void BeginPlay() override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    /** Computes spawn start, launch velocity, and landing point */
    bool ComputeThrow(
        FVector& OutStart,
        FVector& OutVelocity,
        FVector& OutAimPoint
    );

protected:
    /** Samples the physics arc, projects each point to the ground, updates spline */
    void UpdateGroundReticle(
        const FVector& SpawnStart,
        const FVector& AimPoint
    );

private:
    /** Smoothed throw range we actually use each frame */
    float CurrentEffectiveRange = 0.0f;

    /** Flag to ensure one-time initialization of smoothing */
    bool bHasInitializedAim = false;
};