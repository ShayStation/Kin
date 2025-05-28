#include "Components/ThrowAimComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Abilities/ThrownProjectile.h"
#include "Components/SplineComponent.h"


UThrowAimComponent::UThrowAimComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    CurrentEffectiveRange = MaxTraceDistance;
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

    // 1) Wall-distance trace ? smooth CurrentEffectiveRange
    UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
    FVector WallStart = Capsule
        ? Capsule->GetComponentLocation()
        : Owner->GetActorLocation();

    FCollisionQueryParams WallParams(TEXT("WallClamp"), false, Owner);
    FHitResult WallHit;
    bool bHitWall = World->LineTraceSingleByChannel(
        WallHit,
        WallStart,
        WallStart + Owner->GetActorForwardVector() * MaxTraceDistance,
        ECC_WorldStatic,
        WallParams
    );

    float DistanceToWall = bHitWall
        ? (WallHit.Location - WallStart).Size2D()
        : MaxTraceDistance;

    float TargetRange = FMath::Clamp(DistanceToWall + ClearanceBuffer, 0.0f, MaxTraceDistance);
    CurrentEffectiveRange = FMath::FInterpTo(
        CurrentEffectiveRange,
        TargetRange,
        DeltaTime,
        RangeInterpSpeed
    );

    // 2) Compute throw (Start, Velocity, AimPoint)
    FVector SpawnStart, LaunchVelocity, AimPoint;
    if (!ComputeThrow(SpawnStart, LaunchVelocity, AimPoint))
        return;

    // 3) Debug: range line
    FVector TraceStart = WallStart;
    FVector TraceEnd = TraceStart + Owner->GetActorForwardVector() * CurrentEffectiveRange;
    DrawDebugLine(World, TraceStart, TraceEnd, FColor::Blue, false, 0.0f, 0, 2.0f);

    // 4) Debug: landing point
    DrawDebugSphere(World, AimPoint, 8.0f, 12, FColor::Green, false, 0.0f);

    // 5) Debug: velocity arrow
    DrawDebugLine(
        World,
        SpawnStart,
        SpawnStart + LaunchVelocity.GetSafeNormal() * 100.0f,
        FColor::Yellow, false, 0.0f, 0, 2.0f
    );

    // 6) Debug: apex label
    {
        float WorldG = -World->GetGravityZ();
        float EffG = WorldG * ProjectileGravityScale;
        float Apex = LaunchVelocity.Z * LaunchVelocity.Z / (2.0f * EffG);
        DrawDebugString(
            World,
            SpawnStart + FVector(0, 0, Apex),
            FString::Printf(TEXT("Apex: %.1f"), Apex),
            nullptr, FColor::White, 0.0f
        );
    }

    // 7) Ground-projected reticle polyline (horizontally sampled)
    UpdateGroundReticle(SpawnStart, AimPoint);

    // 8) Debug: physics?sampled trajectory (cyan)
    {
        const int32 Segs = 30;
        float WorldG = -World->GetGravityZ();
        float g = WorldG * ProjectileGravityScale;
        float DeltaZ = AimPoint.Z - SpawnStart.Z;
        float Vz = LaunchVelocity.Z;
        float Discr = Vz * Vz - 2.0f * g * DeltaZ;
        if (Discr <= 0.0f) return;

        float TotalT = (Vz + FMath::Sqrt(Discr)) / g;
        FVector PrevPt = SpawnStart;

        for (int32 i = 1; i <= Segs; ++i)
        {
            float tSim = (float(i) / Segs) * TotalT * TimeScale;
            FVector GravAccel = FVector(0, 0, World->GetGravityZ() * ProjectileGravityScale);
            FVector Pt = SpawnStart
                + LaunchVelocity * tSim
                + 0.5f * GravAccel * tSim * tSim;

            DrawDebugLine(World, PrevPt, Pt, FColor::Cyan, false, 0.0f, 0, 1.0f);
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

    // 2) Horizontal trace
    UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
    FVector TraceStart = Capsule
        ? Capsule->GetComponentLocation()
        : OutStart;
    FVector TraceEnd = TraceStart + Owner->GetActorForwardVector() * CurrentEffectiveRange;

    FHitResult Hit;
    FCollisionQueryParams Params(TEXT("ThrowTrace"), false, Owner);
    bool bHit = World->LineTraceSingleByChannel(
        Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params
    );

    FVector LandXY = TraceStart + Owner->GetActorForwardVector() * CurrentEffectiveRange;
    float BaseZ = bHit ? Hit.Location.Z : TraceStart.Z;

    // 3) Snap-up using apex-driven trace
    float H = MaxArcHeight * ArcParam;
    float g = -World->GetGravityZ() * ProjectileGravityScale;
    float Vz = FMath::Sqrt(2.0f * g * H);

    FVector UpStart = FVector(LandXY.X, LandXY.Y, BaseZ + H);
    FHitResult UpHit;
    if (World->LineTraceSingleByChannel(
        UpHit,
        UpStart,
        FVector(LandXY.X, LandXY.Y, BaseZ),
        ECC_WorldStatic,
        Params
    ))
    {
        BaseZ = UpHit.Location.Z;
    }

    OutAimPoint = FVector(LandXY.X, LandXY.Y, BaseZ);

    // 4) Solve flight time & velocity
    float DeltaZ = OutAimPoint.Z - OutStart.Z;
    float Discr = Vz * Vz - 2.0f * g * DeltaZ;
    if (Discr < 0.0f) return false;

    float Time = (Vz + FMath::Sqrt(Discr)) / g;

    FVector Dir2D = FVector(OutAimPoint.X - OutStart.X, OutAimPoint.Y - OutStart.Y, 0.0f);
    float Dist2D = Dir2D.Size();
    if (Dist2D < KINDA_SMALL_NUMBER) return false;
    Dir2D.Normalize();

    float Vx = Dist2D / Time;
    OutVelocity = Dir2D * Vx + FVector(0, 0, Vz);

    return true;
}

void UThrowAimComponent::UpdateGroundReticle(
    const FVector& SpawnStart,
    const FVector& AimPoint
)
{
    UWorld* World = GetWorld();
    if (!World || ReticleSampleCount < 2)
        return;

    // Prepare horizontal direction vector
    FVector Dir2D = (AimPoint - SpawnStart);
    Dir2D.Z = 0.0f;
    float TotalDist = Dir2D.Size();
    if (TotalDist < KINDA_SMALL_NUMBER)
        return;
    Dir2D.Normalize();

    // Sample along the horizontal line
    TArray<FVector> GroundPoints;
    GroundPoints.Reserve(ReticleSampleCount);

    for (int32 i = 0; i < ReticleSampleCount; ++i)
    {
        float Alpha = float(i) / float(ReticleSampleCount - 1);
        FVector HorizontalPt = SpawnStart + Dir2D * (TotalDist * Alpha);

        // Project straight down
        FVector TraceStart = HorizontalPt + FVector(0, 0, ReticleTraceHeight);
        FVector TraceEnd = HorizontalPt - FVector(0, 0, ReticleTraceHeight);
        FHitResult Hit;

        FCollisionObjectQueryParams ObjParams;
        ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);  // only static world
        FCollisionQueryParams Params(TEXT("ReticleTrace"), false, GetOwner());

        FVector GroundPt = HorizontalPt;
        if (World->LineTraceSingleByObjectType(
            Hit,
            TraceStart,
            TraceEnd,
            ObjParams,
            Params
        ))
        {
            GroundPt = Hit.Location;
        }

        GroundPoints.Add(GroundPt);
    }

    // Draw a continuous green polyline
    for (int32 i = 1; i < GroundPoints.Num(); ++i)
    {
        DrawDebugLine(
            World,
            GroundPoints[i - 1],
            GroundPoints[i],
            FColor::Emerald,
            false,  // not persistent
            0.0f,
            0,
            3.0f    // thickness
        );
    }
}
