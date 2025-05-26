// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThrowAimComponent.generated.h"


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KIN_API UThrowAimComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UThrowAimComponent();

    /** Perform the forward trace and compute a valid aim point */
    UFUNCTION(BlueprintCallable, Category = "Throw|Aim")
    void UpdateAimPoint(FVector& OutLocation, FVector& OutNormal, bool bPerformSnapUp = true) const;

    /** Maximum straight-line throw distance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw|Aim")
    float MaxThrowDistance = 1000.0f;

    /** Vertical snap-up height when looking at a ledge */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw|Aim")
    float SnapUpHeight = 200.0f;

protected:
    /** How far in front of camera to trace initially */
    UPROPERTY(EditAnywhere, Category = "Throw|Aim")
    float ForwardTraceDistance = 2000.0f;

    /** Collision channel for your trace (e.g. Visibility or custom) */
    UPROPERTY(EditAnywhere, Category = "Throw|Aim")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    /** Helper: does a single line-trace and returns true if we hit */
    bool DoForwardTrace(FHitResult& OutHit) const;

    /** Helper: clamp to max distance, returns false if out-of-range */
    bool ClampToMaxDistance(const FVector& Start, const FVector& Target, FVector& OutClamped) const;

    /** Helper: optional snap-up to ledge top */
    bool TrySnapUp(const FVector& BaseLocation, FVector& OutSnapped) const;


};
