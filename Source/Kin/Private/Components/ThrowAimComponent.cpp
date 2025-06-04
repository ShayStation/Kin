#include "Components/ThrowAimComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/OverlapResult.h"
#include "CollisionShape.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LockOnTargetComponent.h"
#include "Components/SplineComponent.h"

#include "Abilities/ThrownProjectile.h"
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

    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !Owner)
    {
        return;
    }

    // --- LOCK-ON LOGIC (always runs) ---
    if (LockedTarget)
    {
        // auto-release if out of manual-lock range
        float Dist = FVector::Dist(
            LockedTarget->GetActorLocation(),
            Owner->GetActorLocation()
        );
        if (Dist > ManualLockRange)
        {
            ReleaseManualLock();
        }

        // debug: persistent lock line
        if (bDebugDraw)
        {
            DrawDebugLine(
                World,
                Owner->GetActorLocation(),
                LockedTarget->GetActorLocation(),
                FColor::Red,
                false,
                0.f,
                0,
                1.5f
            );
        }
    }
    else
    {
        PerformSoftLock(DeltaTime);
    }

    // —— ONE-TIME AIM INITIALIZATION ——  
    if (!bHasInitializedAim)
    {
        SmoothedAimDirection = Owner->GetActorForwardVector();
        CurrentEffectiveRange = 0.f;
        LastSmoothedAimDirection = SmoothedAimDirection;
        LastEffectiveRange = 0.f;
        LastSpawnStart = Owner->GetActorLocation();
        LastLaunchVelocity = FVector::ZeroVector;
        LastAimPoint = LastSpawnStart;
        bHasInitializedAim = true;
    }

    // —— OPTIONAL: gate heavy throw-arc debug here ——  
    if (!bDebugDraw)
    {
        return;
    }

    // — read stick input magnitude —
    const float AimMag = AimInput.Size();

    // — compute camera-oriented axes —
    FVector Forward, Right;
    if (APlayerController* PC = Cast<APlayerController>(Owner->GetInstigatorController()))
    {
        float Yaw = PC->GetControlRotation().Yaw;
        FRotator RotY(0.f, Yaw, 0.f);
        Forward = FRotationMatrix(RotY).GetUnitAxis(EAxis::X);
        Right = FRotationMatrix(RotY).GetUnitAxis(EAxis::Y);
    }
    else
    {
        Forward = Owner->GetActorForwardVector();
        Right = Owner->GetActorRightVector();
    }

    // — OUTWARD (stick beyond DeadZone) —
    if (AimMag > DeadZone)
    {
        // 1) Smooth direction
        FVector DesiredDir = SmoothedAimDirection;
        if (AimInput.SizeSquared() > KINDA_SMALL_NUMBER)
        {
            DesiredDir = (Forward * AimInput.Y + Right * AimInput.X).GetSafeNormal();
        }
        SmoothedAimDirection = FMath::VInterpTo(
            SmoothedAimDirection,
            DesiredDir,
            DeltaTime,
            DirectionInterpSpeed * MovementSpeedModifier
        );

        // 2) Smooth range
        if (AimMag > KINDA_SMALL_NUMBER)
        {
            float Ratio = FMath::Min(AimMag / MovementThreshold, 1.f);
            float TargetRange = Ratio * MaxTraceDistance;
            CurrentEffectiveRange = FMath::FInterpTo(
                CurrentEffectiveRange,
                TargetRange,
                DeltaTime,
                RangeInterpSpeed * MovementSpeedModifier
            );
        }

        // 3) Wall clamp (ignore own projectiles)
        UCapsuleComponent* Cap = Owner->FindComponentByClass<UCapsuleComponent>();
        FVector Start = Cap
            ? Cap->GetComponentLocation()
            : Owner->GetActorLocation();

        FHitResult Hit;
        FCollisionQueryParams Params(TEXT("WallClamp"), false, Owner);
        bool bHit = World->LineTraceSingleByChannel(
            Hit,
            Start,
            Start + SmoothedAimDirection * MaxTraceDistance,
            ECC_WorldStatic,
            Params
        );
        if (bHit && Hit.GetActor() && Hit.GetActor()->IsA<AThrownProjectile>())
        {
            bHit = false;
        }

        float DistanceToWall2D = bHit
            ? (Hit.Location - Start).Size2D()
            : MaxTraceDistance;
        float WallClampRange = FMath::Clamp(
            DistanceToWall2D + ClearanceBuffer,
            0.f,
            MaxTraceDistance
        );

        // 4) Apply clamp
        float UseRange = FMath::Min(CurrentEffectiveRange, WallClampRange);
        CurrentEffectiveRange = UseRange;

        // 5) Compute and cache throw
        FVector SpawnStart, LaunchVel, AimPt;
        if (ComputeThrow(SpawnStart, LaunchVel, AimPt))
        {
            LastSmoothedAimDirection = SmoothedAimDirection;
            LastEffectiveRange = UseRange;
            LastSpawnStart = SpawnStart;
            LastLaunchVelocity = LaunchVel;
            LastAimPoint = AimPt;
        }
    }
    // — INWARD (stick past PullThreshold) —
    else if (AimMag > PullThreshold)
    {
        FVector WorldIn = (Forward * AimInput.Y + Right * AimInput.X).GetSafeNormal();
        float DotIn = FVector::DotProduct(WorldIn, LastSmoothedAimDirection);

        if (DotIn < -PullThreshold)
        {
            float NewRange = FMath::FInterpTo(
                LastEffectiveRange,
                0.f,
                DeltaTime,
                RangeInterpSpeed * MovementSpeedModifier
            );
            CurrentEffectiveRange = NewRange;

            FVector SpawnStart, LaunchVel, AimPt;
            if (ComputeThrow(SpawnStart, LaunchVel, AimPt))
            {
                LastEffectiveRange = NewRange;
                LastSpawnStart = SpawnStart;
                LastLaunchVelocity = LaunchVel;
                LastAimPoint = AimPt;
            }
        }
    }

    // — DRAW TRAJECTORY DEBUG based on cached values —

    UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
    FVector TraceStart = Capsule
        ? Capsule->GetComponentLocation()
        : Owner->GetActorLocation();

    // 6) Range line
    DrawDebugLine(
        World,
        TraceStart,
        TraceStart + LastSmoothedAimDirection * LastEffectiveRange,
        FColor::Blue,
        false,
        0.f,
        0,
        2.f
    );

    // 7) Landing point
    DrawDebugSphere(
        World,
        LastAimPoint,
        8.f,
        12,
        FColor::Green,
        false,
        0.f
    );

    // 8) Velocity arrow
    DrawDebugLine(
        World,
        LastSpawnStart,
        LastSpawnStart + LastLaunchVelocity.GetSafeNormal() * 100.f,
        FColor::Yellow,
        false,
        0.f,
        0,
        2.f
    );

    // 9) Apex label
    {
        float WorldG = -World->GetGravityZ() * ProjectileGravityScale;
        float Apex = FMath::Square(LastLaunchVelocity.Z) / (2.f * WorldG);
        DrawDebugString(
            World,
            LastSpawnStart + FVector(0, 0, Apex),
            FString::Printf(TEXT("Apex: %.1f"), Apex),
            nullptr,
            FColor::White,
            0.f
        );
    }

    // 10) Ground reticle
    UpdateGroundReticle(LastSpawnStart, LastAimPoint);

    // 11) Sampled trajectory
    {
        const int32 Segs = ReticleSampleCount;
        float WorldG = -World->GetGravityZ() * ProjectileGravityScale;
        float DeltaZ = LastAimPoint.Z - LastSpawnStart.Z;
        float Vz = LastLaunchVelocity.Z;
        float Discr = (Vz * Vz) - 2.f * WorldG * DeltaZ;

        if (Discr > 0.f)
        {
            float TotalT = (Vz + FMath::Sqrt(Discr)) / WorldG;
            FVector Prev = LastSpawnStart;
            for (int32 i = 1; i <= Segs; ++i)
            {
                float tSim = (float(i) / Segs) * TotalT * TimeScale;
                FVector Grav(0, 0, -WorldG);
                FVector Pt = LastSpawnStart
                    + LastLaunchVelocity * tSim
                    + 0.5f * Grav * tSim * tSim;

                DrawDebugLine(
                    World,
                    Prev,
                    Pt,
                    FColor::Cyan,
                    false,
                    0.f,
                    0,
                    1.f
                );
                Prev = Pt;
            }
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
    FVector TraceStart = Capsule
        ? Capsule->GetComponentLocation()
        : OutStart;
    FVector TraceEnd = TraceStart + SmoothedAimDirection * CurrentEffectiveRange;

    FHitResult Hit;
    FCollisionQueryParams Params(TEXT("ThrowTrace"), false, Owner);
    bool bHit = World->LineTraceSingleByChannel(
        Hit,
        TraceStart,
        TraceEnd,
        ECC_WorldStatic,
        Params
    );
    // ignore your own projectile hits
    if (bHit && Hit.GetActor() && Hit.GetActor()->IsA<AThrownProjectile>())
    {
        bHit = false;
    }

    FVector LandXY = TraceStart + SmoothedAimDirection * CurrentEffectiveRange;
    float BaseZ = bHit ? Hit.Location.Z : TraceStart.Z;

    // 3) Apex?driven upward trace
    float H = MaxArcHeight * ArcParam;
    float g = -World->GetGravityZ() * ProjectileGravityScale;
    float VzInit = FMath::Sqrt(2.f * g * H);
    FVector UpStart = FVector(LandXY.X, LandXY.Y, BaseZ + H);

    FHitResult UpHit;
    bool bApexHit = World->LineTraceSingleByChannel(
        UpHit,
        UpStart,
        FVector(LandXY.X, LandXY.Y, BaseZ),
        ECC_WorldStatic,
        Params
    );
    if (bApexHit && UpHit.GetActor() && UpHit.GetActor()->IsA<AThrownProjectile>())
    {
        // ignore projectile hit
    }
    else if (bApexHit)
    {
        BaseZ = UpHit.Location.Z;
    }

    OutAimPoint = FVector(LandXY.X, LandXY.Y, BaseZ);

    // 4) Solve for flight time & velocity
    float DeltaZ = OutAimPoint.Z - OutStart.Z;
    float Discr2 = VzInit * VzInit - 2.f * g * DeltaZ;
    if (Discr2 < 0.f) return false;
    float Time = (VzInit + FMath::Sqrt(Discr2)) / g;

    FVector Dir2D(OutAimPoint.X - OutStart.X, OutAimPoint.Y - OutStart.Y, 0.f);
    float Dist2D = Dir2D.Size();
    if (Dist2D < KINDA_SMALL_NUMBER) return false;
    Dir2D.Normalize();

    float Vx = Dist2D / Time;
    OutVelocity = Dir2D * Vx + FVector(0.f, 0.f, VzInit);

    return true;
}

void UThrowAimComponent::UpdateGroundReticle(
    const FVector& SpawnStart,
    const FVector& AimPoint
)
{
    UWorld* World = GetWorld();
    if (!World || ReticleSampleCount < 2) return;

    // Flatten direction to XY plane
    FVector Dir2D = AimPoint - SpawnStart;
    Dir2D.Z = 0.f;
    float TotalDist = Dir2D.Size();
    if (TotalDist < KINDA_SMALL_NUMBER) return;
    Dir2D.Normalize();

    // Sample points along the trajectory footprint
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
        ObjParams.AddObjectTypesToQuery(ECC_Pawn);
        ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
        ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);

        FCollisionQueryParams Params(TEXT("ReticleTrace"), false, GetOwner());
        FVector GroundPt = HorizontalPt;
        if (World->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjParams, Params))
        {
            GroundPt = Hit.Location;
        }
        GroundPoints.Add(GroundPt);
    }

    // Draw debug lines between sampled ground points
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


void UThrowAimComponent::PerformSoftLock(float DeltaTime)
{
    // 1) Compute current aim point
    FVector AimPoint, SpawnStart, LaunchVel;
    if (!ComputeThrow(SpawnStart, LaunchVel, AimPoint))
    {
        SoftLockTarget = nullptr;
        return;
    }

    // 2) Sphere-overlap around the aim point, including static, dynamic, and pawn objects
    TArray<FOverlapResult> Hits;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(SoftLockRadius);

    FCollisionObjectQueryParams ObjParams;
    ObjParams.AddObjectTypesToQuery(ECC_Pawn);
    ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);

    GetWorld()->OverlapMultiByObjectType(
        Hits,
        AimPoint,
        FQuat::Identity,
        ObjParams,
        Sphere
    );

    // 3) Find the closest lock-on candidate
    AActor* Best = nullptr;
    float    BestDist = SoftLockRadius;

    for (auto& Result : Hits)
    {
        AActor* Candidate = Result.GetActor();
        if (!Candidate) continue;

        if (Candidate->FindComponentByClass<ULockOnTargetComponent>())
        {
            float Dist = FVector::Dist(Candidate->GetActorLocation(), AimPoint);
            if (Dist < BestDist)
            {
                BestDist = Dist;
                Best = Candidate;
            }
        }
    }

    // 4) Store and (optionally) debug-draw
    SoftLockTarget = Best;

    if (bDebugDraw && SoftLockTarget)
    {
        DrawDebugSphere(
            GetWorld(),
            SoftLockTarget->GetActorLocation(),
            30.f,        // radius
            12,          // segments
            FColor::Green,
            false,       // persistent
            0.1f,        // lifeTime
            0,
            2.f          // thickness
        );
    }
}


void UThrowAimComponent::PerformManualLock()
{
    // 1) Don’t re-lock if already have one
    if (LockedTarget)
    {
        return;
    }

    // 2) Gather all lockable actors in range
    TArray<FOverlapResult> Hits;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(ManualLockRange);

    FCollisionObjectQueryParams ObjParams;
    ObjParams.AddObjectTypesToQuery(ECC_Pawn);
    ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);

    GetWorld()->OverlapMultiByObjectType(
        Hits,
        GetOwner()->GetActorLocation(),
        FQuat::Identity,
        ObjParams,
        Sphere
    );

    // 3) Score and pick the best candidate
    AActor* Best = nullptr;
    float    BestScore = -1.f;

    for (const FOverlapResult& R : Hits)
    {
        AActor* Candidate = R.GetActor();
        if (!Candidate)
        {
            continue;
        }

        if (ULockOnTargetComponent* LC = Candidate->FindComponentByClass<ULockOnTargetComponent>())
        {
            float Dist = FVector::Dist(
                Candidate->GetActorLocation(),
                GetOwner()->GetActorLocation()
            );
            float Score = LC->LockPriority / Dist;
            if (Score > BestScore)
            {
                BestScore = Score;
                Best = Candidate;
            }
        }
    }

    // 4) Lock onto it (and debug?draw)
    if (Best)
    {
        LockedTarget = Best;

        if (bDebugDraw)
        {
            DrawDebugSphere(
                GetWorld(),
                LockedTarget->GetActorLocation(),
                60.f,        // radius
                16,          // segments
                FColor::Red, // color
                false,       // persistent? 
                2.0f,        // lifeTime
                0,
                3.f          // thickness
            );

            UE_LOG(LogTemp, Warning, TEXT("PerformManualLock(): locked onto %s"),
                *LockedTarget->GetName());
        }
    }
}


void UThrowAimComponent::ReleaseManualLock()
{
    // 1) Clear the locked target
    LockedTarget = nullptr;

    // 2) (Optional) Debug indication of unlock
    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Warning, TEXT("ReleaseManualLock(): target unlocked"));
        // Example: small white sphere at player location
        DrawDebugSphere(
            GetWorld(),
            GetOwner()->GetActorLocation(),
            40.f,          // radius
            12,            // segments
            FColor::White, // color
            false,         // persistent
            1.0f,          // lifeTime
            0,
            2.f            // thickness
        );
    }
}

