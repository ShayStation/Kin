#include "Components/ThrowAimComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Abilities/ThrownProjectile.h"
#include "Components/SplineComponent.h"
#include "GameFramework/PlayerController.h"



UThrowAimComponent::UThrowAimComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UThrowAimComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UThrowAimComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bDebugDraw)
        return;

    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !Owner)
        return;

    // —— ONE-TIME INIT: start at forward, zero range ——
    if (!bHasInitializedAim)
    {
        SmoothedAimDirection = Owner->GetActorForwardVector();
        CurrentEffectiveRange = 0.f;
        bHasInitializedAim = true;
    }

    // 1) Direction smoothing
    FVector DesiredDir = SmoothedAimDirection;
    if (AimInput.SizeSquared() > KINDA_SMALL_NUMBER)
    {
        if (APlayerController* PC = Cast<APlayerController>(Owner->GetInstigatorController()))
        {
            float Yaw = PC->GetControlRotation().Yaw;
            FRotator YawRot(0.f, Yaw, 0.f);
            FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
            FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
            DesiredDir = (Forward * AimInput.Y + Right * AimInput.X).GetSafeNormal();
        }
    }
    SmoothedAimDirection = FMath::VInterpTo(
        SmoothedAimDirection,
        DesiredDir,
        DeltaTime,
        DirectionInterpSpeed
    );

    // 2) Range smoothing only when stick held; hold on release
    float AimMag = AimInput.Size();
    if (AimMag > KINDA_SMALL_NUMBER)
    {
        float Ratio = FMath::Min(AimMag / MovementThreshold, 1.f);
        float TargetRange = Ratio * MaxTraceDistance;
        CurrentEffectiveRange = FMath::FInterpTo(
            CurrentEffectiveRange,
            TargetRange,
            DeltaTime,
            RangeInterpSpeed
        );
    }

    // 3) Hard wall-clamp cap
    UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
    FVector Start = Capsule
        ? Capsule->GetComponentLocation()
        : Owner->GetActorLocation();

    FHitResult Hit;
    FCollisionQueryParams Params(TEXT("WallClamp"), false, Owner);
    bool bHitWall = World->LineTraceSingleByChannel(
        Hit,
        Start,
        Start + SmoothedAimDirection * MaxTraceDistance,
        ECC_WorldStatic,
        Params
    );
    float DistanceToWall2D = bHitWall
        ? (Hit.Location - Start).Size2D()
        : MaxTraceDistance;

    float WallClampRange = FMath::Clamp(
        DistanceToWall2D + ClearanceBuffer,
        0.f,
        MaxTraceDistance
    );

    // 4) Final range = min(smoothed, wall-clamp) and hold
    float UseRange = FMath::Min(CurrentEffectiveRange, WallClampRange);
    CurrentEffectiveRange = UseRange;

    // 5) Compute throw (direction uses smoothed range)
    FVector SpawnStart, LaunchVelocity, AimPoint;
    if (!ComputeThrow(SpawnStart, LaunchVelocity, AimPoint))
        return;

    // 6) Debug: range line
    DrawDebugLine(
        World,
        Start,
        Start + SmoothedAimDirection * UseRange,
        FColor::Blue,
        false, 0.f, 0, 2.f
    );

    // 7) Debug: landing point
    DrawDebugSphere(World, AimPoint, 8.f, 12, FColor::Green, false, 0.f);

    // 8) Debug: velocity arrow
    DrawDebugLine(
        World,
        SpawnStart,
        SpawnStart + LaunchVelocity.GetSafeNormal() * 100.f,
        FColor::Yellow,
        false, 0.f, 0, 2.f
    );

    // 9) Debug: apex label
    {
        float WorldG = -World->GetGravityZ() * ProjectileGravityScale;
        float Apex = FMath::Square(LaunchVelocity.Z) / (2.f * WorldG);
        DrawDebugString(
            World,
            SpawnStart + FVector(0, 0, Apex),
            FString::Printf(TEXT("Apex: %.1f"), Apex),
            nullptr, FColor::White, 0.f
        );
    }

    // 10) Ground-projected reticle
    UpdateGroundReticle(SpawnStart, AimPoint);

    // 11) Debug: sampled trajectory
    const int32 Segs = ReticleSampleCount;
    float WorldG = -World->GetGravityZ() * ProjectileGravityScale;
    float DeltaZ = AimPoint.Z - SpawnStart.Z;
    float Vz = LaunchVelocity.Z;
    float Discr = (Vz * Vz) - 2.f * WorldG * DeltaZ;
    if (Discr > 0.f)
    {
        float TotalT = (Vz + FMath::Sqrt(Discr)) / WorldG;
        FVector PrevPt = SpawnStart;
        for (int32 i = 1; i <= Segs; ++i)
        {
            float tSim = (float(i) / Segs) * TotalT * TimeScale;
            FVector GravAccel = FVector(0, 0, -WorldG);
            FVector Pt = SpawnStart
                + LaunchVelocity * tSim
                + 0.5f * GravAccel * tSim * tSim;
            DrawDebugLine(World, PrevPt, Pt, FColor::Cyan, false, 0.f, 0, 1.f);
            PrevPt = Pt;
        }
    }
}

bool UThrowAimComponent::ComputeThrow(
    FVector& OutStart,
    FVector& OutVelocity,
    FVector& OutAimPoint
)
{
    AActor* Owner = GetOwner();
    if (!Owner) return false;
    UWorld* World = Owner->GetWorld();
    if (!World) return false;

    // 1) Spawn at socket
    USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh) return false;
    OutStart = Mesh->GetSocketLocation(TEXT("ThrowSocket"));

    // 2) Horizontal trace using smoothed direction
    UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
    FVector TraceStart = Capsule ? Capsule->GetComponentLocation() : OutStart;
    FVector TraceEnd = TraceStart + SmoothedAimDirection * CurrentEffectiveRange;
    FHitResult Hit;
    FCollisionQueryParams Params(TEXT("ThrowTrace"), false, Owner);
    bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params);
    FVector LandXY = TraceStart + SmoothedAimDirection * CurrentEffectiveRange;
    float BaseZ = bHit ? Hit.Location.Z : TraceStart.Z;

    // 3) Snap-up using apex-driven trace
    float H = MaxArcHeight * ArcParam;
    float g = -World->GetGravityZ() * ProjectileGravityScale;
    float VzInit = FMath::Sqrt(2.f * g * H);
    FVector UpStart = FVector(LandXY.X, LandXY.Y, BaseZ + H);
    FHitResult UpHit;
    if (World->LineTraceSingleByChannel(UpHit, UpStart, FVector(LandXY.X, LandXY.Y, BaseZ), ECC_WorldStatic, Params))
    {
        BaseZ = UpHit.Location.Z;
    }
    OutAimPoint = FVector(LandXY.X, LandXY.Y, BaseZ);

    // 4) Solve flight time & velocity
    float DeltaZ = OutAimPoint.Z - OutStart.Z;
    float Discr2 = VzInit * VzInit - 2.f * g * DeltaZ;
    if (Discr2 < 0.f) return false;
    float Time = (VzInit + FMath::Sqrt(Discr2)) / g;
    FVector Dir2D = FVector(OutAimPoint.X - OutStart.X, OutAimPoint.Y - OutStart.Y, 0.f);
    float Dist2D = Dir2D.Size();
    if (Dist2D < KINDA_SMALL_NUMBER) return false;
    Dir2D.Normalize();
    float Vx = Dist2D / Time;
    OutVelocity = Dir2D * Vx + FVector(0, 0, VzInit);
    return true;
}

void UThrowAimComponent::UpdateGroundReticle(
    const FVector& SpawnStart,
    const FVector& AimPoint
)
{
    UWorld* World = GetWorld();
    if (!World || ReticleSampleCount < 2) return;

    FVector Dir2D = AimPoint - SpawnStart;
    Dir2D.Z = 0.f;
    float TotalDist = Dir2D.Size();
    if (TotalDist < KINDA_SMALL_NUMBER) return;
    Dir2D.Normalize();

    TArray<FVector> GroundPoints;
    GroundPoints.Reserve(ReticleSampleCount);

    for (int32 i = 0; i < ReticleSampleCount; ++i)
    {
        float Alpha = float(i) / float(ReticleSampleCount - 1);
        FVector HorizontalPt = SpawnStart + Dir2D * (TotalDist * Alpha);
        FVector TraceStart = HorizontalPt + FVector(0, 0, ReticleTraceHeight);
        FVector TraceEnd = HorizontalPt - FVector(0, 0, ReticleTraceHeight);
        FHitResult Hit;
        FCollisionObjectQueryParams ObjParams;
        ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
        FCollisionQueryParams Params(TEXT("ReticleTrace"), false, GetOwner());
        FVector GroundPt = HorizontalPt;
        if (World->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjParams, Params))
        {
            GroundPt = Hit.Location;
        }
        GroundPoints.Add(GroundPt);
    }

    for (int32 i = 1; i < GroundPoints.Num(); ++i)
    {
        DrawDebugLine(
            World,
            GroundPoints[i - 1],
            GroundPoints[i],
            FColor::Emerald,
            false,
            0.f,
            0,
            3.f
        );
    }
}
