// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/ThrowAimComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UThrowAimComponent::UThrowAimComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UThrowAimComponent::UpdateAimPoint(FVector& OutLocation, FVector& OutNormal, bool bPerformSnapUp) const
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    // 1) Forward trace from camera
    FHitResult Hit;
    if (!DoForwardTrace(Hit))
    {
        // no hit: abort, return owner location/forward
        OutLocation = Owner->GetActorLocation() + Owner->GetActorForwardVector() * MaxThrowDistance;
        OutNormal = FVector::UpVector;
        return;
    }

    FVector AimPoint = Hit.ImpactPoint;
    OutNormal = Hit.ImpactNormal;

    // 2) Distance clamp
    const FVector StartPos = Owner->GetActorLocation();
    FVector ClampedPoint;
    if (!ClampToMaxDistance(StartPos, AimPoint, ClampedPoint))
    {
        // out of range: you can handle this (e.g. mark invalid)
        AimPoint = ClampedPoint;
    }

    // 3) Optional snap-up
    if (bPerformSnapUp)
    {
        FVector Snapped;
        if (TrySnapUp(Hit.ImpactPoint, Snapped))
        {
            AimPoint = Snapped;
            // you may want to recalc distance clamp here again
        }
    }

    OutLocation = AimPoint;
}

bool UThrowAimComponent::DoForwardTrace(FHitResult& OutHit) const
{
    AActor* Owner = GetOwner();
    if (!Owner) return false;

    // Get camera location & forward
    FVector CamLoc;
    FRotator CamRot;
    Owner->GetActorEyesViewPoint(CamLoc, CamRot);

    const FVector TraceEnd = CamLoc + CamRot.Vector() * ForwardTraceDistance;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        OutHit, CamLoc, TraceEnd, TraceChannel, Params);

#if ENABLE_DRAW_DEBUG
    DrawDebugLine(GetWorld(), CamLoc, bHit ? OutHit.ImpactPoint : TraceEnd,
        bHit ? FColor::Green : FColor::Red, false, 1.0f);
    if (bHit)
    {
        DrawDebugPoint(GetWorld(), OutHit.ImpactPoint, 10.0f, FColor::Yellow, false, 1.0f);
    }
#endif

    return bHit;
}

bool UThrowAimComponent::ClampToMaxDistance(
    const FVector& Start,
    const FVector& Target,
    FVector& OutClamped) const
{
    const float Dist = FVector::Dist(Start, Target);
    if (Dist <= MaxThrowDistance)
    {
        OutClamped = Target;
        return true;
    }

    // clamp to sphere
    const FVector Dir = (Target - Start).GetSafeNormal();
    OutClamped = Start + Dir * MaxThrowDistance;
    return false;
}

bool UThrowAimComponent::TrySnapUp(const FVector& BaseLocation, FVector& OutSnapped) const
{
    FVector UpStart = BaseLocation + FVector(0, 0, 5.0f);
    FVector UpEnd = UpStart + FVector(0, 0, SnapUpHeight);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit, UpStart, UpEnd, TraceChannel, Params);

#if ENABLE_DRAW_DEBUG
    DrawDebugLine(GetWorld(), UpStart, UpEnd, FColor::Blue, false, 1.0f);
    if (bHit)
    {
        DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 10.0f, FColor::Cyan, false, 1.0f);
    }
#endif

    if (bHit)
    {
        OutSnapped = Hit.ImpactPoint;
        return true;
    }
    return false;
}


