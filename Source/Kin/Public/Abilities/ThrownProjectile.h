// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "ThrownProjectile.generated.h"

UCLASS()
class KIN_API AThrownProjectile : public AActor
{
    GENERATED_BODY()

public:
    AThrownProjectile();

    /** Initialize its path.
     *  @param StartLoc     World location to start from
     *  @param InitialVel   The velocity computed by ComputeThrow()
     *  @param InGravityScale  How much world gravity to apply
     *  @param InTimeScale  >1 = faster flight, <1 = slower
     */
    void InitTrajectory(
        const FVector& StartLoc,
        const FVector& InitialVel,
        float InGravityScale,
        float InTimeScale
    );

protected:
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Mesh;

    FVector InitialLocation;
    FVector LaunchVelocity;
    float   GravityScale;
    float   TimeScale;
    float   SpawnTime;
};